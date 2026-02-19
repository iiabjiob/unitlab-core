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
from app.core.events.ws_event_publisher import WsEventPublisher
from app.core.logger import get_logger
from app.infrastructure.db.database import AsyncSessionLocal
from app.infrastructure.protocol.modes import Cmd, State
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.stream_bus import parse_signal_allocation_job_entry
from app.schemas.ws.events import build_signal_job_event
from app.services.command_queue_service import enqueue_do_command, enqueue_request_state
from app.services.signal_job_service import (
    get_signal_job,
    get_signal_job_status,
    refresh_signal_job_ttl,
    release_signal_test_run_workspace_lock,
    update_signal_job,
)
from app.services.worker_health import clear_worker_status, start_worker_heartbeat

settings = get_settings()
logger = get_logger("worker.signal_test_run")

STREAM_NAME = settings.signal_test_run_job_stream
GROUP_NAME = "signal-test-runner"
CONSUMER_NAME = f"{socket.gethostname()}-{os.getpid()}"


async def _ensure_group(redis) -> None:
    try:
        await redis.xgroup_create(STREAM_NAME, GROUP_NAME, id="0", mkstream=True)
        logger.info("✅ Created signal test run consumer group %s", GROUP_NAME)
    except ResponseError as exc:
        if "BUSYGROUP" in str(exc):
            logger.info("ℹ️ Signal test run consumer group already exists")
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
        logger.info("🔁 Replaying %d pending signal test jobs", len(entries))
        await _process_entries(redis, entries)
    if replayed >= replay_limit:
        logger.warning("⚠️ Pending replay limit reached (%d), leaving remaining pending entries for next cycle", replay_limit)


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


