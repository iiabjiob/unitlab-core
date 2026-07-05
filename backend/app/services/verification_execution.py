from __future__ import annotations

from dataclasses import dataclass
from datetime import UTC, datetime, timedelta
from typing import Any, Callable, Sequence
from uuid import uuid4

from app.schemas.verification_schema import (
    SignalVerificationEvidenceSchema,
    SignalVerificationEvidenceSetSchema,
    VerificationExecutionContextSchema,
    VerificationEvidenceDiagnosticSchema,
    VerificationRecoveryStateSchema,
    VerificationRunSchema,
    VerificationSessionSnapshotSchema,
    VerificationSubscriptionSnapshotSchema,
    VerificationStepSchema,
    VerificationSubscriptionPlanGroupSchema,
    VerificationSubscriptionPlanSchema,
    VerificationTargetSchema,
)
from app.services.iec61850.report_runtime import (
    Iec61850DeviceEndpoint,
    Iec61850DataSetMember,
    Iec61850OptionalFields,
    Iec61850ReportControlCandidate,
    Iec61850ReportKind,
    Iec61850ReportSubscriptionPlan,
    Iec61850ReportSubscriptionPlanDevice,
    Iec61850ReportSubscriptionPlanReport,
    Iec61850ReportSubscriptionPlanSignal,
    Iec61850ReportRuntimeAdapter,
    Iec61850SelectedSignal,
    Iec61850RuntimeTriggerOptions,
    build_simulator_endpoint_for_plan_device,
    create_iec61850_simulator_adapter,
)
from app.services.verification_evidence import (
    VerificationEvidenceRepository,
    build_signal_verification_evidence_set,
    build_signal_verification_evidence_summary,
)
from app.services.verification_confidence import (
    derive_run_confidence,
    derive_step_confidence,
)


@dataclass(frozen=True, slots=True)
class VerificationExecutionResult:
    verification_run: VerificationRunSchema
    evidence_set: SignalVerificationEvidenceSetSchema
    evidence_rows: tuple[SignalVerificationEvidenceSchema, ...]
    diagnostics: tuple[VerificationEvidenceDiagnosticSchema, ...]


def build_runtime_subscription_plan(
    verification_plan: VerificationSubscriptionPlanSchema,
) -> Iec61850ReportSubscriptionPlan:
    devices: list[Iec61850ReportSubscriptionPlanDevice] = []
    total_matched_signals = 0

    for group in verification_plan.groups:
        target_indexes = [index for index in group.target_indexes if 0 <= index < len(verification_plan.targets)]
        targets = [verification_plan.targets[index] for index in target_indexes]
        if not targets:
            continue

        candidate = _build_runtime_candidate(group=group, targets=targets)
        matched_signals = tuple(
            Iec61850ReportSubscriptionPlanSignal(
                selected_signal=Iec61850SelectedSignal(id=str(target.signal_id), address=target.signal_path),
                model_reference=target.expected_feedback_path or target.signal_path,
                ied_name=group.ied_name or target.unit_id or candidate.ied_name,
                match_kind=target.coverage_state,
            )
            for target in targets
        )
        total_matched_signals += len(matched_signals)
        devices.append(
            Iec61850ReportSubscriptionPlanDevice(
                ied_name=candidate.ied_name,
                access_point_name=candidate.access_point_name,
                reports=(
                    Iec61850ReportSubscriptionPlanReport(
                        status="required",
                        candidate=candidate,
                        matched_signals=matched_signals,
                    ),
                ),
                endpoint_id=group.endpoint_id,
            )
        )

    total_targets = len(verification_plan.targets)
    unmatched = max(0, total_targets - total_matched_signals)
    required_reports = sum(len(device.reports) for device in devices)

    return Iec61850ReportSubscriptionPlan(
        selected_signal_count=total_targets,
        matched_signal_count=total_matched_signals,
        unmatched_signal_count=unmatched,
        ambiguous_signal_count=0,
        required_report_count=required_reports,
        devices=tuple(devices),
        diagnostics=(),
    )


