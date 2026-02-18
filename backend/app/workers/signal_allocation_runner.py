from __future__ import annotations

import asyncio
import os
import signal
import socket
import time
from contextlib import suppress
from typing import Any

from redis.exceptions import ResponseError

from app.api.v1.signal_sheet import SignalSheetRepository
from app.core.config import get_settings
from app.core.logger import get_logger
from app.infrastructure.db.database import AsyncSessionLocal
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.stream_bus import parse_signal_allocation_job_entry
from app.infrastructure.protocol.modes import Cmd
from app.schemas.signal_sheet_schema import SignalAllocationBulkUpdateSchema, SignalAutoAllocateSchema
from app.schemas.ws.events import SignalAllocationJobEvent
from app.core.events.ws_event_publisher import WsEventPublisher
from app.services.command_queue_service import enqueue_do_command
from app.services.signal_allocation_job_service import update_signal_allocation_job
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
    while True:
        entries = await _fetch(redis, "0", block_ms=100)
        if not entries:
            break
        logger.info("🔁 Replaying %d pending signal allocation jobs", len(entries))
        await _process_entries(redis, entries)


async def _handle_auto_allocate(repo: SignalSheetRepository, workspace_id: int, payload: dict[str, Any]) -> dict[str, Any]:
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
            job_id=str(payload.get("job_id") or ""),
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


async def _handle_bulk_update(repo: SignalSheetRepository, workspace_id: int, payload: dict[str, Any]) -> dict[str, Any]:
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
            job_id=str(payload.get("job_id") or ""),
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


async def _handle_test_run(repo: SignalSheetRepository, workspace_id: int, payload: dict[str, Any]) -> dict[str, Any]:
    requested_ids_raw = payload.get("signal_ids") if isinstance(payload, dict) else None
    toggle_step_ms_raw = payload.get("toggle_step_ms") if isinstance(payload, dict) else None

    requested_ids = []
    if isinstance(requested_ids_raw, list):
        seen: set[int] = set()
        for item in requested_ids_raw:
            signal_id = int(item)
            if signal_id <= 0 or signal_id in seen:
                continue
            requested_ids.append(signal_id)
            seen.add(signal_id)

    toggle_step_ms = int(toggle_step_ms_raw) if toggle_step_ms_raw is not None else 1000
    toggle_step_ms = max(100, min(10000, toggle_step_ms))
    toggle_sleep_seconds = toggle_step_ms / 1000

    rows = await repo.list_allocation_rows_by_signal_ids(workspace_id, requested_ids)
    rows_by_signal_id = {row.signal_id: row for row in rows}

    total = len(requested_ids)
    update_every = max(1, total // 25) if total > 0 else 1
    last_emit_at = 0.0

    succeeded_signal_ids: list[int] = []
    skipped = 0

    for index, signal_id in enumerate(requested_ids, start=1):
        row = rows_by_signal_id.get(signal_id)
        success = False
        if row is None:
            skipped += 1
        elif not row.unit_id or not isinstance(row.channel_index, int):
            skipped += 1
        elif not str(row.channel_type or "").lower().startswith("do"):
            skipped += 1
        elif row.unit_online is False:
            skipped += 1
        else:
            await enqueue_do_command(
                unit_id=str(row.unit_id),
                mode=Cmd.SET_SINGLE_BIT,
                ch=int(row.channel_index),
                value=1,
                correlation_id=f"test-run:{signal_id}:on",
            )
            await asyncio.sleep(toggle_sleep_seconds)
            await enqueue_do_command(
                unit_id=str(row.unit_id),
                mode=Cmd.SET_SINGLE_BIT,
                ch=int(row.channel_index),
                value=0,
                correlation_id=f"test-run:{signal_id}:off",
            )
            await asyncio.sleep(toggle_sleep_seconds)
            success = True

        if success:
            succeeded_signal_ids.append(signal_id)

        should_emit = index >= total or index <= 1 or index % update_every == 0
        now = time.monotonic()
        if should_emit or (now - last_emit_at) >= 0.35:
            last_emit_at = now
            await _publish_running_progress(
                job_id=str(payload.get("job_id") or ""),
                progress_done=index,
                progress_total=total,
                message=f"Signals {index}/{total} · ok {len(succeeded_signal_ids)} · skip {skipped}",
            )

    tested_signal_ids = await repo.mark_signals_tested(workspace_id, succeeded_signal_ids)

    return {
        "processed": total,
        "succeeded": len(succeeded_signal_ids),
        "skipped": skipped,
        "tested_signal_ids": tested_signal_ids,
    }


async def _process_entries(redis, entries) -> None:
    for entry_id, fields in entries:
        job_id: str | None = None
        try:
            _, envelope = parse_signal_allocation_job_entry((entry_id, fields))
            job_id = str(envelope.get("job_id") or "").strip()
            workspace_id = int(envelope.get("workspace_id"))
            operation = str(envelope.get("operation") or "").strip()
            payload = envelope.get("payload") if isinstance(envelope.get("payload"), dict) else {}

            if not job_id or workspace_id <= 0:
                raise ValueError("Invalid signal allocation job payload")

            progress_total = 0
            if operation == "auto_allocate" and isinstance(payload.get("signal_ids"), list):
                progress_total = len(payload.get("signal_ids") or [])
            if operation == "bulk_update" and isinstance(payload.get("entries"), list):
                progress_total = len(payload.get("entries") or [])
            if operation == "test_run" and isinstance(payload.get("signal_ids"), list):
                progress_total = len(payload.get("signal_ids") or [])

            running_snapshot = await update_signal_allocation_job(
                job_id,
                status="running",
                message="Running",
                progress_done=0,
                progress_total=progress_total,
            )
            if running_snapshot:
                await WsEventPublisher.publish(SignalAllocationJobEvent(**running_snapshot))

            async with AsyncSessionLocal() as session:
                repo = SignalSheetRepository(session)
                if not await repo.ensure_workspace(workspace_id):
                    raise ValueError("Workspace not found")

                if operation == "auto_allocate":
                    payload["job_id"] = job_id
                    result = await _handle_auto_allocate(repo, workspace_id, payload)
                elif operation == "bulk_update":
                    payload["job_id"] = job_id
                    result = await _handle_bulk_update(repo, workspace_id, payload)
                elif operation == "test_run":
                    payload["job_id"] = job_id
                    result = await _handle_test_run(repo, workspace_id, payload)
                else:
                    raise ValueError(f"Unknown operation: {operation}")

            done_snapshot = await update_signal_allocation_job(
                job_id,
                status="succeeded",
                message="Completed",
                progress_done=progress_total,
                progress_total=progress_total,
                result=result,
            )
            if done_snapshot:
                await WsEventPublisher.publish(SignalAllocationJobEvent(**done_snapshot))
        except Exception as exc:  # noqa: BLE001
            logger.exception("💥 Failed to process signal allocation job %s: %s", entry_id, exc)
            if job_id:
                failed_snapshot = await update_signal_allocation_job(
                    job_id,
                    status="failed",
                    message="Failed",
                    error=str(exc),
                )
                if failed_snapshot:
                    await WsEventPublisher.publish(SignalAllocationJobEvent(**failed_snapshot))
        finally:
            await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)


async def _publish_running_progress(
    *,
    job_id: str,
    progress_done: int,
    progress_total: int,
    message: str,
) -> None:
    if not job_id:
        return
    snapshot = await update_signal_allocation_job(
        job_id,
        status="running",
        message=message,
        progress_done=progress_done,
        progress_total=progress_total,
    )
    if snapshot is None:
        return
    await WsEventPublisher.publish(SignalAllocationJobEvent(**snapshot))


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
