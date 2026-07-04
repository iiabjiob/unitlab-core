from __future__ import annotations

import asyncio
import json
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Any

from app.core.events.ws_event_publisher import WsEventPublisher
from app.core.logger import get_logger
from app.infrastructure.redis.manager import RedisManager
from app.schemas.verification_schema import VerificationMmsReachabilityTargetSchema
from app.schemas.ws.events import (
    ExternalIedStatusChangedEvent,
    ExternalIedStatusRecord,
    ExternalIedStatusSnapshotEvent,
)
from app.services.verification_mms_reachability import check_mms_tcp_endpoint

logger = get_logger("external_ied")

EXPECTED_POLL_MS = 1_200
OFFLINE_POLL_MS = 5_000
REACHABLE_POLL_MS = 30_000
CHECK_TIMEOUT_MS = 1_200
MAX_CONCURRENT_CHECKS = 3
TARGET_WORKSPACES_KEY = "external_ied:workspaces"


@dataclass(frozen=True)
class ExternalIedTarget:
    ip: str
    port: int
    signal_ids: tuple[int, ...]


def _targets_key(workspace_id: int) -> str:
    return f"external_ied:workspace:{workspace_id}:targets"


def _status_key(workspace_id: int) -> str:
    return f"external_ied:workspace:{workspace_id}:status"


def _lock_key(workspace_id: int, ip: str) -> str:
    return f"external_ied:workspace:{workspace_id}:lock:{ip}"


def _endpoint_key(ip: str, port: int) -> str:
    return f"{ip}:{port}"


def _utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def _normalize_ip(value: Any) -> str | None:
    text = str(value or "").strip()
    parts = text.split(".")
    if len(parts) != 4:
        return None
    try:
        octets = [int(part, 10) for part in parts]
    except ValueError:
        return None
    if any(octet < 0 or octet > 255 for octet in octets):
        return None
    return ".".join(str(octet) for octet in octets)


def _normalize_signal_ids(values: Any) -> tuple[int, ...]:
    if not isinstance(values, list | tuple):
        return ()
    seen: set[int] = set()
    normalized: list[int] = []
    for raw in values:
        try:
            signal_id = int(raw)
        except (TypeError, ValueError):
            continue
        if signal_id <= 0 or signal_id in seen:
            continue
        seen.add(signal_id)
        normalized.append(signal_id)
    return tuple(sorted(normalized))


def _normalize_port(value: Any) -> int:
    try:
        port = int(value)
    except (TypeError, ValueError):
        return 102
    return port if 1 <= port <= 65535 else 102


def _signal_ids_from_json_payload(payload: str | None) -> tuple[int, ...]:
    if not payload:
        return ()
    try:
        parsed = json.loads(payload)
    except json.JSONDecodeError:
        return ()
    if not isinstance(parsed, dict):
        return ()
    return _normalize_signal_ids(parsed.get("signal_ids"))


def _target_from_json_payload(endpoint: str, payload: str | None) -> ExternalIedTarget | None:
    if not payload:
        return None
    try:
        parsed = json.loads(payload)
    except json.JSONDecodeError:
        return None
    if not isinstance(parsed, dict):
        return None
    ip = _normalize_ip(parsed.get("ip"))
    if not ip:
        ip = _normalize_ip(endpoint.rsplit(":", 1)[0])
    if not ip:
        return None
    signal_ids = _normalize_signal_ids(parsed.get("signal_ids"))
    if not signal_ids:
        return None
    return ExternalIedTarget(ip=ip, port=_normalize_port(parsed.get("port")), signal_ids=signal_ids)


def normalize_external_ied_targets(raw_targets: list[Any]) -> dict[str, ExternalIedTarget]:
    targets: dict[str, ExternalIedTarget] = {}
    for raw in raw_targets:
        if not isinstance(raw, dict):
            continue
        ip = _normalize_ip(raw.get("ip"))
        port = _normalize_port(raw.get("port"))
        signal_ids = _normalize_signal_ids(raw.get("signal_ids"))
        if not ip or not signal_ids:
            continue
        key = _endpoint_key(ip, port)
        existing = targets.get(key)
        merged = tuple(sorted(set(signal_ids).union(existing.signal_ids if existing else ())))
        targets[key] = ExternalIedTarget(ip=ip, port=port, signal_ids=merged)
    return targets


