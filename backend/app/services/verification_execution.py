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
    VerificationRunSchema,
    VerificationSessionSnapshotSchema,
    VerificationStepSchema,
    VerificationSubscriptionPlanGroupSchema,
    VerificationSubscriptionPlanSchema,
    VerificationTargetSchema,
)
from app.services.iec61850.report_runtime import (
    Iec61850DataSetMember,
    Iec61850OptionalFields,
    Iec61850ReportControlCandidate,
    Iec61850ReportKind,
    Iec61850ReportSubscriptionPlan,
    Iec61850ReportSubscriptionPlanDevice,
    Iec61850ReportSubscriptionPlanReport,
    Iec61850ReportSubscriptionPlanSignal,
    Iec61850SelectedSignal,
    Iec61850RuntimeTriggerOptions,
    run_simulator_report_subscription_plan,
)
from app.services.verification_evidence import (
    VerificationEvidenceRepository,
    build_signal_verification_evidence_set,
    build_signal_verification_evidence_summary,
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
    now: Callable[[], datetime] | None = None,
) -> VerificationExecutionResult:
    if triggered_at is None:
        triggered_at = datetime.now(UTC)
    runtime_now = now or (lambda: triggered_at + timedelta(milliseconds=max(0, latency_ms)))
    runtime_plan = build_runtime_subscription_plan(subscription_plan)
    runtime_result = run_simulator_report_subscription_plan(
        plan=runtime_plan,
        client_id=client_id,
        now=runtime_now,
    )
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
        evidence, step = _build_step_and_evidence(
            target_index=target_index,
            target=target,
            observation_bundle=observation_bundle,
            triggered_at=triggered_at,
            runtime_result=runtime_result,
            test_run_id=test_run_id,
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
        evidence_rows=evidence_rows,
    )
    verdict_state = _resolve_verdict_state(step_verdicts)
    workflow_state = "completed" if verdict_state != "aborted" else "aborted"

    verification_run = VerificationRunSchema(
        test_run_id=test_run_id,
        verification_targets=list(verification_targets),
        subscription_plan=subscription_plan,
        session_snapshots=session_snapshots,
        evidence_set=evidence_set,
        execution_context=execution_context,
        workflow_state=workflow_state,
        verdict_state=verdict_state,
        selected_group_id=execution_context.selected_group_id,
        operator_id=execution_context.operator_id,
        triggered_at=triggered_at,
        completed_at=_parse_timestamp(runtime_result.finished_at),
        runtime_state=_resolve_runtime_state(session_snapshots),
        runtime_summary=_build_runtime_summary(runtime_result, evidence_rows),
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
    return Iec61850ReportControlCandidate(
        id=group.group_id or f"group:{group.endpoint_id or 'unknown'}",
        ied_name=group.ied_name or group.endpoint_id or "IED",
        access_point_name=group.access_point_name or "AP1",
        logical_device_inst="LD0",
        logical_node_name="LLN0",
        report_control_name=group.report_control_name or group.group_id or "report",
        report_kind=report_kind,
        rpt_id=group.rpt_id,
        data_set_ref=group.data_set_reference or first_target.expected_feedback_path or first_target.signal_path,
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
    observation_bundle: tuple[Any, str | None, str | None, str | None] | None,
    triggered_at: datetime,
    runtime_result,
    test_run_id: str,
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
    diagnostics = [
        VerificationEvidenceDiagnosticSchema(
            code=reason_code,
            message=_diagnostic_message(reason_code, evidence_status),
        )
    ]
    evidence_id = f"{test_run_id}:{target.signal_id}:{target_index}:{uuid4().hex[:8]}"
    evidence = SignalVerificationEvidenceSchema(
        evidence_id=evidence_id,
        signal_id=int(target.signal_id),
        signal_path=target.signal_path,
        expected_path=target.expected_feedback_path or target.signal_path,
        actual_report_path=actual_report_path,
        source_ied=report.ied_name if report is not None else target.protocol_metadata.get("ied_name"),
        endpoint_id=report.event.endpoint_id if report is not None and report.event is not None else target.endpoint_id,
        rpt_id=report.event.rpt_id if report is not None and report.event is not None else target.protocol_metadata.get("report_control_reference_hint"),
        dataset=report.data_set_ref if report is not None else target.protocol_metadata.get("data_set_reference"),
        observed_at=observed_at,
        latency_ms=computed_latency_ms,
        quality="good" if evidence_status == "observed" else None,
        freshness=freshness,
        evidence_status=evidence_status,
        reason_code=reason_code,
        source_generation=None,
        source_report_sequence_generation=report.event.sequence_number if report is not None and report.event is not None else None,
        source_report_sequence_number=report.event.sequence_number if report is not None and report.event is not None else None,
        source_report_sub_sequence_number=None,
        report_reason=report.event.reason.value if report is not None and report.event is not None else None,
        signal_value=signal_value,
        timestamp_summary={"observed_at": observed_at.isoformat()} if observed_at is not None else {},
        stale_reason="runtime_stale" if evidence_status == "stale" else None,
        evidence_kind=evidence_kind,
        diagnostics=diagnostics,
    )
    step = VerificationStepSchema(
        step_id=f"step-{target.signal_id}",
        signal_id=int(target.signal_id),
        target_index=target_index,
        step_state=_resolve_step_state(evidence_status=evidence_status),
        expected_path=evidence.expected_path,
        expected_window_ms=window_ms,
        freshness=freshness,
        evidence_status=evidence_status,
        verdict_state=_resolve_step_verdict_state(evidence_status=evidence_status),
        evidence_ids=[evidence_id],
        actual_report_path=evidence.actual_report_path,
        source_session_id=report.candidate_id if report is not None else None,
        source_generation=None,
        source_report_rpt_id=evidence.rpt_id,
        source_report_dat_set=evidence.dataset,
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


def _resolve_runtime_state(session_snapshots: Sequence[VerificationSessionSnapshotSchema]) -> str | None:
    if not session_snapshots:
        return None
    states = {snapshot.runtime_state for snapshot in session_snapshots}
    if states == {"reporting"}:
        return "reporting"
    if "failed" in states:
        return "degraded"
    return sorted(states)[0]


def _build_session_snapshots(
    *,
    test_run_id: str,
    runtime_result,
    evidence_rows: Sequence[SignalVerificationEvidenceSchema],
) -> list[VerificationSessionSnapshotSchema]:
    evidence_by_group: dict[str, list[SignalVerificationEvidenceSchema]] = {}
    for evidence in evidence_rows:
        evidence_by_group.setdefault(str(evidence.endpoint_id or evidence.source_ied or test_run_id), []).append(evidence)

    snapshots: list[VerificationSessionSnapshotSchema] = []
    for report in runtime_result.reports:
        endpoint_id = report.event.endpoint_id if report.event is not None else f"{report.ied_name}/{report.access_point_name}"
        report_evidence = evidence_by_group.get(endpoint_id, [])
        last_report_at = None
        if report.event is not None:
            last_report_at = _parse_timestamp(report.event.received_at)
        if last_report_at is None and report_evidence:
            last_report_at = max((item.observed_at for item in report_evidence if item.observed_at is not None), default=None)
        snapshots.append(
            VerificationSessionSnapshotSchema(
                session_id=f"{test_run_id}:{endpoint_id}",
                endpoint_id=endpoint_id,
                runtime_state="reporting" if report.error_code is None else "failed",
                connection_generation=1,
                discovery_status="available" if report.error_code is None else "unknown",
                subscription_status="enabled" if report.error_code is None else "failed",
                report_health="healthy" if report.error_code is None else "degraded",
                last_report_at=last_report_at,
                last_error=report.error_message,
                selected_report_control=report.report_control_name,
                selected_data_set=report.data_set_ref,
                current_rptena_owner=None,
                stale_signal_count=0,
                diagnostic_code=report.error_code,
            )
        )
    return snapshots


def _build_runtime_summary(runtime_result, evidence_rows: Sequence[SignalVerificationEvidenceSchema]) -> dict[str, Any]:
    summary = build_signal_verification_evidence_summary(evidence_rows).model_dump()
    summary.update(
        {
            "runtime_reports": len(runtime_result.reports),
            "runtime_diagnostics": len(runtime_result.diagnostics),
            "observations": sum(len(report.observations) for report in runtime_result.reports),
        }
    )
    return summary


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
    return VerificationEvidenceDiagnosticSchema(
        code=str(getattr(diagnostic, "code", "runtime_diagnostic")),
        message=str(getattr(diagnostic, "message", "Runtime diagnostic")),
        severity=str(getattr(diagnostic, "severity", "info")) if getattr(diagnostic, "severity", None) is not None else None,
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
