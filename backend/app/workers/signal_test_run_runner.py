from __future__ import annotations

import asyncio
import random
import time
from collections import defaultdict
from contextlib import suppress
from datetime import datetime, timezone
from typing import Any
from uuid import uuid4

from app.api.v1.signal_sheet import SignalSheetRepository
from app.core.config import get_settings
from app.core.events.ws_event_publisher import WsEventPublisher
from app.core.logger import get_logger
from app.infrastructure.db.database import AsyncSessionLocal
from app.infrastructure.protocol.modes import Cmd, State
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.redis.stream_bus import parse_signal_allocation_job_entry
from app.schemas.ws.events import SignalTestRuntimePatchEvent, build_signal_job_event
from app.services.command_queue_service import enqueue_ao_command, enqueue_do_command, enqueue_request_state
from app.services.signal_job_service import (
    acquire_signal_test_run_execution_lease,
    get_signal_job,
    get_signal_job_progress_cursor,
    get_signal_job_status,
    increment_signal_test_run_execution_lease_stat,
    refresh_signal_job_ttl,
    refresh_signal_test_run_execution_lease,
    release_signal_test_run_execution_lease,
    release_signal_test_run_workspace_lock,
    set_signal_job_progress_cursor,
    update_signal_job,
)
from app.services.processed_job_service import (
    has_processed_job_marker,
    write_processed_job_marker_best_effort,
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
logger = get_logger("worker.signal_test_run")

STREAM_NAME = settings.signal_test_run_job_stream
GROUP_NAME = "signal-test-runner"
CONSUMER_NAME = build_worker_consumer_name()
WORKER_NAME = "signal_test_run_runner"
_lease_stats: dict[str, int] = defaultdict(int)


def _bump_lease_stat(name: str) -> int:
    _lease_stats[name] += 1
    return _lease_stats[name]


def _lease_stats_snapshot() -> dict[str, int]:
    return dict(_lease_stats)


async def _record_lease_stat(name: str) -> int:
    count = _bump_lease_stat(name)
    try:
        await increment_signal_test_run_execution_lease_stat(name)
    except Exception:  # noqa: BLE001
        logger.exception("💥 Failed to persist signal test run lease stat %s", name)
    return count


async def _ensure_group(redis) -> None:
    await ensure_stream_consumer_group(
        redis,
        stream_name=STREAM_NAME,
        group_name=GROUP_NAME,
        logger=logger,
        create_label="signal test run",
        exists_label="Signal test run",
    )


async def _fetch(redis, stream_id: str, block_ms: int = 5000):
    return await fetch_stream_group_entries(
        redis,
        stream_name=STREAM_NAME,
        group_name=GROUP_NAME,
        consumer_name=CONSUMER_NAME,
        stream_id=stream_id,
        count=10,
        block_ms=block_ms,
    )


async def _drain_pending(redis) -> None:
    await drain_pending_stream_entries(
        fetch_pending=lambda stream_id, block_ms: _fetch(redis, stream_id, block_ms=block_ms),
        process_entries=lambda entries: _process_entries(redis, entries),
        logger=logger,
        replay_label="signal test jobs",
        replay_limit=1000,
    )


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


async def _publish_test_runtime_patch(
    *,
    workspace_id: int,
    job_id: str,
    tested_at_by_signal: dict[int, str],
) -> None:
    if not tested_at_by_signal:
        return
    await WsEventPublisher.publish(
        SignalTestRuntimePatchEvent(
            job_id=job_id,
            workspace_id=workspace_id,
            tested_at_by_signal=dict(tested_at_by_signal),
            emitted_at=datetime.now(timezone.utc),
        )
    )


def _extract_job_attempt_meta(job_state: dict[str, Any] | None) -> tuple[str | None, int]:
    if not isinstance(job_state, dict):
        return None, 0
    result = job_state.get("result")
    if not isinstance(result, dict):
        return None, 0
    attempt_id_raw = result.get("attempt_id")
    attempt_no_raw = result.get("attempt_no")
    attempt_id = str(attempt_id_raw).strip() if attempt_id_raw is not None else ""
    try:
        attempt_no = max(0, int(attempt_no_raw or 0))
    except (TypeError, ValueError):
        attempt_no = 0
    return (attempt_id or None), attempt_no


async def _handle_test_run(
    repo: SignalSheetRepository,
    workspace_id: int,
    payload: dict[str, Any],
    job_state: dict[str, Any],
    execution_lease_owner: str | None = None,
    execution_attempt_id: str | None = None,
    execution_attempt_no: int = 0,
) -> dict[str, Any]:
    requested_ids_raw = payload.get("signal_ids") if isinstance(payload, dict) else None
    signal_interval_ms_raw = payload.get("signal_interval_ms") if isinstance(payload, dict) else None
    toggle_mode_raw = payload.get("toggle_mode") if isinstance(payload, dict) else None
    resume_from_cursor_raw = payload.get("resume_from_cursor") if isinstance(payload, dict) else None
    resume_job_id_raw = payload.get("resume_job_id") if isinstance(payload, dict) else None

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
    if toggle_mode not in {"single", "double", "ao_random"}:
        toggle_mode = "single"
    if toggle_mode == "ao_random":
        # Backward compatibility for older queued jobs: DO falls back to single toggle,
        # while AO rows are still handled via the random analog branch below.
        toggle_mode = "single"

    signal_interval_seconds = signal_interval_ms / 1000
    resume_from_cursor = bool(resume_from_cursor_raw)
    redis = RedisManager.get_instance()

    original_requested_ids = list(requested_ids)
    original_total = len(original_requested_ids)
    resume_offset = 0
    resume_base_succeeded = 0
    resume_base_skipped = 0
    cursor_reason = "disabled"
    resume_cursor_job_id = str(resume_job_id_raw or payload.get("job_id") or "").strip()
    if resume_from_cursor and original_total > 0:
        cursor_reason = "not_found"
        if not resume_cursor_job_id:
            cursor_reason = "resume_job_id_missing"
            cursor_payload = None
        else:
            cursor_payload = await get_signal_job_progress_cursor(resume_cursor_job_id)
        if isinstance(cursor_payload, dict):
            cursor_reason = "cursor_loaded"
            try:
                cursor_index = int(cursor_payload.get("index") or 0)
            except (TypeError, ValueError):
                cursor_index = 0
                cursor_reason = "invalid_index"
            resume_offset = max(0, min(cursor_index, original_total))
            try:
                resume_base_succeeded = max(0, int(cursor_payload.get("succeeded") or 0))
            except (TypeError, ValueError):
                resume_base_succeeded = 0
            try:
                resume_base_skipped = max(0, int(cursor_payload.get("skipped") or 0))
            except (TypeError, ValueError):
                resume_base_skipped = 0
            if resume_offset > 0:
                requested_ids = requested_ids[resume_offset:]
                cursor_reason = "resume_applied"
                logger.info(
                    "↪️ Resuming signal test run job %s from cursor job %s index=%s/%s",
                    str(payload.get('job_id') or ''),
                    resume_cursor_job_id,
                    resume_offset,
                    original_total,
                )
            elif cursor_reason == "cursor_loaded":
                cursor_reason = "cursor_zero"
    elif resume_from_cursor and original_total <= 0:
        cursor_reason = "empty_request"

    resume_applied = resume_offset > 0

    total = len(requested_ids)
    progress_total_global = original_total if original_total > 0 else total
    update_every = max(1, total // 25) if total > 0 else 1
    last_emit_at = 0.0
    last_ttl_refresh_at = 0.0
    tested_at_batch_size = max(1, int(settings.signal_test_run_tested_at_batch_size))
    ttl_refresh_seconds = max(1, int(settings.signal_test_run_ttl_refresh_seconds))
    cursor_persist_interval_seconds = 1.0
    last_cursor_persist_at = 0.0

    succeeded_signal_ids: list[int] = []
    tested_at_by_signal: dict[int, str] = {}
    skipped = 0
    skip_reasons = {
        "missing_row": 0,
        "invalid_binding": 0,
        "incompatible_channel_mode": 0,
        "offline_unit": 0,
    }
    evidence_count = 0

    job_id = str(payload.get("job_id") or "")
    pending_tested_at_by_signal: dict[int, str] = {}
    tested_at_patch_since_emit: dict[int, str] = {}

    async def attach_and_publish_tested_at_patch(result_payload: dict[str, Any]) -> None:
        if not tested_at_patch_since_emit:
            return
        patch = dict(tested_at_patch_since_emit)
        result_payload["tested_at_patch"] = patch
        await _publish_test_runtime_patch(
            workspace_id=workspace_id,
            job_id=job_id,
            tested_at_by_signal=patch,
        )
        tested_at_patch_since_emit.clear()

    async def flush_tested_at_batch() -> None:
        if not pending_tested_at_by_signal:
            return
        try:
            await repo.mark_signals_tested_at(
                workspace_id,
                dict(pending_tested_at_by_signal),
                commit=False,
            )
            await repo.db.commit()
        except Exception:
            await repo.db.rollback()
            raise
        pending_tested_at_by_signal.clear()

    async def record_step_evidence(
        *,
        order_index: int,
        signal_id: int,
        status: str,
        row: SignalAllocationRowSchema | None = None,
        reason: str | None = None,
        result_state: str | None = None,
        command_payload: dict[str, Any] | None = None,
        tested_at: datetime | None = None,
    ) -> None:
        nonlocal evidence_count
        if not job_id:
            return
        await repo.record_signal_test_run_step_evidence(
            workspace_id=workspace_id,
            job_id=job_id,
            attempt_id=execution_attempt_id,
            attempt_no=execution_attempt_no,
            order_index=order_index,
            signal_id=signal_id,
            allocation_id=row.allocation_id if row is not None else None,
            channel_id=row.channel_id if row is not None else None,
            device_id=row.device_id if row is not None else None,
            unit_id=row.unit_id if row is not None else None,
            channel_index=row.channel_index if row is not None else None,
            channel_type=row.channel_type if row is not None else None,
            status=status,
            reason=reason,
            result_state=result_state,
            command_payload=command_payload,
            tested_at=tested_at,
        )
        evidence_count += 1

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
        if execution_lease_owner:
            try:
                lease_ok = await refresh_signal_test_run_execution_lease(
                    job_id=job_id,
                    owner=execution_lease_owner,
                )
            except Exception:
                count = await _record_lease_stat("refresh_error")
                logger.exception(
                    "💥 Failed to refresh signal test run execution lease for job %s (refresh_error=%s)",
                    job_id,
                    count,
                )
                raise
            if not lease_ok:
                count = await _record_lease_stat("refresh_lost")
                logger.error(
                    "💥 Signal test run execution lease lost for job %s (refresh_lost=%s)",
                    job_id,
                    count,
                )
                raise RuntimeError("Signal test run execution lease lost")

    async def persist_progress_cursor(
        *,
        phase: str,
        index: int,
        signal_id: int | None = None,
        force: bool = False,
    ) -> None:
        nonlocal last_cursor_persist_at
        if not job_id:
            return
        now_mono = time.monotonic()
        if not force and (now_mono - last_cursor_persist_at) < cursor_persist_interval_seconds:
            return
        last_cursor_persist_at = now_mono
        try:
            await set_signal_job_progress_cursor(
                job_id,
                {
                    "phase": phase,
                    "index": max(0, int(index)),
                    "total": progress_total_global,
                    "signal_id": int(signal_id) if signal_id is not None else None,
                    "succeeded": resume_base_succeeded + len(succeeded_signal_ids),
                    "skipped": resume_base_skipped + skipped,
                    "toggle_mode": toggle_mode,
                    "signal_interval_ms": signal_interval_ms,
                    "resume_offset": resume_offset,
                    "attempt_id": execution_attempt_id,
                    "attempt_no": execution_attempt_no,
                },
            )
        except Exception:  # noqa: BLE001
            logger.exception("💥 Failed to persist signal test run progress cursor for job %s", job_id)

    unit_bitmasks: dict[str, int] = {}

    async def get_unit_bitmask(unit_id: str) -> int:
        if unit_id in unit_bitmasks:
            return unit_bitmasks[unit_id]
        bitmask = 0
        bitmask_raw = await redis.get(f"device:{unit_id}:bitmask")
        if bitmask_raw is not None:
            try:
                bitmask = int(bitmask_raw)
            except (TypeError, ValueError):
                bitmask = 0
        unit_bitmasks[unit_id] = bitmask
        return bitmask

    async def resolve_current_signal_row(signal_id: int):
        rows = await repo.list_allocation_rows_by_signal_ids(workspace_id, [signal_id])
        row = next((item for item in rows if int(item.signal_id) == int(signal_id)), None)
        if row is None:
            return None, "missing_row"
        if not row.unit_id or not isinstance(row.channel_index, int):
            return row, "invalid_binding"
        channel_type = str(row.channel_type or "").strip().lower()
        is_do = channel_type.startswith("do")
        is_ao = channel_type.startswith("ao")
        if not is_do and not is_ao:
            return row, "incompatible_channel_mode"
        if row.unit_online is False:
            return row, "offline_unit"
        return row, None

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
        progress_done_global = min(progress_total_global, resume_offset + index)
        if not await apply_control_state():
            await persist_progress_cursor(
                phase="cancelled",
                index=min(progress_total_global, resume_offset + index - 1),
                force=True,
            )
            result_payload = {
                "processed": min(progress_total_global, resume_offset + index - 1),
                "succeeded": resume_base_succeeded + len(succeeded_signal_ids),
                "skipped": resume_base_skipped + skipped,
                "skip_reasons": skip_reasons,
                "toggle_mode": toggle_mode,
                "signal_interval_ms": signal_interval_ms,
                "tested_signal_ids": sorted(tested_at_by_signal.keys()),
                "cancelled": True,
                "resumed_from_cursor": bool(resume_offset > 0),
                "resume_applied": resume_applied,
                "resume_offset": resume_offset,
                "cursor_reason": cursor_reason,
                "resume_job_id": resume_cursor_job_id or None,
                "evidence_count": evidence_count,
            }
            await attach_and_publish_tested_at_patch(result_payload)
            return result_payload

        row, skip_reason = await resolve_current_signal_row(signal_id)
        success = False
        if skip_reason is not None:
            skipped += 1
            skip_reasons[skip_reason] += 1
            await record_step_evidence(
                order_index=progress_done_global,
                signal_id=signal_id,
                status="skipped",
                row=row,
                reason=skip_reason,
                result_state=skip_reason,
                command_payload={
                    "toggle_mode": toggle_mode,
                    "signal_interval_ms": signal_interval_ms,
                },
            )
        elif row is not None:
            unit_id = str(row.unit_id)
            channel_index = int(row.channel_index)
            channel_type = str(row.channel_type or "").strip().lower()
            command_payload: dict[str, Any]
            if channel_type.startswith("ao"):
                random_value = round(random.uniform(0.0, 24.0), 2)
                ao_correlation_id = f"test-run:{signal_id}:ao:{random_value}"
                state_correlation_id = f"test-run:{signal_id}:state-float"
                command_payload = {
                    "toggle_mode": "ao_random",
                    "signal_interval_ms": signal_interval_ms,
                    "commands": [
                        {
                            "kind": "ao_set",
                            "unit_id": unit_id,
                            "channel_index": channel_index,
                            "value": random_value,
                            "correlation_id": ao_correlation_id,
                        },
                        {
                            "kind": "request_state",
                            "unit_id": unit_id,
                            "channel_index": channel_index,
                            "mode": "REQ_SINGLE_FLOAT",
                            "correlation_id": state_correlation_id,
                        },
                    ],
                }
                await enqueue_ao_command(
                    unit_id=unit_id,
                    ch=channel_index,
                    value=random_value,
                    correlation_id=ao_correlation_id,
                )
                await enqueue_request_state(
                    unit_id=unit_id,
                    mode=State.REQ_SINGLE_FLOAT,
                    ch=channel_index,
                    correlation_id=state_correlation_id,
                )
            else:
                bitmask = await get_unit_bitmask(unit_id)
                current_value = 1 if (bitmask & (1 << channel_index)) else 0
                toggled_value = 0 if current_value else 1
                set_correlation_id = f"test-run:{signal_id}:set:{toggled_value}"
                commands_payload: list[dict[str, Any]] = [
                    {
                        "kind": "do_set",
                        "unit_id": unit_id,
                        "channel_index": channel_index,
                        "value": toggled_value,
                        "correlation_id": set_correlation_id,
                    }
                ]
                await enqueue_do_command(
                    unit_id=unit_id,
                    mode=Cmd.SET_SINGLE_BIT,
                    ch=channel_index,
                    value=toggled_value,
                    correlation_id=set_correlation_id,
                )

                if toggled_value:
                    bitmask = bitmask | (1 << channel_index)
                else:
                    bitmask = bitmask & ~(1 << channel_index)

                if toggle_mode == "double":
                    await asyncio.sleep(signal_interval_seconds)
                    restore_correlation_id = f"test-run:{signal_id}:set:{current_value}"
                    commands_payload.append(
                        {
                            "kind": "do_set",
                            "unit_id": unit_id,
                            "channel_index": channel_index,
                            "value": current_value,
                            "correlation_id": restore_correlation_id,
                        }
                    )
                    await enqueue_do_command(
                        unit_id=unit_id,
                        mode=Cmd.SET_SINGLE_BIT,
                        ch=channel_index,
                        value=current_value,
                        correlation_id=restore_correlation_id,
                    )
                    if current_value:
                        bitmask = bitmask | (1 << channel_index)
                    else:
                        bitmask = bitmask & ~(1 << channel_index)

                unit_bitmasks[unit_id] = bitmask
                state_correlation_id = f"test-run:{signal_id}:state"
                commands_payload.append(
                    {
                        "kind": "request_state",
                        "unit_id": unit_id,
                        "channel_index": channel_index,
                        "mode": "REQ_SINGLE_BIT",
                        "correlation_id": state_correlation_id,
                    }
                )
                command_payload = {
                    "toggle_mode": toggle_mode,
                    "signal_interval_ms": signal_interval_ms,
                    "initial_value": current_value,
                    "target_value": toggled_value,
                    "commands": commands_payload,
                }
                await enqueue_request_state(
                    unit_id=unit_id,
                    mode=State.REQ_SINGLE_BIT,
                    ch=channel_index,
                    correlation_id=state_correlation_id,
                )
            success = True

        if success:
            succeeded_signal_ids.append(signal_id)
            tested_at_dt = datetime.now(timezone.utc)
            tested_at = tested_at_dt.isoformat()
            tested_at_by_signal[signal_id] = tested_at
            pending_tested_at_by_signal[signal_id] = tested_at
            tested_at_patch_since_emit[signal_id] = tested_at
            await record_step_evidence(
                order_index=progress_done_global,
                signal_id=signal_id,
                status="succeeded",
                row=row,
                result_state="commands_enqueued",
                command_payload=command_payload,
                tested_at=tested_at_dt,
            )
            if len(pending_tested_at_by_signal) >= tested_at_batch_size:
                await flush_tested_at_batch()

        should_emit = index >= total or index <= 1 or index % update_every == 0
        now = time.monotonic()
        await maybe_refresh_ttl()
        if should_emit or (now - last_emit_at) >= 0.35:
            last_emit_at = now
            result_payload: dict[str, Any] = {
                "processed": progress_done_global,
                "succeeded": resume_base_succeeded + len(succeeded_signal_ids),
                "skipped": resume_base_skipped + skipped,
                "skip_reasons": dict(skip_reasons),
                "toggle_mode": toggle_mode,
                "signal_interval_ms": signal_interval_ms,
                "resumed_from_cursor": bool(resume_offset > 0),
                "resume_applied": resume_applied,
                "resume_offset": resume_offset,
                "cursor_reason": cursor_reason,
                "resume_job_id": resume_cursor_job_id or None,
                "attempt_id": execution_attempt_id,
                "attempt_no": execution_attempt_no,
                "evidence_count": evidence_count,
            }
            result_payload["progress_cursor"] = {
                "phase": "running",
                "index": progress_done_global,
                "total": progress_total_global,
                "signal_id": signal_id,
                "succeeded": resume_base_succeeded + len(succeeded_signal_ids),
                "skipped": resume_base_skipped + skipped,
                "attempt_id": execution_attempt_id,
                "attempt_no": execution_attempt_no,
            }
            await attach_and_publish_tested_at_patch(result_payload)
            await persist_progress_cursor(phase="running", index=progress_done_global, signal_id=signal_id)
            await _publish_running_progress(
                job_state=job_state,
                progress_done=progress_done_global,
                progress_total=progress_total_global,
                message=f"Signals {progress_done_global}/{progress_total_global} · ok {resume_base_succeeded + len(succeeded_signal_ids)} · skip {resume_base_skipped + skipped}",
                result=result_payload,
            )

        if success and index < total:
            slept = 0.0
            while slept < signal_interval_seconds:
                if not await apply_control_state():
                    await flush_tested_at_batch()
                    await persist_progress_cursor(
                        phase="cancelled",
                        index=progress_done_global,
                        signal_id=signal_id,
                        force=True,
                    )
                    result_payload = {
                        "processed": progress_done_global,
                        "succeeded": resume_base_succeeded + len(succeeded_signal_ids),
                        "skipped": resume_base_skipped + skipped,
                        "skip_reasons": skip_reasons,
                        "toggle_mode": toggle_mode,
                        "signal_interval_ms": signal_interval_ms,
                        "tested_signal_ids": sorted(tested_at_by_signal.keys()),
                        "cancelled": True,
                        "resumed_from_cursor": bool(resume_offset > 0),
                        "resume_applied": resume_applied,
                        "resume_offset": resume_offset,
                        "cursor_reason": cursor_reason,
                        "resume_job_id": resume_cursor_job_id or None,
                        "attempt_id": execution_attempt_id,
                        "attempt_no": execution_attempt_no,
                        "evidence_count": evidence_count,
                    }
                    await attach_and_publish_tested_at_patch(result_payload)
                    return result_payload
                step = min(0.2, signal_interval_seconds - slept)
                await asyncio.sleep(step)
                slept += step

    await flush_tested_at_batch()
    result_payload: dict[str, Any] = {
        "processed": min(progress_total_global, resume_offset + total),
        "succeeded": resume_base_succeeded + len(succeeded_signal_ids),
        "skipped": resume_base_skipped + skipped,
        "skip_reasons": skip_reasons,
        "toggle_mode": toggle_mode,
        "signal_interval_ms": signal_interval_ms,
        "tested_signal_ids": sorted(tested_at_by_signal.keys()),
        "resumed_from_cursor": bool(resume_offset > 0),
        "resume_applied": resume_applied,
        "resume_offset": resume_offset,
        "cursor_reason": cursor_reason,
        "resume_job_id": resume_cursor_job_id or None,
        "attempt_id": execution_attempt_id,
        "attempt_no": execution_attempt_no,
        "evidence_count": evidence_count,
    }
    result_payload["progress_cursor"] = {
        "phase": "completed",
        "index": min(progress_total_global, resume_offset + total),
        "total": progress_total_global,
        "signal_id": None,
        "succeeded": resume_base_succeeded + len(succeeded_signal_ids),
        "skipped": resume_base_skipped + skipped,
        "attempt_id": execution_attempt_id,
        "attempt_no": execution_attempt_no,
    }
    await persist_progress_cursor(
        phase="completed",
        index=min(progress_total_global, resume_offset + total),
        force=True,
    )
    await attach_and_publish_tested_at_patch(result_payload)
    return result_payload


async def _process_entries(redis, entries) -> None:
    for entry_id, fields in entries:
        job_id: str | None = None
        workspace_id = 0
        should_ack = False
        job_started_monotonic: float | None = None
        execution_lease_owner: str | None = None
        execution_attempt_id: str | None = None
        execution_attempt_no = 0
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
            toggle_mode = str(payload.get("toggle_mode") or "single").strip().lower()
            try:
                signal_interval_ms = int(payload.get("signal_interval_ms") or 1000)
            except (TypeError, ValueError):
                signal_interval_ms = 1000
            resume_requested = bool(payload.get("resume_from_cursor"))
            resume_job_id = str(payload.get("resume_job_id") or "").strip() or None

            job_state = await get_signal_job(job_id)
            if job_state:
                current_status = str(job_state.get("status") or "").strip().lower()
                if current_status in {"cancelled", "succeeded", "failed"}:
                    await release_signal_test_run_workspace_lock(workspace_id, job_id)
                    should_ack = True
                    logger.info("ℹ️ Skipping terminal signal test job %s (status=%s)", job_id, current_status)
                    continue

                # Long-running jobs are not replayed silently after a crash/restart.
                # If we see cursor/attempt evidence for a previously started execution,
                # we fail the job explicitly and require a new resume-from-cursor run.
                existing_cursor = job_state.get("progress_cursor")
                previous_attempt_id, previous_attempt_no = _extract_job_attempt_meta(job_state)
                cursor_phase = ""
                if isinstance(existing_cursor, dict):
                    cursor_phase = str(existing_cursor.get("phase") or "").strip().lower()
                    if not previous_attempt_id:
                        previous_attempt_id = str(existing_cursor.get("attempt_id") or "").strip() or None
                    if previous_attempt_no <= 0:
                        try:
                            previous_attempt_no = max(0, int(existing_cursor.get("attempt_no") or 0))
                        except (TypeError, ValueError):
                            previous_attempt_no = 0
                cursor_index_value = 0
                if isinstance(existing_cursor, dict):
                    try:
                        cursor_index_value = max(0, int(existing_cursor.get("index") or 0))
                    except (TypeError, ValueError):
                        cursor_index_value = 0
                has_prior_execution_evidence = bool(previous_attempt_id) or (
                    isinstance(existing_cursor, dict)
                    and (
                        cursor_phase in {"precheck", "running", "cancelled", "completed"}
                        or cursor_index_value > 0
                    )
                )
                if has_prior_execution_evidence:
                    recovery_result: dict[str, Any] = {
                        "recovery_policy": "fail_on_replay_after_started_attempt",
                        "recovery_reason": "replayed_pending_entry_after_started_attempt",
                        "resume_supported": True,
                        "resume_hint": "Start a new test run with resume_from_cursor=true and resume_job_id=<failed_job_id>.",
                        "attempt_id": previous_attempt_id,
                        "attempt_no": previous_attempt_no,
                    }
                    if isinstance(existing_cursor, dict):
                        recovery_result["progress_cursor"] = existing_cursor
                    failed_state = await update_signal_job(
                        job_id,
                        status="failed",
                        message="Interrupted (replay detected, explicit resume required)",
                        error="replay_after_started_attempt",
                        progress_done=min(
                            progress_total,
                            cursor_index_value,
                        ),
                        progress_total=progress_total,
                        result=recovery_result,
                    )
                    if failed_state:
                        await WsEventPublisher.publish(build_signal_job_event(failed_state))
                        should_ack = True
                        logger.warning(
                            "⚠️ Marked signal test run job %s as failed on replay after started attempt (attempt_id=%s)",
                            job_id,
                            previous_attempt_id or "unknown",
                        )
                        continue

            execution_lease_owner = f"{CONSUMER_NAME}:{entry_id}"
            lease_acquired = await acquire_signal_test_run_execution_lease(
                job_id=job_id,
                owner=execution_lease_owner,
            )
            if not lease_acquired:
                count = await _record_lease_stat("acquire_busy")
                logger.warning(
                    "⚠️ Signal test run execution lease busy for job %s; leaving entry pending (acquire_busy=%s)",
                    job_id,
                    count,
                )
                continue

            execution_attempt_id = uuid4().hex
            _, prev_attempt_no = _extract_job_attempt_meta(job_state)
            execution_attempt_no = max(1, prev_attempt_no + 1)
            running_result: dict[str, Any] = {
                "attempt_id": execution_attempt_id,
                "attempt_no": execution_attempt_no,
                "execution_lease_owner": execution_lease_owner,
                "execution_started_at": datetime.now(timezone.utc).isoformat(),
                "execution_policy": "lease+cursor+explicit_replay_fail",
            }

            running_state = await update_signal_job(
                job_id,
                status="running",
                message="Running",
                progress_done=0,
                progress_total=progress_total,
                result=running_result,
            )
            if running_state:
                await WsEventPublisher.publish(build_signal_job_event(running_state))
            else:
                logger.warning("⚠️ Job state missing before start, leaving entry pending: %s", job_id)
                continue

            job_started_monotonic = time.monotonic()
            logger.info(
                "▶️ Signal test run job start | workspace=%s job=%s entries=%s toggle=%s interval_ms=%s resume=%s resume_job=%s attempt_no=%s",
                workspace_id,
                job_id,
                progress_total,
                toggle_mode,
                signal_interval_ms,
                resume_requested,
                resume_job_id or "-",
                execution_attempt_no,
            )

            async with AsyncSessionLocal() as session:
                try:
                    if await has_processed_job_marker(session, worker_name=WORKER_NAME, job_id=job_id):
                        logger.warning(
                            "⚠️ Replayed already-processed signal test job %s; recovering terminal state only",
                            job_id,
                        )
                        recovered_state = await update_signal_job(
                            job_id,
                            status="succeeded",
                            message="Completed (recovered from replay)",
                            progress_done=progress_total,
                            progress_total=progress_total,
                        )
                        if recovered_state:
                            await WsEventPublisher.publish(build_signal_job_event(recovered_state))
                            should_ack = True
                        continue

                    repo = SignalSheetRepository(session)
                    if not await repo.ensure_workspace(workspace_id):
                        raise ValueError("Workspace not found")
                    payload_with_job_id = {**payload, "job_id": job_id}
                    result = await _handle_test_run(
                        repo,
                        workspace_id,
                        payload_with_job_id,
                        running_state,
                        execution_lease_owner=execution_lease_owner,
                        execution_attempt_id=execution_attempt_id,
                        execution_attempt_no=execution_attempt_no,
                    )
                    await write_processed_job_marker_best_effort(
                        session,
                        worker_name=WORKER_NAME,
                        job_id=job_id,
                        stream_name=STREAM_NAME,
                        entry_id=entry_id,
                    )
                    await session.commit()
                except Exception:
                    await session.rollback()
                    raise

            cancelled = bool((result or {}).get("cancelled")) if isinstance(result, dict) else False
            current_before_terminal = await get_signal_job(job_id)
            current_attempt_id, _ = _extract_job_attempt_meta(current_before_terminal)
            if execution_attempt_id and current_attempt_id and current_attempt_id != execution_attempt_id:
                logger.warning(
                    "⚠️ Skipping terminal update for job %s due to attempt mismatch (current=%s, expected=%s)",
                    job_id,
                    current_attempt_id,
                    execution_attempt_id,
                )
                continue

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
                result_payload = result if isinstance(result, dict) else {}
                duration_ms = int(((time.monotonic() - job_started_monotonic) * 1000)) if job_started_monotonic else 0
                logger.info(
                    "✅ Signal test run job complete | workspace=%s job=%s status=%s duration=%sms processed=%s ok=%s skip=%s cancelled=%s resume_applied=%s resume_offset=%s reason=%s",
                    workspace_id,
                    job_id,
                    "cancelled" if cancelled else "succeeded",
                    duration_ms,
                    result_payload.get("processed", progress_total),
                    result_payload.get("succeeded", 0),
                    result_payload.get("skipped", 0),
                    bool(result_payload.get("cancelled", False)),
                    bool(result_payload.get("resume_applied", False)),
                    int(result_payload.get("resume_offset") or 0),
                    str(result_payload.get("cursor_reason") or "-"),
                )
                should_ack = True
        except Exception as exc:  # noqa: BLE001
            duration_ms = int(((time.monotonic() - job_started_monotonic) * 1000)) if job_started_monotonic else 0
            logger.error(
                "❌ Signal test run job failed | workspace=%s job=%s duration=%sms err=%s",
                workspace_id if workspace_id > 0 else "-",
                job_id or "-",
                duration_ms,
                str(exc),
            )
            logger.exception("💥 Failed to process signal test run job %s: %s", entry_id, exc)
            if job_id:
                current_before_fail = await get_signal_job(job_id)
                current_attempt_id, _ = _extract_job_attempt_meta(current_before_fail)
                if execution_attempt_id and current_attempt_id and current_attempt_id != execution_attempt_id:
                    logger.warning(
                        "⚠️ Suppressing failed terminal update for job %s due to attempt mismatch (current=%s, expected=%s)",
                        job_id,
                        current_attempt_id,
                        execution_attempt_id,
                    )
                    should_ack = False
                    continue
                failed_state = await update_signal_job(
                    job_id,
                    status="failed",
                    message="Failed",
                    error=str(exc),
                    result={
                        "attempt_id": execution_attempt_id,
                        "attempt_no": execution_attempt_no,
                        "failure_policy": "attempt_scoped_terminal_update",
                    } if execution_attempt_id else None,
                )
                if failed_state:
                    await WsEventPublisher.publish(build_signal_job_event(failed_state))
                    should_ack = True
            else:
                should_ack = True
        finally:
            if job_id and execution_lease_owner:
                try:
                    released = await release_signal_test_run_execution_lease(
                        job_id=job_id,
                        owner=execution_lease_owner,
                    )
                    if not released:
                        count = await _record_lease_stat("release_not_owner_or_missing")
                        logger.debug(
                            "ℹ️ Signal test run execution lease release skipped for job %s (release_not_owner_or_missing=%s)",
                            job_id,
                            count,
                        )
                except Exception:  # noqa: BLE001
                    count = await _record_lease_stat("release_error")
                    logger.exception("💥 Failed to release execution lease for signal test run job %s", job_id)
                    logger.debug("🔎 Lease stats: %s", _lease_stats_snapshot())
            if job_id and workspace_id > 0:
                await release_signal_test_run_workspace_lock(workspace_id, job_id)
            if should_ack:
                await redis.xack(STREAM_NAME, GROUP_NAME, entry_id)
            else:
                logger.warning("⚠️ Leaving stream entry pending due to missing persisted terminal state: %s", entry_id)


async def main() -> None:
    await RedisManager.start()
    redis = RedisManager.get_instance()

    heartbeat_task = start_worker_heartbeat("signal_test_run_runner")
    stop_event = asyncio.Event()
    install_stop_signal_handlers(
        stop_event=stop_event,
        logger=logger,
        stop_message="🛑 Stop signal received, shutting down signal test run runner...",
    )

    await _ensure_group(redis)
    await _drain_pending(redis)

    logger.info(
        "🚀 Signal test run runner ready (stream=%s, group=%s, consumer=%s)",
        STREAM_NAME,
        GROUP_NAME,
        CONSUMER_NAME,
    )
    logger.debug("🔎 Signal test run execution lease stats: %s", _lease_stats_snapshot())

    try:
        await run_consume_loop(
            stop_event=stop_event,
            fetch_entries=lambda: _fetch(redis, ">"),
            process_entries=lambda entries: _process_entries(redis, entries),
            logger=logger,
        )
    finally:
        heartbeat_task.cancel()
        with suppress(asyncio.CancelledError):
            await heartbeat_task
        await clear_worker_status("signal_test_run_runner")
        await RedisManager.stop()


if __name__ == "__main__":
    asyncio.run(main())
