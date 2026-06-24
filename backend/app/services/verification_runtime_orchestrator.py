from __future__ import annotations

from contextlib import suppress
from dataclasses import dataclass, field
from datetime import UTC, datetime
from typing import Any, Callable, Sequence
from uuid import uuid4

from app.schemas.verification_schema import (
    SignalVerificationEvidenceSetSchema,
    VerificationExecutionContextSchema,
    VerificationEvidenceDiagnosticSchema,
    VerificationRunSchema,
    VerificationSessionSnapshotSchema,
    VerificationSubscriptionSnapshotSchema,
    VerificationSubscriptionPlanSchema,
    VerificationTargetSchema,
)
from app.services.iec61850.report_runtime import (
    Iec61850DeviceEndpoint,
    Iec61850ReportRuntimeService,
    Iec61850RuntimeDiagnostic,
    build_simulator_endpoint_for_plan_device,
    create_iec61850_simulator_adapter,
)
from app.services.verification_evidence import build_signal_verification_evidence_set
from app.services.verification_execution import _parse_timestamp, _resolve_runtime_state, build_runtime_subscription_plan


@dataclass
class VerificationRuntimeSessionState:
    session_id: str
    endpoint_id: str
    runtime_state: str = "connecting"
    connection_generation: int = 1
    discovery_status: str = "discovering"
    last_error: str | None = None
    diagnostic_code: str | None = None

    def to_snapshot(self) -> VerificationSessionSnapshotSchema:
        return VerificationSessionSnapshotSchema(
            session_id=self.session_id,
            endpoint_id=self.endpoint_id,
            runtime_state=self.runtime_state,
            connection_generation=self.connection_generation,
            discovery_status=self.discovery_status,
            last_error=self.last_error,
            diagnostic_code=self.diagnostic_code,
        )


@dataclass
class VerificationRuntimeSubscriptionState:
    subscription_id: str
    session_id: str
    endpoint_id: str
    group_id: str | None = None
    report_control_reference: str | None = None
    report_control_name: str | None = None
    data_set_reference: str | None = None
    subscription_state: str = "pending"
    report_health: str = "unknown"
    last_report_at: datetime | None = None
    current_rptena_owner: str | None = None
    stale_signal_count: int | None = None
    last_error: str | None = None
    diagnostic_code: str | None = None
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = field(default_factory=list)

    def to_snapshot(self) -> VerificationSubscriptionSnapshotSchema:
        return VerificationSubscriptionSnapshotSchema(
            subscription_id=self.subscription_id,
            session_id=self.session_id,
            endpoint_id=self.endpoint_id,
            group_id=self.group_id,
            report_control_reference=self.report_control_reference,
            report_control_name=self.report_control_name,
            data_set_reference=self.data_set_reference,
            subscription_state=self.subscription_state,
            report_health=self.report_health,
            last_report_at=self.last_report_at,
            current_rptena_owner=self.current_rptena_owner,
            stale_signal_count=self.stale_signal_count,
            last_error=self.last_error,
            diagnostic_code=self.diagnostic_code,
            diagnostics=list(self.diagnostics),
        )


@dataclass(frozen=True, slots=True)
class VerificationRuntimeOrchestrationResult:
    orchestration_id: str
    verification_run: VerificationRunSchema
    session_snapshots: tuple[VerificationSessionSnapshotSchema, ...]
    subscription_snapshots: tuple[VerificationSubscriptionSnapshotSchema, ...]
    diagnostics: tuple[VerificationEvidenceDiagnosticSchema, ...]
    active_session_ids: tuple[str, ...]


@dataclass
class _VerificationRuntimeOrchestrationHandle:
    workspace_id: int
    test_run_id: str
    runtime_service: Iec61850ReportRuntimeService
    session_states: dict[str, VerificationRuntimeSessionState]
    subscription_states: dict[str, VerificationRuntimeSubscriptionState]
    session_order: list[str]
    subscription_order: list[str]
    verification_targets: list[VerificationTargetSchema]
    subscription_plan: VerificationSubscriptionPlanSchema
    execution_context: VerificationExecutionContextSchema
    client_id: str
    started_at: datetime
    diagnostics: list[VerificationEvidenceDiagnosticSchema]


