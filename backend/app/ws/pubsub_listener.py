from __future__ import annotations

import asyncio
import json
import time
from contextlib import suppress

from redis.exceptions import ConnectionError as RedisConnectionError

from app.core.logger import get_logger
from app.infrastructure.redis.stream_bus import subscribe_ws_events
from app.ws.manager import WebSocketManager

logger = get_logger("ws.sub")

_STATE_FLUSH_DEBOUNCE_SEC = 0.03
_STATUS_FAST_THROTTLE_SEC = 1.0
_STATUS_PENDING_MAX = 512


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


def _status_coalesce_key(data: dict) -> str | None:
    if not isinstance(data, dict):
        return None
    if data.get("channel") != "devices/status":
        return None
    unit_id = data.get("unit_id")
    if not isinstance(unit_id, str) or not unit_id.strip():
        return None
    return unit_id


def _status_heartbeat_kind(data: dict) -> str | None:
    kind = data.get("heartbeat_kind")
    if isinstance(kind, str) and kind:
        return kind
    return None


async def _flush_pending_status_events(
    manager: WebSocketManager,
    pending_status_events: dict[str, dict],
    last_status_emit_at: dict[str, float],
    *,
    now: float,
    force: bool,
) -> int:
    if not pending_status_events:
        return 0

    sent = 0
    for unit_id, event in list(pending_status_events.items()):
        last_emit = last_status_emit_at.get(unit_id)
        if not force and last_emit is not None and (now - last_emit) < _STATUS_FAST_THROTTLE_SEC:
            continue
        await manager.broadcast(event)
        last_status_emit_at[unit_id] = now
        pending_status_events.pop(unit_id, None)
        sent += 1

    return sent


