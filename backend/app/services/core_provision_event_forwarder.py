from __future__ import annotations

import json
import os
import socket
from datetime import datetime, timezone
from typing import Any

from redis.exceptions import ResponseError

from app.core.config import get_settings
from app.core.events.ws_event_publisher import WsEventPublisher
from app.core.logger import get_logger
from app.infrastructure.redis.manager import RedisManager
from app.schemas.ws.events import CoreProvisionStateEvent

settings = get_settings()
logger = get_logger("core.provision.forwarder")

STREAM_NAME = settings.core_provision_event_stream
GROUP_NAME = "core-provision-events"
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


async def _ensure_group(redis) -> None:
    try:
        await redis.xgroup_create(STREAM_NAME, GROUP_NAME, id="0", mkstream=True)
        logger.info("✅ Created consumer group %s for core provision events", GROUP_NAME)
    except ResponseError as exc:
        if "BUSYGROUP" in str(exc):
            logger.info("ℹ️ Core provision event consumer group already exists")
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
        logger.info("🔁 Replaying %d pending core provision events", len(entries))
        await _process_entries(redis, entries)


async def _process_entries(redis, entries) -> None:
    for entry_id, fields in entries:
        try:
            payload = _parse_json_field(fields)
            if not payload:
                logger.warning("⚠️ Invalid core provision event payload in %s", entry_id)
            elif str(payload.get("event") or "") == "state":
                await WsEventPublisher.publish(
                    CoreProvisionStateEvent(
                        snapshot=payload,
                        changed_at=_parse_changed_at(payload),
                    )
                )
        except Exception as exc:  # noqa: BLE001
            logger.exception("💥 Failed to forward core provision event %s: %s", entry_id, exc)
        finally:
            await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)


async def forward_core_provision_events() -> None:
    redis = RedisManager.get_instance()
    await _ensure_group(redis)
    await _drain_pending(redis)
    while True:
        entries = await _fetch(redis, ">")
        if not entries:
            continue
        await _process_entries(redis, entries)

