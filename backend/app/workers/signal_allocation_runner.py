from __future__ import annotations

import asyncio
import os
import signal
import socket
import time
from contextlib import suppress
from datetime import datetime, timezone
from typing import Any

from redis.exceptions import ResponseError

from app.api.v1.signal_sheet import SignalSheetRepository
from app.core.config import get_settings
from app.core.logger import get_logger
from app.infrastructure.db.database import AsyncSessionLocal
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.stream_bus import parse_signal_allocation_job_entry
from app.schemas.signal_sheet_schema import SignalAllocationBulkUpdateSchema, SignalAutoAllocateSchema
from app.schemas.ws.events import build_signal_job_event
from app.core.events.ws_event_publisher import WsEventPublisher
from app.services.signal_job_service import get_signal_job, update_signal_job
from app.services.worker_health import clear_worker_status, start_worker_heartbeat

settings = get_settings()
logger = get_logger("worker.signal_allocation")

STREAM_NAME = settings.signal_allocation_job_stream
GROUP_NAME = "signal-allocation-runner"
CONSUMER_NAME = f"{socket.gethostname()}-{os.getpid()}"


async def _ensure_group(redis) -> None:
    try:
        await redis.xgroup_create(STREAM_NAME, GROUP_NAME, id="0", mkstream=True)
        logger.info("✅ Created signal allocation consumer group %s", GROUP_NAME)
    except ResponseError as exc:
        if "BUSYGROUP" in str(exc):
            logger.info("ℹ️ Signal allocation consumer group already exists")
        else:
            raise


async def _fetch(redis, stream_id: str, block_ms: int = 5000):
    result = await redis.xreadgroup(
        GROUP_NAME,
        CONSUMER_NAME,
        streams={STREAM_NAME: stream_id},
        count=10,
        block=block_ms,
    )
    if not result:
        return []
    return result[0][1]


async def _drain_pending(redis) -> None:
    replayed = 0
    replay_limit = 1000
    while replayed < replay_limit:
        entries = await _fetch(redis, "0", block_ms=100)
        if not entries:
            break
        replayed += len(entries)
        logger.info("🔁 Replaying %d pending signal allocation jobs", len(entries))
        await _process_entries(redis, entries)
    if replayed >= replay_limit:
        logger.warning("⚠️ Pending replay limit reached (%d), leaving remaining pending entries for next cycle", replay_limit)