async def execute_simulated_verification_run(
    *,
    workspace_id: int,
    test_run_id: str,
    verification_targets: Sequence[VerificationTargetSchema],
    subscription_plan: VerificationSubscriptionPlanSchema,
    execution_context: VerificationExecutionContextSchema,
    repository: VerificationEvidenceRepository | None = None,
    triggered_at: datetime | None = None,
    latency_ms: int = 250,
    client_id: str = "unitlab-backend-simulator",
    endpoint_for_device: Callable[[Iec61850ReportSubscriptionPlanDevice], Iec61850DeviceEndpoint] = build_simulator_endpoint_for_plan_device,
    now: Callable[[], datetime] | None = None,
    simulate_missing_signal_ids: Sequence[int] = (),
    simulate_stale_signal_ids: Sequence[int] = (),
) -> VerificationExecutionResult:
    if triggered_at is None:
        triggered_at = datetime.now(UTC)
    runtime_now = now or (lambda: triggered_at + timedelta(milliseconds=max(0, latency_ms)))
    simulator_adapter = create_iec61850_simulator_adapter(now=runtime_now)
    return await execute_verification_run(
        workspace_id=workspace_id,
        test_run_id=test_run_id,
        verification_targets=verification_targets,
        subscription_plan=subscription_plan,
        execution_context=execution_context,
        adapter=simulator_adapter,
        endpoint_for_device=endpoint_for_device,
        repository=repository,
        triggered_at=triggered_at,
        latency_ms=latency_ms,
        client_id=client_id,
        now=now,
        simulate_missing_signal_ids=simulate_missing_signal_ids,
        simulate_stale_signal_ids=simulate_stale_signal_ids,
    )


