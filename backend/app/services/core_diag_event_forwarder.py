from __future__ import annotations

import json
import hashlib
import os
import socket
from datetime import datetime, timezone
from typing import Any

from redis.exceptions import ResponseError

from app.core.config import get_settings
from app.core.events.ws_event_publisher import WsEventPublisher
from app.core.logger import get_logger
from app.infrastructure.redis.manager import RedisManager
from app.schemas.ws.events import CoreDiagnosticsStateEvent

settings = get_settings()
logger = get_logger("core.diag.forwarder")

STREAM_NAME = settings.core_diag_event_stream
GROUP_NAME = "core-diag-events"
CONSUMER_NAME = f"{socket.gethostname()}-{os.getpid()}"


def _parse_json_field(fields: dict[str, Any]) -> dict[str, Any] | None:
    raw = fields.get("json")
    if not isinstance(raw, str):
        return None
    try:
        payload = json.loads(raw)
    except json.JSONDecodeError:
        return None
    return payload if isinstance(payload, dict) else None


def _parse_changed_at(payload: dict[str, Any]) -> datetime:
    raw = payload.get("updated_at")
    if isinstance(raw, str):
        try:
            return datetime.fromisoformat(raw)
        except ValueError:
            pass
    return datetime.now(timezone.utc)


def _incident_id(payload: dict[str, Any]) -> str | None:
    """Build a stable identity from diagnostic categories, not measurements."""
    mode = str(payload.get("mode") or "unknown").strip().lower()
    categories: list[str] = []
    cpu = payload.get("cpu") if isinstance(payload.get("cpu"), dict) else {}
    memory = payload.get("memory") if isinstance(payload.get("memory"), dict) else {}
    disk = payload.get("disk_root") if isinstance(payload.get("disk_root"), dict) else {}
    try:
        if float(cpu.get("temperature_c")) >= 85:
            categories.append("cpu_temperature")
    except (TypeError, ValueError):
        pass
    try:
        if float(memory.get("used_percent")) >= 95:
            categories.append("memory_pressure")
    except (TypeError, ValueError):
        pass
    try:
        if float(disk.get("used_percent")) >= 95:
            categories.append("disk_pressure")
    except (TypeError, ValueError):
        pass
    inactive = sorted(
        str(service.get("name") or "").strip()
        for service in (payload.get("services") if isinstance(payload.get("services"), list) else [])
        if isinstance(service, dict)
        and str(service.get("name") or "") in {"docker", "NetworkManager"}
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


async def _ensure_group(redis) -> None:
    try:
        await redis.xgroup_create(STREAM_NAME, GROUP_NAME, id="0", mkstream=True)
        logger.info("✅ Created consumer group %s for core diagnostics events", GROUP_NAME)
    except ResponseError as exc:
        if "BUSYGROUP" in str(exc):
            logger.info("ℹ️ Core diagnostics event consumer group already exists")
        else:
            raise


async def _fetch(redis, stream_id: str, block_ms: int = 5000):
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


async def _drain_pending(redis) -> None:
    while True:
        entries = await _fetch(redis, "0", block_ms=100)
        if not entries:
            break
        logger.info("🔁 Replaying %d pending core diagnostics events", len(entries))
        await _process_entries(redis, entries)


async def _process_entries(redis, entries) -> None:
    for entry_id, fields in entries:
        try:
            payload = _parse_json_field(fields)
            if not payload:
                logger.warning("⚠️ Invalid core diagnostics event payload in %s", entry_id)
            elif str(payload.get("event") or "") == "state":
                snapshot = {**payload}
                incident_id = _incident_id(snapshot)
                if incident_id is None:
                    snapshot.pop("incident_id", None)
                else:
                    snapshot["incident_id"] = incident_id
                await WsEventPublisher.publish(
                    CoreDiagnosticsStateEvent(
                        snapshot=snapshot,
                        changed_at=_parse_changed_at(payload),
                    )
                )
        except Exception as exc:  # noqa: BLE001
            logger.exception("💥 Failed to forward core diagnostics event %s: %s", entry_id, exc)
        finally:
            await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)


async def forward_core_diag_events() -> None:
    redis = RedisManager.get_instance()
    await _ensure_group(redis)
    await _drain_pending(redis)
    while True:
        entries = await _fetch(redis, ">")
        if not entries:
            continue
        await _process_entries(redis, entries)