class VerificationRuntimeOrchestrator:
    def __init__(
        self,
        *,
        now: Callable[[], datetime] | None = None,
    ) -> None:
        self._now = now or (lambda: datetime.now(UTC))
        self._handles: dict[str, _VerificationRuntimeOrchestrationHandle] = {}

    def start(
        self,
        *,
        workspace_id: int,
        test_run_id: str,
        verification_targets: Sequence[VerificationTargetSchema],
        subscription_plan: VerificationSubscriptionPlanSchema,
        execution_context: VerificationExecutionContextSchema,
        client_id: str = "unitlab-backend-simulator",
    ) -> VerificationRuntimeOrchestrationResult:
        orchestration_id = f"{workspace_id}:{test_run_id}:{uuid4().hex[:8]}"
        if orchestration_id in self._handles:
            raise RuntimeError(f'Verification orchestration "{orchestration_id}" already exists.')
        started_at = self._now()

        runtime_plan = build_runtime_subscription_plan(subscription_plan)
        adapter = create_iec61850_simulator_adapter(now=self._now)
        runtime_service = Iec61850ReportRuntimeService(adapter)
        session_states: dict[str, VerificationRuntimeSessionState] = {}
        subscription_states: dict[str, VerificationRuntimeSubscriptionState] = {}
        session_order: list[str] = []
        subscription_order: list[str] = []
        diagnostics: list[VerificationEvidenceDiagnosticSchema] = []

        try:
            for device_index, device in enumerate(runtime_plan.devices):
                endpoint = build_simulator_endpoint_for_plan_device(device)
                session_id = f"{orchestration_id}:{device_index}:{device.ied_name}/{device.access_point_name}"
                runtime_service.open_session(
                    session_id=session_id,
                    endpoint=endpoint,
                    candidates=[report.candidate for report in device.reports],
                )
                session_order.append(session_id)
                session_states[session_id] = VerificationRuntimeSessionState(
                    session_id=session_id,
                    endpoint_id=endpoint.id,
                )
                self._transition(session_states[session_id], "discovering", "discovering")

                for report in device.reports:
                    subscription_state = self._activate_report_subscription(
                        runtime_service=runtime_service,
                        session_state=session_states[session_id],
                        endpoint=endpoint,
                        report=report,
                        client_id=client_id,
                        diagnostics=diagnostics,
                        session_id=session_id,
                    )
                    subscription_states[subscription_state.subscription_id] = subscription_state
                    subscription_order.append(subscription_state.subscription_id)
        except Exception:
            for session_id in reversed(session_order):
                with suppress(Exception):
                    runtime_service.close_session(session_id)
            raise

        handle = _VerificationRuntimeOrchestrationHandle(
            workspace_id=workspace_id,
            test_run_id=test_run_id,
            runtime_service=runtime_service,
            session_states=session_states,
            subscription_states=subscription_states,
            session_order=session_order,
            subscription_order=subscription_order,
            verification_targets=list(verification_targets),
            subscription_plan=subscription_plan,
            execution_context=execution_context,
            client_id=client_id,
            started_at=started_at,
            diagnostics=diagnostics,
        )
        self._handles[orchestration_id] = handle
        return self.snapshot(orchestration_id)

    def snapshot(self, orchestration_id: str) -> VerificationRuntimeOrchestrationResult:
        handle = self._require_handle(orchestration_id)
        session_snapshots = tuple(handle.session_states[session_id].to_snapshot() for session_id in handle.session_order)
        subscription_snapshots = tuple(
            handle.subscription_states[subscription_id].to_snapshot() for subscription_id in handle.subscription_order
        )
        evidence_set = build_signal_verification_evidence_set(
            test_run_id=handle.test_run_id,
            evidence=(),
            diagnostics=handle.diagnostics,
        )
        workflow_state = "running"
        verdict_state = "pending"
        runtime_state = _resolve_runtime_state(session_snapshots, subscription_snapshots) or "connecting"
        verification_run = VerificationRunSchema(
            test_run_id=handle.test_run_id,
            verification_targets=list(handle.verification_targets),
            subscription_plan=handle.subscription_plan,
            session_snapshots=list(session_snapshots),
            subscription_snapshots=list(subscription_snapshots),
            evidence_set=evidence_set,
            execution_context=handle.execution_context,
            recovery_state=None,
            workflow_state=workflow_state,
            verdict_state=verdict_state,
            triggered_at=handle.started_at,
            completed_at=None,
            runtime_state=runtime_state,
            runtime_summary=_build_runtime_summary_for_orchestration(
                session_snapshots=session_snapshots,
                subscription_snapshots=subscription_snapshots,
                client_id=handle.client_id,
                diagnostics=handle.diagnostics,
            ),
            diagnostics=list(handle.diagnostics),
            verification_steps=[],
        )
        return VerificationRuntimeOrchestrationResult(
            orchestration_id=orchestration_id,
            verification_run=verification_run,
            session_snapshots=session_snapshots,
            subscription_snapshots=subscription_snapshots,
            diagnostics=tuple(handle.diagnostics),
            active_session_ids=tuple(handle.session_order),
        )

    def stop(self, orchestration_id: str) -> VerificationRuntimeOrchestrationResult:
        handle = self._require_handle(orchestration_id)
        for session_id in reversed(handle.session_order):
            with suppress(Exception):
                handle.runtime_service.close_session(session_id)
            if session_id in handle.session_states:
                handle.session_states[session_id].runtime_state = "closed"
        for subscription_id in handle.subscription_order:
            if subscription_id in handle.subscription_states:
                handle.subscription_states[subscription_id].subscription_state = "closed"
                handle.subscription_states[subscription_id].report_health = "unknown"
        result = self.snapshot(orchestration_id)
        self._handles.pop(orchestration_id, None)
        return result

    def _activate_report_subscription(
        self,
        *,
        runtime_service: Iec61850ReportRuntimeService,
        session_state: VerificationRuntimeSessionState,
        endpoint: Iec61850DeviceEndpoint,
        report,
        client_id: str,
        diagnostics: list[VerificationEvidenceDiagnosticSchema],
        session_id: str,
    ) -> VerificationRuntimeSubscriptionState:
        state = runtime_service.read_report_control(
            session_id=session_id,
            endpoint=endpoint,
            candidate=report.candidate,
        )
        session_diagnostics = _runtime_diagnostics_to_evidence_diagnostics(state.diagnostics)
        diagnostics.extend(session_diagnostics)
        subscription_state = VerificationRuntimeSubscriptionState(
            subscription_id=_resolve_subscription_id_for_runtime(session_id=session_id, report=report),
            session_id=session_id,
            endpoint_id=endpoint.id,
            group_id=report.candidate.id,
            report_control_reference=report.candidate.rpt_id or report.candidate.report_control_name,
            report_control_name=report.candidate.report_control_name,
            data_set_reference=report.candidate.data_set_ref,
            diagnostics=list(session_diagnostics),
        )
        if any(diagnostic.severity == "error" for diagnostic in session_diagnostics):
            subscription_state.subscription_state = "failed"
            subscription_state.report_health = "degraded"
            subscription_state.last_error = "ReportControl precheck failed"
            subscription_state.diagnostic_code = state.diagnostics[0].code if state.diagnostics else "REPORT_CONTROL_PRECHECK_FAILED"
            session_state.discovery_status = "available"
            session_state.diagnostic_code = None
            return subscription_state

        session_state.discovery_status = "available"
        runtime_service.reserve_report_control(session_id=session_id, candidate=report.candidate, client_id=client_id)
        subscription_state.subscription_state = "reserving"
        runtime_service.enable_report_control(session_id=session_id, candidate=report.candidate, client_id=client_id)
        subscription_state.subscription_state = "enabled"
        event = runtime_service.send_general_interrogation(session_id=session_id, candidate=report.candidate, client_id=client_id)
        session_state.runtime_state = "reporting"
        subscription_state.subscription_state = "reporting"
        subscription_state.report_health = "healthy"
        subscription_state.current_rptena_owner = client_id
        subscription_state.last_report_at = _parse_timestamp(event.received_at)
        subscription_state.stale_signal_count = 0
        session_state.diagnostic_code = None
        return subscription_state

    def _transition(self, session_state: VerificationRuntimeSessionState, runtime_state: str, discovery_status: str) -> None:
        session_state.runtime_state = runtime_state
        session_state.discovery_status = discovery_status

    def _require_handle(self, orchestration_id: str) -> _VerificationRuntimeOrchestrationHandle:
        handle = self._handles.get(orchestration_id)
        if handle is None:
            raise RuntimeError(f'Verification orchestration "{orchestration_id}" not found.')
        return handle


