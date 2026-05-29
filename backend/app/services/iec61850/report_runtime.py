from __future__ import annotations

from dataclasses import dataclass, replace
from datetime import UTC, datetime
from enum import Enum
from typing import Callable, Protocol, Sequence


class Iec61850RuntimeMode(str, Enum):
    SIMULATOR = "simulator"
    MMS = "mms"


class Iec61850ReportKind(str, Enum):
    BUFFERED = "buffered"
    UNBUFFERED = "unbuffered"


class Iec61850RuntimeStatus(str, Enum):
    DISCONNECTED = "disconnected"
    CONNECTED = "connected"
    READ = "read"
    RESERVED = "reserved"
    ENABLED = "enabled"
    GI_PENDING = "gi-pending"
    REPORTING = "reporting"
    DISABLED = "disabled"
    RELEASED = "released"
    FAILED = "failed"


class Iec61850ReportReason(str, Enum):
    GENERAL_INTERROGATION = "general-interrogation"
    DATA_CHANGE = "data-change"
    QUALITY_CHANGE = "quality-change"
    DATA_UPDATE = "data-update"
    INTEGRITY = "integrity"


@dataclass(frozen=True, slots=True)
class Iec61850DeviceEndpoint:
    id: str
    mode: Iec61850RuntimeMode
    ied_name: str
    access_point_name: str
    host: str | None
    port: int = 102


@dataclass(frozen=True, slots=True)
class Iec61850ReportControlRef:
    ied_name: str
    access_point_name: str
    logical_device_inst: str
    logical_node_name: str
    report_control_name: str
    report_kind: Iec61850ReportKind


@dataclass(frozen=True, slots=True)
class Iec61850RuntimeTriggerOptions:
    data_change: bool | None = None
    quality_change: bool | None = None
    data_update: bool | None = None
    periodic: bool | None = None
    general_interrogation: bool | None = None


@dataclass(frozen=True, slots=True)
class Iec61850OptionalFields:
    sequence_number: bool | None = None
    timestamp: bool | None = None
    reason_code: bool | None = None
    data_set_name: bool | None = None
    data_reference: bool | None = None
    entry_id: bool | None = None
    config_revision: bool | None = None
    buffer_overflow: bool | None = None


@dataclass(frozen=True, slots=True)
class Iec61850DataSetMember:
    reference: str
    kind: str = "FCDA"
    fc: str | None = None


@dataclass(frozen=True, slots=True)
class Iec61850ReportControlCandidate:
    id: str
    ied_name: str
    access_point_name: str
    logical_device_inst: str
    logical_node_name: str
    report_control_name: str
    report_kind: Iec61850ReportKind
    rpt_id: str | None
    data_set_ref: str | None
    conf_rev: str | None
    indexed: bool | None
    buffer_time_ms: int | None
    integrity_period_ms: int | None
    trigger_options: Iec61850RuntimeTriggerOptions
    optional_fields: Iec61850OptionalFields
    signals: tuple[Iec61850DataSetMember, ...]

    @property
    def signal_count(self) -> int:
        return len(self.signals)


@dataclass(slots=True)
class Iec61850ReportControlState:
    reference: Iec61850ReportControlRef
    runtime_status: Iec61850RuntimeStatus
    rpt_id: str | None
    data_set_ref: str | None
    conf_rev: str | None
    indexed: bool | None
    buffer_time_ms: int | None
    integrity_period_ms: int | None
    trigger_options: Iec61850RuntimeTriggerOptions
    optional_fields: Iec61850OptionalFields
    signal_count: int
    enabled: bool = False
    reserved_by: str | None = None
    owner: str | None = None
    sequence_number: int = 0
    gi_in_progress: bool = False


@dataclass(frozen=True, slots=True)
class Iec61850RuntimeDiagnostic:
    severity: str
    code: str
    message: str
    reference: Iec61850ReportControlRef


@dataclass(frozen=True, slots=True)
class Iec61850ReportControlReadResult:
    endpoint: Iec61850DeviceEndpoint
    candidate_id: str
    state: Iec61850ReportControlState
    diagnostics: tuple[Iec61850RuntimeDiagnostic, ...]


@dataclass(frozen=True, slots=True)
class Iec61850ReportEventValue:
    data_set_index: int
    reference: str
    data_reference: str | None
    value: bool | int | float | str | None
    reason_code: Iec61850ReportReason
    timestamp: str


@dataclass(frozen=True, slots=True)
class Iec61850ReportEvent:
    id: str
    endpoint_id: str
    received_at: str
    report_control: Iec61850ReportControlRef
    rpt_id: str | None
    data_set_ref: str | None
    conf_rev: str | None
    sequence_number: int | None
    time_of_entry: str | None
    entry_id: str | None
    buffer_overflow: bool | None
    reason: Iec61850ReportReason
    values: tuple[Iec61850ReportEventValue, ...]


@dataclass(frozen=True, slots=True)
class Iec61850SelectedSignal:
    id: str
    address: str
    label: str | None = None


@dataclass(frozen=True, slots=True)
class Iec61850ReportSubscriptionPlanSignal:
    selected_signal: Iec61850SelectedSignal
    model_reference: str
    ied_name: str
    match_kind: str