def _record_from_payload(ip: str, payload: str | None, target: ExternalIedTarget | None = None) -> ExternalIedStatusRecord:
    parsed: dict[str, Any] = {}
    if payload:
        try:
            candidate = json.loads(payload)
            if isinstance(candidate, dict):
                parsed = candidate
        except json.JSONDecodeError:
            parsed = {}
    signal_ids = list(target.signal_ids if target else _normalize_signal_ids(parsed.get("signal_ids")))
    return ExternalIedStatusRecord(
        ip=ip,
        port=target.port if target else _normalize_port(parsed.get("port")),
        status=parsed.get("status") if parsed.get("status") in {"unknown", "expected", "reachable", "offline"} else "expected",
        signal_ids=signal_ids,
        last_checked_at=parsed.get("last_checked_at") if isinstance(parsed.get("last_checked_at"), str) else None,
        last_error=parsed.get("last_error") if isinstance(parsed.get("last_error"), str) else None,
        check_kind=parsed.get("check_kind") if parsed.get("check_kind") in {"none", "tcp_connect"} else "none",
        failure_code=parsed.get("failure_code") if parsed.get("failure_code") in {"unreachable", "mms_unavailable", "network_unreachable", "probe_failed"} else None,
    )


def _serialize_record(record: ExternalIedStatusRecord, *, next_check_at_ms: int | None = None) -> str:
    payload = record.model_dump(mode="json")
    if next_check_at_ms is not None:
        payload["next_check_at_ms"] = next_check_at_ms
    return json.dumps(payload, separators=(",", ":"))


def _next_interval_ms(status: str) -> int:
    if status == "offline":
        return OFFLINE_POLL_MS
    if status == "reachable":
        return REACHABLE_POLL_MS
    return EXPECTED_POLL_MS


async def configure_external_ied_targets(workspace_id: int, raw_targets: list[Any]) -> ExternalIedStatusSnapshotEvent:
    redis = RedisManager.get_instance()
    targets = normalize_external_ied_targets(raw_targets)
    target_key = _targets_key(workspace_id)
    status_key = _status_key(workspace_id)
    previous_targets = await redis.hgetall(target_key)
    previous_statuses = await redis.hgetall(status_key)
    previous_signal_ids = [
        signal_id
        for payload in previous_targets.values()
        for signal_id in _signal_ids_from_json_payload(payload)
    ]

    if not targets:
        if previous_targets or previous_statuses:
            await redis.delete(target_key, status_key)
            await redis.srem(TARGET_WORKSPACES_KEY, str(workspace_id))
            logger.info("External IED targets cleared | workspace=%s", workspace_id)
        event = ExternalIedStatusSnapshotEvent(
            workspace_id=workspace_id,
            devices=[],
            removed_signal_ids=sorted(set(previous_signal_ids)),
            emitted_at=_utc_now_iso(),
        )
        if previous_targets or previous_statuses:
            await WsEventPublisher.publish(event)
        return event

    await redis.sadd(TARGET_WORKSPACES_KEY, str(workspace_id))
    pipe = redis.pipeline()
    pipe.delete(target_key)
    pipe.delete(status_key)
    devices: list[ExternalIedStatusRecord] = []
    now_ms = int(time.time() * 1000)
    for key, target in targets.items():
        record = _record_from_payload(target.ip, previous_statuses.get(key), target)
        devices.append(record)
        pipe.hset(target_key, key, json.dumps({"ip": target.ip, "port": target.port, "signal_ids": list(target.signal_ids)}, separators=(",", ":")))
        pipe.hset(status_key, key, _serialize_record(record, next_check_at_ms=now_ms))
    await pipe.execute()

    endpoint_sample = ", ".join(sorted(targets.keys())[:8])
    logger.info(
        "External IED targets configured | workspace=%s endpoints=%s sample=%s",
        workspace_id,
        len(targets),
        endpoint_sample,
    )

    event = ExternalIedStatusSnapshotEvent(
        workspace_id=workspace_id,
        devices=sorted(devices, key=lambda item: tuple(int(part) for part in item.ip.split("."))),
        removed_signal_ids=sorted(set(previous_signal_ids).difference(signal_id for target in targets.values() for signal_id in target.signal_ids)),
        emitted_at=_utc_now_iso(),
    )
    await WsEventPublisher.publish(event)
    return event


async def list_external_ied_status_snapshots() -> list[ExternalIedStatusSnapshotEvent]:
    redis = RedisManager.get_instance()
    raw_workspace_ids = await redis.smembers(TARGET_WORKSPACES_KEY)
    events: list[ExternalIedStatusSnapshotEvent] = []
    for raw_workspace_id in raw_workspace_ids:
        try:
            workspace_id = int(raw_workspace_id)
        except (TypeError, ValueError):
            continue
        statuses = await redis.hgetall(_status_key(workspace_id))
        devices = [_record_from_payload(endpoint.rsplit(":", 1)[0], payload) for endpoint, payload in statuses.items()]
        events.append(
            ExternalIedStatusSnapshotEvent(
                workspace_id=workspace_id,
                devices=sorted(devices, key=lambda item: tuple(int(part) for part in item.ip.split("."))),
                removed_signal_ids=[],
                emitted_at=_utc_now_iso(),
            )
        )
    return events


