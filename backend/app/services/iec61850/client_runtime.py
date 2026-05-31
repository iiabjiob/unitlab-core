from __future__ import annotations

from datetime import datetime
from typing import Callable, Sequence

from .report_runtime import (
    Iec61850DeviceEndpoint,
    Iec61850ReportControlCandidate,
    Iec61850ReportControlReadResult,
    Iec61850ReportControlState,
    Iec61850ReportEvent,
    Iec61850ReportRuntimeAdapter,
    Iec61850ReportRuntimeService,
    Iec61850ReportSession,
    Iec61850ReportSubscriptionPlan,
    Iec61850ReportSubscriptionPlanDevice,
    Iec61850ReportSubscriptionRunResult,
    Iec61850RuntimeDiagnostic,
    Iec61850SimulatorSubscriptionRunResult,
    build_simulator_endpoint_for_plan_device,
    run_report_subscription_plan,
    run_simulator_report_subscription_plan,
)


class Iec61850MmsClientRuntime:
    """Client-side orchestration facade over the shared report-runtime contract.

    The client runtime owns no MMS wire details. It delegates association/session
    lifecycle and report-control orchestration to the UnitLab report-runtime
    contract so the same flow can be used for simulator and future real IEDs.
    """

    def __init__(self, adapter: Iec61850ReportRuntimeAdapter) -> None:
        self._adapter = adapter
        self._service = Iec61850ReportRuntimeService(adapter)

    def open_session(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidates: Sequence[Iec61850ReportControlCandidate],
    ) -> None:
        self._service.open_session(session_id=session_id, endpoint=endpoint, candidates=candidates)

    def close_session(self, session_id: str) -> None:
        self._service.close_session(session_id)

    def read_report_control(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidate: Iec61850ReportControlCandidate,
    ) -> Iec61850ReportControlReadResult:
        return self._service.read_report_control(session_id=session_id, endpoint=endpoint, candidate=candidate)

    def reserve_report_control(
        self,
        *,
        session_id: str,
        candidate: Iec61850ReportControlCandidate,
        client_id: str,
    ) -> Iec61850ReportControlState:
        return self._service.reserve_report_control(session_id=session_id, candidate=candidate, client_id=client_id)

    def release_report_control(
        self,
        *,
        session_id: str,
        candidate: Iec61850ReportControlCandidate,
        client_id: str,
    ) -> Iec61850ReportControlState:
        return self._service.release_report_control(session_id=session_id, candidate=candidate, client_id=client_id)

    def enable_report_control(
        self,
        *,
        session_id: str,
        candidate: Iec61850ReportControlCandidate,
        client_id: str,
    ) -> Iec61850ReportControlState:
        return self._service.enable_report_control(session_id=session_id, candidate=candidate, client_id=client_id)

    def disable_report_control(
        self,
        *,
        session_id: str,
        candidate: Iec61850ReportControlCandidate,
        client_id: str,
    ) -> Iec61850ReportControlState:
        return self._service.disable_report_control(session_id=session_id, candidate=candidate, client_id=client_id)

    def send_general_interrogation(
        self,
        *,
        session_id: str,
        candidate: Iec61850ReportControlCandidate,
        client_id: str,
    ) -> Iec61850ReportEvent:
        return self._service.send_general_interrogation(session_id=session_id, candidate=candidate, client_id=client_id)

    def run_report_subscription_plan(
        self,
        *,
        plan: Iec61850ReportSubscriptionPlan,
        client_id: str,
        endpoint_for_device: Callable[[Iec61850ReportSubscriptionPlanDevice], Iec61850DeviceEndpoint] = build_simulator_endpoint_for_plan_device,
        session_id_prefix: str = "iec61850-client",
        now: Callable[[], datetime] | None = None,
    ) -> Iec61850ReportSubscriptionRunResult:
        return run_report_subscription_plan(
            plan=plan,
            adapter=self._adapter,
            client_id=client_id,
            endpoint_for_device=endpoint_for_device,
            session_id_prefix=session_id_prefix,
            now=now,
        )

    def run_simulator_report_subscription_plan(
        self,
        *,
        plan: Iec61850ReportSubscriptionPlan,
        client_id: str = "unitlab-backend-client",
        now: Callable[[], datetime] | None = None,
    ) -> Iec61850SimulatorSubscriptionRunResult:
        return run_simulator_report_subscription_plan(plan=plan, client_id=client_id, now=now)


__all__ = ["Iec61850MmsClientRuntime"]