def _runtime_diagnostics_to_evidence_diagnostics(
    diagnostics: Sequence[Iec61850RuntimeDiagnostic],
) -> list[VerificationEvidenceDiagnosticSchema]:
    return [
        VerificationEvidenceDiagnosticSchema(
            code=diagnostic.code,
            message=diagnostic.message,
            severity=diagnostic.severity,
            details={
                "endpoint_id": diagnostic.reference.ied_name if diagnostic.reference is not None else None,
                "report_control_name": diagnostic.reference.report_control_name if diagnostic.reference is not None else None,
            },
        )
        for diagnostic in diagnostics
    ]


def _resolve_subscription_id_for_runtime(*, session_id: str, report) -> str:
    candidate_id = str(getattr(report.candidate, "id", "")).strip()
    if candidate_id:
        return f"{session_id}:{candidate_id}"
    report_control_name = str(getattr(report.candidate, "report_control_name", "")).strip()
    data_set_ref = str(getattr(report.candidate, "data_set_ref", "")).strip()
    if report_control_name:
        return f"{session_id}:{report_control_name}"
    if data_set_ref:
        return f"{session_id}:{data_set_ref}"
    return f"{session_id}:subscription"


def _runtime_summary_key(session_state: VerificationSessionSnapshotSchema) -> str:
    return f"{session_state.endpoint_id}:{session_state.runtime_state}"