@dataclass(frozen=True, slots=True)
class Iec61850ReportSubscriptionPlanReport:
    status: str
    candidate: Iec61850ReportControlCandidate
    matched_signals: tuple[Iec61850ReportSubscriptionPlanSignal, ...]


@dataclass(frozen=True, slots=True)
class Iec61850ReportSubscriptionPlanDevice:
    ied_name: str
    access_point_name: str
    reports: tuple[Iec61850ReportSubscriptionPlanReport, ...]


@dataclass(frozen=True, slots=True)
class Iec61850ReportSubscriptionPlanDiagnostic:
    severity: str
    code: str
    message: str
    signal_id: str | None = None
    address: str | None = None


@dataclass(frozen=True, slots=True)
class Iec61850ReportSubscriptionPlan:
    selected_signal_count: int
    matched_signal_count: int
    unmatched_signal_count: int
    ambiguous_signal_count: int
    required_report_count: int
    devices: tuple[Iec61850ReportSubscriptionPlanDevice, ...]
    diagnostics: tuple[Iec61850ReportSubscriptionPlanDiagnostic, ...] = ()


@dataclass(frozen=True, slots=True)
class Iec61850SignalObservation:
    event_id: str
    report_candidate_id: str
    selected_signal_id: str
    selected_signal_address: str
    selected_signal_label: str | None
    ied_name: str
    model_reference: str
    match_kind: str
    data_set_index: int
    data_reference: str | None
    value: bool | int | float | str | None
    reason_code: Iec61850ReportReason
    timestamp: str


@dataclass(frozen=True, slots=True)
class Iec61850UnselectedReportValue:
    data_set_index: int
    reference: str
    data_reference: str | None
    value: bool | int | float | str | None


@dataclass(frozen=True, slots=True)
class Iec61850ReportObservationDiagnostic:
    severity: str
    code: str
    message: str
    reference: Iec61850ReportControlRef
    signal_id: str | None = None
    address: str | None = None
    data_reference: str | None = None


@dataclass(frozen=True, slots=True)
class Iec61850ReportObservationResult:
    event_id: str
    report_candidate_id: str | None
    observations: tuple[Iec61850SignalObservation, ...]
    unselected_values: tuple[Iec61850UnselectedReportValue, ...]
    diagnostics: tuple[Iec61850ReportObservationDiagnostic, ...]


@dataclass(frozen=True, slots=True)
class Iec61850ReportSubscriptionRunReportResult:
    candidate_id: str
    ied_name: str
    access_point_name: str
    report_control_name: str
    report_kind: Iec61850ReportKind
    data_set_ref: str | None
    signal_count: int
    matched_signal_count: int
    runtime_status: Iec61850RuntimeStatus
    diagnostics: tuple[Iec61850RuntimeDiagnostic | Iec61850ReportObservationDiagnostic, ...]
    observations: tuple[Iec61850SignalObservation, ...]
    event: Iec61850ReportEvent | None
    error_code: str | None
    error_message: str | None


@dataclass(frozen=True, slots=True)
class Iec61850ReportSubscriptionRunResult:
    plan: Iec61850ReportSubscriptionPlan
    reports: tuple[Iec61850ReportSubscriptionRunReportResult, ...]
    diagnostics: tuple[Iec61850ReportSubscriptionPlanDiagnostic | Iec61850RuntimeDiagnostic | Iec61850ReportObservationDiagnostic, ...]
    started_at: str
    finished_at: str


@dataclass(frozen=True, slots=True)
class Iec61850SimulatorSubscriptionRunResult:
    plan: Iec61850ReportSubscriptionPlan
    reports: tuple[Iec61850ReportSubscriptionRunReportResult, ...]
    diagnostics: tuple[Iec61850ReportSubscriptionPlanDiagnostic | Iec61850RuntimeDiagnostic | Iec61850ReportObservationDiagnostic, ...]
    event_log: tuple[Iec61850ReportRuntimeEvent, ...]
    started_at: str
    finished_at: str


@dataclass(frozen=True, slots=True)
class Iec61850ReportRuntimeEvent:
    id: str
    at: str
    session_id: str
    endpoint_id: str
    report_control_key: str | None
    kind: str
    runtime_status: Iec61850RuntimeStatus
    client_id: str | None = None
    code: str | None = None
    message: str | None = None


class Iec61850ReportRuntimeError(Exception):
    def __init__(self, code: str, message: str) -> None:
        super().__init__(message)
        self.code = code


