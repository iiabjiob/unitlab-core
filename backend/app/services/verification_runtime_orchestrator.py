from __future__ import annotations

from contextlib import suppress
from dataclasses import dataclass
from datetime import UTC, datetime
from typing import Any, Callable, Sequence
from uuid import uuid4

from app.schemas.verification_schema import (
    SignalVerificationEvidenceSetSchema,
    VerificationExecutionContextSchema,
    VerificationEvidenceDiagnosticSchema,
    VerificationRunSchema,
    VerificationSessionSnapshotSchema,
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
    subscription_status: str = "pending"
    report_health: str = "unknown"
    last_report_at: datetime | None = None
    last_error: str | None = None
    selected_report_control: str | None = None
    selected_data_set: str | None = None
    current_rptena_owner: str | None = None
    stale_signal_count: int | None = None
    diagnostic_code: str | None = None

    def to_snapshot(self) -> VerificationSessionSnapshotSchema:
        return VerificationSessionSnapshotSchema(
            session_id=self.session_id,
            endpoint_id=self.endpoint_id,
            runtime_state=self.runtime_state,
            connection_generation=self.connection_generation,
            discovery_status=self.discovery_status,
            subscription_status=self.subscription_status,
            report_health=self.report_health,
            last_report_at=self.last_report_at,
            last_error=self.last_error,
            selected_report_control=self.selected_report_control,
            selected_data_set=self.selected_data_set,
            current_rptena_owner=self.current_rptena_owner,
            stale_signal_count=self.stale_signal_count,
            diagnostic_code=self.diagnostic_code,
        )


@dataclass(frozen=True, slots=True)
class VerificationRuntimeOrchestrationResult:
    orchestration_id: str
    verification_run: VerificationRunSchema
    session_snapshots: tuple[VerificationSessionSnapshotSchema, ...]
    diagnostics: tuple[VerificationEvidenceDiagnosticSchema, ...]
    active_session_ids: tuple[str, ...]


@dataclass
class _VerificationRuntimeOrchestrationHandle:
    workspace_id: int
    test_run_id: str
    runtime_service: Iec61850ReportRuntimeService
    session_states: dict[str, VerificationRuntimeSessionState]
    session_order: list[str]
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
        session_order: list[str] = []
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
                self._transition(session_states[session_id], "discovering", "discovering", "unknown")

                for report in device.reports:
                    self._activate_report_session(
                        runtime_service=runtime_service,
                        session_state=session_states[session_id],
                        endpoint=endpoint,
                        report=report,
                        client_id=client_id,
                        diagnostics=diagnostics,
                        session_id=session_id,
                    )
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
            session_order=session_order,
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
        evidence_set = build_signal_verification_evidence_set(
            test_run_id=handle.test_run_id,
            evidence=(),
            diagnostics=handle.diagnostics,
        )
        workflow_state = "running"
        verdict_state = "pending"
        runtime_state = _resolve_runtime_state(session_snapshots) or "connecting"
        verification_run = VerificationRunSchema(
            test_run_id=handle.test_run_id,
            verification_targets=list(handle.verification_targets),
            subscription_plan=handle.subscription_plan,
            session_snapshots=list(session_snapshots),
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
                handle.session_states[session_id].subscription_status = "closed"
                handle.session_states[session_id].report_health = "unknown"
        result = self.snapshot(orchestration_id)
        self._handles.pop(orchestration_id, None)
        return result

    def _activate_report_session(
        self,
        *,
        runtime_service: Iec61850ReportRuntimeService,
        session_state: VerificationRuntimeSessionState,
        endpoint: Iec61850DeviceEndpoint,
        report,
        client_id: str,
        diagnostics: list[VerificationEvidenceDiagnosticSchema],
        session_id: str,
    ) -> None:
        state = runtime_service.read_report_control(
            session_id=session_id,
            endpoint=endpoint,
            candidate=report.candidate,
        )
        session_diagnostics = _runtime_diagnostics_to_evidence_diagnostics(state.diagnostics)
        diagnostics.extend(session_diagnostics)
        if any(diagnostic.severity == "error" for diagnostic in session_diagnostics):
            session_state.runtime_state = "failed"
            session_state.discovery_status = "failed"
            session_state.subscription_status = "failed"
            session_state.report_health = "degraded"
            session_state.last_error = "ReportControl precheck failed"
            session_state.diagnostic_code = state.diagnostics[0].code if state.diagnostics else "REPORT_CONTROL_PRECHECK_FAILED"
            return

        session_state.discovery_status = "available"
        session_state.runtime_state = "subscribing"
        runtime_service.reserve_report_control(session_id=session_id, candidate=report.candidate, client_id=client_id)
        session_state.subscription_status = "reserved"
        runtime_service.enable_report_control(session_id=session_id, candidate=report.candidate, client_id=client_id)
        session_state.subscription_status = "enabled"
        event = runtime_service.send_general_interrogation(session_id=session_id, candidate=report.candidate, client_id=client_id)
        session_state.runtime_state = "reporting"
        session_state.report_health = "healthy"
        session_state.current_rptena_owner = client_id
        session_state.selected_report_control = report.candidate.report_control_name
        session_state.selected_data_set = report.candidate.data_set_ref
        session_state.last_report_at = _parse_timestamp(event.received_at)
        session_state.stale_signal_count = 0
        session_state.diagnostic_code = None

    def _transition(self, session_state: VerificationRuntimeSessionState, runtime_state: str, discovery_status: str, report_health: str) -> None:
        session_state.runtime_state = runtime_state
        session_state.discovery_status = discovery_status
        session_state.report_health = report_health

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


def _runtime_summary_key(session_state: VerificationSessionSnapshotSchema) -> str:
    return f"{session_state.endpoint_id}:{session_state.runtime_state}"


def _build_runtime_summary_for_orchestration(
    *,
    session_snapshots: Sequence[VerificationSessionSnapshotSchema],
    client_id: str,
    diagnostics: Sequence[VerificationEvidenceDiagnosticSchema],
) -> dict[str, Any]:
    summary = {
        "active_sessions": sum(1 for snapshot in session_snapshots if snapshot.runtime_state != "closed"),
        "reporting_sessions": sum(1 for snapshot in session_snapshots if snapshot.runtime_state == "reporting"),
        "failed_sessions": sum(1 for snapshot in session_snapshots if snapshot.runtime_state == "failed"),
        "closed_sessions": sum(1 for snapshot in session_snapshots if snapshot.runtime_state == "closed"),
        "client_id": client_id,
        "runtime_diagnostics": len(diagnostics),
        "session_states": [_runtime_summary_key(snapshot) for snapshot in session_snapshots],
    }
    return summary
