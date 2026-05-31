from __future__ import annotations

from dataclasses import dataclass
from datetime import UTC, datetime
from threading import RLock
from typing import Callable

from .client_runtime import Iec61850MmsClientEvent, Iec61850MmsClientRuntime
from .report_runtime import (
    Iec61850DataSetMember,
    Iec61850DeviceEndpoint,
    Iec61850OptionalFields,
    Iec61850ReportControlCandidate,
    Iec61850ReportControlReadResult,
    Iec61850ReportControlState,
    Iec61850ReportEvent,
    Iec61850ReportKind,
    Iec61850ReportRuntimeError,
    Iec61850ReportSubscriptionPlan,
    Iec61850ReportSubscriptionPlanDevice,
    Iec61850ReportSubscriptionPlanReport,
    Iec61850ReportSubscriptionPlanSignal,
    Iec61850RuntimeMode,
    Iec61850RuntimeTriggerOptions,
    Iec61850SelectedSignal,
    create_iec61850_simulator_adapter,
)


@dataclass(frozen=True, slots=True)
class Iec61850ClientControlDiagnostic:
    action: str
    code: str
    message: str


@dataclass(frozen=True, slots=True)
class Iec61850ClientControlSnapshot:
    session_id: str
    client_id: str
    session_open: bool
    endpoint: Iec61850DeviceEndpoint
    candidate: Iec61850ReportControlCandidate
    last_read: Iec61850ReportControlReadResult | None
    last_state: Iec61850ReportControlState | None
    last_report: Iec61850ReportEvent | None
    last_plan: Iec61850ReportSubscriptionPlan | None
    transcript: tuple[Iec61850MmsClientEvent, ...]
    last_diagnostic: Iec61850ClientControlDiagnostic | None