class Iec61850ReportSession(Protocol):
    def read_report_control(self, reference: Iec61850ReportControlRef) -> Iec61850ReportControlState: ...
    def reserve_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState: ...
    def release_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState: ...
    def enable_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState: ...
    def disable_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState: ...
    def send_general_interrogation(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportEvent: ...
    def disconnect(self) -> None: ...


class Iec61850ReportRuntimeAdapter(Protocol):
    def connect(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidates: Sequence[Iec61850ReportControlCandidate],
    ) -> Iec61850ReportSession: ...


class Iec61850ReportRuntimeService:
    def __init__(self, adapter: Iec61850ReportRuntimeAdapter) -> None:
        self._adapter = adapter
        self._sessions: dict[str, Iec61850ReportSession] = {}

    def open_session(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidates: Sequence[Iec61850ReportControlCandidate],
    ) -> None:
        if session_id in self._sessions:
            raise Iec61850ReportRuntimeError("SESSION_EXISTS", f'IEC 61850 report session "{session_id}" already exists.')
        self._sessions[session_id] = self._adapter.connect(
            session_id=session_id,
            endpoint=endpoint,
            candidates=candidates,
        )

    def close_session(self, session_id: str) -> None:
        session = self._require_session(session_id)
        try:
            session.disconnect()
        finally:
            self._sessions.pop(session_id, None)

    def read_report_control(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidate: Iec61850ReportControlCandidate,
    ) -> Iec61850ReportControlReadResult:
        state = self._require_session(session_id).read_report_control(to_report_control_ref(candidate))
        return Iec61850ReportControlReadResult(
            endpoint=endpoint,
            candidate_id=candidate.id,
            state=state,
            diagnostics=compare_report_control_state(candidate, state),
        )

    def reserve_report_control(
        self,
        *,
        session_id: str,
        candidate: Iec61850ReportControlCandidate,
        client_id: str,
    ) -> Iec61850ReportControlState:
        return self._require_session(session_id).reserve_report_control(to_report_control_ref(candidate), client_id)

    def release_report_control(
        self,
        *,
        session_id: str,
        candidate: Iec61850ReportControlCandidate,
        client_id: str,
    ) -> Iec61850ReportControlState:
        return self._require_session(session_id).release_report_control(to_report_control_ref(candidate), client_id)

    def enable_report_control(
        self,
        *,
        session_id: str,
        candidate: Iec61850ReportControlCandidate,
        client_id: str,
    ) -> Iec61850ReportControlState:
        return self._require_session(session_id).enable_report_control(to_report_control_ref(candidate), client_id)

    def disable_report_control(
        self,
        *,
        session_id: str,
        candidate: Iec61850ReportControlCandidate,
        client_id: str,
    ) -> Iec61850ReportControlState:
        return self._require_session(session_id).disable_report_control(to_report_control_ref(candidate), client_id)

    def send_general_interrogation(
        self,
        *,
        session_id: str,
        candidate: Iec61850ReportControlCandidate,
        client_id: str,
    ) -> Iec61850ReportEvent:
        return self._require_session(session_id).send_general_interrogation(to_report_control_ref(candidate), client_id)

    def _require_session(self, session_id: str) -> Iec61850ReportSession:
        session = self._sessions.get(session_id)
        if session is None:
            raise Iec61850ReportRuntimeError("SESSION_NOT_FOUND", f'IEC 61850 report session "{session_id}" is not open.')
        return session


def to_report_control_ref(candidate: Iec61850ReportControlCandidate) -> Iec61850ReportControlRef:
    return Iec61850ReportControlRef(
        ied_name=candidate.ied_name,
        access_point_name=candidate.access_point_name,
        logical_device_inst=candidate.logical_device_inst,
        logical_node_name=candidate.logical_node_name,
        report_control_name=candidate.report_control_name,
        report_kind=candidate.report_kind,
    )


def compare_report_control_state(
    candidate: Iec61850ReportControlCandidate,
    state: Iec61850ReportControlState,
) -> tuple[Iec61850RuntimeDiagnostic, ...]:
    reference = to_report_control_ref(candidate)
    diagnostics: list[Iec61850RuntimeDiagnostic] = []

    if state.data_set_ref != candidate.data_set_ref:
        diagnostics.append(Iec61850RuntimeDiagnostic("error", "DATASET_MISMATCH", "Live DatSet does not match SCD.", reference))
    if state.conf_rev != candidate.conf_rev:
        diagnostics.append(Iec61850RuntimeDiagnostic("error", "CONFREV_MISMATCH", "Live ConfRev does not match SCD.", reference))
    if state.signal_count != candidate.signal_count:
        diagnostics.append(Iec61850RuntimeDiagnostic("error", "SIGNAL_COUNT_MISMATCH", "Live DataSet signal count does not match SCD.", reference))
    if state.trigger_options != candidate.trigger_options:
        diagnostics.append(Iec61850RuntimeDiagnostic("warning", "TRGOPS_MISMATCH", "Live TrgOps does not match SCD.", reference))
    if state.optional_fields != candidate.optional_fields:
        diagnostics.append(Iec61850RuntimeDiagnostic("warning", "OPTFIELDS_MISMATCH", "Live OptFlds does not match SCD.", reference))

    return tuple(diagnostics)


def map_report_event_to_signal_observations(
    *,
    candidate: Iec61850ReportControlCandidate,
    matched_signals: Sequence[Iec61850ReportSubscriptionPlanSignal],
    event: Iec61850ReportEvent,
) -> Iec61850ReportObservationResult:
    values_by_reference = {
        _normalize_observation_reference(value.reference, candidate): value
        for value in event.values
    }
    selected_references: set[str] = set()
    observations: list[Iec61850SignalObservation] = []
    diagnostics: list[Iec61850ReportObservationDiagnostic] = []

    for signal in matched_signals:
        key = _normalize_observation_reference(signal.model_reference, candidate)
        selected_references.add(key)
        value = values_by_reference.get(key)
        if value is None:
            diagnostics.append(Iec61850ReportObservationDiagnostic(
                severity="info",
                code="SIGNAL_NOT_INCLUDED_IN_REPORT_EVENT",
                message=f'Selected signal "{signal.selected_signal.address}" was not included in this report event.',
                reference=event.report_control,
                signal_id=signal.selected_signal.id,
                address=signal.selected_signal.address,
                data_reference=signal.model_reference,
            ))
            continue

        observations.append(Iec61850SignalObservation(
            event_id=event.id,
            report_candidate_id=candidate.id,
            selected_signal_id=signal.selected_signal.id,
            selected_signal_address=signal.selected_signal.address,
            selected_signal_label=signal.selected_signal.label,
            ied_name=signal.ied_name,
            model_reference=signal.model_reference,
            match_kind=signal.match_kind,
            data_set_index=value.data_set_index,
            data_reference=value.data_reference,
            value=value.value,
            reason_code=value.reason_code,
            timestamp=value.timestamp,
        ))

    unselected_values = tuple(
        Iec61850UnselectedReportValue(
            data_set_index=value.data_set_index,
            reference=value.reference,
            data_reference=value.data_reference,
            value=value.value,
        )
        for value in event.values
        if _normalize_observation_reference(value.reference, candidate) not in selected_references
    )

    return Iec61850ReportObservationResult(
        event_id=event.id,
        report_candidate_id=candidate.id,
        observations=tuple(sorted(observations, key=lambda observation: (observation.data_set_index, observation.selected_signal_id))),
        unselected_values=unselected_values,
        diagnostics=tuple(diagnostics),
    )


def map_report_event_to_subscription_plan_observations(
    *,
    plan: Iec61850ReportSubscriptionPlan,
    event: Iec61850ReportEvent,
) -> Iec61850ReportObservationResult:
    report = _find_plan_report(plan, event.report_control)
    if report is None:
        return Iec61850ReportObservationResult(
            event_id=event.id,
            report_candidate_id=None,
            observations=(),
            unselected_values=tuple(_to_unselected_report_value(value) for value in event.values),
            diagnostics=(
                Iec61850ReportObservationDiagnostic(
                    severity="error",
                    code="REPORT_NOT_IN_PLAN",
                    message="Report event does not match any required ReportControl in the subscription plan.",
                    reference=event.report_control,
                ),
            ),
        )

    return map_report_event_to_signal_observations(
        candidate=report.candidate,
        matched_signals=report.matched_signals,
        event=event,
    )


def normalize_report_data_reference(data_reference: str, candidate: Iec61850ReportControlCandidate) -> str:
    value = data_reference.strip()
    if not value:
        return ""
    if "!" in value:
        value = value.split("!", 1)[1]
    ied_prefixed_ld = f"{candidate.ied_name}{candidate.logical_device_inst}"
    if value.startswith(ied_prefixed_ld):
        value = value[len(candidate.ied_name):]
    value = _mms_reference_to_dot_reference(value)
    value = _slash_reference_to_dot_reference(value)
    return value


def run_report_subscription_plan(
    *,
    plan: Iec61850ReportSubscriptionPlan,
    adapter: Iec61850ReportRuntimeAdapter,
    client_id: str,
    endpoint_for_device: Callable[[Iec61850ReportSubscriptionPlanDevice], Iec61850DeviceEndpoint],
    session_id_prefix: str = "iec61850-plan",
    now: Callable[[], datetime] | None = None,
) -> Iec61850ReportSubscriptionRunResult:
    timestamp = now or _utc_now
    service = Iec61850ReportRuntimeService(adapter)
    started_at = timestamp().isoformat().replace("+00:00", "Z")
    reports: list[Iec61850ReportSubscriptionRunReportResult] = []
    diagnostics: list[Iec61850ReportSubscriptionPlanDiagnostic | Iec61850RuntimeDiagnostic | Iec61850ReportObservationDiagnostic] = list(plan.diagnostics)

    for device_index, device in enumerate(plan.devices):
        endpoint = endpoint_for_device(device)
        session_id = f"{session_id_prefix}:{device_index}:{device.ied_name}/{device.access_point_name}"
        try:
            service.open_session(
                session_id=session_id,
                endpoint=endpoint,
                candidates=[report.candidate for report in device.reports],
            )
        except Iec61850ReportRuntimeError as error:
            failed_reports = tuple(
                _failed_plan_report(
                    report,
                    error.code,
                    str(error),
                    diagnostics=(_runtime_diagnostic_from_error(report.candidate, error.code, error),),
                )
                for report in device.reports
            )
            reports.extend(failed_reports)
            for failed_report in failed_reports:
                diagnostics.extend(failed_report.diagnostics)
            continue

        try:
            for report in device.reports:
                result = _run_plan_report(
                    service=service,
                    session_id=session_id,
                    endpoint=endpoint,
                    report=report,
                    client_id=client_id,
                )
                reports.append(result)
                diagnostics.extend(result.diagnostics)
        finally:
            try:
                service.close_session(session_id)
            except Iec61850ReportRuntimeError:
                pass

    return Iec61850ReportSubscriptionRunResult(
        plan=plan,
        reports=tuple(reports),
        diagnostics=tuple(diagnostics),
        started_at=started_at,
        finished_at=timestamp().isoformat().replace("+00:00", "Z"),
    )


def run_simulator_report_subscription_plan(
    *,
    plan: Iec61850ReportSubscriptionPlan,
    client_id: str = "unitlab-backend-simulator",
    now: Callable[[], datetime] | None = None,
) -> Iec61850SimulatorSubscriptionRunResult:
    adapter = create_iec61850_simulator_adapter(now=now)
    result = run_report_subscription_plan(
        plan=plan,
        adapter=adapter,
        client_id=client_id,
        endpoint_for_device=build_simulator_endpoint_for_plan_device,
        now=now,
    )
    return Iec61850SimulatorSubscriptionRunResult(
        plan=result.plan,
        reports=result.reports,
        diagnostics=result.diagnostics,
        event_log=adapter.get_event_log(),
        started_at=result.started_at,
        finished_at=result.finished_at,
    )


def build_simulator_endpoint_for_plan_device(
    device: Iec61850ReportSubscriptionPlanDevice,
) -> Iec61850DeviceEndpoint:
    return Iec61850DeviceEndpoint(
        id=f"sim:{device.ied_name}/{device.access_point_name}",
        mode=Iec61850RuntimeMode.SIMULATOR,
        ied_name=device.ied_name,
        access_point_name=device.access_point_name,
        host=None,
        port=102,
    )


def _run_plan_report(
    *,
    service: Iec61850ReportRuntimeService,
    session_id: str,
    endpoint: Iec61850DeviceEndpoint,
    report: Iec61850ReportSubscriptionPlanReport,
    client_id: str,
) -> Iec61850ReportSubscriptionRunReportResult:
    candidate = report.candidate
    diagnostics: list[Iec61850RuntimeDiagnostic | Iec61850ReportObservationDiagnostic] = []
    last_state: Iec61850ReportControlState | None = None
    event: Iec61850ReportEvent | None = None
    observations: tuple[Iec61850SignalObservation, ...] = ()
    reserved = False
    enabled = False
    error_code: str | None = None
    error_message: str | None = None

    try:
        read_result = service.read_report_control(session_id=session_id, endpoint=endpoint, candidate=candidate)
        last_state = read_result.state
        diagnostics.extend(read_result.diagnostics)
        last_state = service.reserve_report_control(session_id=session_id, candidate=candidate, client_id=client_id)
        reserved = True
        last_state = service.enable_report_control(session_id=session_id, candidate=candidate, client_id=client_id)
        enabled = True
        event = service.send_general_interrogation(session_id=session_id, candidate=candidate, client_id=client_id)
        observation_result = map_report_event_to_signal_observations(
            candidate=candidate,
            matched_signals=report.matched_signals,
            event=event,
        )
        observations = observation_result.observations
        diagnostics.extend(observation_result.diagnostics)
    except Iec61850ReportRuntimeError as error:
        error_code = error.code
        error_message = str(error)
        diagnostics.append(_runtime_diagnostic_from_error(candidate, error.code, error))
    except Exception as error:
        error_code = "REPORT_RUN_FAILED"
        error_message = str(error)
        diagnostics.append(Iec61850RuntimeDiagnostic(
            severity="error",
            code="REPORT_RUN_FAILED",
            message=str(error),
            reference=to_report_control_ref(candidate),
        ))
    finally:
        cleanup_state, cleanup_diagnostics = _cleanup_report_control(
            service=service,
            session_id=session_id,
            candidate=candidate,
            client_id=client_id,
            enabled=enabled,
            reserved=reserved,
        )
        if cleanup_state is not None:
            last_state = cleanup_state
        diagnostics.extend(cleanup_diagnostics)
        if error_code is None and cleanup_diagnostics:
            error_code = cleanup_diagnostics[0].code
            error_message = cleanup_diagnostics[0].message

    return Iec61850ReportSubscriptionRunReportResult(
        candidate_id=candidate.id,
        ied_name=candidate.ied_name,
        access_point_name=candidate.access_point_name,
        report_control_name=candidate.report_control_name,
        report_kind=candidate.report_kind,
        data_set_ref=candidate.data_set_ref,
        signal_count=candidate.signal_count,
        matched_signal_count=len(report.matched_signals),
        runtime_status=last_state.runtime_status if last_state is not None else Iec61850RuntimeStatus.FAILED,
        diagnostics=tuple(diagnostics),
        observations=observations,
        event=event,
        error_code=error_code,
        error_message=error_message,
    )


def _cleanup_report_control(
    *,
    service: Iec61850ReportRuntimeService,
    session_id: str,
    candidate: Iec61850ReportControlCandidate,
    client_id: str,
    enabled: bool,
    reserved: bool,
) -> tuple[Iec61850ReportControlState | None, tuple[Iec61850RuntimeDiagnostic, ...]]:
    diagnostics: list[Iec61850RuntimeDiagnostic] = []
    last_state: Iec61850ReportControlState | None = None

    if enabled:
        try:
            last_state = service.disable_report_control(session_id=session_id, candidate=candidate, client_id=client_id)
        except Iec61850ReportRuntimeError as error:
            diagnostics.append(_runtime_diagnostic_from_error(candidate, "DISABLE_CLEANUP_FAILED", error))

    if reserved:
        try:
            last_state = service.release_report_control(session_id=session_id, candidate=candidate, client_id=client_id)
        except Iec61850ReportRuntimeError as error:
            diagnostics.append(_runtime_diagnostic_from_error(candidate, "RELEASE_CLEANUP_FAILED", error))

    return last_state, tuple(diagnostics)


def _runtime_diagnostic_from_error(
    candidate: Iec61850ReportControlCandidate,
    code: str,
    error: Iec61850ReportRuntimeError,
) -> Iec61850RuntimeDiagnostic:
    return Iec61850RuntimeDiagnostic(
        severity="error",
        code=code,
        message=str(error),
        reference=to_report_control_ref(candidate),
    )


def _find_plan_report(
    plan: Iec61850ReportSubscriptionPlan,
    reference: Iec61850ReportControlRef,
) -> Iec61850ReportSubscriptionPlanReport | None:
    for device in plan.devices:
        for report in device.reports:
            if _same_report_control(report.candidate, reference):
                return report
    return None


def _same_report_control(
    candidate: Iec61850ReportControlCandidate,
    reference: Iec61850ReportControlRef,
) -> bool:
    return (
        candidate.ied_name == reference.ied_name
        and candidate.access_point_name == reference.access_point_name
        and candidate.logical_device_inst == reference.logical_device_inst
        and candidate.logical_node_name == reference.logical_node_name
        and candidate.report_control_name == reference.report_control_name
        and candidate.report_kind == reference.report_kind
    )


def _to_unselected_report_value(value: Iec61850ReportEventValue) -> Iec61850UnselectedReportValue:
    return Iec61850UnselectedReportValue(
        data_set_index=value.data_set_index,
        reference=value.reference,
        data_reference=value.data_reference,
        value=value.value,
    )


def _failed_plan_report(
    report: Iec61850ReportSubscriptionPlanReport,
    error_code: str,
    error_message: str,
    diagnostics: tuple[Iec61850RuntimeDiagnostic | Iec61850ReportObservationDiagnostic, ...] = (),
) -> Iec61850ReportSubscriptionRunReportResult:
    candidate = report.candidate
    return Iec61850ReportSubscriptionRunReportResult(
        candidate_id=candidate.id,
        ied_name=candidate.ied_name,
        access_point_name=candidate.access_point_name,
        report_control_name=candidate.report_control_name,
        report_kind=candidate.report_kind,
        data_set_ref=candidate.data_set_ref,
        signal_count=candidate.signal_count,
        matched_signal_count=len(report.matched_signals),
        runtime_status=Iec61850RuntimeStatus.FAILED,
        diagnostics=diagnostics,
        observations=(),
        event=None,
        error_code=error_code,
        error_message=error_message,
    )


def create_iec61850_simulator_adapter(
    *,
    now: Callable[[], datetime] | None = None,
    strict_disconnect_while_enabled: bool = False,
) -> "_Iec61850SimulatorAdapter":
    return _Iec61850SimulatorAdapter(
        now=now or _utc_now,
        strict_disconnect_while_enabled=strict_disconnect_while_enabled,
    )


class _Iec61850SimulatorAdapter:
    def __init__(self, *, now: Callable[[], datetime], strict_disconnect_while_enabled: bool) -> None:
        self._now = now
        self._strict_disconnect_while_enabled = strict_disconnect_while_enabled
        self._events: list[Iec61850ReportRuntimeEvent] = []
        self._event_sequence = 0

    def connect(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidates: Sequence[Iec61850ReportControlCandidate],
    ) -> Iec61850ReportSession:
        if endpoint.mode != Iec61850RuntimeMode.SIMULATOR:
            raise Iec61850ReportRuntimeError("UNSUPPORTED_ENDPOINT_MODE", "IEC 61850 simulator adapter only accepts simulator endpoints.")
        session = _Iec61850SimulatorSession(
            session_id=session_id,
            endpoint=endpoint,
            candidates=tuple(candidates),
            now=self._now,
            append_event=self._append_event,
            strict_disconnect_while_enabled=self._strict_disconnect_while_enabled,
        )
        self._append_event(session_id, endpoint.id, None, "connect", Iec61850RuntimeStatus.CONNECTED)
        return session

    def get_event_log(self) -> tuple[Iec61850ReportRuntimeEvent, ...]:
        return tuple(self._events)

    def clear_event_log(self) -> None:
        self._events.clear()
        self._event_sequence = 0

    def _append_event(
        self,
        session_id: str,
        endpoint_id: str,
        report_key: str | None,
        kind: str,
        runtime_status: Iec61850RuntimeStatus,
        client_id: str | None = None,
        code: str | None = None,
        message: str | None = None,
    ) -> None:
        self._event_sequence += 1
        self._events.append(Iec61850ReportRuntimeEvent(
            id=f"{session_id}:{self._event_sequence}",
            at=self._now().isoformat().replace("+00:00", "Z"),
            session_id=session_id,
            endpoint_id=endpoint_id,
            report_control_key=report_key,
            kind=kind,
            runtime_status=runtime_status,
            client_id=client_id,
            code=code,
            message=message,
        ))


@dataclass(slots=True)
class _SimulatorReportRuntime:
    candidate: Iec61850ReportControlCandidate
    expected_conf_rev: str | None
    state: Iec61850ReportControlState


class _Iec61850SimulatorSession:
    def __init__(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidates: tuple[Iec61850ReportControlCandidate, ...],
        now: Callable[[], datetime],
        append_event: Callable[..., None],
        strict_disconnect_while_enabled: bool,
    ) -> None:
        self._session_id = session_id
        self._endpoint = endpoint
        self._now = now
        self._append_event = append_event
        self._strict_disconnect_while_enabled = strict_disconnect_while_enabled
        self._connected = True
        self._reports = {
            report_control_key(to_report_control_ref(candidate)): _SimulatorReportRuntime(
                candidate=candidate,
                expected_conf_rev=candidate.conf_rev,
                state=_state_from_candidate(candidate),
            )
            for candidate in candidates
        }

    def read_report_control(self, reference: Iec61850ReportControlRef) -> Iec61850ReportControlState:
        runtime = self._require_runtime(reference)
        self._ensure_connected(runtime)
        self._transition(runtime, "read", Iec61850RuntimeStatus.READ)
        return _clone_state(runtime.state)

    def reserve_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        runtime = self._require_runtime(reference)
        self._ensure_connected(runtime)
        state = runtime.state
        if state.enabled:
            self._fail(runtime, "RESERVE_WHILE_ENABLED", "ReportControl must be disabled before reservation changes.", client_id)
        if state.reserved_by is not None and state.reserved_by != client_id:
            self._fail(runtime, "RESERVATION_CONFLICT", f'ReportControl is already reserved by "{state.reserved_by}".', client_id)
        state.reserved_by = client_id
        state.owner = client_id
        self._transition(runtime, "reserve", Iec61850RuntimeStatus.RESERVED, client_id)
        return _clone_state(state)

    def release_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        runtime = self._require_runtime(reference)
        self._ensure_connected(runtime)
        state = runtime.state
        if state.enabled:
            self._fail(runtime, "RELEASE_WHILE_ENABLED", "ReportControl must be disabled before reservation release.", client_id)
        if state.reserved_by is not None and state.reserved_by != client_id:
            self._fail(runtime, "RESERVATION_CONFLICT", f'ReportControl is reserved by "{state.reserved_by}".', client_id)
        state.reserved_by = None
        state.owner = None
        self._transition(runtime, "release", Iec61850RuntimeStatus.RELEASED, client_id)
        return _clone_state(state)

    def enable_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        runtime = self._require_runtime(reference)
        self._ensure_connected(runtime)
        state = runtime.state
        if state.conf_rev != runtime.expected_conf_rev:
            self._fail(runtime, "CONFREV_STALE", "Live ConfRev does not match SCD.", client_id)
        if state.reserved_by is None:
            self._fail(runtime, "ENABLE_WITHOUT_RESERVATION", "ReportControl must be reserved before enable.", client_id)
        if state.reserved_by != client_id:
            self._fail(runtime, "RESERVATION_CONFLICT", f'ReportControl is reserved by "{state.reserved_by}".', client_id)
        state.enabled = True
        state.owner = client_id
        self._transition(runtime, "enable", Iec61850RuntimeStatus.ENABLED, client_id)
        return _clone_state(state)

    def disable_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        runtime = self._require_runtime(reference)
        self._ensure_connected(runtime)
        state = runtime.state
        if state.owner is not None and state.owner != client_id:
            self._fail(runtime, "OWNERSHIP_CONFLICT", f'ReportControl is owned by "{state.owner}".', client_id)
        state.enabled = False
        self._transition(runtime, "disable", Iec61850RuntimeStatus.DISABLED, client_id)
        return _clone_state(state)

    def send_general_interrogation(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportEvent:
        runtime = self._require_runtime(reference)
        self._ensure_connected(runtime)
        state = runtime.state
        if not state.enabled:
            self._fail(runtime, "GI_WHILE_DISABLED", "ReportControl must be enabled before GI.", client_id)
        if state.owner is not None and state.owner != client_id:
            self._fail(runtime, "OWNERSHIP_CONFLICT", f'ReportControl is owned by "{state.owner}".', client_id)
        state.gi_in_progress = True
        self._transition(runtime, "general-interrogation", Iec61850RuntimeStatus.GI_PENDING, client_id)
        state.sequence_number += 1
        received_at = self._now().isoformat().replace("+00:00", "Z")
        values = tuple(
            Iec61850ReportEventValue(
                data_set_index=index,
                reference=signal.reference,
                data_reference=signal.reference,
                value=index,
                reason_code=Iec61850ReportReason.GENERAL_INTERROGATION,
                timestamp=received_at,
            )
            for index, signal in enumerate(runtime.candidate.signals)
        )
        state.gi_in_progress = False
        self._transition(runtime, "report", Iec61850RuntimeStatus.REPORTING, client_id)
        return Iec61850ReportEvent(
            id=f"{self._endpoint.id}:{report_control_key(reference)}:{state.sequence_number}",
            endpoint_id=self._endpoint.id,
            received_at=received_at,
            report_control=reference,
            rpt_id=state.rpt_id,
            data_set_ref=state.data_set_ref,
            conf_rev=state.conf_rev,
            sequence_number=state.sequence_number,
            time_of_entry=received_at,
            entry_id=f"{self._endpoint.id}:{report_control_key(reference)}:{state.sequence_number}",
            buffer_overflow=False,
            reason=Iec61850ReportReason.GENERAL_INTERROGATION,
            values=values,
        )

    def disconnect(self) -> None:
        if not self._connected:
            return
        self._connected = False
        self._append_event(self._session_id, self._endpoint.id, None, "disconnect", Iec61850RuntimeStatus.DISCONNECTED)
        if not self._strict_disconnect_while_enabled:
            return
        for runtime in self._reports.values():
            if runtime.state.enabled:
                self._fail(runtime, "DISCONNECT_WHILE_ENABLED", "Connection disconnected while a ReportControl was still enabled.")

    def _require_runtime(self, reference: Iec61850ReportControlRef) -> _SimulatorReportRuntime:
        key = report_control_key(reference)
        runtime = self._reports.get(key)
        if runtime is None:
            raise Iec61850ReportRuntimeError("REPORT_CONTROL_NOT_FOUND", f'ReportControl "{key}" is not available in simulator session.')
        return runtime

    def _ensure_connected(self, runtime: _SimulatorReportRuntime) -> None:
        if not self._connected:
            self._fail(runtime, "CONNECTION_CLOSED", "Simulator connection is already disconnected.")

    def _transition(
        self,
        runtime: _SimulatorReportRuntime,
        kind: str,
        runtime_status: Iec61850RuntimeStatus,
        client_id: str | None = None,
    ) -> None:
        runtime.state.runtime_status = runtime_status
        self._append_event(
            self._session_id,
            self._endpoint.id,
            report_control_key(runtime.state.reference),
            kind,
            runtime_status,
            client_id,
        )

    def _fail(
        self,
        runtime: _SimulatorReportRuntime,
        code: str,
        message: str,
        client_id: str | None = None,
    ) -> None:
        runtime.state.runtime_status = Iec61850RuntimeStatus.FAILED
        self._append_event(
            self._session_id,
            self._endpoint.id,
            report_control_key(runtime.state.reference),
            "failure",
            Iec61850RuntimeStatus.FAILED,
            client_id,
            code,
            message,
        )
        raise Iec61850ReportRuntimeError(code, message)


def _state_from_candidate(candidate: Iec61850ReportControlCandidate) -> Iec61850ReportControlState:
    return Iec61850ReportControlState(
        reference=to_report_control_ref(candidate),
        runtime_status=Iec61850RuntimeStatus.DISCONNECTED,
        rpt_id=candidate.rpt_id,
        data_set_ref=candidate.data_set_ref,
        conf_rev=candidate.conf_rev,
        indexed=candidate.indexed,
        buffer_time_ms=candidate.buffer_time_ms,
        integrity_period_ms=candidate.integrity_period_ms,
        trigger_options=candidate.trigger_options,
        optional_fields=candidate.optional_fields,
        signal_count=candidate.signal_count,
    )


def _clone_state(state: Iec61850ReportControlState) -> Iec61850ReportControlState:
    return replace(state)


def report_control_key(reference: Iec61850ReportControlRef) -> str:
    return "/".join((
        reference.ied_name,
        reference.access_point_name,
        reference.logical_device_inst,
        reference.logical_node_name,
        reference.report_control_name,
        reference.report_kind.value,
    ))


def _normalize_observation_reference(reference: str, candidate: Iec61850ReportControlCandidate) -> str:
    return normalize_report_data_reference(reference, candidate).lower()


def _mms_reference_to_dot_reference(reference: str) -> str:
    if "$" not in reference:
        return reference
    logical_node_ref, fc, *path = reference.split("$")
    if not logical_node_ref or not fc or not path:
        return reference.replace("$", ".")
    return f"{logical_node_ref}.{'.'.join(path)}[{fc}]"


def _slash_reference_to_dot_reference(reference: str) -> str:
    fc = _extract_functional_constraint(reference)
    without_fc = _remove_functional_constraint(reference)
    parts = [part for part in without_fc.split("/") if part]
    if len(parts) < 3:
        return reference
    ld_inst, logical_node_name, *data_path = parts
    return f"{ld_inst}/{logical_node_name}.{'.'.join(data_path)}{f'[{fc}]' if fc else ''}"


def _extract_functional_constraint(reference: str) -> str | None:
    if not reference.endswith("]") or "[" not in reference:
        return None
    start = reference.rfind("[")
    if start < 0 or start >= len(reference) - 1:
        return None
    return reference[start + 1:-1]


def _remove_functional_constraint(reference: str) -> str:
    if not reference.endswith("]") or "[" not in reference:
        return reference
    return reference[:reference.rfind("[")]


def _utc_now() -> datetime:
    return datetime.now(UTC)
