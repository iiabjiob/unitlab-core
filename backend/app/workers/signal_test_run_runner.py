from __future__ import annotations

import asyncio
import random
import threading
import time
from collections import defaultdict
from contextlib import suppress
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Any
from uuid import uuid4

from sqlalchemy import select

from app.api.v1.signal_sheet import SignalSheetRepository
from app.core.config import get_settings
from app.core.events.ws_event_publisher import WsEventPublisher
from app.core.logger import get_logger
from app.infrastructure.db.database import AsyncSessionLocal
from app.infrastructure.protocol.modes import Cmd, State
from app.infrastructure.redis.manager import RedisManager
from app.models.signal_revision import SignalTestRunPlan
from app.infrastructure.redis.stream_bus import parse_signal_allocation_job_entry
from app.schemas.ws.events import SignalTestRuntimePatchEvent, build_signal_job_event
from app.schemas.signal_sheet_schema import SignalAllocationRowSchema
from app.schemas.verification_schema import (
    VerificationAutoRunStartSchema,
    VerificationEvidenceDiagnosticSchema,
    VerificationExecutionContextSchema,
)
from app.services.command_queue_service import enqueue_ao_command, enqueue_do_command, enqueue_request_state
from app.services.hardware_command_admission import HardwareChannelLease, HardwareCommandAdmission
from app.services.hardware_command_intent import (
    has_hardware_recovery_required,
    mark_hardware_command_intent_delivery_failure,
    mark_hardware_command_intent_queued,
    reconcile_unfinished_hardware_command_intents,
    record_hardware_command_intent,
)
from app.services.hardware_command_ack import wait_for_hardware_command_acks
from app.services.external_ied_discovery_scheduler import schedule_external_ied_discovery_for_verification
from app.services.iec61850.report_runtime import group_report_subscription_plan_devices_by_endpoint
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
from app.services.verification_evidence import VerificationEvidenceRepository, build_signal_verification_evidence_set
from app.services.verification_execution import build_runtime_subscription_plan
from app.services.verification_run_service import build_verification_runtime_start_context
from app.services.verification_runtime_orchestrator import VerificationRuntimeOrchestrator
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


async def _wait_for_bit_readback(
    redis,
    *,
    unit_id: str,
    channel_index: int,
    expected_value: int,
    timeout_ms: int,
    packet_id: int | None = None,
) -> bool:
    deadline = time.monotonic() + max(100, timeout_ms) / 1000
    mask = 1 << int(channel_index)
    while True:
        fresh = True
        if packet_id is not None:
            try:
                fresh = int(await redis.get(f"device:{unit_id}:last_state_packet_id")) == int(packet_id)
            except (TypeError, ValueError):
                fresh = False
        raw_bitmask = await redis.get(f"device:{unit_id}:bitmask")
        try:
            if raw_bitmask is None:
                raise ValueError("missing bitmask")
            bitmask = int(raw_bitmask)
        except (TypeError, ValueError):
            bitmask = None
        if bitmask is None:
            if time.monotonic() >= deadline:
                return False
            await asyncio.sleep(0.05)
            continue
        actual_value = 1 if bitmask & mask else 0
        if fresh and actual_value == int(expected_value):
            return True
        if time.monotonic() >= deadline:
            return False
        await asyncio.sleep(0.05)


async def _wait_for_float_readback(
    redis,
    *,
    unit_id: str,
    channel_index: int,
    expected_value: float,
    timeout_ms: int,
    packet_id: int | None = None,
) -> bool:
    deadline = time.monotonic() + max(100, timeout_ms) / 1000
    while True:
        fresh = True
        if packet_id is not None:
            try:
                fresh = int(await redis.get(f"device:{unit_id}:last_state_packet_id")) == int(packet_id)
            except (TypeError, ValueError):
                fresh = False
        raw_value = await redis.hget(f"device:{unit_id}:ao", str(int(channel_index)))
        try:
            actual_value = float(raw_value)
        except (TypeError, ValueError):
            actual_value = None
        if fresh and actual_value is not None and abs(actual_value - float(expected_value)) <= 0.01:
            return True
        if time.monotonic() >= deadline:
            return False
        await asyncio.sleep(0.05)


async def _wait_for_fresh_bitmask_snapshot(
    redis,
    *,
    unit_id: str,
    packet_id: int | None,
    timeout_ms: int,
) -> int | None:
    deadline = time.monotonic() + max(100, int(timeout_ms)) / 1000
    while True:
        fresh = True
        if packet_id is not None:
            try:
                fresh = int(await redis.get(f"device:{unit_id}:last_state_packet_id")) == int(packet_id)
            except (TypeError, ValueError):
                fresh = False
        raw_bitmask = await redis.get(f"device:{unit_id}:bitmask")
        try:
            bitmask = int(raw_bitmask)
        except (TypeError, ValueError):
            bitmask = None
        if fresh and bitmask is not None:
            return bitmask
        if time.monotonic() >= deadline:
            return None
        await asyncio.sleep(0.05)


async def _deliver_durable_command(
    db,
    *,
    command_id: str,
    action: str,
    command_sender,
) -> None:
    """Publish a persisted command, retrying idempotent restore delivery once."""
    delivery_attempts = 2 if action == "restore" else 1
    for attempt_no in range(delivery_attempts):
        try:
            await command_sender(command_id)
            return
        except Exception:
            await db.rollback()
            await mark_hardware_command_intent_delivery_failure(
                db,
                command_id=command_id,
                status="recovery_required" if action == "restore" else "unknown",
            )
            await db.commit()
            if attempt_no + 1 == delivery_attempts:
                raise
            logger.warning(
                "Restore command delivery failed; retrying command_id=%s",
                command_id,
            )


async def _record_lease_stat(name: str) -> int:
    count = _bump_lease_stat(name)
    try:
        await increment_signal_test_run_execution_lease_stat(name)
    except Exception:  # noqa: BLE001
        logger.exception("💥 Failed to persist signal test run lease stat %s", name)
    return count


async def _schedule_external_ied_discovery_for_verification_run(
    *,
    workspace_id: int,
    runtime_context,
) -> None:
    if getattr(runtime_context.runtime_selection, "runtime_mode", None) != "mms":
        return
    runtime_plan = build_runtime_subscription_plan(runtime_context.subscription_plan)
    device_groups = group_report_subscription_plan_devices_by_endpoint(
        plan=runtime_plan,
        endpoint_for_device=runtime_context.runtime_selection.endpoint_for_device,
    )
    seen_endpoints: set[str] = set()
    for device_group in device_groups:
        endpoint = device_group.endpoint
        if not endpoint.host:
            continue
        endpoint_key = f"{endpoint.host}:{endpoint.port}"
        if endpoint_key in seen_endpoints:
            continue
        seen_endpoints.add(endpoint_key)
        try:
            await schedule_external_ied_discovery_for_verification(
                workspace_id=workspace_id,
                endpoint=endpoint_key,
            )
        except Exception as exc:  # noqa: BLE001
            logger.warning(
                "External IED discovery scheduling failed for verification run | workspace=%s endpoint=%s error=%s",
                workspace_id,
                endpoint_key,
                exc,
            )