async def _handle_auto_allocate(
    repo: SignalSheetRepository,
    workspace_id: int,
    payload: dict[str, Any],
    job_state: dict[str, Any],
) -> dict[str, Any]:
    request = SignalAutoAllocateSchema.model_validate(payload)

    total = len(request.signal_ids or []) if isinstance(request.signal_ids, list) else 0
    update_every = max(1, total // 25) if total > 0 else 1
    last_emit_at = 0.0

    async def progress_callback(done: int, progress_total: int) -> None:
        nonlocal last_emit_at
        should_emit = (
            done >= progress_total
            or done <= 1
            or done % update_every == 0
        )
        now = time.monotonic()
        if not should_emit and (now - last_emit_at) < 0.35:
            return
        last_emit_at = now
        await _publish_running_progress(
            job_state=job_state,
            progress_done=done,
            progress_total=max(progress_total, total),
            message=f"Signals {done}/{max(progress_total, total)}",
        )

    result = await repo.auto_allocate(
        workspace_id=workspace_id,
        signal_ids=request.signal_ids,
        prefer_online=request.prefer_online,
        prefer_single_unit=request.prefer_single_unit,
        overwrite_existing=request.overwrite_existing,
        progress_callback=progress_callback,
    )
    return {
        "assigned": result.assigned,
        "skipped": result.skipped,
        "missing": result.missing,
        "unassigned_signal_ids": result.unassigned_signal_ids,
        "changed_signal_ids": result.changed_signal_ids,
    }


async def _handle_bulk_update(
    repo: SignalSheetRepository,
    workspace_id: int,
    payload: dict[str, Any],
    job_state: dict[str, Any],
) -> dict[str, Any]:
    request = SignalAllocationBulkUpdateSchema.model_validate(payload)
    entries = [item.model_dump() for item in request.entries]
    total = len(entries)
    update_every = max(1, total // 25) if total > 0 else 1
    last_emit_at = 0.0

    async def progress_callback(done: int, progress_total: int) -> None:
        nonlocal last_emit_at
        should_emit = (
            done >= progress_total
            or done <= 1
            or done % update_every == 0
        )
        now = time.monotonic()
        if not should_emit and (now - last_emit_at) < 0.35:
            return
        last_emit_at = now
        await _publish_running_progress(
            job_state=job_state,
            progress_done=done,
            progress_total=max(progress_total, total),
            message=f"Signals {done}/{max(progress_total, total)}",
        )

    await repo.update_allocations(workspace_id, entries, progress_callback=progress_callback)
    signal_ids = sorted({int(item["signal_id"]) for item in entries})
    return {
        "updated": len(signal_ids),
        "changed_signal_ids": signal_ids,
    }


async def _process_entries(redis, entries) -> None:
    for entry_id, fields in entries:
        job_id: str | None = None
        should_ack = False
        try:
            _, envelope = parse_signal_allocation_job_entry((entry_id, fields))
            job_id = str(envelope.get("job_id") or "").strip()
            workspace_id = int(envelope.get("workspace_id"))
            operation = str(envelope.get("operation") or "").strip()
            payload = envelope.get("payload") if isinstance(envelope.get("payload"), dict) else {}

            if not job_id or workspace_id <= 0:
                should_ack = True
                raise ValueError("Invalid signal allocation job payload")

            progress_total = 0
            if operation == "auto_allocate" and isinstance(payload.get("signal_ids"), list):
                progress_total = len(payload.get("signal_ids") or [])
            if operation == "bulk_update" and isinstance(payload.get("entries"), list):
                progress_total = len(payload.get("entries") or [])
            if operation == "test_run" and isinstance(payload.get("signal_ids"), list):
                progress_total = len(payload.get("signal_ids") or [])

            job_state = await get_signal_job(job_id)
            if job_state and str(job_state.get("status") or "") == "cancelled":
                should_ack = True
                continue

            running_state = await update_signal_job(
                job_id,
                status="running",
                message="Running",
                progress_done=0,
                progress_total=progress_total,
            )
            if running_state:
                await WsEventPublisher.publish(build_signal_job_event(running_state))
            else:
                logger.warning("⚠️ Job state missing before start, leaving entry pending: %s", job_id)
                continue

            async with AsyncSessionLocal() as session:
                repo = SignalSheetRepository(session)
                if not await repo.ensure_workspace(workspace_id):
                    raise ValueError("Workspace not found")

                if operation == "auto_allocate":
                    payload["job_id"] = job_id
                    result = await _handle_auto_allocate(repo, workspace_id, payload, running_state)
                elif operation == "bulk_update":
                    payload["job_id"] = job_id
                    result = await _handle_bulk_update(repo, workspace_id, payload, running_state)
                elif operation == "test_run":
                    raise ValueError("test_run operation must be processed by signal_test_run_runner")
                else:
                    raise ValueError(f"Unknown operation: {operation}")

            cancelled = bool((result or {}).get("cancelled")) if isinstance(result, dict) else False
            completed_state = await update_signal_job(
                job_id,
                status="cancelled" if cancelled else "succeeded",
                message="Cancelled" if cancelled else "Completed",
                progress_done=(result.get("processed", 0) if cancelled and isinstance(result, dict) else progress_total),
                progress_total=progress_total,
                result=result,
            )
            if completed_state:
                await WsEventPublisher.publish(build_signal_job_event(completed_state))
                should_ack = True
        except Exception as exc:  # noqa: BLE001
            logger.exception("💥 Failed to process signal allocation job %s: %s", entry_id, exc)
            if job_id:
                failed_state = await update_signal_job(
                    job_id,
                    status="failed",
                    message="Failed",
                    error=str(exc),
                )
                if failed_state:
                    await WsEventPublisher.publish(build_signal_job_event(failed_state))
                    should_ack = True
            else:
                should_ack = True
        finally:
            if should_ack:
                await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)
            else:
                logger.warning("⚠️ Leaving stream entry pending due to missing persisted terminal state: %s", entry_id)


async def _publish_running_progress(
    *,
    job_state: dict[str, Any],
    progress_done: int,
    progress_total: int,
    message: str,
    result: dict[str, Any] | None = None,
) -> None:
    next_state = dict(job_state)
    next_state["status"] = "running"
    next_state["message"] = message
    next_state["progress_done"] = max(0, int(progress_done))
    next_state["progress_total"] = max(0, int(progress_total))
    if result is not None:
        next_state["result"] = result
    next_state["updated_at"] = datetime.now(timezone.utc).isoformat()
    job_state.update(next_state)
    await WsEventPublisher.publish(build_signal_job_event(next_state))


async def main() -> None:
    await RedisManager.start()
    redis = RedisManager.get_instance()

    await _ensure_group(redis)
    await _drain_pending(redis)

    heartbeat_task = start_worker_heartbeat("signal_allocation_runner")
    stop_event = asyncio.Event()

    def _signal_handler() -> None:
        logger.info("🛑 Stop signal received, shutting down signal allocation runner...")
        stop_event.set()

    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        try:
            loop.add_signal_handler(sig, _signal_handler)
        except NotImplementedError:
            pass

    logger.info(
        "🚀 Signal allocation runner ready (stream=%s, group=%s, consumer=%s)",
        STREAM_NAME,
        GROUP_NAME,
        CONSUMER_NAME,
    )

    try:
        while not stop_event.is_set():
            entries = await _fetch(redis, ">")
            if not entries:
                continue
            await _process_entries(redis, entries)
    finally:
        heartbeat_task.cancel()
        with suppress(asyncio.CancelledError):
            await heartbeat_task
        await clear_worker_status("signal_allocation_runner")
        await RedisManager.stop()


if __name__ == "__main__":
    asyncio.run(main())