async def forward_ws_events_from_pubsub():
    """Listen to the Redis Pub/Sub channel and forward events to WebSocketManager."""
    manager = WebSocketManager.get_instance()
    pending_state_events: dict[tuple[str, str | int, str | int | None], dict] = {}
    pending_status_events: dict[str, dict] = {}
    last_status_emit_at: dict[str, float] = {}
    coalesced_state_replacements = 0
    coalesced_status_replacements = 0
    throttled_status_events = 0
    retry_delay_sec = 1.0
    pubsub = None
    state_flush_task: asyncio.Task[None] | None = None

    async def flush_state_events(reason: str) -> None:
        nonlocal coalesced_state_replacements, state_flush_task
        if state_flush_task is not None and state_flush_task is not asyncio.current_task():
            state_flush_task.cancel()
            with suppress(asyncio.CancelledError):
                await state_flush_task
            state_flush_task = None
        if pending_state_events:
            logger.debug(
                "WS state coalesce flush: pending=%d replacements=%d (%s)",
                len(pending_state_events),
                coalesced_state_replacements,
                reason,
            )
            await _flush_pending_state_events(manager, pending_state_events)
            coalesced_state_replacements = 0

    def ensure_state_flush_timer() -> None:
        nonlocal state_flush_task, coalesced_state_replacements
        if state_flush_task is not None and not state_flush_task.done():
            return

        async def _delayed_flush() -> None:
            nonlocal state_flush_task, coalesced_state_replacements
            try:
                await asyncio.sleep(_STATE_FLUSH_DEBOUNCE_SEC)
                if pending_state_events:
                    logger.debug(
                        "WS state coalesce flush: pending=%d replacements=%d (debounce %.0f ms)",
                        len(pending_state_events),
                        coalesced_state_replacements,
                        _STATE_FLUSH_DEBOUNCE_SEC * 1000,
                    )
                    await _flush_pending_state_events(manager, pending_state_events)
                    coalesced_state_replacements = 0
            except asyncio.CancelledError:
                raise
            finally:
                state_flush_task = None

        state_flush_task = asyncio.create_task(_delayed_flush(), name="ws-state-flush")

    try:
        while True:
            try:
                pubsub = await subscribe_ws_events()
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
                    now = time.monotonic()

                    status_key = _status_coalesce_key(data)
                    if status_key is not None:
                        hb_kind = _status_heartbeat_kind(data)
                        if hb_kind == "diag":
                            pending_status_events.pop(status_key, None)
                            if pending_state_events:
                                await flush_state_events(
                                    f"diag boundary channel={data.get('channel') if isinstance(data, dict) else None}"
                                )

                            await manager.broadcast(data)
                            last_status_emit_at[status_key] = now

                            flushed_status = await _flush_pending_status_events(
                                manager,
                                pending_status_events,
                                last_status_emit_at,
                                now=now,
                                force=False,
                            )
                            if flushed_status:
                                logger.debug(
                                    "WS status coalesce flush: sent=%d pending=%d replacements=%d throttled=%d (diag boundary)",
                                    flushed_status,
                                    len(pending_status_events),
                                    coalesced_status_replacements,
                                    throttled_status_events,
                                )
                            continue

                        last_emit = last_status_emit_at.get(status_key)
                        can_emit_now = last_emit is None or (now - last_emit) >= _STATUS_FAST_THROTTLE_SEC
                        if can_emit_now:
                            if pending_state_events:
                                await flush_state_events(
                                    f"status boundary channel={data.get('channel') if isinstance(data, dict) else None}"
                                )

                            await manager.broadcast(data)
                            last_status_emit_at[status_key] = now

                            flushed_status = await _flush_pending_status_events(
                                manager,
                                pending_status_events,
                                last_status_emit_at,
                                now=now,
                                force=False,
                            )
                            if flushed_status:
                                logger.debug(
                                    "WS status coalesce flush: sent=%d pending=%d replacements=%d throttled=%d",
                                    flushed_status,
                                    len(pending_status_events),
                                    coalesced_status_replacements,
                                    throttled_status_events,
                                )
                            continue

                        throttled_status_events += 1
                        if status_key in pending_status_events:
                            coalesced_status_replacements += 1
                        pending_status_events[status_key] = data

                        flushed_status = await _flush_pending_status_events(
                            manager,
                            pending_status_events,
                            last_status_emit_at,
                            now=now,
                            force=False,
                        )
                        if flushed_status:
                            logger.debug(
                                "WS status coalesce flush: sent=%d pending=%d replacements=%d throttled=%d",
                                flushed_status,
                                len(pending_status_events),
                                coalesced_status_replacements,
                                throttled_status_events,
                            )

                        if len(pending_status_events) >= _STATUS_PENDING_MAX:
                            forced = await _flush_pending_status_events(
                                manager,
                                pending_status_events,
                                last_status_emit_at,
                                now=now,
                                force=True,
                            )
                            logger.debug(
                                "WS status coalesce flush: sent=%d pending=%d replacements=%d throttled=%d (threshold)",
                                forced,
                                len(pending_status_events),
                                coalesced_status_replacements,
                                throttled_status_events,
                            )
                        continue

                    state_key = _state_coalesce_key(data)
                    if state_key is not None:
                        if state_key in pending_state_events:
                            coalesced_state_replacements += 1
                        pending_state_events[state_key] = data
                        ensure_state_flush_timer()
                        if len(pending_state_events) >= 512:
                            await flush_state_events("threshold")
                        continue

                    if pending_state_events:
                        await flush_state_events(
                            f"boundary channel={data.get('channel') if isinstance(data, dict) else None}"
                        )

                    flushed_status = await _flush_pending_status_events(
                        manager,
                        pending_status_events,
                        last_status_emit_at,
                        now=now,
                        force=False,
                    )
                    if flushed_status:
                        logger.debug(
                            "WS status coalesce flush: sent=%d pending=%d replacements=%d throttled=%d (boundary channel=%s)",
                            flushed_status,
                            len(pending_status_events),
                            coalesced_status_replacements,
                            throttled_status_events,
                            data.get("channel") if isinstance(data, dict) else None,
                        )

                    await manager.broadcast(data)
            except asyncio.CancelledError:
                raise
            except RedisConnectionError as exc:
                logger.warning("WS Pub/Sub disconnected from Redis; retrying in %.1fs: %s", retry_delay_sec, exc)
                await asyncio.sleep(retry_delay_sec)
            finally:
                if pubsub is not None:
                    with suppress(asyncio.CancelledError):
                        if state_flush_task is not None:
                            state_flush_task.cancel()
                            await asyncio.gather(state_flush_task, return_exceptions=True)
                            state_flush_task = None
                        await _flush_pending_state_events(manager, pending_state_events)
                        final_now = time.monotonic()
                        await _flush_pending_status_events(
                            manager,
                            pending_status_events,
                            last_status_emit_at,
                            now=final_now,
                            force=True,
                        )
                        await pubsub.unsubscribe()
                        await pubsub.close()
                pubsub = None
    except asyncio.CancelledError:
        raise
    except Exception as exc:
        logger.error("💥 Error inside WS Pub/Sub listener: %s", exc)
        raise