async def _resolve_mapped_verification_signal_ids(
    rows: list[SignalAllocationRowSchema],
    signal_ids: list[int],
) -> list[int]:
    if not signal_ids:
        return []
    mapped_by_signal_id = {
        int(row.signal_id)
        for row in rows
        if _row_has_iec61850_verification_mapping(row)
    }
    return [signal_id for signal_id in signal_ids if signal_id in mapped_by_signal_id]


async def _load_immutable_plan_rows(
    repo: SignalSheetRepository,
    workspace_id: int,
    job_id: str,
) -> list[SignalAllocationRowSchema]:
    plan = await repo.db.scalar(
        select(SignalTestRunPlan).where(
            SignalTestRunPlan.job_id == job_id,
            SignalTestRunPlan.workspace_id == workspace_id,
        )
    )
    if plan is None:
        raise RuntimeError("Immutable test-run plan is missing")
    return [
        SignalAllocationRowSchema.model_validate(item.snapshot)
        for item in sorted(plan.items, key=lambda item: item.order_index)
    ]


def _row_has_iec61850_verification_mapping(row: SignalAllocationRowSchema) -> bool:
    metadata = row.signal_metadata or {}
    verification = metadata.get("verification") if isinstance(metadata.get("verification"), dict) else {}
    if verification.get("enabled") is not True:
        return False
    return _first_row_metadata_string(verification, "iec61850_address", "iec61850", "mms_reference") is not None


@dataclass(frozen=True)
class PeripheralPreflightResult:
    executable_signal_ids: list[int]
    notes: list[str]


async def _collect_peripheral_preflight(
    rows: list[SignalAllocationRowSchema],
    signal_ids: list[int],
) -> PeripheralPreflightResult:
    if not signal_ids:
        return PeripheralPreflightResult(executable_signal_ids=[], notes=[])
    rows_by_signal_id = {int(row.signal_id): row for row in rows}
    executable_signal_ids: list[int] = []
    notes: list[str] = []
    for signal_id in signal_ids:
        row = rows_by_signal_id.get(int(signal_id))
        if row is None:
            notes.append(f"{signal_id}: missing allocation row")
            continue
        if not row.unit_id or row.channel_id is None or not isinstance(row.channel_index, int):
            notes.append(f"{signal_id}: missing peripheral binding")
            continue
        channel_type = str(row.channel_type or "").strip().lower()
        if not channel_type.startswith("do") and not channel_type.startswith("ao"):
            notes.append(f"{signal_id}: incompatible peripheral channel")
            continue
        if row.unit_online is False:
            notes.append(f"{signal_id}: peripheral device offline ({row.unit_id})")
            continue
        executable_signal_ids.append(int(signal_id))
    return PeripheralPreflightResult(executable_signal_ids=executable_signal_ids, notes=notes)


def _first_row_metadata_string(payload: dict[str, Any], *keys: str) -> str | None:
    for key in keys:
        value = payload.get(key)
        if isinstance(value, str) and value.strip():
            return value.strip()
    return None


def _build_signal_test_report_entry(
    *,
    signal_id: int,
    order_index: int,
    row: SignalAllocationRowSchema | None,
    test_status: str,
    result_state: str,
    command_payload: dict[str, Any] | None,
    tested_at: datetime | None = None,
    skip_reason: str | None = None,
) -> dict[str, Any]:
    metadata = row.signal_metadata if row is not None and isinstance(row.signal_metadata, dict) else {}
    verification = metadata.get("verification") if isinstance(metadata.get("verification"), dict) else {}
    entry: dict[str, Any] = {
        "signal_id": signal_id,
        "order_index": order_index,
        "test_status": test_status,
        "result_state": result_state,
        "tested_at": tested_at.isoformat() if tested_at is not None else None,
        "skip_reason": skip_reason,
    }
    if row is not None:
        entry.update(
            {
                "row_id": row.row_id,
                "signal_key": row.signal_key,
                "signal_name": row.signal_name,
                "signal_direction": row.signal_direction,
                "allocation_id": row.allocation_id,
                "channel_id": row.channel_id,
                "channel_label": row.channel_label,
                "channel_index": row.channel_index,
                "channel_type": row.channel_type,
                "device_id": row.device_id,
                "unit_id": row.unit_id,
                "unit_online": row.unit_online,
                "iec61850_address": _first_row_metadata_string(
                    verification,
                    "iec61850_address",
                    "iec61850",
                    "mms_reference",
                ),
            }
        )
    if command_payload is not None:
        entry["command"] = command_payload
        verification_payload = command_payload.get("iec61850_verification")
        if isinstance(verification_payload, dict):
            entry["iec61850_verification"] = verification_payload
    return entry


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
    test_status_by_signal: dict[int, str] | None = None,
) -> None:
    if not tested_at_by_signal and not test_status_by_signal:
        return
    await WsEventPublisher.publish(
        SignalTestRuntimePatchEvent(
            job_id=job_id,
            workspace_id=workspace_id,
            tested_at_by_signal=dict(tested_at_by_signal),
            test_status_by_signal=dict(test_status_by_signal or {}),
            emitted_at=datetime.now(timezone.utc),
        )
    )


def _verification_prepare_step(
    *,
    step_id: str,
    label: str,
    status: str,
    detail: str | None = None,
    extra: dict[str, Any] | None = None,
) -> dict[str, Any]:
    return {
        "id": step_id,
        "label": label,
        "status": status,
        "detail": detail,
        **(extra or {}),
    }


def _verification_prepare_runtime_summary(runtime_snapshot) -> dict[str, Any]:
    sessions = list(getattr(runtime_snapshot, "session_snapshots", ()) or ())
    subscriptions = list(getattr(runtime_snapshot, "subscription_snapshots", ()) or ())
    return {
        "runtime_state": getattr(getattr(runtime_snapshot, "verification_run", None), "runtime_state", None),
        "session_count": len(sessions),
        "subscription_count": len(subscriptions),
        "enabled_subscriptions": sum(1 for item in subscriptions if item.subscription_state in {"enabled", "reporting"}),
        "reporting_subscriptions": sum(1 for item in subscriptions if item.subscription_state == "reporting"),
        "failed_subscriptions": sum(1 for item in subscriptions if item.subscription_state == "failed"),
        "degraded_subscriptions": sum(
            1 for item in subscriptions if item.subscription_state == "degraded" or item.report_health == "degraded"
        ),
        "gi_requested_count": sum(1 for item in subscriptions if item.gi_requested),
        "last_report_value_count": sum(max(0, int(item.last_report_value_count or 0)) for item in subscriptions),
    }


def _normalize_verification_value(value: Any) -> Any:
    if isinstance(value, bool):
        return value
    if isinstance(value, int) and value in {0, 1}:
        return bool(value)
    if isinstance(value, str):
        normalized = value.strip().lower()
        if normalized in {"true", "1", "on"}:
            return True
        if normalized in {"false", "0", "off"}:
            return False
    return value


def _verification_diagnostic_codes(evidence: Any) -> set[str]:
    return {
        str(getattr(diagnostic, "code", "") or "").strip()
        for diagnostic in (getattr(evidence, "diagnostics", ()) or ())
        if str(getattr(diagnostic, "code", "") or "").strip()
    }


