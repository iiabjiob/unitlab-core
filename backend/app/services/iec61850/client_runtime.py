from __future__ import annotations

from dataclasses import dataclass
from datetime import UTC, datetime
from typing import Callable, Sequence

from .report_runtime import (
    Iec61850DeviceEndpoint,
    Iec61850ReportControlCandidate,
    Iec61850ReportControlReadResult,
    Iec61850ReportControlState,
    Iec61850ReportEvent,
    Iec61850ReportRuntimeAdapter,
    Iec61850ReportRuntimeError,
    Iec61850ReportRuntimeService,
    Iec61850ReportSubscriptionPlan,
    Iec61850ReportSubscriptionPlanDevice,
    Iec61850ReportSubscriptionRunResult,
    Iec61850SimulatorSubscriptionRunResult,
    build_simulator_endpoint_for_plan_device,
    run_report_subscription_plan,
    run_simulator_report_subscription_plan,
)


@dataclass(frozen=True, slots=True)
class Iec61850MmsClientEvent:
    id: str
    at: str
    kind: str
    session_id: str
    endpoint_id: str
    candidate_id: str | None = None
    report_control_name: str | None = None
    client_id: str | None = None
    outcome: str | None = None
    code: str | None = None
    message: str | None = None


class Iec61850MmsClientRuntime:
    """Client-side orchestration facade over the shared report-runtime contract.

    The client runtime owns no MMS wire details. It delegates association/session
    lifecycle and report-control orchestration to the UnitLab report-runtime
    contract so the same flow can be used for simulator and future real IEDs.
    """

    def __init__(self, adapter: Iec61850ReportRuntimeAdapter) -> None:
        self._adapter = adapter
        self._service = Iec61850ReportRuntimeService(adapter)
        self._events: list[Iec61850MmsClientEvent] = []
        self._event_sequence = 0

    def transcript(self) -> tuple[Iec61850MmsClientEvent, ...]:
        return tuple(self._events)

    def clear_transcript(self) -> None:
        self._events.clear()
        self._event_sequence = 0

    def open_session(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidates: Sequence[Iec61850ReportControlCandidate],
    ) -> None:
        self._service.open_session(session_id=session_id, endpoint=endpoint, candidates=candidates)
        self._append_event(
            kind="session-open",
            session_id=session_id,
            endpoint_id=endpoint.id,
            outcome="connected",
        )

    def close_session(self, session_id: str) -> None:
        self._service.close_session(session_id)
        self._append_event(
            kind="session-close",
            session_id=session_id,
            endpoint_id=session_id,
            outcome="closed",
        )

    def read_report_control(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidate: Iec61850ReportControlCandidate,
    ) -> Iec61850ReportControlReadResult:
        result = self._service.read_report_control(session_id=session_id, endpoint=endpoint, candidate=candidate)
        self._append_event(
            kind="report-control-read",
            session_id=session_id,
            endpoint_id=endpoint.id,
            candidate_id=candidate.id,
            report_control_name=candidate.report_control_name,
            outcome=result.state.runtime_status.value,
            code=self._diagnostic_code(result.diagnostics),
            message=self._diagnostic_message(result.diagnostics),
        )
        return result

    def reserve_report_control(
        self,
        *,
        session_id: str,
        candidate: Iec61850ReportControlCandidate,
        client_id: str,
    ) -> Iec61850ReportControlState:
        state = self._service.reserve_report_control(session_id=session_id, candidate=candidate, client_id=client_id)
        self._append_event(
            kind="report-control-reserve",
            session_id=session_id,
            endpoint_id=self._endpoint_id(candidate),
            candidate_id=candidate.id,
            report_control_name=candidate.report_control_name,
            client_id=client_id,
            outcome=state.runtime_status.value,
        )
        return state

    def release_report_control(
        self,
        *,
        session_id: str,
        candidate: Iec61850ReportControlCandidate,
        client_id: str,
    ) -> Iec61850ReportControlState:
        state = self._service.release_report_control(session_id=session_id, candidate=candidate, client_id=client_id)
        self._append_event(
            kind="report-control-release",
            session_id=session_id,
            endpoint_id=self._endpoint_id(candidate),
            candidate_id=candidate.id,
            report_control_name=candidate.report_control_name,
            client_id=client_id,
            outcome=state.runtime_status.value,
        )
        return state

    def enable_report_control(
        self,
        *,
        session_id: str,
        candidate: Iec61850ReportControlCandidate,
        client_id: str,
    ) -> Iec61850ReportControlState:
        state = self._service.enable_report_control(session_id=session_id, candidate=candidate, client_id=client_id)
        self._append_event(
            kind="report-control-enable",
            session_id=session_id,
            endpoint_id=self._endpoint_id(candidate),
            candidate_id=candidate.id,
            report_control_name=candidate.report_control_name,
            client_id=client_id,
            outcome=state.runtime_status.value,
        )
        return state

    def disable_report_control(
        self,
        *,
        session_id: str,
        candidate: Iec61850ReportControlCandidate,
        client_id: str,
    ) -> Iec61850ReportControlState:
        state = self._service.disable_report_control(session_id=session_id, candidate=candidate, client_id=client_id)
        self._append_event(
            kind="report-control-disable",
            session_id=session_id,
            endpoint_id=self._endpoint_id(candidate),
            candidate_id=candidate.id,
            report_control_name=candidate.report_control_name,
            client_id=client_id,
            outcome=state.runtime_status.value,
        )
        return state

    def send_general_interrogation(
        self,
        *,
        session_id: str,
        candidate: Iec61850ReportControlCandidate,
        client_id: str,
    ) -> Iec61850ReportEvent:
        event = self._service.send_general_interrogation(session_id=session_id, candidate=candidate, client_id=client_id)
        self._append_event(
            kind="report-control-gi",
            session_id=session_id,
            endpoint_id=event.endpoint_id,
            candidate_id=candidate.id,
            report_control_name=candidate.report_control_name,
            client_id=client_id,
            outcome=event.reason.value,
        )
        return event

    def run_report_subscription_plan(
        self,
        *,
        plan: Iec61850ReportSubscriptionPlan,
        client_id: str,
        endpoint_for_device: Callable[[Iec61850ReportSubscriptionPlanDevice], Iec61850DeviceEndpoint] = build_simulator_endpoint_for_plan_device,
        session_id_prefix: str = "iec61850-client",
        now: Callable[[], datetime] | None = None,
    ) -> Iec61850ReportSubscriptionRunResult:
        result = run_report_subscription_plan(
            plan=plan,
            adapter=self._adapter,
            client_id=client_id,
            endpoint_for_device=endpoint_for_device,
            session_id_prefix=session_id_prefix,
            now=now,
        )
        for report in result.reports:
            self._append_event(
                kind="subscription-plan-report",
                session_id=f"{session_id_prefix}:{report.ied_name}/{report.access_point_name}",
                endpoint_id=f"sim:{report.ied_name}/{report.access_point_name}",
                candidate_id=report.candidate_id,
                report_control_name=report.report_control_name,
                client_id=client_id,
                outcome=report.runtime_status.value,
                code=report.error_code,
                message=report.error_message,
            )
        return result

    def run_simulator_report_subscription_plan(
        self,
        *,
        plan: Iec61850ReportSubscriptionPlan,
        client_id: str = "unitlab-backend-client",
        now: Callable[[], datetime] | None = None,
    ) -> Iec61850SimulatorSubscriptionRunResult:
        result = run_simulator_report_subscription_plan(plan=plan, client_id=client_id, now=now)
        for report in result.reports:
            self._append_event(
                kind="simulator-subscription-report",
                session_id=f"sim:{report.ied_name}/{report.access_point_name}",
                endpoint_id=f"sim:{report.ied_name}/{report.access_point_name}",
                candidate_id=report.candidate_id,
                report_control_name=report.report_control_name,
                client_id=client_id,
                outcome=report.runtime_status.value,
                code=report.error_code,
                message=report.error_message,
            )
        return result

    def _append_event(
        self,
        *,
        kind: str,
        session_id: str,
        endpoint_id: str,
        candidate_id: str | None = None,
        report_control_name: str | None = None,
        client_id: str | None = None,
        outcome: str | None = None,
        code: str | None = None,
        message: str | None = None,
    ) -> None:
        self._event_sequence += 1
        self._events.append(Iec61850MmsClientEvent(
            id=f"{session_id}:{self._event_sequence}",
            at=datetime.now(UTC).isoformat().replace("+00:00", "Z"),
            kind=kind,
            session_id=session_id,
            endpoint_id=endpoint_id,
            candidate_id=candidate_id,
            report_control_name=report_control_name,
            client_id=client_id,
            outcome=outcome,
            code=code,
            message=message,
        ))

    def _endpoint_id(self, candidate: Iec61850ReportControlCandidate) -> str:
        return f"{candidate.ied_name}:{candidate.access_point_name}"

    def _diagnostic_code(self, diagnostics: Sequence[Iec61850RuntimeDiagnostic]) -> str | None:
        return diagnostics[0].code if diagnostics else None

    def _diagnostic_message(self, diagnostics: Sequence[Iec61850RuntimeDiagnostic]) -> str | None:
        return diagnostics[0].message if diagnostics else None


__all__ = ["Iec61850MmsClientEvent", "Iec61850MmsClientRuntime"]
