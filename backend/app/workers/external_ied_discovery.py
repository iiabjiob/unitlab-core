from __future__ import annotations

import asyncio
import json
import time
from contextlib import suppress
from typing import cast

from app.core.config import get_settings
from app.core.logger import get_logger
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.types import RedisStreamClient, RedisStreamEntries
from app.services.external_ied_availability import publish_external_ied_status_snapshot
from app.services.external_ied_discovery_scheduler import (
    ExternalIedDiscoveryRequest,
    emit_external_ied_discovery_completed,
    mark_external_ied_discovery_running,
    record_external_ied_discovery_failure,
    record_external_ied_discovery_result,
)
from app.services.external_ied_discovery_worker import (
    DiscoveryResult,
    ExternalIedDiscoveryEngine,
    MmsExternalIedDiscoveryEngine,
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
logger = get_logger("worker.external_ied_discovery")

STREAM_NAME = settings.external_ied_discovery_job_stream
GROUP_NAME = "external-ied-discovery-worker"
CONSUMER_NAME = build_worker_consumer_name()
WORKER_NAME = "external_ied_discovery"
MAX_EARLIEST_WAIT_CHUNK_SECONDS = 1.0


async def _ensure_group(redis: RedisStreamClient) -> None:
    await ensure_stream_consumer_group(
        redis,
        stream_name=STREAM_NAME,
        group_name=GROUP_NAME,
        logger=logger,
        create_label="external IED discovery",
        exists_label="External IED discovery",
    )


async def _fetch(redis: RedisStreamClient, stream_id: str, block_ms: int = 5000) -> RedisStreamEntries:
    return await fetch_stream_group_entries(
        redis,
        stream_name=STREAM_NAME,
        group_name=GROUP_NAME,
        consumer_name=CONSUMER_NAME,
        stream_id=stream_id,
        count=5,
        block_ms=block_ms,
    )


async def _drain_pending(redis: RedisStreamClient, *, engine: ExternalIedDiscoveryEngine) -> None:
    await drain_pending_stream_entries(
        fetch_pending=lambda stream_id, block_ms: _fetch(redis, stream_id, block_ms=block_ms),
        process_entries=lambda entries: _process_entries(redis, entries, engine=engine),
        logger=logger,
        replay_label="external IED discovery jobs",
        replay_limit=1000,
    )


async def execute_discovery_request(
    request: ExternalIedDiscoveryRequest,
    *,
    engine: ExternalIedDiscoveryEngine,
) -> DiscoveryResult:
    started = time.monotonic()
    _ = await mark_external_ied_discovery_running(
        workspace_id=request.workspace_id,
        endpoint=request.endpoint,
        request_id=request.request_id,
    )
    try:
        result = await _run_discovery(engine, request)
    except Exception as exc:  # noqa: BLE001
        _ = await record_external_ied_discovery_failure(
            workspace_id=request.workspace_id,
            endpoint=request.endpoint,
            error=str(exc),
            duration_ms=max(0, int((time.monotonic() - started) * 1000)),
        )
        with suppress(Exception):
            _ = await publish_external_ied_status_snapshot(request.workspace_id)
        raise
    _ = await record_external_ied_discovery_result(
        workspace_id=request.workspace_id,
        endpoint=request.endpoint,
        metadata=result.metadata,
        model=result.model,
    )
    _ = await emit_external_ied_discovery_completed(
        workspace_id=request.workspace_id,
        endpoint=request.endpoint,
        request=request,
        metadata=result.metadata,
    )
    with suppress(Exception):
        _ = await publish_external_ied_status_snapshot(request.workspace_id)
    return result


async def _process_entries(redis: RedisStreamClient, entries: RedisStreamEntries, *, engine: ExternalIedDiscoveryEngine, stop_event: asyncio.Event | None = None) -> None:
    for entry_id, fields in entries:
        request: ExternalIedDiscoveryRequest | None = None
        should_ack = False
        try:
            request = _parse_request(fields)
            if request is None:
                should_ack = True
                raise ValueError("Invalid external IED discovery request payload")
            if request.workspace_id <= 0 or not request.endpoint or not request.ip:
                should_ack = True
                raise ValueError("External IED discovery request is missing required target fields")

            logger.info(
                "External IED discovery started | workspace=%s endpoint=%s reason=%s priority=%s entry_id=%s",
                request.workspace_id,
                request.endpoint,
                request.reason,
                request.priority,
                entry_id,
            )
            await _wait_until_due(request, stop_event=stop_event)
            _ = await execute_discovery_request(request, engine=engine)
            logger.info(
                "External IED discovery completed | workspace=%s endpoint=%s request_id=%s",
                request.workspace_id,
                request.endpoint,
                request.request_id,
            )
            should_ack = True
        except Exception as exc:  # noqa: BLE001
            if request is not None:
                logger.warning(
                    "External IED discovery failed | workspace=%s endpoint=%s request_id=%s error=%s",
                    request.workspace_id,
                    request.endpoint,
                    request.request_id,
                    exc,
                )
            else:
                logger.warning("External IED discovery stream entry failed | entry_id=%s error=%s", entry_id, exc)
            should_ack = True
        finally:
            if should_ack:
                _ = await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)


def _parse_request(fields: dict[str, object]) -> ExternalIedDiscoveryRequest | None:
    data = fields.get("data")
    if isinstance(data, bytes):
        data = data.decode("utf-8")
    if not isinstance(data, str) or not data.strip():
        return None
    payload = cast(object, json.loads(data))
    if not isinstance(payload, dict):
        return None
    return ExternalIedDiscoveryRequest.from_payload(cast(dict[str, object], payload))


async def _run_discovery(engine: ExternalIedDiscoveryEngine, request: ExternalIedDiscoveryRequest) -> DiscoveryResult:
    if bool(getattr(engine, "non_blocking", False)):
        return engine.discover(request)
    return await asyncio.to_thread(engine.discover, request)


async def _wait_until_due(
    request: ExternalIedDiscoveryRequest,
    *,
    stop_event: asyncio.Event | None = None,
    max_sleep_seconds: float = MAX_EARLIEST_WAIT_CHUNK_SECONDS,
) -> None:
    while True:
        if stop_event is not None and stop_event.is_set():
            raise asyncio.CancelledError("External IED discovery worker shutdown while waiting for due request.")
        delay = max(0.0, (request.earliest_execution_at_ms - int(time.time() * 1000)) / 1000.0)
        if delay <= 0:
            return
        await asyncio.sleep(min(delay, max(0.05, max_sleep_seconds)))


async def main() -> None:
    await RedisManager.start()
    redis = cast(RedisStreamClient, cast(object, RedisManager.get_instance()))
    engine = MmsExternalIedDiscoveryEngine()

    heartbeat_task = start_worker_heartbeat(WORKER_NAME)
    stop_event = asyncio.Event()
    install_stop_signal_handlers(
        stop_event=stop_event,
        logger=logger,
        stop_message="Stop signal received, shutting down external IED discovery worker...",
    )

    await _ensure_group(redis)
    await _drain_pending(redis, engine=engine)

    logger.info(
        "External IED discovery worker ready (stream=%s, group=%s, consumer=%s)",
        STREAM_NAME,
        GROUP_NAME,
        CONSUMER_NAME,
    )

    try:
        await run_consume_loop(
            stop_event=stop_event,
            fetch_entries=lambda: _fetch(redis, ">"),
            process_entries=lambda entries: _process_entries(redis, entries, engine=engine, stop_event=stop_event),
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
