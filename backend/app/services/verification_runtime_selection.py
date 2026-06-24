from __future__ import annotations

from contextlib import suppress
from dataclasses import dataclass
from datetime import datetime
from typing import Callable, Literal

from app.schemas.verification_schema import VerificationExecutionContextSchema
from app.services.iec61850.client_control import Iec61850ClientControlService
from app.services.iec61850.mms_adapter import Iec61850MmsEndpointCatalog
from app.services.iec61850.report_runtime import (
    Iec61850DeviceEndpoint,
    Iec61850ReportControlReadResult,
    Iec61850ReportControlRef,
    Iec61850ReportControlState,
    Iec61850ReportEvent,
    Iec61850ReportRuntimeAdapter,
    Iec61850ReportRuntimeError,
    Iec61850ReportSession,
    Iec61850ReportSubscriptionPlanDevice,
    Iec61850ReportControlCandidate,
    Iec61850RuntimeStatus,
    build_simulator_endpoint_for_plan_device,
    create_iec61850_simulator_adapter,
    compare_report_control_state,
    report_control_key,
    to_report_control_ref,
)


VerificationRuntimeMode = Literal["simulator", "mms"]
VerificationRuntimeTransportSource = Literal["simulator", "explicit_request", "settings_catalog", "loaded_scd", "validation_override", "signal_list_fallback", "unavailable"]
VerificationRuntimeModelSource = Literal["simulator", "loaded_scd", "discovery_fallback"]


@dataclass(frozen=True, slots=True)
class VerificationRuntimeSelection:
    runtime_mode: VerificationRuntimeMode
    adapter: Iec61850ReportRuntimeAdapter
    endpoint_for_device: Callable[[Iec61850ReportSubscriptionPlanDevice], Iec61850DeviceEndpoint]
    transport_source: VerificationRuntimeTransportSource
    model_source: VerificationRuntimeModelSource

    @property
    def runtime_source(self) -> VerificationRuntimeTransportSource:
        return self.transport_source


class _ClientControlMmsRuntimeAdapter:
    def __init__(
        self,
        *,
        client_id: str = "unitlab-backend-simulator",
        control_service_factory: Callable[..., Iec61850ClientControlService] = Iec61850ClientControlService,
    ) -> None:
        self._client_id = client_id
        self._control_service_factory = control_service_factory

    def connect(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidates: list[Iec61850ReportControlCandidate] | tuple[Iec61850ReportControlCandidate, ...],
    ) -> Iec61850ReportSession:
        if endpoint.mode.name != "MMS":
            raise Iec61850ReportRuntimeError(
                "UNSUPPORTED_ENDPOINT_MODE",
                "IEC 61850 MMS adapter only accepts MMS endpoints.",
            )
        return _ClientControlMmsSession(
            control_service_factory=self._control_service_factory,
            candidates=tuple(candidates),
            endpoint=endpoint,
            session_id=session_id,
            client_id=self._client_id,
        )


