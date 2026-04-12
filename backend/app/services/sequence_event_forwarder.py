from __future__ import annotations

import asyncio
import os
import socket
from collections import deque

from redis.exceptions import ResponseError
from typing import Any, Dict

from app.core.config import get_settings
from app.core.events.ws_event_publisher import WsEventPublisher
from app.core.logger import get_logger
from app.core.sequence_dto import SequenceEventType
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.stream_bus import parse_sequence_event_entry
from app.schemas.ws.events import (
    SequenceCompletedEvent,
    SequenceErrorEvent,
    SequenceProgressEvent,
    SequenceStartedEvent,
    SequenceStepErrorEvent,
    SequenceStoppingEvent,
    SequenceStoppedEvent,
)

settings = get_settings()
logger = get_logger("sequence.forwarder")

STREAM_NAME = settings.sequence_event_stream
GROUP_NAME = "sequence-events"
CONSUMER_NAME = f"{socket.gethostname()}-{os.getpid()}"

_STOPPING_CACHE_LIMIT = 2048
_stopping_emitted_runs: set[int] = set()
_finished_run_order: deque[int] = deque()
_finished_run_set: set[int] = set()


def _mark_run_finished(run_id: int) -> None:
    if run_id in _finished_run_set:
        return
    _finished_run_order.append(run_id)
    _finished_run_set.add(run_id)
    while len(_finished_run_order) > _STOPPING_CACHE_LIMIT:
        expired = _finished_run_order.popleft()
        _finished_run_set.discard(expired)


async def _ensure_group(redis) -> None:
    try:
        await redis.xgroup_create(STREAM_NAME, GROUP_NAME, id="0", mkstream=True)
        logger.info("✅ Created consumer group %s for sequence events", GROUP_NAME)
    except ResponseError as exc:
        if "BUSYGROUP" in str(exc):
            logger.info("ℹ️ Sequence event consumer group already exists")
        else:
            raise


async def _fetch(redis, stream_id: str, block_ms: int = 5000):
    result = await redis.xreadgroup(
        GROUP_NAME,
        CONSUMER_NAME,
        streams={STREAM_NAME: stream_id},
        count=50,
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
        logger.info("🔁 Replaying %d pending sequence events", len(entries))
        await _process_entries(redis, entries)


async def _process_entries(redis, entries) -> None:
    for entry_id, fields in entries:
        try:
            _, event = parse_sequence_event_entry((entry_id, fields))
            await _forward(event.type, event.sequence_id, event.run_id, event.data or {})
        except Exception as exc:  # noqa: BLE001
            logger.exception("💥 Failed to forward sequence event %s: %s", entry_id, exc)
        finally:
            await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)


async def _forward(event_type: SequenceEventType, sequence_id: int, run_id: int, data: Dict[str, Any]):
    if event_type == SequenceEventType.STARTED:
        await WsEventPublisher.publish(
            SequenceStartedEvent(
                sequence_id=sequence_id,
                run_id=run_id,
                total_steps=int(data.get("total_steps", 0)),
                runtime=data.get("runtime"),
            )
        )
        return

    if event_type == SequenceEventType.STOPPING:
        if run_id in _finished_run_set:
            logger.debug("Skipping late stopping event for already finished run %s", run_id)
            return

        await WsEventPublisher.publish(
            SequenceStoppingEvent(
                sequence_id=sequence_id,
                run_id=run_id,
                current_step_index=int(data.get("current_step_index", 0)),
                total_steps=int(data.get("total_steps", 0)),
                runtime=data.get("runtime"),
            )
        )
        _stopping_emitted_runs.add(run_id)
        return

    if event_type == SequenceEventType.STEP_COMPLETED:
        await WsEventPublisher.publish(
            SequenceProgressEvent(
                sequence_id=sequence_id,
                run_id=run_id,
                step_index=int(data.get("step_index", 0)),
                step_id=int(data.get("step_id", 0)),
                step_type=str(data.get("step_type", "")),
                progress_scope=str(data.get("progress_scope", "step")),
                step_elapsed_ms=int(data.get("step_elapsed_ms", 0)),
                run_elapsed_ms=int(data.get("run_elapsed_ms", 0)),
                completed_steps=list(data.get("completed_step_ids", [])),
                runtime=data.get("runtime"),
            )
        )
        return

    if event_type == SequenceEventType.FINISHED:
        status = data.get("status")
        elapsed_ms = int(data.get("elapsed_ms", 0))
        if status == "completed":
            _stopping_emitted_runs.discard(run_id)
            _mark_run_finished(run_id)
            await WsEventPublisher.publish(
                SequenceCompletedEvent(
                    sequence_id=sequence_id,
                    run_id=run_id,
                    elapsed_ms=elapsed_ms,
                    runtime=data.get("runtime"),
                )
            )
        elif status == "stopped":
            current_step_index = int(data.get("current_step_index", 0))
            total_steps = int(data.get("total_steps", 0))
            if run_id not in _stopping_emitted_runs:
                await WsEventPublisher.publish(
                    SequenceStoppingEvent(
                        sequence_id=sequence_id,
                        run_id=run_id,
                        current_step_index=current_step_index,
                        total_steps=total_steps,
                        runtime=data.get("runtime"),
                    )
                )
            else:
                _stopping_emitted_runs.discard(run_id)

            _mark_run_finished(run_id)
            await WsEventPublisher.publish(
                SequenceStoppedEvent(
                    sequence_id=sequence_id,
                    run_id=run_id,
                    runtime=data.get("runtime"),
                )
            )
        else:
            logger.warning("⚠️ Unknown finished status %s", status)
        return

    if event_type == SequenceEventType.FAILED:
        message = str(data.get("message", "Sequence failed"))
        step_index = data.get("step_index")
        step_id = data.get("step_id")
        if step_id is not None and step_index is not None:
            await WsEventPublisher.publish(
                SequenceStepErrorEvent(
                    sequence_id=sequence_id,
                    run_id=run_id,
                    step_index=int(step_index),
                    step_id=int(step_id),
                    message=message,
                    runtime=data.get("runtime"),
                )
            )
        await WsEventPublisher.publish(
            SequenceErrorEvent(
                sequence_id=sequence_id,
                run_id=run_id,
                message=message,
                runtime=data.get("runtime"),
            )
        )
        return

    if event_type == SequenceEventType.STEP_STARTED:
        # Currently not exposed via WS; reserved for future UX.
        return

    logger.warning("⚠️ Unsupported sequence event type: %s", event_type)


async def forward_sequence_events() -> None:
    redis = RedisManager.get_instance()
    await _ensure_group(redis)
    await _drain_pending(redis)

    while True:
        entries = await _fetch(redis, ">")
        if not entries:
            continue
        await _process_entries(redis, entries)