def _derive_test_status_from_verification(
    evidence: Any,
    *,
    expected_value: Any | None,
) -> str:
    evidence_status = str(getattr(evidence, "evidence_status", "") or "").strip().lower()
    actual_value = getattr(evidence, "signal_value", None)
    if evidence_status in {"observed", "late"}:
        if expected_value is not None and actual_value is not None:
            expected_normalized = _normalize_verification_value(expected_value)
            actual_normalized = _normalize_verification_value(actual_value)
            if expected_normalized != actual_normalized:
                return "inverted" if isinstance(expected_normalized, bool) and isinstance(actual_normalized, bool) else "value_mismatch"
        return "verified" if evidence_status == "observed" else "late"

    if evidence_status == "timeout":
        diagnostics = _verification_diagnostic_codes(evidence)
        if "SIGNAL_NOT_INCLUDED_IN_REPORT_EVENT" in diagnostics:
            return "unexpected"
        return "missing"

    if evidence_status == "out_of_window":
        return "late"
    if evidence_status in {"invalid", "stale"}:
        return evidence_status
    return "not_validated"


def _step_evidence_status(test_status: str) -> str:
    """Separate command delivery success from the final FAT verdict."""
    return "succeeded" if test_status in {"tested", "verified"} else "failed"


async def _sleep_before_restore(seconds: float) -> bool:
    """Return whether cancellation was deferred during the restore window."""
    try:
        await asyncio.sleep(seconds)
    except asyncio.CancelledError:
        return True
    return False


def _verification_runtime_ready(runtime_snapshot) -> bool:
    subscriptions = list(getattr(runtime_snapshot, "subscription_snapshots", ()) or ())
    if not subscriptions:
        return False
    return all(item.subscription_state == "reporting" and bool(item.gi_requested) for item in subscriptions)


def _verification_runtime_failed(runtime_snapshot) -> bool:
    sessions = list(getattr(runtime_snapshot, "session_snapshots", ()) or ())
    subscriptions = list(getattr(runtime_snapshot, "subscription_snapshots", ()) or ())
    return any(item.runtime_state in {"failed", "degraded"} for item in sessions) or any(
        item.subscription_state in {"failed", "degraded"} or item.report_health == "degraded" for item in subscriptions
    )


