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

UnitLabMmsDataSetMember = Iec61850DataSetMember
UnitLabMmsDeviceEndpoint = Iec61850DeviceEndpoint
UnitLabMmsOptionalFields = Iec61850OptionalFields
UnitLabMmsReportControlCandidate = Iec61850ReportControlCandidate
UnitLabMmsReportControlReadResult = Iec61850ReportControlReadResult
UnitLabMmsReportControlRef = Iec61850ReportControlRef
UnitLabMmsReportControlState = Iec61850ReportControlState
UnitLabMmsReportEvent = Iec61850ReportEvent
UnitLabMmsReportEventValue = Iec61850ReportEventValue
UnitLabMmsReportKind = Iec61850ReportKind
UnitLabMmsReportObservationDiagnostic = Iec61850ReportObservationDiagnostic
UnitLabMmsReportObservationResult = Iec61850ReportObservationResult
UnitLabMmsReportReason = Iec61850ReportReason
UnitLabMmsRuntimeError = Iec61850ReportRuntimeError
UnitLabMmsRuntimeEvent = Iec61850ReportRuntimeEvent
UnitLabMmsRuntimeMode = Iec61850RuntimeMode
UnitLabMmsRuntimeStatus = Iec61850RuntimeStatus
UnitLabMmsRuntimeTriggerOptions = Iec61850RuntimeTriggerOptions
UnitLabMmsSelectedSignal = Iec61850SelectedSignal
UnitLabMmsSignalObservation = Iec61850SignalObservation
UnitLabMmsSimulatorSubscriptionRunResult = Iec61850SimulatorSubscriptionRunResult
UnitLabMmsUnselectedReportValue = Iec61850UnselectedReportValue


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


@runtime_checkable
class UnitLabMmsTransport(Protocol):
    def send(self, payload: bytes) -> bytes: ...
    def close(self) -> None: ...


@runtime_checkable
class UnitLabMmsSession(Protocol):
    def read_report_control(self, reference: UnitLabMmsReportControlRef) -> UnitLabMmsReportControlState: ...
    def reserve_report_control(self, reference: UnitLabMmsReportControlRef, client_id: str) -> UnitLabMmsReportControlState: ...
    def release_report_control(self, reference: UnitLabMmsReportControlRef, client_id: str) -> UnitLabMmsReportControlState: ...
    def enable_report_control(self, reference: UnitLabMmsReportControlRef, client_id: str) -> UnitLabMmsReportControlState: ...
    def disable_report_control(self, reference: UnitLabMmsReportControlRef, client_id: str) -> UnitLabMmsReportControlState: ...
    def send_general_interrogation(self, reference: UnitLabMmsReportControlRef, client_id: str) -> UnitLabMmsReportEvent: ...
    def disconnect(self) -> None: ...


UnitLabMmsReportControl = UnitLabMmsSession


@runtime_checkable
class UnitLabMmsRuntimeAdapter(Protocol):
    def connect(
        self,
        *,
        session_id: str,
        endpoint: UnitLabMmsDeviceEndpoint,
        candidates: Sequence[UnitLabMmsReportControlCandidate],
    ) -> UnitLabMmsSession: ...


__all__ = [
    "UnitLabMmsDataSetMember",
    "UnitLabMmsDeviceEndpoint",
    "UnitLabMmsOptionalFields",
    "UnitLabMmsReportControl",
    "UnitLabMmsReportControlCandidate",
    "UnitLabMmsReportControlReadResult",
    "UnitLabMmsReportControlRef",
    "UnitLabMmsReportControlState",
    "UnitLabMmsReportEvent",
    "UnitLabMmsReportEventValue",
    "UnitLabMmsReportKind",
    "UnitLabMmsReportObservationDiagnostic",
    "UnitLabMmsReportObservationResult",
    "UnitLabMmsReportReason",
    "UnitLabMmsRuntimeAdapter",
    "UnitLabMmsScriptedTransport",
    "UnitLabMmsTransportExchange",
    "UnitLabMmsRuntimeError",
    "UnitLabMmsRuntimeEvent",
    "UnitLabMmsRuntimeMode",
    "UnitLabMmsRuntimeStatus",
    "UnitLabMmsRuntimeTriggerOptions",
    "UnitLabMmsSelectedSignal",
    "UnitLabMmsSession",
    "UnitLabMmsSignalObservation",
    "UnitLabMmsSimulatorSubscriptionRunResult",
    "UnitLabMmsTransport",
    "UnitLabMmsUnselectedReportValue",
]
