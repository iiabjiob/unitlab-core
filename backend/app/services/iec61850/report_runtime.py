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


def _utc_now() -> datetime:
    return datetime.now(UTC)