async def execute_verification_run(
    *,
    workspace_id: int,
    test_run_id: str,
    verification_targets: Sequence[VerificationTargetSchema],
    subscription_plan: VerificationSubscriptionPlanSchema,
    execution_context: VerificationExecutionContextSchema,
    adapter: Iec61850ReportRuntimeAdapter,
    endpoint_for_device: Callable[[Iec61850ReportSubscriptionPlanDevice], Iec61850DeviceEndpoint] = build_simulator_endpoint_for_plan_device,
    repository: VerificationEvidenceRepository | None = None,
    triggered_at: datetime | None = None,
    latency_ms: int = 250,
    client_id: str = "unitlab-backend-simulator",
    now: Callable[[], datetime] | None = None,
    simulate_missing_signal_ids: Sequence[int] = (),
    simulate_stale_signal_ids: Sequence[int] = (),
) -> VerificationExecutionResult:
    if triggered_at is None:
        triggered_at = datetime.now(UTC)
    runtime_now = now or (lambda: triggered_at + timedelta(milliseconds=max(0, latency_ms)))
    missing_signal_ids = {int(signal_id) for signal_id in simulate_missing_signal_ids}
    stale_signal_ids = {int(signal_id) for signal_id in simulate_stale_signal_ids}
    runtime_plan = build_runtime_subscription_plan(subscription_plan)
    from app.services.iec61850.report_runtime import run_report_subscription_plan

    runtime_result = run_report_subscription_plan(
        plan=runtime_plan,
        adapter=adapter,
        client_id=client_id,
        endpoint_for_device=endpoint_for_device,
        now=runtime_now,
    )
    connection_generation = _resolve_connection_generation(runtime_result)
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = [
        _runtime_diagnostic_to_evidence_diagnostic(diagnostic) for diagnostic in runtime_result.diagnostics
    ]

    observations_by_signal_id: dict[int, tuple[Any, str | None, str | None, str | None]] = {}
    for report in runtime_result.reports:
        for observation in report.observations:
            target_signal_id = _resolve_signal_id(observation.selected_signal_id)
            if target_signal_id is None:
                continue
            observations_by_signal_id[target_signal_id] = (
                report,
                observation.model_reference,
                observation.value,
                observation.timestamp,
            )

    evidence_rows: list[SignalVerificationEvidenceSchema] = []
    step_rows: list[VerificationStepSchema] = []
    step_verdicts: list[str] = []

    for target_index, target in enumerate(verification_targets):
        observation_bundle = observations_by_signal_id.get(int(target.signal_id))
        forced_evidence_status = None
        if int(target.signal_id) in missing_signal_ids:
            observation_bundle = None
            forced_evidence_status = "timeout"
        elif int(target.signal_id) in stale_signal_ids:
            forced_evidence_status = "stale"
        evidence, step = _build_step_and_evidence(
            target_index=target_index,
            target=target,
            group=_resolve_target_group(subscription_plan, target_index),
            observation_bundle=observation_bundle,
            triggered_at=triggered_at,
            runtime_result=runtime_result,
            test_run_id=test_run_id,
            source_generation=connection_generation,
            forced_evidence_status=forced_evidence_status,
        )
        evidence_rows.append(evidence)
        step_rows.append(step)
        step_verdicts.append(step.verdict_state)

        if repository is not None:
            await repository.record_signal_verification_evidence(
                workspace_id=workspace_id,
                test_run_id=test_run_id,
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

    evidence_set = build_signal_verification_evidence_set(
        test_run_id=test_run_id,
        evidence=evidence_rows,
        diagnostics=diagnostics,
    )
    if repository is not None:
        await repository.upsert_signal_verification_evidence_set(
            workspace_id=workspace_id,
            test_run_id=test_run_id,
            evidence=evidence_rows,
            diagnostics=diagnostics,
        )

    session_snapshots = _build_session_snapshots(
        test_run_id=test_run_id,
        runtime_result=runtime_result,
        connection_generation=connection_generation,
    )
    subscription_snapshots = _build_subscription_snapshots(
        test_run_id=test_run_id,
        runtime_result=runtime_result,
        subscription_plan=subscription_plan,
        connection_generation=connection_generation,
        evidence_rows=evidence_rows,
        client_id=client_id,
    )
    recovery_state = _build_recovery_state(
        test_run_id=test_run_id,
        verification_targets=verification_targets,
        subscription_plan=subscription_plan,
        execution_context=execution_context,
        session_snapshots=session_snapshots,
        subscription_snapshots=subscription_snapshots,
        evidence_rows=evidence_rows,
        runtime_result=runtime_result,
    )
    verdict_state = _resolve_verdict_state(step_verdicts)
    verification_confidence, confidence_reason = derive_run_confidence(
        steps=step_rows,
        session_snapshots=session_snapshots,
        subscription_snapshots=subscription_snapshots,
    )
    workflow_state = "completed" if verdict_state != "aborted" else "aborted"

    verification_run = VerificationRunSchema(
        test_run_id=test_run_id,
        verification_targets=list(verification_targets),
        subscription_plan=subscription_plan,
        session_snapshots=session_snapshots,
        subscription_snapshots=subscription_snapshots,
        evidence_set=evidence_set,
        execution_context=execution_context,
        recovery_state=recovery_state,
        workflow_state=workflow_state,
        verdict_state=verdict_state,
        verification_confidence=verification_confidence,
        confidence_reason=confidence_reason,
        selected_group_id=execution_context.selected_group_id,
        operator_id=execution_context.operator_id,
        triggered_at=triggered_at,
        completed_at=_parse_timestamp(runtime_result.finished_at),
        runtime_state=_resolve_runtime_state(session_snapshots, subscription_snapshots),
        runtime_summary=_build_runtime_summary(runtime_result, evidence_rows, subscription_snapshots),
        diagnostics=diagnostics,
        verification_steps=step_rows,
    )
    return VerificationExecutionResult(
        verification_run=verification_run,
        evidence_set=evidence_set,
        evidence_rows=tuple(evidence_rows),
        diagnostics=tuple(diagnostics),
    )


def _build_runtime_candidate(
    *,
    group: VerificationSubscriptionPlanGroupSchema,
    targets: Sequence[VerificationTargetSchema],
) -> Iec61850ReportControlCandidate:
    first_target = targets[0]
    report_kind = Iec61850ReportKind.BUFFERED
    if str(group.report_kind or "").strip().lower() == "unbuffered":
        report_kind = Iec61850ReportKind.UNBUFFERED
    report_control_name = group.report_control_name or (group.group_id if group.source_classification == "fallback" else "")
    discovery_only = group.source_classification == "fallback" and not group.report_control_reference and not group.data_set_reference
    candidate_ied_name = group.ied_name or ("" if discovery_only else group.endpoint_id or "IED")
    return Iec61850ReportControlCandidate(
        id=group.group_id or f"group:{group.endpoint_id or 'unknown'}",
        ied_name=candidate_ied_name,
        access_point_name=group.access_point_name or "AP1",
        logical_device_inst="" if discovery_only else "LD0",
        logical_node_name="" if discovery_only else "LLN0",
        report_control_name=report_control_name if discovery_only else report_control_name or group.group_id or "report",
        report_kind=report_kind,
        rpt_id=group.rpt_id,
        data_set_ref=None if discovery_only else group.data_set_reference or first_target.expected_feedback_path or first_target.signal_path,
        conf_rev=None,
        indexed=None,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=tuple(
            Iec61850DataSetMember(
                reference=target.expected_feedback_path or target.signal_path,
                fc=None,
            )
            for target in targets
        ),
    )


def _build_step_and_evidence(
    *,
    target_index: int,
    target: VerificationTargetSchema,
    group: VerificationSubscriptionPlanGroupSchema | None,
    observation_bundle: tuple[Any, str | None, str | None, str | None] | None,
    triggered_at: datetime,
    runtime_result,
    test_run_id: str,
    source_generation: int | None = None,
    forced_evidence_status: str | None = None,
) -> tuple[SignalVerificationEvidenceSchema, VerificationStepSchema]:
    report = observation_bundle[0] if observation_bundle is not None else None
    actual_report_path = observation_bundle[1] if observation_bundle is not None else None
    signal_value = observation_bundle[2] if observation_bundle is not None else None
    observed_at = _parse_timestamp(report.event.received_at) if report is not None and report.event is not None else None
    computed_latency_ms = _latency_ms(triggered_at, observed_at) if observed_at is not None else None
    window_ms = int(target.window_ms)
    timeout_ms = int(target.timeout_ms)
    evidence_status, freshness, reason_code, evidence_kind = _resolve_evidence_state(
        target=target,
        actual_report_path=actual_report_path,
        observed_at=observed_at,
        latency_ms=computed_latency_ms,
        runtime_result=runtime_result,
        window_ms=window_ms,
        timeout_ms=timeout_ms,
    )
    if forced_evidence_status is not None:
        evidence_status = forced_evidence_status
        freshness = "stale" if forced_evidence_status == "stale" else freshness
        if forced_evidence_status == "timeout":
            reason_code = "no_confirmation"
            evidence_kind = "timeout"
        elif forced_evidence_status == "stale":
            reason_code = "stale_generation"
            evidence_kind = "report_observation"
    diagnostics = [
        VerificationEvidenceDiagnosticSchema(
            code=reason_code,
            message=_diagnostic_message(reason_code, evidence_status),
        )
    ]
    source_session_id = _resolve_source_session_id(
        test_run_id=test_run_id,
        report=report,
        target=target,
    )
    source_subscription_id = _resolve_source_subscription_id(
        test_run_id=test_run_id,
        group=group,
        report=report,
        target=target,
        source_session_id=source_session_id,
    )
    evidence_id = f"{test_run_id}:{target.signal_id}:{target_index}:{uuid4().hex[:8]}"
    evidence = SignalVerificationEvidenceSchema(
        evidence_id=evidence_id,
        signal_id=int(target.signal_id),
        signal_path=target.signal_path,
        expected_path=target.expected_feedback_path or target.signal_path,
        actual_report_path=actual_report_path,
        source_ied=report.ied_name if report is not None else target.protocol_metadata.get("ied_name"),
        endpoint_id=report.event.endpoint_id if report is not None and report.event is not None else target.endpoint_id,
        rpt_id=_first_non_empty_report_text(
            report.event.rpt_id if report is not None and report.event is not None else None,
            report.report_control_name if report is not None else None,
            target.protocol_metadata.get("report_control_reference_hint"),
        ),
        dataset=report.data_set_ref if report is not None else target.protocol_metadata.get("data_set_reference"),
        observed_at=observed_at,
        latency_ms=computed_latency_ms,
        quality="good" if evidence_status == "observed" else None,
        freshness=freshness,
        evidence_status=evidence_status,
        reason_code=reason_code,
        source_generation=source_generation,
        source_report_sequence_generation=report.event.sequence_number if report is not None and report.event is not None else None,
        source_report_sequence_number=report.event.sequence_number if report is not None and report.event is not None else None,
        source_report_sub_sequence_number=None,
        report_reason=report.event.reason.value if report is not None and report.event is not None else None,
        signal_value=signal_value,
        timestamp_summary={"observed_at": observed_at.isoformat()} if observed_at is not None else {},
        stale_reason=reason_code if evidence_status == "stale" else None,
        evidence_kind=evidence_kind,
        diagnostics=diagnostics,
    )
    verification_confidence, confidence_reason = derive_step_confidence(
        target=target,
        evidence=evidence,
        group=group,
    )
    step = VerificationStepSchema(
        step_id=f"step-{target.signal_id}",
        signal_id=int(target.signal_id),
        target_index=target_index,
        session_id=source_session_id or f"{test_run_id}:unknown-session",
        subscription_id=source_subscription_id or f"{test_run_id}:unknown-subscription",
        group_id=group.group_id if group is not None else None,
        step_state=_resolve_step_state(evidence_status=evidence_status),
        expected_path=evidence.expected_path,
        expected_window_ms=window_ms,
        freshness=freshness,
        evidence_status=evidence_status,
        verdict_state=_resolve_step_verdict_state(evidence_status=evidence_status),
        evidence_ids=[evidence_id],
        actual_report_path=evidence.actual_report_path,
        source_session_id=source_session_id,
        source_subscription_id=source_subscription_id,
        source_generation=source_generation,
        source_report_rpt_id=evidence.rpt_id,
        source_report_dat_set=evidence.dataset,
        verification_confidence=verification_confidence,
        confidence_reason=confidence_reason,
        triggered_at=triggered_at,
        observed_at=observed_at,
        latency_ms=computed_latency_ms,
        reason=reason_code,
        diagnostics=diagnostics,
    )
    return evidence, step


def _resolve_signal_id(signal_id: str | None) -> int | None:
    if signal_id is None:
        return None
    try:
        return int(str(signal_id).strip())
    except (TypeError, ValueError):
        return None


def _latency_ms(triggered_at: datetime, observed_at: datetime | None) -> int | None:
    if observed_at is None:
        return None
    delta = observed_at - triggered_at
    return max(0, int(delta.total_seconds() * 1000))


def _resolve_evidence_state(
    *,
    target: VerificationTargetSchema,
    actual_report_path: str | None,
    observed_at: datetime | None,
    latency_ms: int | None,
    runtime_result,
    window_ms: int,
    timeout_ms: int,
) -> tuple[str, str | None, str, str]:
    if observed_at is None:
        return "timeout", "unknown", "no_confirmation", "timeout"
    if actual_report_path is None:
        return "invalid", "unknown", "missing_report_path", "report_observation"
    if latency_ms is None:
        return "invalid", "unknown", "missing_latency", "report_observation"
    if latency_ms > timeout_ms:
        return "out_of_window", "live", "window_exceeded", "report_observation"
    if latency_ms > window_ms:
        return "late", "live", "window_exceeded", "report_observation"
    if runtime_result.diagnostics:
        first = runtime_result.diagnostics[0]
        if getattr(first, "severity", "") == "error":
            return "invalid", "unknown", first.code, "report_observation"
    return "observed", "live", "report_received", "report_observation"


def _build_recovery_state(
    *,
    test_run_id: str,
    verification_targets: Sequence[VerificationTargetSchema],
    subscription_plan: VerificationSubscriptionPlanSchema,
    execution_context: VerificationExecutionContextSchema,
    session_snapshots: Sequence[VerificationSessionSnapshotSchema],
    subscription_snapshots: Sequence[VerificationSubscriptionSnapshotSchema],
    evidence_rows: Sequence[SignalVerificationEvidenceSchema],
    runtime_result,
) -> VerificationRecoveryStateSchema | None:
    recovery_evidence_rows = [evidence for evidence in evidence_rows if _is_recovery_evidence_status(evidence.evidence_status)]
    runtime_diagnostics = tuple(_runtime_diagnostic_to_evidence_diagnostic(diagnostic) for diagnostic in runtime_result.diagnostics)
    snapshot_failed = any(
        snapshot.runtime_state != "reporting" for snapshot in session_snapshots
    ) or any(snapshot.report_health != "healthy" for snapshot in subscription_snapshots)
    diagnostics = [
        *runtime_diagnostics,
        *(diagnostic for evidence in recovery_evidence_rows for diagnostic in evidence.diagnostics),
    ]
    if recovery_evidence_rows:
        runtime_state = "degraded"
        recovery_reason = _resolve_recovery_reason(recovery_evidence_rows, runtime_diagnostics, snapshot_failed)
        desired_state = "reconnecting"
    elif snapshot_failed or any(diagnostic.severity == "error" for diagnostic in runtime_diagnostics):
        runtime_state = "failed"
        recovery_reason = "runtime_failure"
        desired_state = "reconnecting"
    else:
        runtime_state = "reporting"
        recovery_reason = None
        desired_state = "reporting"

    representative_snapshot = None
    if session_snapshots:
        representative_snapshot = session_snapshots[0]

    group_ids = _unique_non_empty_strings(
        [
            execution_context.selected_group_id,
            *(group.group_id for group in subscription_plan.groups),
        ]
    )
    report_controls = _unique_non_empty_strings(
        [
            group.report_control_reference or group.rpt_id or group.report_control_name or group.group_id
            for group in subscription_plan.groups
        ]
    )

    return VerificationRecoveryStateSchema(
        session_id=representative_snapshot.session_id if representative_snapshot is not None else f"{test_run_id}:recovery",
        endpoint_id=representative_snapshot.endpoint_id if representative_snapshot is not None else execution_context.selected_group_id or test_run_id,
        runtime_state=runtime_state,
        desired_state=desired_state,
        active_generation=max((snapshot.connection_generation for snapshot in session_snapshots), default=1),
        recovery_reason=recovery_reason,
        desired_subscription_plan_id=subscription_plan.plan_id or test_run_id,
        desired_group_ids=group_ids,
        desired_report_controls=report_controls,
        desired_target_ids=[int(target.signal_id) for target in verification_targets],
        active_verification_run_id=test_run_id,
        preserved_evidence_count=len(evidence_rows),
        in_flight=runtime_state != "reporting",
        preserved_verification_targets=list(verification_targets),
        stale_signal_count=len(recovery_evidence_rows),
        diagnostics=list(diagnostics),
    )


def _resolve_step_state(*, evidence_status: str) -> str:
    if evidence_status == "timeout":
        return "failed"
    if evidence_status in {"invalid", "stale", "late", "out_of_window"}:
        return "completed"
    if evidence_status == "observed":
        return "completed"
    return "failed"


def _resolve_step_verdict_state(*, evidence_status: str) -> str:
    if evidence_status == "observed":
        return "pass"
    if evidence_status == "timeout":
        return "fail"
    if evidence_status in {"invalid", "stale", "late", "out_of_window"}:
        return "fail"
    return "inconclusive"


def _resolve_verdict_state(step_verdicts: Sequence[str]) -> str:
    if not step_verdicts:
        return "pending"
    if any(verdict == "fail" for verdict in step_verdicts):
        return "fail"
    if all(verdict == "pass" for verdict in step_verdicts):
        return "pass"
    if any(verdict == "aborted" for verdict in step_verdicts):
        return "aborted"
    return "inconclusive"


def _resolve_runtime_state(
    session_snapshots: Sequence[VerificationSessionSnapshotSchema],
    subscription_snapshots: Sequence[VerificationSubscriptionSnapshotSchema] = (),
) -> str | None:
    if not session_snapshots and not subscription_snapshots:
        return None
    states = {snapshot.runtime_state for snapshot in session_snapshots}
    subscription_states = {snapshot.subscription_state for snapshot in subscription_snapshots}
    if states == {"reporting"} and subscription_states.issubset({"reporting", "enabled"}):
        return "reporting"
    if "failed" in states or "failed" in subscription_states or "degraded" in subscription_states:
        return "degraded"
    if states:
        return sorted(states)[0]
    return "reporting" if subscription_snapshots else None


def _is_recovery_evidence_status(evidence_status: str) -> bool:
    return evidence_status in {"timeout", "stale", "invalid"}


def _resolve_recovery_reason(
    recovery_evidence_rows: Sequence[SignalVerificationEvidenceSchema],
    runtime_diagnostics: Sequence[VerificationEvidenceDiagnosticSchema],
    snapshot_failed: bool,
) -> str | None:
    evidence_statuses = {evidence.evidence_status for evidence in recovery_evidence_rows}
    if "timeout" in evidence_statuses:
        return "timeout"
    if "stale" in evidence_statuses:
        return "stale_generation"
    if "invalid" in evidence_statuses:
        return "runtime_failure"
    if snapshot_failed:
        return "report_health_degraded"
    if any(diagnostic.severity == "error" for diagnostic in runtime_diagnostics):
        return "runtime_failure"
    return None


def _first_non_empty_report_text(*values: Any) -> str | None:
    for value in values:
        if not isinstance(value, str):
            continue
        normalized = value.strip()
        if normalized and normalized.lower() not in {"<empty>", "<none>", "none", "null"}:
            return normalized
    return None


def _unique_non_empty_strings(values: Sequence[str | None]) -> list[str]:
    unique_values: list[str] = []
    seen: set[str] = set()
    for value in values:
        if value is None:
            continue
        text = str(value).strip()
        if not text or text in seen:
            continue
        seen.add(text)
        unique_values.append(text)
    return unique_values


def _build_session_snapshots(
    *,
    test_run_id: str,
    runtime_result,
    connection_generation: int,
) -> list[VerificationSessionSnapshotSchema]:
    snapshots: list[VerificationSessionSnapshotSchema] = []
    seen_endpoints: set[str] = set()
    for report in runtime_result.reports:
        report_endpoint_id = getattr(report, "endpoint_id", None)
        event_endpoint_id = report.event.endpoint_id if report.event is not None else None
        endpoint_id = report_endpoint_id or event_endpoint_id or f"{report.ied_name}/{report.access_point_name}"
        if endpoint_id in seen_endpoints:
            continue
        seen_endpoints.add(endpoint_id)
        snapshots.append(
            VerificationSessionSnapshotSchema(
                session_id=f"{test_run_id}:{endpoint_id}",
                endpoint_id=endpoint_id,
                runtime_state="reporting" if runtime_result.reports else "connecting",
                connection_generation=connection_generation,
                discovery_status="available" if runtime_result.reports else "discovering",
                last_error=None,
                diagnostic_code=None,
            )
        )
    return snapshots


def _build_subscription_snapshots(
    *,
    test_run_id: str,
    runtime_result,
    subscription_plan: VerificationSubscriptionPlanSchema,
    connection_generation: int,
    evidence_rows: Sequence[SignalVerificationEvidenceSchema],
    client_id: str,
) -> list[VerificationSubscriptionSnapshotSchema]:
    evidence_by_endpoint_and_report: dict[tuple[str, str | None], list[SignalVerificationEvidenceSchema]] = {}
    for evidence in evidence_rows:
        endpoint_id = str(evidence.endpoint_id or evidence.source_ied or test_run_id)
        report_reference = str(evidence.rpt_id or evidence.dataset or "").strip() or None
        evidence_by_endpoint_and_report.setdefault((endpoint_id, report_reference), []).append(evidence)

    snapshots: list[VerificationSubscriptionSnapshotSchema] = []
    for report in runtime_result.reports:
        report_endpoint_id = getattr(report, "endpoint_id", None)
        event_endpoint_id = report.event.endpoint_id if report.event is not None else None
        endpoint_id = report_endpoint_id or event_endpoint_id or f"{report.ied_name}/{report.access_point_name}"
        group = _resolve_plan_group_for_report(subscription_plan, report)
        subscription_id = _resolve_subscription_id(
            test_run_id=test_run_id,
            endpoint_id=endpoint_id,
            group=group,
            report=report,
        )
        report_reference = str(report.report_control_name or report.data_set_ref or "").strip() or None
        matching_evidence = evidence_by_endpoint_and_report.get((endpoint_id, report_reference), [])
        stale_signal_count = sum(1 for item in matching_evidence if _is_recovery_evidence_status(item.evidence_status))
        last_report_at = _parse_timestamp(report.event.received_at) if report.event is not None else None
        if last_report_at is None and matching_evidence:
            last_report_at = max((item.observed_at for item in matching_evidence if item.observed_at is not None), default=None)
        snapshots.append(
            VerificationSubscriptionSnapshotSchema(
                subscription_id=subscription_id,
                session_id=f"{test_run_id}:{endpoint_id}",
                endpoint_id=endpoint_id,
                group_id=group.group_id if group is not None else None,
                report_control_reference=(
                    group.report_control_reference
                    if group is not None and group.report_control_reference is not None
                    else report.report_control_name
                ),
                report_control_name=report.report_control_name,
                data_set_reference=report.data_set_ref,
                subscription_state="reporting" if report.error_code is None else "failed",
                report_health="healthy" if report.error_code is None else "degraded",
                last_report_at=last_report_at,
                current_rptena_owner=client_id if report.error_code is None else None,
                stale_signal_count=stale_signal_count or None,
                last_error=report.error_message,
                diagnostic_code=report.error_code,
                diagnostics=[
                    VerificationEvidenceDiagnosticSchema(
                        code=diagnostic.code,
                        message=diagnostic.message,
                        severity=diagnostic.severity,
                        details={
                            "ied_name": diagnostic.reference.ied_name if getattr(diagnostic, "reference", None) is not None else None,
                            "report_control_name": diagnostic.reference.report_control_name if getattr(diagnostic, "reference", None) is not None else None,
                        },
                    )
                    for diagnostic in report.diagnostics
                ],
            )
        )
    return snapshots


def _build_runtime_summary(
    runtime_result,
    evidence_rows: Sequence[SignalVerificationEvidenceSchema],
    subscription_snapshots: Sequence[VerificationSubscriptionSnapshotSchema] = (),
) -> dict[str, Any]:
    summary = build_signal_verification_evidence_summary(evidence_rows).model_dump()
    summary.update(
        {
            "runtime_reports": len(runtime_result.reports),
            "runtime_diagnostics": len(runtime_result.diagnostics),
            "observations": sum(len(report.observations) for report in runtime_result.reports),
            "subscription_reports": len(runtime_result.reports),
            "active_subscriptions": sum(1 for snapshot in subscription_snapshots if snapshot.subscription_state != "closed"),
            "reporting_subscriptions": sum(
                1 for snapshot in subscription_snapshots if snapshot.subscription_state == "reporting"
            ),
            "failed_subscriptions": sum(1 for snapshot in subscription_snapshots if snapshot.subscription_state == "failed"),
        }
    )
    return summary


def _resolve_plan_group_for_report(
    subscription_plan: VerificationSubscriptionPlanSchema,
    report,
) -> VerificationSubscriptionPlanGroupSchema | None:
    report_control_name = str(getattr(report, "report_control_name", "")).strip()
    data_set_ref = str(getattr(report, "data_set_ref", "")).strip()
    endpoint_id = str(getattr(report.event, "endpoint_id", "")).strip() if getattr(report, "event", None) is not None else ""
    for group in subscription_plan.groups:
        if group.report_control_name == report_control_name and (group.endpoint_id is None or group.endpoint_id == endpoint_id):
            return group
        if group.data_set_reference == data_set_ref and (group.endpoint_id is None or group.endpoint_id == endpoint_id):
            return group
    return None


def _resolve_subscription_id(
    *,
    test_run_id: str,
    endpoint_id: str,
    group: VerificationSubscriptionPlanGroupSchema | None,
    report,
) -> str:
    if group is not None and group.group_id.strip():
        return f"{test_run_id}:{group.group_id}"
    report_control_name = str(getattr(report, "report_control_name", "")).strip()
    data_set_ref = str(getattr(report, "data_set_ref", "")).strip()
    if report_control_name:
        return f"{test_run_id}:{endpoint_id}:{report_control_name}"
    if data_set_ref:
        return f"{test_run_id}:{endpoint_id}:{data_set_ref}"
    return f"{test_run_id}:{endpoint_id}:subscription"


def _resolve_source_session_id(
    *,
    test_run_id: str,
    report,
    target: VerificationTargetSchema,
) -> str | None:
    if report is not None and report.event is not None:
        return f"{test_run_id}:{report.event.endpoint_id}"
    if target.endpoint_id is not None:
        return f"{test_run_id}:{target.endpoint_id}"
    return None


def _resolve_source_subscription_id(
    *,
    test_run_id: str,
    group: VerificationSubscriptionPlanGroupSchema | None,
    report,
    target: VerificationTargetSchema,
    source_session_id: str | None,
) -> str | None:
    if group is not None and group.group_id.strip():
        return f"{test_run_id}:{group.group_id}"
    report_control_name = str(getattr(report, "report_control_name", "")).strip()
    data_set_ref = str(getattr(report, "data_set_ref", "")).strip()
    if source_session_id and report_control_name:
        return f"{source_session_id}:{report_control_name}"
    if source_session_id and data_set_ref:
        return f"{source_session_id}:{data_set_ref}"
    if report_control_name:
        return f"{test_run_id}:{target.endpoint_id or 'unknown'}:{report_control_name}"
    if data_set_ref:
        return f"{test_run_id}:{target.endpoint_id or 'unknown'}:{data_set_ref}"
    return None


def _parse_timestamp(value: Any) -> datetime | None:
    if value is None:
        return None
    if isinstance(value, datetime):
        return value if value.tzinfo is not None else value.replace(tzinfo=UTC)
    if isinstance(value, str):
        text = value.strip()
        if not text:
            return None
        try:
            parsed = datetime.fromisoformat(text.replace("Z", "+00:00"))
        except ValueError:
            return None
        return parsed if parsed.tzinfo is not None else parsed.replace(tzinfo=UTC)
    return None


def _diagnostic_message(code: str, evidence_status: str) -> str:
    messages = {
        "report_received": "Report received within the verification window.",
        "window_exceeded": "Report was observed outside the allowed window.",
        "no_confirmation": "No report confirmation arrived before timeout.",
        "missing_report_path": "Report confirmation did not include a path.",
        "missing_latency": "Report confirmation did not include a latency measurement.",
    }
    if code in messages:
        return messages[code]
    return f"Evidence status is {evidence_status}."


def _runtime_diagnostic_to_evidence_diagnostic(diagnostic: Any) -> VerificationEvidenceDiagnosticSchema:
    severity = getattr(diagnostic, "severity", None)
    severity_value = str(severity).strip() if severity is not None else ""
    return VerificationEvidenceDiagnosticSchema(
        code=str(getattr(diagnostic, "code", "runtime_diagnostic")),
        message=str(getattr(diagnostic, "message", "Runtime diagnostic")),
        severity=severity_value or "info",
        details={
            key: value
            for key, value in {
                "reference": getattr(getattr(diagnostic, "reference", None), "report_control_name", None),
                "signal_id": getattr(diagnostic, "signal_id", None),
                "address": getattr(diagnostic, "address", None),
                "data_reference": getattr(diagnostic, "data_reference", None),
            }.items()
            if value is not None
        }
        or None,
    )


def _resolve_connection_generation(runtime_result) -> int:
    generation = getattr(runtime_result, "connection_generation", None)
    if isinstance(generation, int) and generation > 0:
        return generation
    return 1


def _resolve_target_group_id(
    subscription_plan: VerificationSubscriptionPlanSchema,
    target_index: int,
) -> str | None:
    for group in subscription_plan.groups:
        if target_index in group.target_indexes:
            return group.group_id or None
    return None


def _resolve_target_group(
    subscription_plan: VerificationSubscriptionPlanSchema,
    target_index: int,
) -> VerificationSubscriptionPlanGroupSchema | None:
    for group in subscription_plan.groups:
        if target_index in group.target_indexes:
            return group
    return None
