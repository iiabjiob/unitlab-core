from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol, Sequence, runtime_checkable

from .report_runtime import (
    Iec61850DataSetMember,
    Iec61850DeviceEndpoint,
    Iec61850OptionalFields,
    Iec61850ReportControlCandidate,
    Iec61850ReportControlReadResult,
    Iec61850ReportControlRef,
    Iec61850ReportControlState,
    Iec61850ReportEvent,
    Iec61850ReportEventValue,
    Iec61850ReportKind,
    Iec61850ReportObservationDiagnostic,
    Iec61850ReportObservationResult,
    Iec61850ReportReason,
    Iec61850ReportRuntimeError,
    Iec61850ReportRuntimeEvent,
    Iec61850ReportSubscriptionPlan,
    Iec61850ReportSubscriptionPlanDevice,
    Iec61850ReportSubscriptionPlanDiagnostic,
    Iec61850ReportSubscriptionPlanReport,
    Iec61850ReportSubscriptionPlanSignal,
    Iec61850ReportSubscriptionRunReportResult,
    Iec61850ReportSubscriptionRunResult,
    Iec61850RuntimeMode,
    Iec61850RuntimeStatus,
    Iec61850RuntimeTriggerOptions,
    Iec61850SelectedSignal,
    Iec61850SignalObservation,
    Iec61850SimulatorSubscriptionRunResult,
    Iec61850UnselectedReportValue,
)


class UnitLabMmsRuntimeError(Iec61850ReportRuntimeError):
    pass


@dataclass(frozen=True, slots=True)
class UnitLabMmsTransportExchange:
    request: bytes
    response: bytes


class UnitLabMmsScriptedTransport:
    def __init__(self, responses: Sequence[bytes] = ()) -> None:
        self._responses = [bytes(response) for response in responses]
        self._sent_payloads: list[bytes] = []
        self._closed = False

    def send(self, payload: bytes) -> bytes:
        if self._closed:
            raise UnitLabMmsRuntimeError('TRANSPORT_CLOSED', 'UnitLab MMS transport is already closed.')
        payload_bytes = bytes(payload)
        self._sent_payloads.append(payload_bytes)
        if not self._responses:
            raise UnitLabMmsRuntimeError('TRANSPORT_RESPONSE_UNAVAILABLE', 'UnitLab MMS scripted transport has no queued response.')
        return self._responses.pop(0)

    def close(self) -> None:
        self._closed = True

    def sent_payloads(self) -> tuple[bytes, ...]:
        return tuple(self._sent_payloads)


class UnitLabMmsRecordedTransport:
    def __init__(self, transport: UnitLabMmsTransport) -> None:
        self._transport = transport
        self._transcript: list[UnitLabMmsTransportExchange] = []

    def send(self, payload: bytes) -> bytes:
        response = self._transport.send(payload)
        self._transcript.append(UnitLabMmsTransportExchange(request=bytes(payload), response=bytes(response)))
        return response

    def close(self) -> None:
        self._transport.close()

    def transcript(self) -> tuple[UnitLabMmsTransportExchange, ...]:
        return tuple(self._transcript)


@dataclass(frozen=True, slots=True)
class UnitLabMmsAssociationState:
    session_id: str
    endpoint_id: str
    opened: bool
    released: bool
    aborted: bool
    abort_code: str | None = None
    abort_message: str | None = None


@runtime_checkable
class UnitLabMmsAssociation(Protocol):
    def open(self) -> UnitLabMmsAssociationState: ...
    def release(self) -> UnitLabMmsAssociationState: ...
    def abort(self, code: str, message: str) -> UnitLabMmsAssociationState: ...
    def state(self) -> UnitLabMmsAssociationState: ...


class UnitLabMmsInMemoryAssociation:
    def __init__(self, *, session_id: str, endpoint: Iec61850DeviceEndpoint, transport: UnitLabMmsTransport) -> None:
        self._session_id = session_id
        self._endpoint = endpoint
        self._transport = transport
        self._state = UnitLabMmsAssociationState(
            session_id=session_id,
            endpoint_id=endpoint.id,
            opened=False,
            released=False,
            aborted=False,
        )

    def open(self) -> UnitLabMmsAssociationState:
        if self._state.opened:
            raise UnitLabMmsRuntimeError('ASSOCIATION_ALREADY_OPEN', 'UnitLab MMS association is already open.')
        if self._state.released or self._state.aborted:
            raise UnitLabMmsRuntimeError('ASSOCIATION_ALREADY_CLOSED', 'UnitLab MMS association has already been closed.')
        self._state = UnitLabMmsAssociationState(
            session_id=self._session_id,
            endpoint_id=self._endpoint.id,
            opened=True,
            released=False,
            aborted=False,
        )
        return self.state()

    def release(self) -> UnitLabMmsAssociationState:
        if not self._state.opened or self._state.released or self._state.aborted:
            raise UnitLabMmsRuntimeError('ASSOCIATION_NOT_OPEN', 'UnitLab MMS association is not open.')
        self._transport.close()
        self._state = UnitLabMmsAssociationState(
            session_id=self._session_id,
            endpoint_id=self._endpoint.id,
            opened=False,
            released=True,
            aborted=False,
        )
        return self.state()

    def abort(self, code: str, message: str) -> UnitLabMmsAssociationState:
        self._transport.close()
        self._state = UnitLabMmsAssociationState(
            session_id=self._session_id,
            endpoint_id=self._endpoint.id,
            opened=False,
            released=False,
            aborted=True,
            abort_code=code,
            abort_message=message,
        )
        return self.state()

    def state(self) -> UnitLabMmsAssociationState:
        return self._state


