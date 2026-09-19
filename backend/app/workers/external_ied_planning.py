from __future__ import annotations

import asyncio
import json
from contextlib import suppress
from typing import cast

from app.core.config import get_settings
from app.core.logger import get_logger
from app.infrastructure.db.database import AsyncSessionLocal
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.types import RedisStreamClient, RedisStreamEntries
from app.services.external_ied_planning import (
    ExternalIedPlanningRequest,
    execute_external_ied_planning_request,
)
from app.services.worker_health import clear_worker_status, start_worker_heartbeat
from app.workers.stream_worker_runtime import (
    build_worker_consumer_name,
    drain_pending_stream_entries,
    ensure_stream_consumer_group,
    fetch_stream_group_entries,
)
from app.workers.worker_lifecycle import install_stop_signal_handlers, run_consume_loop

settings = get_settings()
logger = get_logger("worker.external_ied_planning")

STREAM_NAME = settings.external_ied_planning_event_stream
GROUP_NAME = "external-ied-planning-worker"
CONSUMER_NAME = build_worker_consumer_name()
WORKER_NAME = "external_ied_planning"


async def _ensure_group(redis: RedisStreamClient) -> None:
    await ensure_stream_consumer_group(
        redis,
        stream_name=STREAM_NAME,
        group_name=GROUP_NAME,
        logger=logger,
        create_label="external IED planning",
        exists_label="External IED planning",
    )


async def _fetch(redis: RedisStreamClient, stream_id: str, block_ms: int = 5000) -> RedisStreamEntries:
    return await fetch_stream_group_entries(
        redis,
        stream_name=STREAM_NAME,
        group_name=GROUP_NAME,
        consumer_name=CONSUMER_NAME,
        stream_id=stream_id,
        count=10,
        block_ms=block_ms,
    )


async def _drain_pending(redis: RedisStreamClient) -> None:
    await drain_pending_stream_entries(
        fetch_pending=lambda stream_id, block_ms: _fetch(redis, stream_id, block_ms=block_ms),
        process_entries=lambda entries: _process_entries(redis, entries),
        logger=logger,
        replay_label="external IED planning events",
        replay_limit=1000,
    )


async def _process_entries(redis: RedisStreamClient, entries: RedisStreamEntries) -> None:
    for entry_id, fields in entries:
        request: ExternalIedPlanningRequest | None = None
        should_ack = False
        try:
            request = _parse_request(fields)
            if request is None:
                should_ack = True
                raise ValueError("Invalid external IED planning event payload")
            if request.workspace_id <= 0 or not request.endpoint:
                should_ack = True
                raise ValueError("External IED planning request is missing workspace or endpoint")

            logger.info(
                "External IED planning started | workspace=%s endpoint=%s reason=%s entry_id=%s",
                request.workspace_id,
                request.endpoint,
                request.reason,
                entry_id,
            )
            async with AsyncSessionLocal() as db:
                _ = await execute_external_ied_planning_request(request, db=db)
            logger.info(
                "External IED planning completed | workspace=%s endpoint=%s request_id=%s",
                request.workspace_id,
                request.endpoint,
                request.request_id,
            )
            should_ack = True
        except Exception as exc:  # noqa: BLE001
            if request is not None:
                logger.warning(
                    "External IED planning failed | workspace=%s endpoint=%s request_id=%s error=%s",
                    request.workspace_id,
                    request.endpoint,
                    request.request_id,
                    exc,
                )
            else:
                logger.warning("External IED planning stream entry failed | entry_id=%s error=%s", entry_id, exc)
            should_ack = True
        finally:
            if should_ack:
                _ = await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)


def _parse_request(fields: dict[str, object]) -> ExternalIedPlanningRequest | None:
    data = fields.get("data")
    if isinstance(data, bytes):
        data = data.decode("utf-8")
    if not isinstance(data, str) or not data.strip():
        return None
    payload = cast(object, json.loads(data))
    if not isinstance(payload, dict):
        return None
    typed_payload = cast(dict[str, object], payload)
    event = str(typed_payload.get("event") or "")
    if event not in {"ExternalIedDiscoveryCompleted", "ExternalIedPlanningRequested"}:
        return None
    return ExternalIedPlanningRequest.from_payload(typed_payload)


async def main() -> None:
    await RedisManager.start()
    redis = cast(RedisStreamClient, cast(object, RedisManager.get_instance()))

    heartbeat_task = start_worker_heartbeat(WORKER_NAME)
    stop_event = asyncio.Event()
    install_stop_signal_handlers(
        stop_event=stop_event,
        logger=logger,
        stop_message="Stop signal received, shutting down external IED planning worker...",
    )

    await _ensure_group(redis)
    await _drain_pending(redis)

    logger.info(
        "External IED planning worker ready (stream=%s, group=%s, consumer=%s)",
        STREAM_NAME,
        GROUP_NAME,
        CONSUMER_NAME,
    )

    try:
        await run_consume_loop(
            stop_event=stop_event,
            fetch_entries=lambda: _fetch(redis, ">"),
            process_entries=lambda entries: _process_entries(redis, entries),
            logger=logger,
        )
    finally:
        _ = heartbeat_task.cancel()
        with suppress(asyncio.CancelledError):
            await heartbeat_task
        await clear_worker_status(WORKER_NAME)
        await RedisManager.stop()


if __name__ == "__main__":
    asyncio.run(main())
