from __future__ import annotations

import json
import hashlib
import os
import socket
from datetime import datetime, timezone
from collections.abc import Mapping
from typing import cast

from redis.exceptions import ResponseError

from app.core.config import get_settings
from app.core.events.ws_event_publisher import WsEventPublisher
from app.core.logger import get_logger
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.types import RedisStreamClient, RedisStreamEntries
from app.schemas.ws.events import CoreDiagnosticsStateEvent
from app.services.core_diagnostics_incident import DiagnosticsRedisClient, is_incident_acknowledged


settings = get_settings()
logger = get_logger("core.diag.forwarder")

STREAM_NAME = settings.core_diag_event_stream
GROUP_NAME = "core-diag-events"
CONSUMER_NAME = f"{socket.gethostname()}-{os.getpid()}"


def _parse_json_field(fields: Mapping[str, object]) -> dict[str, object] | None:
    raw = fields.get("json")
    if not isinstance(raw, str):
        return None
    try:
        payload = cast(object, json.loads(raw))
    except json.JSONDecodeError:
        return None
    return cast(dict[str, object], payload) if isinstance(payload, dict) else None


def _parse_changed_at(payload: dict[str, object]) -> datetime:
    raw = payload.get("updated_at")
    if isinstance(raw, str):
        try:
            return datetime.fromisoformat(raw)
        except ValueError:
            pass
    return datetime.now(timezone.utc)


def _incident_id(payload: dict[str, object]) -> str | None:
    """Build a stable identity from diagnostic categories, not measurements."""
    mode = str(payload.get("mode") or "unknown").strip().lower()
    categories: list[str] = []
    cpu = _payload_mapping(payload.get("cpu"))
    memory = _payload_mapping(payload.get("memory"))
    disk = _payload_mapping(payload.get("disk_root"))
    if (temperature := _float_or_none(cpu.get("temperature_c"))) is not None and temperature >= 85:
        categories.append("cpu_temperature")
    if (used_memory := _float_or_none(memory.get("used_percent"))) is not None and used_memory >= 95:
        categories.append("memory_pressure")
    if (used_disk := _float_or_none(disk.get("used_percent"))) is not None and used_disk >= 95:
        categories.append("disk_pressure")
    inactive = sorted(
        str(service.get("name") or "").strip()
        for service in _payload_list(payload.get("services"))
        if str(service.get("name") or "") in {"docker", "NetworkManager"}
        and service.get("active") is False
    )
    if inactive:
        categories.append(f"inactive_services:{','.join(inactive)}")
    if not categories and mode not in {"error", "degraded"}:
        return None
    canonical = json.dumps(
        {"mode": mode, "categories": sorted(categories)},
        ensure_ascii=True,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return f"core-diag:{hashlib.sha256(canonical).hexdigest()[:24]}"


def _payload_mapping(value: object) -> dict[str, object]:
    return cast(dict[str, object], value) if isinstance(value, dict) else {}


def _payload_list(value: object) -> list[dict[str, object]]:
    if not isinstance(value, list):
        return []
    items = cast(list[object], value)
    return [cast(dict[str, object], item) for item in items if isinstance(item, dict)]


def _float_or_none(value: object) -> float | None:
    try:
        return float(cast(str | int | float, value))
    except (TypeError, ValueError):
        return None


async def _ensure_group(redis: RedisStreamClient) -> None:
    try:
        _ = await redis.xgroup_create(STREAM_NAME, GROUP_NAME, id="0", mkstream=True)
        logger.info("✅ Created consumer group %s for core diagnostics events", GROUP_NAME)
    except ResponseError as exc:
        if "BUSYGROUP" in str(exc):
            logger.info("ℹ️ Core diagnostics event consumer group already exists")
        else:
            raise


async def _fetch(redis: RedisStreamClient, stream_id: str, block_ms: int = 5000) -> RedisStreamEntries:
    result = await redis.xreadgroup(
        GROUP_NAME,
        CONSUMER_NAME,
        streams={STREAM_NAME: stream_id},
        count=100,
        block=block_ms,
    )
    if not result:
        return []
    return result[0][1]


async def _drain_pending(redis: RedisStreamClient) -> None:
    while True:
        entries = await _fetch(redis, "0", block_ms=100)
        if not entries:
            break
        logger.info("🔁 Replaying %d pending core diagnostics events", len(entries))
        await _process_entries(redis, entries)


async def _process_entries(redis: RedisStreamClient, entries: RedisStreamEntries) -> None:
    for entry_id, fields in entries:
        try:
            payload = _parse_json_field(fields)
            if not payload:
                logger.warning("⚠️ Invalid core diagnostics event payload in %s", entry_id)
            elif str(payload.get("event") or "") == "state":
                snapshot = {**payload}
                incident_id = _incident_id(snapshot)
                if incident_id is None:
                    _ = snapshot.pop("incident_id", None)
                else:
                    snapshot["incident_id"] = incident_id
                    snapshot["incident_acknowledged"] = await is_incident_acknowledged(
                        cast(DiagnosticsRedisClient, cast(object, redis)),
                        hostname=str(snapshot.get("hostname") or "unknown"),
                        incident_id=incident_id,
                    )
                await WsEventPublisher.publish(
                    CoreDiagnosticsStateEvent(
                        snapshot=snapshot,
                        changed_at=_parse_changed_at(payload),
                    )
                )
        except Exception as exc:  # noqa: BLE001
            logger.exception("💥 Failed to forward core diagnostics event %s: %s", entry_id, exc)
        finally:
            _ = await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)


async def forward_core_diag_events() -> None:
    redis = cast(RedisStreamClient, cast(object, RedisManager.get_instance()))
    await _ensure_group(redis)
    await _drain_pending(redis)
    while True:
        entries = await _fetch(redis, ">")
        if not entries:
            continue
        await _process_entries(redis, entries)