@dataclass(frozen=True, slots=True)
class UnitLabMmsNamedVariable:
    reference: str
    value: bool | int | float | str | None
    writable: bool = False
    data_type: str | None = None


@dataclass(frozen=True, slots=True)
class UnitLabMmsNamedVariableState:
    reference: str
    value: bool | int | float | str | None
    writable: bool
    data_type: str | None
    read_count: int = 0
    write_count: int = 0


@runtime_checkable
class UnitLabMmsNamedVariableAccess(Protocol):
    def read(self, reference: str) -> UnitLabMmsNamedVariableState: ...
    def write(self, reference: str, value: bool | int | float | str | None) -> UnitLabMmsNamedVariableState: ...
    def snapshot(self) -> tuple[UnitLabMmsNamedVariableState, ...]: ...


class UnitLabMmsInMemoryNamedVariableAccess:
    def __init__(self, *, association: UnitLabMmsAssociation, variables: Sequence[UnitLabMmsNamedVariable] = ()) -> None:
        self._association = association
        self._variables = {
            variable.reference: UnitLabMmsNamedVariableState(
                reference=variable.reference,
                value=variable.value,
                writable=variable.writable,
                data_type=variable.data_type,
            )
            for variable in variables
        }

    def read(self, reference: str) -> UnitLabMmsNamedVariableState:
        self._require_open_association()
        state = self._require_variable(reference)
        updated = UnitLabMmsNamedVariableState(
            reference=state.reference,
            value=state.value,
            writable=state.writable,
            data_type=state.data_type,
            read_count=state.read_count + 1,
            write_count=state.write_count,
        )
        self._variables[reference] = updated
        return updated

    def write(self, reference: str, value: bool | int | float | str | None) -> UnitLabMmsNamedVariableState:
        self._require_open_association()
        state = self._require_variable(reference)
        if not state.writable:
            raise UnitLabMmsRuntimeError('NAMED_VARIABLE_READ_ONLY', f'Named variable "{reference}" is read-only.')
        if state.value is not None and value is not None and type(state.value) is not type(value):
            raise UnitLabMmsRuntimeError('NAMED_VARIABLE_TYPE_MISMATCH', f'Named variable "{reference}" does not accept value type {type(value).__name__}.')
        updated = UnitLabMmsNamedVariableState(
            reference=state.reference,
            value=value,
            writable=state.writable,
            data_type=state.data_type,
            read_count=state.read_count,
            write_count=state.write_count + 1,
        )
        self._variables[reference] = updated
        return updated

    def snapshot(self) -> tuple[UnitLabMmsNamedVariableState, ...]:
        self._require_open_association()
        return tuple(self._variables[reference] for reference in sorted(self._variables))

    def _require_open_association(self) -> None:
        state = self._association.state()
        if not state.opened or state.released or state.aborted:
            raise UnitLabMmsRuntimeError('ASSOCIATION_NOT_OPEN', 'UnitLab MMS association is not open.')

    def _require_variable(self, reference: str) -> UnitLabMmsNamedVariableState:
        variable = self._variables.get(reference)
        if variable is None:
            raise UnitLabMmsRuntimeError('NAMED_VARIABLE_NOT_FOUND', f'Named variable "{reference}" is not available.')
        return variable


@runtime_checkable
class UnitLabMmsTransport(Protocol):
    def send(self, payload: bytes) -> bytes: ...
    def close(self) -> None: ...


@runtime_checkable
class UnitLabMmsSession(Protocol):
    def read_report_control(self, reference: Iec61850ReportControlRef) -> Iec61850ReportControlState: ...
    def reserve_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState: ...
    def release_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState: ...
    def enable_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState: ...
    def disable_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState: ...
    def send_general_interrogation(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportEvent: ...
    def disconnect(self) -> None: ...


UnitLabMmsReportControl = UnitLabMmsSession


@runtime_checkable
class UnitLabMmsRuntimeAdapter(Protocol):
    def connect(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidates: Sequence[Iec61850ReportControlCandidate],
    ) -> UnitLabMmsSession: ...


__all__ = [
    "UnitLabMmsAssociation",
    "UnitLabMmsAssociationState",
    "UnitLabMmsInMemoryAssociation",
    "UnitLabMmsInMemoryNamedVariableAccess",
    "UnitLabMmsNamedVariable",
    "UnitLabMmsNamedVariableAccess",
    "UnitLabMmsNamedVariableState",
    "UnitLabMmsRecordedTransport",
    "UnitLabMmsRuntimeAdapter",
    "UnitLabMmsRuntimeError",
    "UnitLabMmsScriptedTransport",
    "UnitLabMmsSession",
    "UnitLabMmsTransport",
    "UnitLabMmsTransportExchange",
]
