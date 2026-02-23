from __future__ import annotations

import asyncio
import json
from contextlib import suppress

from app.core.logger import get_logger
from app.infrastructure.redis.stream_bus import subscribe_ws_events
from app.ws.manager import WebSocketManager

logger = get_logger("ws.sub")


def _state_coalesce_key(data: dict) -> tuple[str, str | int, str | int | None] | None:
    if not isinstance(data, dict):
        return None
    if data.get("channel") != "devices/state":
        return None
    unit_id = data.get("unit_id")
    mode = data.get("mode")
    payload = data.get("payload") if isinstance(data.get("payload"), dict) else None
    if not isinstance(unit_id, str) or not unit_id.strip():
        return None
    if mode is None:
        return None
    # Important: STATE_SINGLE_BIT / STATE_SINGLE_FLOAT carry channel-specific updates.
    # Coalescing only by (unit_id, mode) drops one side of pair commands.
    channel_key = payload.get("ch") if isinstance(payload, dict) and "ch" in payload else None
    return unit_id, mode, channel_key


async def _flush_pending_state_events(
    manager: WebSocketManager,
    pending_state_events: dict[tuple[str, str | int], dict],
) -> None:
    if not pending_state_events:
        return
    pending_values = list(pending_state_events.values())
    pending_state_events.clear()
    for event in pending_values:
        await manager.broadcast(event)


async def forward_ws_events_from_pubsub():
    """Listen to the Redis Pub/Sub channel and forward events to WebSocketManager."""

    pubsub = await subscribe_ws_events()
    manager = WebSocketManager.get_instance()
    pending_state_events: dict[tuple[str, str | int, str | int | None], dict] = {}
    coalesced_state_replacements = 0

    try:
        async for message in pubsub.listen():
            if message["type"] != "message":
                continue

            raw = message.get("data")
            if raw is None:
                continue

            try:
                payload = json.loads(raw)
            except json.JSONDecodeError as exc:
                logger.error("💥 Invalid WS payload: %s", exc)
                continue

            data = payload.get("payload", payload)
            state_key = _state_coalesce_key(data)
            if state_key is not None:
                if state_key in pending_state_events:
                    coalesced_state_replacements += 1
                pending_state_events[state_key] = data
                if len(pending_state_events) >= 512:
                    logger.debug(
                        "WS state coalesce flush: pending=%d replacements=%d (threshold)",
                        len(pending_state_events),
                        coalesced_state_replacements,
                    )
                    await _flush_pending_state_events(manager, pending_state_events)
                    coalesced_state_replacements = 0
                continue

            if pending_state_events:
                logger.debug(
                    "WS state coalesce flush: pending=%d replacements=%d (boundary channel=%s)",
                    len(pending_state_events),
                    coalesced_state_replacements,
                    data.get("channel") if isinstance(data, dict) else None,
                )
                await _flush_pending_state_events(manager, pending_state_events)
                coalesced_state_replacements = 0

            await manager.broadcast(data)
    except asyncio.CancelledError:
        raise
    except Exception as exc:
        logger.error("💥 Error inside WS Pub/Sub listener: %s", exc)
        raise
    finally:
        with suppress(asyncio.CancelledError):
            await _flush_pending_state_events(manager, pending_state_events)
            await pubsub.unsubscribe()
            await pubsub.close()