async def _check_one(workspace_id: int, endpoint: str, target: ExternalIedTarget, previous: ExternalIedStatusRecord) -> None:
    redis = RedisManager.get_instance()
    locked = await redis.set(_lock_key(workspace_id, endpoint), "1", nx=True, ex=10)
    if not locked:
        return

    try:
        logger.debug(
            "External IED TCP probe started | workspace=%s endpoint=%s timeout_ms=%s",
            workspace_id,
            endpoint,
            CHECK_TIMEOUT_MS,
        )
        result = await check_mms_tcp_endpoint(
            VerificationMmsReachabilityTargetSchema(host=target.ip, port=target.port),
            timeout_ms=CHECK_TIMEOUT_MS,
        )
        logger.debug(
            "External IED TCP probe completed | workspace=%s endpoint=%s reachable=%s failure=%s error=%s checked_at=%s",
            workspace_id,
            endpoint,
            result.reachable,
            result.failure_code,
            result.error,
            result.checked_at,
        )
        next_status = "reachable" if result.reachable else "offline"
        checked_at = result.checked_at
        next_record = ExternalIedStatusRecord(
            ip=target.ip,
            port=target.port,
            status=next_status,
            signal_ids=list(target.signal_ids),
            last_checked_at=checked_at,
            last_error=result.error,
            check_kind=result.check_kind,
            failure_code=result.failure_code,
        )
        await redis.hset(
            _status_key(workspace_id),
            endpoint,
            _serialize_record(
                next_record,
                next_check_at_ms=int(time.time() * 1000) + _next_interval_ms(next_status),
            ),
        )
        if previous.status == next_status:
            return
        logger.info(
            "External IED status changed | workspace=%s endpoint=%s old=%s new=%s failure=%s",
            workspace_id,
            endpoint,
            previous.status,
            next_status,
            result.failure_code,
        )
        await WsEventPublisher.publish(
            ExternalIedStatusChangedEvent(
                workspace_id=workspace_id,
                ip=target.ip,
                port=target.port,
                old_status=previous.status,
                new_status=next_status,
                signal_ids=list(target.signal_ids),
                checked_at=checked_at,
                check_kind=result.check_kind,
                failure_code=result.failure_code,
                error=result.error,
            )
        )
    finally:
        await redis.delete(_lock_key(workspace_id, endpoint))


async def run_external_ied_availability_checker(stop_event: asyncio.Event | None = None) -> None:
    semaphore = asyncio.Semaphore(MAX_CONCURRENT_CHECKS)
    in_flight: set[tuple[int, str]] = set()

    async def guarded_check(workspace_id: int, endpoint: str, target: ExternalIedTarget, previous: ExternalIedStatusRecord) -> None:
        key = (workspace_id, endpoint)
        if key in in_flight:
            return
        in_flight.add(key)
        try:
            async with semaphore:
                await _check_one(workspace_id, endpoint, target, previous)
        except Exception as exc:  # noqa: BLE001
            logger.warning("External IED TCP check failed | workspace=%s endpoint=%s error=%s", workspace_id, endpoint, exc)
        finally:
            in_flight.discard(key)

    while stop_event is None or not stop_event.is_set():
        redis = RedisManager.get_instance()
        raw_workspace_ids = await redis.smembers(TARGET_WORKSPACES_KEY)
        now_ms = int(time.time() * 1000)
        for raw_workspace_id in raw_workspace_ids:
            try:
                workspace_id = int(raw_workspace_id)
            except (TypeError, ValueError):
                continue
            target_payloads = await redis.hgetall(_targets_key(workspace_id))
            status_payloads = await redis.hgetall(_status_key(workspace_id))
            targets = {
                endpoint: target
                for endpoint, payload in target_payloads.items()
                if (target := _target_from_json_payload(endpoint, payload)) is not None
            }
            for endpoint, target in targets.items():
                if not target.signal_ids:
                    continue
                payload = status_payloads.get(endpoint)
                previous = _record_from_payload(target.ip, payload, target)
                next_check_at_ms = now_ms
                if payload:
                    try:
                        parsed = json.loads(payload)
                        next_check_at_ms = int(parsed.get("next_check_at_ms") or now_ms)
                    except (TypeError, ValueError, json.JSONDecodeError):
                        next_check_at_ms = now_ms
                if next_check_at_ms <= now_ms:
                    asyncio.create_task(guarded_check(workspace_id, endpoint, target, previous))
        await asyncio.sleep(1)