class _ClientControlMmsSession:
    def __init__(
        self,
        *,
        control_service_factory: Callable[..., Iec61850ClientControlService],
        candidates: tuple[Iec61850ReportControlCandidate, ...],
        endpoint: Iec61850DeviceEndpoint,
        session_id: str,
        client_id: str,
    ) -> None:
        self._control_service_factory = control_service_factory
        self._candidates = candidates
        self._endpoint = endpoint
        self._session_id = session_id
        self._client_id = client_id
        self._candidate_states: dict[str, dict[str, bool]] = {}
        self._control_services: dict[str, Iec61850ClientControlService] = {}

    def read_report_control(self, reference: Iec61850ReportControlRef) -> Iec61850ReportControlReadResult:
        candidate = self._candidate_for_reference(reference)
        control_service = self._control_service_for_candidate(candidate)
        control_service.discover_ied()
        state = self._build_state(candidate, runtime_status="read")
        return Iec61850ReportControlReadResult(
            endpoint=self._endpoint,
            candidate_id=candidate.id,
            state=state,
            diagnostics=compare_report_control_state(candidate, state),
        )

    def reserve_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        candidate = self._candidate_for_reference(reference)
        self._candidate_state(candidate)["reserved"] = True
        return self._build_state(candidate, runtime_status="reserved", reserved_by=client_id)

    def release_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        candidate = self._candidate_for_reference(reference)
        state = self._candidate_state(candidate)
        state["reserved"] = False
        state["enabled"] = False
        return self._build_state(candidate, runtime_status="released")

    def enable_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        candidate = self._candidate_for_reference(reference)
        control_service = self._control_service_for_candidate(candidate)
        control_service.enable_reporting()
        state = self._candidate_state(candidate)
        state["enabled"] = True
        state["opened"] = True
        return self._build_state(candidate, runtime_status="enabled", enabled=True, reserved_by=client_id, owner=client_id)

    def disable_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        candidate = self._candidate_for_reference(reference)
        self._candidate_state(candidate)["enabled"] = False
        return self._build_state(candidate, runtime_status="disabled")

    def send_general_interrogation(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportEvent:
        candidate = self._candidate_for_reference(reference)
        control_service = self._control_service_for_candidate(candidate)
        control_service.send_general_interrogation()
        snapshot = control_service.snapshot()
        report = snapshot.last_report
        if report is None:
            raise Iec61850ReportRuntimeError(
                "MMS_REPORT_NOT_OBSERVED",
                "IEC 61850 MMS client did not surface a report event.",
            )
        state = self._candidate_state(candidate)
        state["enabled"] = True
        state["opened"] = True
        return report

    def disconnect(self) -> None:
        for control_service in self._control_services.values():
            with suppress(Exception):
                control_service.close_ied()
        self._control_services.clear()
        self._candidate_states.clear()

    def _candidate_for_reference(self, reference: Iec61850ReportControlRef) -> Iec61850ReportControlCandidate:
        for candidate in self._candidates:
            if to_report_control_ref(candidate) == reference:
                return candidate
        raise Iec61850ReportRuntimeError(
            "MMS_REPORT_CONTROL_MISMATCH",
            "IEC 61850 MMS adapter was asked to operate on an unexpected report control.",
        )

    def _control_service_for_candidate(self, candidate: Iec61850ReportControlCandidate) -> Iec61850ClientControlService:
        key = report_control_key(to_report_control_ref(candidate))
        service = self._control_services.get(key)
        if service is None:
            service = self._control_service_factory(
                session_id=self._session_id,
                client_id=self._client_id,
                endpoint=self._endpoint,
                candidate=candidate,
            )
            self._control_services[key] = service
        return service

    def _candidate_state(self, candidate: Iec61850ReportControlCandidate) -> dict[str, bool]:
        key = report_control_key(to_report_control_ref(candidate))
        return self._candidate_states.setdefault(key, {"opened": False, "reserved": False, "enabled": False})

    def _build_state(
        self,
        candidate: Iec61850ReportControlCandidate,
        *,
        runtime_status: str,
        enabled: bool | None = None,
        reserved_by: str | None = None,
        owner: str | None = None,
    ) -> Iec61850ReportControlState:
        state = self._candidate_state(candidate)
        status = {
            "read": Iec61850RuntimeStatus.READ,
            "reserved": Iec61850RuntimeStatus.RESERVED,
            "enabled": Iec61850RuntimeStatus.ENABLED,
            "disabled": Iec61850RuntimeStatus.DISABLED,
            "released": Iec61850RuntimeStatus.RELEASED,
        }.get(runtime_status, Iec61850RuntimeStatus.CONNECTED)
        return Iec61850ReportControlState(
            reference=to_report_control_ref(candidate),
            runtime_status=status,
            rpt_id=candidate.rpt_id,
            data_set_ref=candidate.data_set_ref,
            conf_rev=candidate.conf_rev,
            indexed=candidate.indexed,
            buffer_time_ms=candidate.buffer_time_ms,
            integrity_period_ms=candidate.integrity_period_ms,
            trigger_options=candidate.trigger_options,
            optional_fields=candidate.optional_fields,
            signal_count=candidate.signal_count,
            enabled=state["enabled"] if enabled is None else enabled,
            reserved_by=reserved_by,
            owner=owner,
            sequence_number=0,
            gi_in_progress=False,
        )


def resolve_verification_runtime(
    *,
    execution_context: VerificationExecutionContextSchema,
    now: Callable[[], datetime] | None = None,
    endpoint_catalog: Iec61850MmsEndpointCatalog | None = None,
    transport_source: VerificationRuntimeTransportSource | None = None,
    model_source: VerificationRuntimeModelSource | None = None,
    transport_override_host: str | None = None,
    transport_override_port: int | None = None,
    simulator_endpoint_for_device: Callable[[Iec61850ReportSubscriptionPlanDevice], Iec61850DeviceEndpoint] = build_simulator_endpoint_for_plan_device,
    mms_control_service_factory: Callable[..., Iec61850ClientControlService] = Iec61850ClientControlService,
) -> VerificationRuntimeSelection:
    runtime_mode = _normalize_runtime_mode(execution_context.runtime_version)
    if runtime_mode == "mms":
        if endpoint_catalog is None:
            raise ValueError("MMS runtime requires an endpoint catalog.")
        if transport_override_host is not None and transport_override_host.strip() or (transport_override_port is not None and transport_override_port > 0):
            override_host = transport_override_host.strip() if transport_override_host is not None and transport_override_host.strip() else None
            override_port = transport_override_port if transport_override_port is not None and transport_override_port > 0 else None

            def _endpoint_for_device_with_override(device: Iec61850ReportSubscriptionPlanDevice) -> Iec61850DeviceEndpoint:
                endpoint, _notes = endpoint_catalog.resolve_transport_endpoint(
                    ied_name=device.ied_name,
                    access_point_name=device.access_point_name,
                    requested_host=override_host,
                    requested_port=override_port,
                )
                return endpoint

            endpoint_for_device = _endpoint_for_device_with_override
            resolved_transport_source: VerificationRuntimeTransportSource = "validation_override"
        else:
            endpoint_for_device = endpoint_catalog.endpoint_for_plan_device
            resolved_transport_source = transport_source or "explicit_request"
        return VerificationRuntimeSelection(
            runtime_mode="mms",
            adapter=_ClientControlMmsRuntimeAdapter(control_service_factory=mms_control_service_factory),
            endpoint_for_device=endpoint_for_device,
            transport_source=resolved_transport_source,
            model_source=model_source or "discovery_fallback",
        )

    return VerificationRuntimeSelection(
        runtime_mode="simulator",
        adapter=create_iec61850_simulator_adapter(now=now),
        endpoint_for_device=simulator_endpoint_for_device,
        transport_source="simulator",
        model_source="simulator",
    )


def _normalize_runtime_mode(runtime_version: str | None) -> VerificationRuntimeMode:
    value = (runtime_version or "").strip().lower()
    if value in {"mms", "live", "live-mms", "real-mms"}:
        return "mms"
    return "simulator"