class Iec61850ClientControlService:
    def __init__(
        self,
        *,
        now: Callable[[], datetime] | None = None,
        session_id: str = "iec61850-client-test",
        client_id: str = "unitlab-test-client",
        endpoint: Iec61850DeviceEndpoint | None = None,
        candidate: Iec61850ReportControlCandidate | None = None,
    ) -> None:
        adapter = create_iec61850_simulator_adapter(now=now or _utc_now)
        self._runtime = Iec61850MmsClientRuntime(adapter)
        self._session_id = session_id
        self._client_id = client_id
        self._endpoint = endpoint or _default_endpoint()
        self._candidate = candidate or _default_candidate()
        self._last_read: Iec61850ReportControlReadResult | None = None
        self._last_state: Iec61850ReportControlState | None = None
        self._last_report: Iec61850ReportEvent | None = None
        self._last_plan: Iec61850ReportSubscriptionPlan | None = None
        self._last_diagnostic: Iec61850ClientControlDiagnostic | None = None
        self._session_open = False
        self._lock = RLock()

    @property
    def session_id(self) -> str:
        return self._session_id

    @property
    def client_id(self) -> str:
        return self._client_id

    def snapshot(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return Iec61850ClientControlSnapshot(
                session_id=self._session_id,
                client_id=self._client_id,
                session_open=self._session_open,
                endpoint=self._endpoint,
                candidate=self._candidate,
                last_read=self._last_read,
                last_state=self._last_state,
                last_report=self._last_report,
                last_plan=self._last_plan,
                transcript=self._runtime.transcript(),
                last_diagnostic=self._last_diagnostic,
            )

    def open_session(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run(
                "open-session",
                lambda: self._runtime.open_session(session_id=self._session_id, endpoint=self._endpoint, candidates=[self._candidate]),
                post=lambda _result: setattr(self, "_session_open", True),
            )

    def close_session(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run("close-session", lambda: self._runtime.close_session(self._session_id), post=lambda _result: setattr(self, "_session_open", False))

    def read_report_control(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run(
                "read-report-control",
                lambda: self._runtime.read_report_control(session_id=self._session_id, endpoint=self._endpoint, candidate=self._candidate),
                post=self._capture_read,
            )

    def reserve_report_control(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run(
                "reserve-report-control",
                lambda: self._runtime.reserve_report_control(session_id=self._session_id, candidate=self._candidate, client_id=self._client_id),
                post=lambda result: setattr(self, "_last_state", result),
            )

    def enable_report_control(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run(
                "enable-report-control",
                lambda: self._runtime.enable_report_control(session_id=self._session_id, candidate=self._candidate, client_id=self._client_id),
                post=lambda result: setattr(self, "_last_state", result),
            )

    def send_general_interrogation(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run(
                "send-general-interrogation",
                lambda: self._runtime.send_general_interrogation(session_id=self._session_id, candidate=self._candidate, client_id=self._client_id),
                post=lambda result: setattr(self, "_last_report", result),
            )

    def disable_report_control(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run(
                "disable-report-control",
                lambda: self._runtime.disable_report_control(session_id=self._session_id, candidate=self._candidate, client_id=self._client_id),
                post=lambda result: setattr(self, "_last_state", result),
            )

    def release_report_control(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run(
                "release-report-control",
                lambda: self._runtime.release_report_control(session_id=self._session_id, candidate=self._candidate, client_id=self._client_id),
                post=lambda result: setattr(self, "_last_state", result),
            )

    def clear_transcript(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            self._runtime.clear_transcript()
            self._last_diagnostic = None
            return self.snapshot()

    def run_subscription_plan(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run(
                "run-subscription-plan",
                lambda: self._runtime.run_simulator_report_subscription_plan(plan=_build_subscription_plan(self._candidate), client_id=self._client_id),
                post=self._capture_subscription_run,
            )

    def _run(self, action: str, operation, post=None):
        try:
            result = operation()
        except Iec61850ReportRuntimeError as exc:
            self._last_diagnostic = Iec61850ClientControlDiagnostic(action=action, code=exc.code, message=str(exc))
            raise
        else:
            self._last_diagnostic = None
            if post is not None:
                post(result)
            return self.snapshot()

    def _capture_read(self, result) -> None:
        self._last_read = result
        self._last_state = result.state

    def _capture_subscription_run(self, result) -> None:
        self._last_plan = result.plan
        self._last_report = result.reports[0].event if result.reports else None


def get_iec61850_client_control_service() -> Iec61850ClientControlService:
    return _CLIENT_CONTROL_SERVICE


def _utc_now() -> datetime:
    return datetime.now(UTC)


def _default_endpoint() -> Iec61850DeviceEndpoint:
    return Iec61850DeviceEndpoint(
        id="sim:IED1/AP1",
        mode=Iec61850RuntimeMode.SIMULATOR,
        ied_name="IED1",
        access_point_name="AP1",
        host="127.0.0.1",
        port=102,
    )


def _default_candidate() -> Iec61850ReportControlCandidate:
    return Iec61850ReportControlCandidate(
        id="report-1",
        ied_name="IED1",
        access_point_name="AP1",
        logical_device_inst="LD0",
        logical_node_name="LLN0",
        report_control_name="brcbEvents",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id="IED1LD0/LLN0.BR.Events",
        data_set_ref="IED1/AP1/LD0/LLN0.dsEvents",
        conf_rev="7",
        indexed=True,
        buffer_time_ms=100,
        integrity_period_ms=1000,
        trigger_options=Iec61850RuntimeTriggerOptions(
            data_change=True,
            quality_change=True,
            data_update=False,
            periodic=False,
            general_interrogation=True,
        ),
        optional_fields=Iec61850OptionalFields(
            sequence_number=True,
            timestamp=True,
            reason_code=True,
            data_set_name=True,
            data_reference=True,
            entry_id=True,
            config_revision=True,
            buffer_overflow=True,
        ),
        signals=(
            Iec61850DataSetMember(reference="LD0/XCBR1.Pos.stVal[ST]", fc="ST"),
        ),
    )


def _build_subscription_plan(candidate: Iec61850ReportControlCandidate) -> Iec61850ReportSubscriptionPlan:
    return Iec61850ReportSubscriptionPlan(
        selected_signal_count=1,
        matched_signal_count=1,
        unmatched_signal_count=0,
        ambiguous_signal_count=0,
        required_report_count=1,
        devices=(
            Iec61850ReportSubscriptionPlanDevice(
                ied_name=candidate.ied_name,
                access_point_name=candidate.access_point_name,
                reports=(
                    Iec61850ReportSubscriptionPlanReport(
                        status="required",
                        candidate=candidate,
                        matched_signals=(
                            Iec61850ReportSubscriptionPlanSignal(
                                selected_signal=Iec61850SelectedSignal(id="sig-1", address="IED1LD0/XCBR1/Pos/stVal[ST]"),
                                model_reference="LD0/XCBR1.Pos.stVal[ST]",
                                ied_name=candidate.ied_name,
                                match_kind="exact",
                            ),
                        ),
                    ),
                ),
            ),
        ),
    )


_CLIENT_CONTROL_SERVICE = Iec61850ClientControlService()