def _build_runtime_summary_for_orchestration(
    *,
    session_snapshots: Sequence[VerificationSessionSnapshotSchema],
    subscription_snapshots: Sequence[VerificationSubscriptionSnapshotSchema],
    client_id: str,
    diagnostics: Sequence[VerificationEvidenceDiagnosticSchema],
) -> dict[str, Any]:
    summary = {
        "active_sessions": sum(1 for snapshot in session_snapshots if snapshot.runtime_state != "closed"),
        "reporting_sessions": sum(1 for snapshot in session_snapshots if snapshot.runtime_state == "reporting"),
        "failed_sessions": sum(1 for snapshot in session_snapshots if snapshot.runtime_state == "failed"),
        "closed_sessions": sum(1 for snapshot in session_snapshots if snapshot.runtime_state == "closed"),
        "active_subscriptions": sum(1 for snapshot in subscription_snapshots if snapshot.subscription_state != "closed"),
        "reporting_subscriptions": sum(
            1 for snapshot in subscription_snapshots if snapshot.subscription_state == "reporting"
        ),
        "failed_subscriptions": sum(1 for snapshot in subscription_snapshots if snapshot.subscription_state == "failed"),
        "client_id": client_id,
        "runtime_diagnostics": len(diagnostics),
        "session_states": [_runtime_summary_key(snapshot) for snapshot in session_snapshots],
    }
    return summary