def _mms_subscription_plan_blockers(runtime_context) -> list[str]:
    plan = getattr(runtime_context, "subscription_plan", None)
    groups = list(getattr(plan, "groups", ()) or ())
    blockers: list[str] = []
    if not groups:
        blockers.append("no IEC 61850 report groups were resolved")
        return blockers
    for group in groups:
        group_id = str(getattr(group, "group_id", "") or "unknown")
        source_classification = str(getattr(group, "source_classification", "") or "").strip().lower()
        report_control_name = str(getattr(group, "report_control_name", "") or "").strip()
        report_control_reference = str(getattr(group, "report_control_reference", "") or "").strip()
        data_set_reference = str(getattr(group, "data_set_reference", "") or "").strip()
        missing = []
        if source_classification == "fallback":
            missing.append("fallback planning")
        if not report_control_name and not report_control_reference:
            missing.append("report control")
        if not data_set_reference:
            missing.append("dataset")
        if missing:
            blockers.append(f"{group_id}: missing {', '.join(missing)}")
    return blockers


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
    active_hardware_leases: dict[str, HardwareChannelLease] | None = None,
) -> dict[str, Any]:
    requested_ids_raw = payload.get("signal_ids") if isinstance(payload, dict) else None
    signal_interval_ms_raw = payload.get("signal_interval_ms") if isinstance(payload, dict) else None
    toggle_mode_raw = payload.get("toggle_mode") if isinstance(payload, dict) else None
    resume_from_cursor_raw = payload.get("resume_from_cursor") if isinstance(payload, dict) else None
    resume_job_id_raw = payload.get("resume_job_id") if isinstance(payload, dict) else None
    verification_enabled = bool(payload.get("verification_enabled")) if isinstance(payload, dict) else False
    verification_policy = str(payload.get("verification_policy") or "").strip().lower() if isinstance(payload, dict) else ""
    if verification_policy not in {"off", "optional", "required"}:
        verification_policy = "optional" if verification_enabled else "off"
    verification_enabled = verification_enabled or verification_policy == "required"
    verification_required_blocked = False
    verification_runtime_version = str(payload.get("verification_runtime_version") or "simulator").strip().lower()
    verification_orchestration_id = str(payload.get("verification_orchestration_id") or "").strip() or None
    try:
        verification_signal_list_revision_id = int(payload.get("verification_signal_list_revision_id") or 0)
    except (TypeError, ValueError):
        verification_signal_list_revision_id = 0
    try:
        verification_timeout_ms = int(payload.get("verification_timeout_ms") or 5000)
    except (TypeError, ValueError):
        verification_timeout_ms = 5000
    verification_timeout_ms = max(100, min(60000, verification_timeout_ms))

    requested_ids: list[int] = []
    if isinstance(requested_ids_raw, list):
        seen: set[int] = set()
        for item in requested_ids_raw:
            signal_id = int(item)
            if signal_id <= 0 or signal_id in seen:
                continue
            requested_ids.append(signal_id)
            seen.add(signal_id)

    job_id = str(payload.get("job_id") or "").strip()
    plan_rows = await _load_immutable_plan_rows(repo, workspace_id, job_id)
    plan_rows_by_signal_id = {int(row.signal_id): row for row in plan_rows}
    requested_set = set(requested_ids)
    if not requested_set.issubset(plan_rows_by_signal_id):
        missing_from_plan = sorted(requested_set - set(plan_rows_by_signal_id))
        raise RuntimeError(f"Test-run selection is not contained in immutable plan: {missing_from_plan}")
    requested_ids = [int(row.signal_id) for row in plan_rows if int(row.signal_id) in requested_set]

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
    hardware_admission = HardwareCommandAdmission(redis)

    async def enqueue_durable_command(
        *,
        action: str,
        channel_id: int,
        device_id: int | None,
        unit_id: str,
        payload: dict[str, Any],
        command_sender,
    ) -> str:
        command_id = uuid4().hex
        await record_hardware_command_intent(
            repo.db,
            command_id=command_id,
            workspace_id=workspace_id,
            job_id=job_id,
            attempt_id=execution_attempt_id,
            owner_kind="fat",
            owner_id=job_id,
            device_id=device_id,
            channel_id=channel_id,
            unit_id=unit_id,
            action=action,
            payload=payload,
            fencing_epoch=channel_lease.fencing_epoch if channel_lease is not None else None,
        )
        # The intent must survive a worker crash before the outbound stream publish.
        await repo.db.commit()
        await _deliver_durable_command(
            repo.db,
            command_id=command_id,
            action=action,
            command_sender=command_sender,
        )
        await mark_hardware_command_intent_queued(repo.db, command_id=command_id)
        await repo.db.commit()
        return command_id

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
    test_status_by_signal: dict[int, str] = {}
    test_report_by_signal: dict[int, dict[str, Any]] = {}
    skipped = 0
    skip_reasons = {
        "missing_row": 0,
        "invalid_binding": 0,
        "incompatible_channel_mode": 0,
        "offline_unit": 0,
        "initial_state_unknown": 0,
        "ao_profile_required": 0,
        "binding_changed": 0,
        "channel_lease_busy": 0,
        "recovery_required": 0,
        "iec61850_required_unavailable": 0,
    }
    evidence_count = 0
    verification_failed = 0
    verification_observed = 0
    verification_signal_ids: list[int] = []
    verification_signal_id_set: set[int] = set()
    verification_requested_signal_count = 0
    verification_evidence_rows = []
    verification_diagnostics = []
    verification_prepare_error: str | None = None
    verification_prepare_warning: str | None = None
    verification_prepare_steps: list[dict[str, Any]] = []
    verification_orchestrator: VerificationRuntimeOrchestrator | None = None
    verification_local_orchestration_id: str | None = None

    pending_tested_at_by_signal: dict[int, str] = {}
    tested_at_patch_since_emit: dict[int, str] = {}
    test_status_patch_since_emit: dict[int, str] = {}

    def set_verification_prepare_step(
        *,
        step_id: str,
        label: str,
        status: str,
        detail: str | None = None,
        extra: dict[str, Any] | None = None,
    ) -> None:
        next_step = _verification_prepare_step(
            step_id=step_id,
            label=label,
            status=status,
            detail=detail,
            extra=extra,
        )
        for index, step in enumerate(verification_prepare_steps):
            if step.get("id") == step_id:
                verification_prepare_steps[index] = next_step
                return
        verification_prepare_steps.append(next_step)

    async def publish_verification_prepare_progress(
        *,
        message: str,
        runtime_snapshot=None,
    ) -> None:
        result_payload: dict[str, Any] = {
            "phase": "preparing_iec61850",
            "verification_enabled": True,
            "verification_policy": verification_policy,
            "verification_available": verification_orchestrator is not None and verification_local_orchestration_id is not None,
            "verification_requested_signal_count": verification_requested_signal_count,
            "verification_prepare_steps": list(verification_prepare_steps),
        }
        if verification_prepare_error:
            result_payload["verification_prepare_error"] = verification_prepare_error
        if verification_prepare_warning:
            result_payload["verification_prepare_warning"] = verification_prepare_warning
        if runtime_snapshot is not None:
            result_payload["verification_prepare_summary"] = _verification_prepare_runtime_summary(runtime_snapshot)
        await _publish_running_progress(
            job_state=job_state,
            progress_done=0,
            progress_total=progress_total_global,
            message=message,
            result=result_payload,
        )

    async def wait_for_verification_runtime_ready(runtime_start):
        if verification_orchestrator is None:
            return None
        orchestration_id = str(getattr(runtime_start, "orchestration_id", "") or "")
        if not orchestration_id:
            return None
        deadline = time.monotonic() + 90.0
        last_signature: tuple[Any, ...] | None = None
        while True:
            runtime_snapshot = verification_orchestrator.snapshot(orchestration_id)
            summary = _verification_prepare_runtime_summary(runtime_snapshot)
            subscriptions = list(getattr(runtime_snapshot, "subscription_snapshots", ()) or ())
            enabled = int(summary["enabled_subscriptions"])
            reporting = int(summary["reporting_subscriptions"])
            total_subscriptions = int(summary["subscription_count"])
            gi_count = int(summary["gi_requested_count"])
            value_count = int(summary["last_report_value_count"])
            failed = int(summary["failed_subscriptions"])
            degraded = int(summary["degraded_subscriptions"])
            failed_or_degraded = failed + degraded
            set_verification_prepare_step(
                step_id="subscribe_reports",
                label="Subscribe reports",
                status="done" if total_subscriptions > 0 and enabled == total_subscriptions else ("failed" if failed_or_degraded else "running"),
                detail=None,
                extra={
                    "enabled": enabled,
                    "reporting": reporting,
                    "total": total_subscriptions,
                    "failed": failed,
                    "degraded": degraded,
                },
            )
            set_verification_prepare_step(
                step_id="general_interrogation",
                label="General interrogation",
                status="done" if total_subscriptions > 0 and gi_count == total_subscriptions else ("failed" if failed_or_degraded else "running"),
                detail=None,
                extra={
                    "gi_requested": gi_count,
                    "total": total_subscriptions,
                    "last_report_value_count": value_count,
                    "failed": failed,
                    "degraded": degraded,
                },
            )
            signature = (
                summary.get("runtime_state"),
                enabled,
                reporting,
                total_subscriptions,
                gi_count,
                value_count,
                failed,
                degraded,
                tuple(
                    (
                        item.subscription_id,
                        item.subscription_state,
                        item.gi_requested,
                        item.last_report_value_count,
                    )
                    for item in subscriptions
                ),
            )
            if signature != last_signature:
                last_signature = signature
                await publish_verification_prepare_progress(
                    message="Preparing IEC 61850 verification.",
                    runtime_snapshot=runtime_snapshot,
                )
            if _verification_runtime_ready(runtime_snapshot):
                set_verification_prepare_step(
                    step_id="start_test",
                    label="Start test",
                    status="done",
                    detail=None,
                )
                await publish_verification_prepare_progress(
                    message="IEC 61850 ready; starting test.",
                    runtime_snapshot=runtime_snapshot,
                )
                return runtime_snapshot
            if _verification_runtime_failed(runtime_snapshot):
                await publish_verification_prepare_progress(
                    message="IEC 61850 preparation failed; starting test without verification.",
                    runtime_snapshot=runtime_snapshot,
                )
                raise RuntimeError("IEC 61850 preparation failed before test run commands.")
            if time.monotonic() >= deadline:
                await publish_verification_prepare_progress(
                    message="IEC 61850 preparation timed out before report subscriptions reached reporting.",
                    runtime_snapshot=runtime_snapshot,
                )
                raise TimeoutError("IEC 61850 preparation timed out before report subscriptions reached reporting.")
            await asyncio.sleep(0.25)

    if verification_enabled and job_id and requested_ids:
        verification_signal_ids = await _resolve_mapped_verification_signal_ids(plan_rows, original_requested_ids)
        verification_signal_id_set = set(verification_signal_ids)
        verification_requested_signal_count = len(verification_signal_ids)
        if verification_policy == "required" and not verification_signal_ids:
            verification_required_blocked = True

    if verification_enabled and job_id and verification_signal_ids:
        set_verification_prepare_step(step_id="peripheral_online", label="Peripheral online", status="running")
        set_verification_prepare_step(step_id="subscribe_reports", label="Subscribe reports", status="pending")
        set_verification_prepare_step(step_id="general_interrogation", label="General interrogation", status="pending")
        set_verification_prepare_step(step_id="start_test", label="Start test", status="pending")
        await publish_verification_prepare_progress(message="Checking UnitLab peripheral device.")
        peripheral_preflight = await _collect_peripheral_preflight(plan_rows, requested_ids)
        if peripheral_preflight.notes:
            verification_prepare_warning = "; ".join(peripheral_preflight.notes[:3])
            if len(peripheral_preflight.notes) > 3:
                verification_prepare_warning = f"{verification_prepare_warning}; +{len(peripheral_preflight.notes) - 3} more"
            set_verification_prepare_step(
                step_id="peripheral_online",
                label="Peripheral online",
                status="warning",
                extra={"notes": peripheral_preflight.notes},
            )
            await publish_verification_prepare_progress(message="Some selected UnitLab peripheral devices are offline; they will be skipped.")
        else:
            set_verification_prepare_step(step_id="peripheral_online", label="Peripheral online", status="done")

        executable_signal_id_set = set(peripheral_preflight.executable_signal_ids)
        verification_signal_ids = [signal_id for signal_id in verification_signal_ids if signal_id in executable_signal_id_set]
        verification_signal_id_set = set(verification_signal_ids)
        if not verification_signal_ids:
            set_verification_prepare_step(step_id="subscribe_reports", label="Subscribe reports", status="pending")
            set_verification_prepare_step(step_id="general_interrogation", label="General interrogation", status="pending")
            set_verification_prepare_step(step_id="start_test", label="Start test", status="done")
            await publish_verification_prepare_progress(message="No online IEC 61850 mapped signals; starting test with offline rows skipped.")
        else:
            set_verification_prepare_step(step_id="subscribe_reports", label="Subscribe reports", status="running")
            await publish_verification_prepare_progress(message="Preparing IEC 61850 verification.")
        if verification_signal_ids:
            try:
                execution_context = VerificationExecutionContextSchema(
                    project_id=workspace_id,
                    signal_list_revision_id=verification_signal_list_revision_id,
                    planner_version="unitlab-test-run.v1",
                    runtime_version=verification_runtime_version or "simulator",
                    policy_version="iec61850-test-run.v1",
                )
                runtime_context = await build_verification_runtime_start_context(
                    workspace_id=workspace_id,
                    payload=VerificationAutoRunStartSchema(
                        signal_ids=verification_signal_ids,
                        execution_context=execution_context,
                        client_id="unitlab-test-run",
                        test_run_id=job_id,
                    ),
                    db=repo.db,
                    require_discovery_planning=verification_runtime_version in {"mms", "live", "live-mms", "real-mms"},
                )
                if verification_runtime_version in {"mms", "live", "live-mms", "real-mms"}:
                    blockers = _mms_subscription_plan_blockers(runtime_context)
                    if blockers:
                        detail = "; ".join(blockers[:3])
                        if len(blockers) > 3:
                            detail = f"{detail}; +{len(blockers) - 3} more"
                        raise RuntimeError(f"IEC 61850 discovery planning is incomplete: {detail}")
                if not runtime_context.subscription_plan.groups:
                    raise RuntimeError("IEC 61850 preparation failed: no report groups resolved for selected signals.")
                await _schedule_external_ied_discovery_for_verification_run(
                    workspace_id=workspace_id,
                    runtime_context=runtime_context,
                )
                verification_orchestrator = VerificationRuntimeOrchestrator()
                runtime_start_method = getattr(verification_orchestrator, "start_deferred", None)
                runtime_start = (runtime_start_method or verification_orchestrator.start)(
                    workspace_id=workspace_id,
                    test_run_id=job_id,
                    verification_targets=runtime_context.subscription_plan.targets,
                    subscription_plan=runtime_context.subscription_plan,
                    execution_context=runtime_context.execution_context,
                    client_id="unitlab-test-run",
                    endpoint_for_device=runtime_context.runtime_selection.endpoint_for_device,
                    adapter=runtime_context.runtime_selection.adapter,
                    initial_diagnostics=runtime_context.diagnostics,
                )
                verification_local_orchestration_id = runtime_start.orchestration_id
                if runtime_start_method is not None:
                    await wait_for_verification_runtime_ready(runtime_start)
                else:
                    set_verification_prepare_step(step_id="start_test", label="Start test", status="done")
                    await publish_verification_prepare_progress(
                        message="IEC 61850 report runtime started.",
                        runtime_snapshot=runtime_start,
                    )
            except Exception as exc:  # noqa: BLE001
                verification_prepare_error = str(exc)
                logger.warning(
                    "IEC 61850 verification preparation failed; continuing test without verification | workspace=%s job=%s error=%s",
                    workspace_id,
                    job_id,
                    verification_prepare_error,
                )
                if verification_orchestrator is not None and verification_local_orchestration_id is not None:
                    with suppress(Exception):
                        verification_orchestrator.stop(verification_local_orchestration_id)
                verification_orchestrator = None
                verification_local_orchestration_id = None
                verification_diagnostics.append(
                    VerificationEvidenceDiagnosticSchema(
                        code="IEC61850_PREPARATION_FAILED",
                        message=verification_prepare_error,
                        severity="warning",
                    )
                )
                verification_signal_ids = []
                verification_signal_id_set = set()
                set_verification_prepare_step(step_id="subscribe_reports", label="Subscribe reports", status="failed")
                set_verification_prepare_step(step_id="general_interrogation", label="General interrogation", status="failed")
                set_verification_prepare_step(step_id="start_test", label="Start test", status="done")
                await publish_verification_prepare_progress(message="IEC 61850 unavailable; starting test without verification.")
                if verification_policy == "required":
                    verification_required_blocked = True

    async def attach_and_publish_tested_at_patch(result_payload: dict[str, Any]) -> None:
        if not tested_at_patch_since_emit and not test_status_patch_since_emit:
            return
        patch = dict(tested_at_patch_since_emit)
        status_patch = dict(test_status_patch_since_emit)
        if patch:
            result_payload["tested_at_patch"] = patch
        if status_patch:
            result_payload["test_status_patch"] = status_patch
        logger.info(
            "Signal test runtime patch publishing | workspace=%s job=%s tested=%s statuses=%s",
            workspace_id,
            job_id,
            patch,
            status_patch,
        )
        await _publish_test_runtime_patch(
            workspace_id=workspace_id,
            job_id=job_id,
            tested_at_by_signal=patch,
            test_status_by_signal=status_patch,
        )
        tested_at_patch_since_emit.clear()
        test_status_patch_since_emit.clear()

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

    async def record_verification_evidence(capture_result) -> None:
        nonlocal verification_failed, verification_observed
        if capture_result is None:
            return
        evidence = capture_result.evidence
        verification_evidence_rows.append(evidence)
        verification_diagnostics.extend(capture_result.diagnostics)
        if evidence.evidence_status == "observed":
            verification_observed += 1
        else:
            verification_failed += 1
        repository = VerificationEvidenceRepository(repo.db)
        await repository.record_signal_verification_evidence(
            workspace_id=workspace_id,
            test_run_id=job_id,
            evidence_id=evidence.evidence_id,
            signal_id=evidence.signal_id,
            signal_path=evidence.signal_path,
            expected_path=evidence.expected_path,
            actual_report_path=evidence.actual_report_path,
            source_ied=evidence.source_ied,
            endpoint_id=evidence.endpoint_id,
            rpt_id=evidence.rpt_id,
            dataset=evidence.dataset,
            observed_at=evidence.observed_at,
            latency_ms=evidence.latency_ms,
            quality=evidence.quality,
            freshness=evidence.freshness,
            evidence_status=evidence.evidence_status,
            reason_code=evidence.reason_code,
            source_generation=evidence.source_generation,
            source_report_sequence_generation=evidence.source_report_sequence_generation,
            source_report_sequence_number=evidence.source_report_sequence_number,
            source_report_sub_sequence_number=evidence.source_report_sub_sequence_number,
            report_reason=evidence.report_reason,
            signal_value=evidence.signal_value,
            timestamp_summary=evidence.timestamp_summary,
            stale_reason=evidence.stale_reason,
            evidence_kind=evidence.evidence_kind,
            diagnostics=evidence.diagnostics,
        )

    def verification_result_payload(*, include_report: bool = False) -> dict[str, Any]:
        payload = {
            "verification_enabled": verification_requested_signal_count > 0,
            "verification_policy": verification_policy,
            "verification_available": verification_orchestrator is not None and verification_local_orchestration_id is not None,
            "verification_runtime_version": verification_runtime_version,
            "verification_requested_orchestration_id": verification_orchestration_id,
            "verification_local_orchestration_id": verification_local_orchestration_id,
            "verification_requested_signal_count": verification_requested_signal_count,
            "verification_observed": verification_observed,
            "verification_failed": verification_failed,
            "test_status_by_signal": dict(test_status_by_signal),
        }
        if include_report:
            payload["test_report_by_signal"] = dict(test_report_by_signal)
        if verification_prepare_error:
            payload["verification_prepare_error"] = verification_prepare_error
        if verification_prepare_warning:
            payload["verification_prepare_warning"] = verification_prepare_warning
        return payload

    async def flush_verification_evidence_set() -> None:
        if not verification_evidence_rows:
            return
        repository = VerificationEvidenceRepository(repo.db)
        evidence_set = build_signal_verification_evidence_set(
            test_run_id=job_id,
            evidence=verification_evidence_rows,
            diagnostics=verification_diagnostics,
        )
        await repository.upsert_signal_verification_evidence_set(
            workspace_id=workspace_id,
            test_run_id=job_id,
            evidence=evidence_set.evidence,
            diagnostics=evidence_set.diagnostics,
        )

    def close_verification_orchestration() -> None:
        nonlocal verification_local_orchestration_id
        if verification_orchestrator is None or verification_local_orchestration_id is None:
            return
        with suppress(Exception):
            verification_orchestrator.stop(verification_local_orchestration_id)
        verification_local_orchestration_id = None

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

    unit_bitmasks: dict[str, int | None] = {}
    unit_state_unknown: set[str] = set()

    async def get_unit_bitmask(unit_id: str) -> int | None:
        if unit_id in unit_bitmasks:
            return unit_bitmasks[unit_id]
        try:
            request_packet_id = await enqueue_request_state(
                unit_id=unit_id,
                mode=State.REQ_ALL_BIT,
                correlation_id=f"test-run:{job_id}:initial-state:{unit_id}",
            )
            bitmask = await _wait_for_fresh_bitmask_snapshot(
                redis,
                unit_id=unit_id,
                packet_id=request_packet_id,
                timeout_ms=min(2000, max(250, verification_timeout_ms)),
            )
        except Exception:  # noqa: BLE001
            unit_bitmasks[unit_id] = None
            return None
        unit_bitmasks[unit_id] = bitmask
        return bitmask

    async def resolve_plan_signal_row(signal_id: int):
        row = plan_rows_by_signal_id.get(int(signal_id))
        if row is None:
            return None, "missing_row"
        if not row.unit_id or not isinstance(row.channel_index, int):
            return row, "invalid_binding"
        channel_type = str(row.channel_type or "").strip().lower()
        is_do = channel_type.startswith("do")
        is_ao = channel_type.startswith("ao")
        if not is_do and not is_ao:
            return row, "incompatible_channel_mode"
        if is_ao:
            return row, "ao_profile_required"
        if is_do and str(row.unit_id) in unit_state_unknown:
            return row, "initial_state_unknown"
        if hasattr(repo, "get_execution_binding_with_recovery"):
            current = await repo.get_execution_binding_with_recovery(workspace_id, signal_id)
            if current is not None and current.recovery_required:
                return row, "recovery_required"
        elif hasattr(repo, "get_execution_binding"):
            if row.channel_id is not None and await has_hardware_recovery_required(
                repo.db,
                workspace_id=workspace_id,
                channel_id=int(row.channel_id),
            ):
                return row, "recovery_required"
            current = await repo.get_execution_binding(workspace_id, signal_id)
        else:
            current_rows = await repo.list_allocation_rows_by_signal_ids(workspace_id, [signal_id])
            current = next((item for item in current_rows if int(item.signal_id) == int(signal_id)), None)
        if current is None or current.unit_online is not True:
            return row, "offline_unit"
        if (
            int(current.channel_id) != int(row.channel_id)
            or row.device_id is None
            or int(current.device_id) != int(row.device_id)
            or int(current.channel_index) != int(row.channel_index)
            or str(current.unit_id) != str(row.unit_id)
        ):
            return row, "binding_changed"
        if str(row.channel_type or "").strip().lower().startswith("do"):
            if await get_unit_bitmask(str(row.unit_id)) is None:
                return row, "initial_state_unknown"
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
                **verification_result_payload(include_report=True),
            }
            await flush_verification_evidence_set()
            close_verification_orchestration()
            await attach_and_publish_tested_at_patch(result_payload)
            return result_payload

        row, skip_reason = await resolve_plan_signal_row(signal_id)
        if skip_reason is None and verification_required_blocked:
            skip_reason = "iec61850_required_unavailable"
        channel_lease: HardwareChannelLease | None = None
        if skip_reason is None and row is not None and row.channel_id is not None:
            channel_lease = await hardware_admission.acquire(
                channel_id=row.channel_id,
                owner_kind="fat",
                owner_id=job_id,
            )
            if channel_lease is None:
                skip_reason = "channel_lease_busy"
            elif active_hardware_leases is not None:
                active_hardware_leases[channel_lease.lease_id] = channel_lease
        success = False
        command_payload: dict[str, Any] | None = None
        test_status = "pending"
        verification_expected_value: Any | None = None
        command_ids: list[str] = []
        if skip_reason is not None:
            skipped += 1
            skip_reasons[skip_reason] += 1
            test_status = skip_reason
            skip_command_payload = {
                "toggle_mode": toggle_mode,
                "signal_interval_ms": signal_interval_ms,
            }
            test_report_by_signal[signal_id] = _build_signal_test_report_entry(
                signal_id=signal_id,
                order_index=progress_done_global,
                row=row,
                test_status=test_status,
                result_state=skip_reason,
                command_payload=skip_command_payload,
                skip_reason=skip_reason,
            )
            await record_step_evidence(
                order_index=progress_done_global,
                signal_id=signal_id,
                status="skipped",
                row=row,
                reason=skip_reason,
                result_state=skip_reason,
                command_payload=skip_command_payload,
            )
        elif row is not None:
            unit_id = str(row.unit_id)
            channel_index = int(row.channel_index)
            channel_type = str(row.channel_type or "").strip().lower()
            if channel_type.startswith("ao"):
                random_value = round(random.uniform(0.0, 24.0), 2)
                ao_correlation_id = f"test-run:{signal_id}:ao:{random_value}"
                state_correlation_id = f"test-run:{signal_id}:state-float"
                command_payload = {
                    "toggle_mode": "ao_random",
                    "signal_interval_ms": signal_interval_ms,
                    "expected_feedback_value": random_value,
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
                ao_command_id = await enqueue_durable_command(
                    action="ao_set",
                    channel_id=int(row.channel_id),
                    device_id=int(row.device_id) if row.device_id is not None else None,
                    unit_id=unit_id,
                    payload={
                        "channel_index": channel_index,
                        "value": random_value,
                        "correlation_id": ao_correlation_id,
                    },
                    command_sender=lambda command_id: enqueue_ao_command(
                        unit_id=unit_id,
                        ch=channel_index,
                        value=random_value,
                        correlation_id=ao_correlation_id,
                        command_id=command_id,
                    ),
                )
                command_payload["commands"][0]["command_id"] = ao_command_id
                command_ids.append(ao_command_id)
                verification_expected_value = random_value
                ao_readback_ok = False
                set_readback_packet_id = await enqueue_request_state(
                    unit_id=unit_id,
                    mode=State.REQ_SINGLE_FLOAT,
                    ch=channel_index,
                    correlation_id=state_correlation_id,
                )
                ao_readback_ok = await _wait_for_float_readback(
                    redis,
                    unit_id=unit_id,
                    channel_index=channel_index,
                    expected_value=random_value,
                    timeout_ms=min(2000, max(250, verification_timeout_ms)),
                    packet_id=set_readback_packet_id,
                )
                if not ao_readback_ok:
                    await mark_hardware_command_intent_delivery_failure(
                        repo.db,
                        command_id=ao_command_id,
                        status="recovery_required",
                    )
                    await repo.db.commit()
                    test_status = "blocked"
            else:
                bitmask = await get_unit_bitmask(unit_id)
                current_value = 1 if (bitmask & (1 << channel_index)) else 0
                toggled_value = 0 if current_value else 1
                readback_timeout_ms = min(2000, max(250, verification_timeout_ms))
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
                set_command_id = await enqueue_durable_command(
                    action="do_set",
                    channel_id=int(row.channel_id),
                    device_id=int(row.device_id) if row.device_id is not None else None,
                    unit_id=unit_id,
                    payload={
                        "channel_index": channel_index,
                        "value": toggled_value,
                        "correlation_id": set_correlation_id,
                    },
                    command_sender=lambda command_id: enqueue_do_command(
                        unit_id=unit_id,
                        mode=Cmd.SET_SINGLE_BIT,
                        ch=channel_index,
                        value=toggled_value,
                        correlation_id=set_correlation_id,
                        command_id=command_id,
                    ),
                )
                commands_payload[0]["command_id"] = set_command_id
                command_ids.append(set_command_id)

                if toggled_value:
                    bitmask = bitmask | (1 << channel_index)
                else:
                    bitmask = bitmask & ~(1 << channel_index)

                set_readback_correlation_id = f"test-run:{signal_id}:readback:set:{toggled_value}"
                set_readback_packet_id = await enqueue_request_state(
                    unit_id=unit_id,
                    mode=State.REQ_SINGLE_BIT,
                    ch=channel_index,
                    correlation_id=set_readback_correlation_id,
                )
                readback_ok = await _wait_for_bit_readback(
                    redis,
                    unit_id=unit_id,
                    channel_index=channel_index,
                    expected_value=toggled_value,
                    timeout_ms=readback_timeout_ms,
                    packet_id=set_readback_packet_id,
                )
                if not readback_ok:
                    unit_state_unknown.add(unit_id)
                    await mark_hardware_command_intent_delivery_failure(
                        repo.db,
                        command_id=set_command_id,
                        status="recovery_required",
                    )
                    await repo.db.commit()
                    test_status = "blocked"

                if toggle_mode == "double":
                    # Do not leave a toggled output behind when the worker task
                    # is cancelled during the bounded SET -> RESTORE window.
                    cancelled_during_restore_window = await _sleep_before_restore(signal_interval_seconds)
                    restore_correlation_id = f"test-run:{signal_id}:set:{current_value}"
                    commands_payload.append(
                        {
                            "kind": "do_set",
                            "role": "restore",
                            "unit_id": unit_id,
                            "channel_index": channel_index,
                            "value": current_value,
                            "correlation_id": restore_correlation_id,
                        }
                    )
                    restore_command_id = await enqueue_durable_command(
                        action="restore",
                        channel_id=int(row.channel_id),
                        device_id=int(row.device_id) if row.device_id is not None else None,
                        unit_id=unit_id,
                        payload={
                            "channel_index": channel_index,
                            "value": current_value,
                            "correlation_id": restore_correlation_id,
                        },
                        command_sender=lambda command_id: enqueue_do_command(
                            unit_id=unit_id,
                            mode=Cmd.SET_SINGLE_BIT,
                            ch=channel_index,
                            value=current_value,
                            correlation_id=restore_correlation_id,
                            command_id=command_id,
                        ),
                    )
                    commands_payload[1]["command_id"] = restore_command_id
                    command_ids.append(restore_command_id)
                    if current_value:
                        bitmask = bitmask | (1 << channel_index)
                    else:
                        bitmask = bitmask & ~(1 << channel_index)

                    restore_readback_correlation_id = f"test-run:{signal_id}:readback:restore:{current_value}"
                    restore_readback_packet_id = await enqueue_request_state(
                        unit_id=unit_id,
                        mode=State.REQ_SINGLE_BIT,
                        ch=channel_index,
                        correlation_id=restore_readback_correlation_id,
                    )
                    readback_ok = await _wait_for_bit_readback(
                        redis,
                        unit_id=unit_id,
                        channel_index=channel_index,
                        expected_value=current_value,
                        timeout_ms=readback_timeout_ms,
                        packet_id=restore_readback_packet_id,
                    )
                    if not readback_ok:
                        unit_state_unknown.add(unit_id)
                        await mark_hardware_command_intent_delivery_failure(
                            repo.db,
                            command_id=restore_command_id,
                            status="recovery_required",
                        )
                        await repo.db.commit()
                        test_status = "blocked"
                    if cancelled_during_restore_window:
                        raise asyncio.CancelledError

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
                    "expected_feedback_value": current_value if toggle_mode == "double" else toggled_value,
                    "commands": commands_payload,
                }
                verification_expected_value = command_payload["expected_feedback_value"]
                await enqueue_request_state(
                    unit_id=unit_id,
                    mode=State.REQ_SINGLE_BIT,
                    ch=channel_index,
                    correlation_id=state_correlation_id,
                )
            ack_states = await wait_for_hardware_command_acks(
                repo.db,
                command_ids=command_ids,
                timeout_ms=min(10000, max(1000, verification_timeout_ms)),
            )
            if command_payload is not None:
                command_payload["ack_states"] = ack_states
            success = bool(command_ids) and all(
                state == "acknowledged" for state in ack_states.values()
            )
            if not success:
                test_status = "blocked"

            verification_capture = None
            if (
                success
                and verification_orchestrator is not None
                and verification_local_orchestration_id is not None
                and signal_id in verification_signal_id_set
            ):
                if command_payload is None:
                    command_payload = {}
                verification_triggered_at = datetime.now(timezone.utc)
                try:
                    capture_cancel_event = threading.Event()
                    capture_task = asyncio.create_task(
                        asyncio.to_thread(
                            verification_orchestrator.capture_triggered_signal,
                            verification_local_orchestration_id,
                            signal_id=signal_id,
                            triggered_at=verification_triggered_at,
                            test_run_id=job_id,
                            timeout_ms=verification_timeout_ms,
                            cancel_event=capture_cancel_event,
                        )
                    )
                    try:
                        verification_capture = await asyncio.shield(capture_task)
                    except asyncio.CancelledError:
                        capture_cancel_event.set()
                        with suppress(asyncio.CancelledError, Exception):
                            await asyncio.wait_for(asyncio.shield(capture_task), timeout=1.0)
                        raise
                    await record_verification_evidence(verification_capture)
                    command_payload["iec61850_verification"] = {
                        "source": "worker_runtime_orchestration",
                        "requested_online_orchestration_id": verification_orchestration_id,
                        "local_orchestration_id": verification_local_orchestration_id,
                        "triggered_at": verification_triggered_at.isoformat(),
                        "timeout_ms": verification_timeout_ms,
                        "evidence": verification_capture.evidence.model_dump(mode="json"),
                        "step": verification_capture.step.model_dump(mode="json"),
                        "expected_value": verification_expected_value,
                    }
                    test_status = _derive_test_status_from_verification(
                        verification_capture.evidence,
                        expected_value=verification_expected_value,
                    )
                except Exception as exc:  # noqa: BLE001
                    verification_failed += 1
                    test_status = "not_validated"
                    verification_diagnostics.append(
                        VerificationEvidenceDiagnosticSchema(
                            code="IEC61850_SIGNAL_VERIFICATION_FAILED",
                            message=str(exc),
                            severity="warning",
                            details={"signal_id": signal_id},
                        )
                    )
                    command_payload["iec61850_verification"] = {
                        "source": "worker_runtime_orchestration",
                        "requested_online_orchestration_id": verification_orchestration_id,
                        "local_orchestration_id": verification_local_orchestration_id,
                        "triggered_at": verification_triggered_at.isoformat(),
                        "timeout_ms": verification_timeout_ms,
                        "status": "not_validated",
                        "error": str(exc),
                    }

        if channel_lease is not None:
            await hardware_admission.release(channel_lease)
            if active_hardware_leases is not None:
                active_hardware_leases.pop(channel_lease.lease_id, None)
            channel_lease = None

        if success:
            if test_status == "pending":
                test_status = "tested"
            succeeded_signal_ids.append(signal_id)
            tested_at_dt = datetime.now(timezone.utc)
            tested_at = tested_at_dt.isoformat()
            tested_at_by_signal[signal_id] = tested_at
            test_status_by_signal[signal_id] = test_status
            pending_tested_at_by_signal[signal_id] = tested_at
            tested_at_patch_since_emit[signal_id] = tested_at
            test_status_patch_since_emit[signal_id] = test_status
            result_state = f"commands_enqueued_report_{test_status}" if verification_capture is not None else "commands_enqueued"
            test_report_by_signal[signal_id] = _build_signal_test_report_entry(
                signal_id=signal_id,
                order_index=progress_done_global,
                row=row,
                test_status=test_status,
                result_state=result_state,
                command_payload=command_payload,
                tested_at=tested_at_dt,
            )
            await record_step_evidence(
                order_index=progress_done_global,
                signal_id=signal_id,
                status=_step_evidence_status(test_status),
                row=row,
                result_state=result_state,
                command_payload=command_payload,
                tested_at=tested_at_dt,
            )
            if len(pending_tested_at_by_signal) >= tested_at_batch_size:
                await flush_tested_at_batch()
        elif row is not None and command_payload:
            failure_state = (
                "hardware_command_ack_timeout"
                if command_payload.get("ack_states") and "timeout" in command_payload["ack_states"].values()
                else "hardware_command_negative_ack"
                if command_payload.get("ack_states")
                else "iec61850_report_not_observed"
                if verification_orchestrator is not None
                else "command_failed"
            )
            test_report_by_signal[signal_id] = _build_signal_test_report_entry(
                signal_id=signal_id,
                order_index=progress_done_global,
                row=row,
                test_status=test_status,
                result_state=failure_state,
                command_payload=command_payload,
            )
            await record_step_evidence(
                order_index=progress_done_global,
                signal_id=signal_id,
                status="failed",
                row=row,
                reason=failure_state,
                result_state=failure_state,
                command_payload=command_payload,
            )

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
                **verification_result_payload(),
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
                        **verification_result_payload(include_report=True),
                    }
                    await flush_verification_evidence_set()
                    close_verification_orchestration()
                    await attach_and_publish_tested_at_patch(result_payload)
                    return result_payload
                step = min(0.2, signal_interval_seconds - slept)
                await asyncio.sleep(step)
                slept += step

    await flush_tested_at_batch()
    await flush_verification_evidence_set()
    close_verification_orchestration()
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
        **verification_result_payload(include_report=True),
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
                    async with AsyncSessionLocal() as recovery_session:
                        reconciled_intents = await reconcile_unfinished_hardware_command_intents(
                            recovery_session,
                            job_id=job_id,
                            attempt_id=previous_attempt_id,
                        )
                        await recovery_session.commit()
                    recovery_result: dict[str, Any] = {
                        "recovery_policy": "fail_on_replay_after_started_attempt",
                        "recovery_reason": "replayed_pending_entry_after_started_attempt",
                        "resume_supported": True,
                        "resume_hint": "Start a new test run with resume_from_cursor=true and resume_job_id=<failed_job_id>.",
                        "attempt_id": previous_attempt_id,
                        "attempt_no": previous_attempt_no,
                        "reconciled_hardware_intents": reconciled_intents,
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
                active_hardware_leases: dict[str, HardwareChannelLease] = {}
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
                        active_hardware_leases=active_hardware_leases,
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
                finally:
                    for hardware_lease in list(active_hardware_leases.values()):
                        try:
                            await HardwareCommandAdmission(RedisManager.get_instance()).release(hardware_lease)
                        except Exception:  # noqa: BLE001
                            logger.exception(
                                "Failed to release hardware channel lease after test-run exception: %s",
                                hardware_lease.channel_id,
                            )
                    active_hardware_leases.clear()

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
            reconciled_hardware_intents = 0
            if job_id and execution_attempt_id:
                try:
                    async with AsyncSessionLocal() as recovery_session:
                        reconciled_hardware_intents = await reconcile_unfinished_hardware_command_intents(
                            recovery_session,
                            job_id=job_id,
                            attempt_id=execution_attempt_id,
                        )
                        await recovery_session.commit()
                except Exception:  # noqa: BLE001
                    logger.exception(
                        "💥 Failed to reconcile hardware intents after signal test run failure job=%s attempt=%s",
                        job_id,
                        execution_attempt_id,
                    )
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
                        "reconciled_hardware_intents": reconciled_hardware_intents,
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