async def _handle_test_run(
    repo: SignalSheetRepository,
    workspace_id: int,
    payload: dict[str, Any],
    job_state: dict[str, Any],
) -> dict[str, Any]:
    requested_ids_raw = payload.get("signal_ids") if isinstance(payload, dict) else None
    signal_interval_ms_raw = payload.get("signal_interval_ms") if isinstance(payload, dict) else None
    toggle_mode_raw = payload.get("toggle_mode") if isinstance(payload, dict) else None

    requested_ids: list[int] = []
    if isinstance(requested_ids_raw, list):
        seen: set[int] = set()
        for item in requested_ids_raw:
            signal_id = int(item)
            if signal_id <= 0 or signal_id in seen:
                continue
            requested_ids.append(signal_id)
            seen.add(signal_id)

    signal_interval_ms = int(signal_interval_ms_raw) if signal_interval_ms_raw is not None else 1000
    signal_interval_ms = max(100, min(10000, signal_interval_ms))
    toggle_mode = str(toggle_mode_raw or "single").strip().lower()
    if toggle_mode not in {"single", "double"}:
        toggle_mode = "single"

    signal_interval_seconds = signal_interval_ms / 1000
    redis = RedisManager.get_instance()

    rows = await repo.list_allocation_rows_by_signal_ids(workspace_id, requested_ids)
    rows_by_signal_id = {row.signal_id: row for row in rows}

    total = len(requested_ids)
    update_every = max(1, total // 25) if total > 0 else 1
    last_emit_at = 0.0
    last_ttl_refresh_at = 0.0
    tested_at_batch_size = max(1, int(settings.signal_test_run_tested_at_batch_size))
    ttl_refresh_seconds = max(1, int(settings.signal_test_run_ttl_refresh_seconds))

    succeeded_signal_ids: list[int] = []
    tested_at_by_signal: dict[int, str] = {}
    skipped = 0
    skip_reasons = {
        "missing_row": 0,
        "invalid_binding": 0,
        "non_do_channel": 0,
        "offline_unit": 0,
    }

    job_id = str(payload.get("job_id") or "")
    pending_tested_at_by_signal: dict[int, str] = {}
    tested_at_patch_since_emit: dict[int, str] = {}

    async def flush_tested_at_batch() -> None:
        if not pending_tested_at_by_signal:
            return
        await repo.mark_signals_tested_at(workspace_id, dict(pending_tested_at_by_signal))
        pending_tested_at_by_signal.clear()

    async def maybe_refresh_ttl(force: bool = False) -> None:
        nonlocal last_ttl_refresh_at
        if not job_id:
            return
        now_mono = time.monotonic()
        if not force and (now_mono - last_ttl_refresh_at) < ttl_refresh_seconds:
            return
        last_ttl_refresh_at = now_mono
        await refresh_signal_job_ttl(
            job_id,
            workspace_id=workspace_id,
            include_test_run_lock=True,
        )

    unit_ids: list[str] = sorted({str(row.unit_id) for row in rows if row.unit_id})
    unit_bitmasks: dict[str, int] = {}
    if unit_ids:
        keys = [f"device:{unit_id}:bitmask" for unit_id in unit_ids]
        values = await redis.mget(*keys)
        for unit_id, bitmask_raw in zip(unit_ids, values):
            bitmask = 0
            if bitmask_raw is not None:
                try:
                    bitmask = int(bitmask_raw)
                except (TypeError, ValueError):
                    bitmask = 0
            unit_bitmasks[unit_id] = bitmask

    async def apply_control_state() -> bool:
        if not job_id:
            return True

        while True:
            status = str(await get_signal_job_status(job_id) or "")
            if not status:
                job_state_payload = await get_signal_job(job_id)
                status = str((job_state_payload or {}).get("status") or "")

            if status in {"cancelling", "cancelled"}:
                await flush_tested_at_batch()
                return False
            if status == "paused":
                await maybe_refresh_ttl(force=True)
                await asyncio.sleep(0.2)
                continue
            return True

    for index, signal_id in enumerate(requested_ids, start=1):
        if not await apply_control_state():
            return {
                "processed": index - 1,
                "succeeded": len(succeeded_signal_ids),
                "skipped": skipped,
                "skip_reasons": skip_reasons,
                "toggle_mode": toggle_mode,
                "signal_interval_ms": signal_interval_ms,
                "tested_signal_ids": sorted(tested_at_by_signal.keys()),
                "cancelled": True,
            }

        row = rows_by_signal_id.get(signal_id)
        success = False
        if row is None:
            skipped += 1
            skip_reasons["missing_row"] += 1
        elif not row.unit_id or not isinstance(row.channel_index, int):
            skipped += 1
            skip_reasons["invalid_binding"] += 1
        elif not str(row.channel_type or "").lower().startswith("do"):
            skipped += 1
            skip_reasons["non_do_channel"] += 1
        else:
            unit_id = str(row.unit_id)
            bitmask = unit_bitmasks.get(unit_id, 0)

            channel_index = int(row.channel_index)
            current_value = 1 if (bitmask & (1 << channel_index)) else 0
            toggled_value = 0 if current_value else 1
            await enqueue_do_command(
                unit_id=unit_id,
                mode=Cmd.SET_SINGLE_BIT,
                ch=channel_index,
                value=toggled_value,
                correlation_id=f"test-run:{signal_id}:set:{toggled_value}",
            )

            if toggled_value:
                bitmask = bitmask | (1 << channel_index)
            else:
                bitmask = bitmask & ~(1 << channel_index)

            if toggle_mode == "double":
                await asyncio.sleep(signal_interval_seconds)
                await enqueue_do_command(
                    unit_id=unit_id,
                    mode=Cmd.SET_SINGLE_BIT,
                    ch=channel_index,
                    value=current_value,
                    correlation_id=f"test-run:{signal_id}:set:{current_value}",
                )
                if current_value:
                    bitmask = bitmask | (1 << channel_index)
                else:
                    bitmask = bitmask & ~(1 << channel_index)

            unit_bitmasks[unit_id] = bitmask
            await enqueue_request_state(
                unit_id=unit_id,
                mode=State.REQ_SINGLE_BIT,
                ch=channel_index,
                correlation_id=f"test-run:{signal_id}:state",
            )
            success = True

        if success:
            succeeded_signal_ids.append(signal_id)
            tested_at = datetime.now(timezone.utc).isoformat()
            tested_at_by_signal[signal_id] = tested_at
            pending_tested_at_by_signal[signal_id] = tested_at
            tested_at_patch_since_emit[signal_id] = tested_at
            if len(pending_tested_at_by_signal) >= tested_at_batch_size:
                await flush_tested_at_batch()

        should_emit = index >= total or index <= 1 or index % update_every == 0
        now = time.monotonic()
        await maybe_refresh_ttl()
        if should_emit or (now - last_emit_at) >= 0.35:
            last_emit_at = now
            result_payload: dict[str, Any] = {
                "processed": index,
                "succeeded": len(succeeded_signal_ids),
                "skipped": skipped,
                "toggle_mode": toggle_mode,
                "signal_interval_ms": signal_interval_ms,
            }
            if tested_at_patch_since_emit:
                result_payload["tested_at_patch"] = dict(tested_at_patch_since_emit)
                tested_at_patch_since_emit.clear()
            await _publish_running_progress(
                job_state=job_state,
                progress_done=index,
                progress_total=total,
                message=f"Signals {index}/{total} · ok {len(succeeded_signal_ids)} · skip {skipped}",
                result=result_payload,
            )

        if index < total:
            slept = 0.0
            while slept < signal_interval_seconds:
                if not await apply_control_state():
                    await flush_tested_at_batch()
                    return {
                        "processed": index,
                        "succeeded": len(succeeded_signal_ids),
                        "skipped": skipped,
                        "skip_reasons": skip_reasons,
                        "toggle_mode": toggle_mode,
                        "signal_interval_ms": signal_interval_ms,
                        "tested_signal_ids": sorted(tested_at_by_signal.keys()),
                        "cancelled": True,
                    }
                step = min(0.2, signal_interval_seconds - slept)
                await asyncio.sleep(step)
                slept += step

    await flush_tested_at_batch()
    result_payload: dict[str, Any] = {
        "processed": total,
        "succeeded": len(succeeded_signal_ids),
        "skipped": skipped,
        "skip_reasons": skip_reasons,
        "toggle_mode": toggle_mode,
        "signal_interval_ms": signal_interval_ms,
        "tested_signal_ids": sorted(tested_at_by_signal.keys()),
    }
    if tested_at_patch_since_emit:
        result_payload["tested_at_patch"] = dict(tested_at_patch_since_emit)
    return result_payload


async def _process_entries(redis, entries) -> None:
    for entry_id, fields in entries:
        job_id: str | None = None
        workspace_id = 0
        should_ack = False
        try:
            _, envelope = parse_signal_allocation_job_entry((entry_id, fields))
            job_id = str(envelope.get("job_id") or "").strip()
            workspace_id = int(envelope.get("workspace_id"))
            operation = str(envelope.get("operation") or "").strip()
            payload = envelope.get("payload") if isinstance(envelope.get("payload"), dict) else {}

            if not job_id or workspace_id <= 0:
                should_ack = True
                raise ValueError("Invalid signal test run job payload")
            if operation != "test_run":
                should_ack = True
                raise ValueError(f"Unexpected operation for test runner: {operation}")

            progress_total = len(payload.get("signal_ids") or []) if isinstance(payload.get("signal_ids"), list) else 0

            job_state = await get_signal_job(job_id)
            if job_state and str(job_state.get("status") or "") == "cancelled":
                await release_signal_test_run_workspace_lock(workspace_id, job_id)
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
                payload["job_id"] = job_id
                result = await _handle_test_run(repo, workspace_id, payload, running_state)

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
            logger.exception("💥 Failed to process signal test run job %s: %s", entry_id, exc)
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
            if job_id and workspace_id > 0:
                await release_signal_test_run_workspace_lock(workspace_id, job_id)
            if should_ack:
                await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)
            else:
                logger.warning("⚠️ Leaving stream entry pending due to missing persisted terminal state: %s", entry_id)


async def main() -> None:
    await RedisManager.start()
    redis = RedisManager.get_instance()

    await _ensure_group(redis)
    await _drain_pending(redis)

    heartbeat_task = start_worker_heartbeat("signal_test_run_runner")
    stop_event = asyncio.Event()

    def _signal_handler() -> None:
        logger.info("🛑 Stop signal received, shutting down signal test run runner...")
        stop_event.set()

    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        try:
            loop.add_signal_handler(sig, _signal_handler)
        except NotImplementedError:
            pass

    logger.info(
        "🚀 Signal test run runner ready (stream=%s, group=%s, consumer=%s)",
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
        await clear_worker_status("signal_test_run_runner")
        await RedisManager.stop()


if __name__ == "__main__":
    asyncio.run(main())
