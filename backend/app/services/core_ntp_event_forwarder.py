from __future__ import annotations

import json
import os
import socket
import time
from datetime import datetime, timezone
from collections.abc import Mapping
from typing import cast

from redis.exceptions import ResponseError

from app.core.config import get_settings
from app.core.events.ws_event_publisher import WsEventPublisher
from app.core.logger import get_logger
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.types import RedisStreamClient, RedisStreamEntries
from app.schemas.ws.events import CoreNtpStateEvent
from app.services.worker_health import mark_clock_adjustment_grace


settings = get_settings()
logger = get_logger("core.ntp.forwarder")

STREAM_NAME = settings.core_ntp_event_stream
GROUP_NAME = "core-ntp-events"
CONSUMER_NAME = f"{socket.gethostname()}-{os.getpid()}"
_last_state_system_time: datetime | None = None
_last_state_monotonic: float | None = None


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


async def _ensure_group(redis: RedisStreamClient) -> None:
    try:
        _ = await redis.xgroup_create(STREAM_NAME, GROUP_NAME, id="0", mkstream=True)
        logger.info("✅ Created consumer group %s for core NTP events", GROUP_NAME)
    except ResponseError as exc:
        if "BUSYGROUP" in str(exc):
            logger.info("ℹ️ Core NTP event consumer group already exists")
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
        logger.info("🔁 Replaying %d pending core NTP events", len(entries))
        await _process_entries(redis, entries)


async def _process_entries(redis: RedisStreamClient, entries: RedisStreamEntries) -> None:
    global _last_state_system_time, _last_state_monotonic
    for entry_id, fields in entries:
        try:
            payload = _parse_json_field(fields)
            if not payload:
                logger.warning("⚠️ Invalid core NTP event payload in %s", entry_id)
            elif str(payload.get("event") or "") in {"system_time_set_started", "system_time_set_success"}:
                await mark_clock_adjustment_grace()
            elif str(payload.get("event") or "") == "state":
                current_system_time = _parse_changed_at(payload)
                current_monotonic = time.monotonic()
                if _last_state_system_time is not None and _last_state_monotonic is not None:
                    wall_delta = (current_system_time - _last_state_system_time).total_seconds()
                    monotonic_delta = current_monotonic - _last_state_monotonic
                    if abs(wall_delta - monotonic_delta) > 5:
                        await mark_clock_adjustment_grace()
                _last_state_system_time = current_system_time
                _last_state_monotonic = current_monotonic
                await WsEventPublisher.publish(
                    CoreNtpStateEvent(
                        snapshot=payload,
                        changed_at=_parse_changed_at(payload),
                    )
                )
        except Exception as exc:  # noqa: BLE001
            logger.exception("💥 Failed to forward core NTP event %s: %s", entry_id, exc)
        finally:
            _ = await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)


async def forward_core_ntp_events() -> None:
    redis = cast(RedisStreamClient, cast(object, RedisManager.get_instance()))
    await _ensure_group(redis)
    await _drain_pending(redis)
    while True:
        entries = await _fetch(redis, ">")
        if not entries:
            continue
        await _process_entries(redis, entries)
