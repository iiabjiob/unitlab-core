from __future__ import annotations

from dataclasses import dataclass
import re
from threading import RLock
from typing import Callable

from app.services.iec61850.client_control import Iec61850ClientControlService
from app.services.iec61850.report_runtime import (
    Iec61850DataSetMember,
    Iec61850DeviceEndpoint,
    Iec61850OptionalFields,
    Iec61850ReportControlCandidate,
    Iec61850ReportKind,
    Iec61850RuntimeMode,
    Iec61850RuntimeTriggerOptions,
)


@dataclass(frozen=True, slots=True)
class ExternalIedManualReportRequest:
    workspace_id: int
    endpoint: str
    report_reference: str
    report_name: str | None = None
    report_kind: str | None = None
    dataset_reference: str | None = None


@dataclass(frozen=True, slots=True)
class ExternalIedManualReportResult:
    workspace_id: int
    endpoint: str
    report_reference: str
    enabled: bool
    status: str
    message: str | None = None


class ExternalIedManualReportControlService:
    def __init__(
        self,
        *,
        control_service_factory: Callable[..., Iec61850ClientControlService] = Iec61850ClientControlService,
    ) -> None:
        self._control_service_factory = control_service_factory
        self._sessions: dict[str, Iec61850ClientControlService] = {}
        self._lock = RLock()

    def set_report_enabled(
        self,
        request: ExternalIedManualReportRequest,
        *,
        enabled: bool,
    ) -> ExternalIedManualReportResult:
        endpoint = _parse_endpoint(request.endpoint)
        candidate = _candidate_from_report_request(request)
        session_key = _session_key(request.workspace_id, endpoint, candidate.id)
        with self._lock:
            if enabled:
                service = self._sessions.get(session_key)
                if service is None:
                    service = self._control_service_factory(
                        session_id=f"external-ied-manual:{request.workspace_id}:{endpoint.host}:{endpoint.port}:{len(self._sessions)}",
                        client_id="unitlab-manual-ied-inspector",
                        endpoint=endpoint,
                        candidate=candidate,
                        available_candidates=(candidate,),
                    )
                    self._sessions[session_key] = service
                snapshot = service.enable_reporting()
                return ExternalIedManualReportResult(
                    workspace_id=request.workspace_id,
                    endpoint=request.endpoint,
                    report_reference=request.report_reference,
                    enabled=True,
                    status=str(snapshot.last_state.runtime_status.value if snapshot.last_state else "enabled"),
                    message=snapshot.last_diagnostic.message if snapshot.last_diagnostic else None,
                )

            service = self._sessions.pop(session_key, None)
            if service is not None:
                service.close_ied()
            return ExternalIedManualReportResult(
                workspace_id=request.workspace_id,
                endpoint=request.endpoint,
                report_reference=request.report_reference,
                enabled=False,
                status="disabled",
            )


def _parse_endpoint(endpoint: str) -> Iec61850DeviceEndpoint:
    host, separator, raw_port = endpoint.rpartition(":")
    if separator != ":" or not host.strip():
        raise ValueError("External IED endpoint must be formatted as host:port.")
    try:
        port = int(raw_port)
    except ValueError as exc:
        raise ValueError("External IED endpoint port must be a number.") from exc
    if port < 1 or port > 65535:
        raise ValueError("External IED endpoint port is out of range.")
    return Iec61850DeviceEndpoint(
        id=f"external-ied:{host.strip()}:{port}",
        mode=Iec61850RuntimeMode.MMS,
        ied_name="",
        access_point_name="AP1",
        host=host.strip(),
        port=port,
    )


def _candidate_from_report_request(request: ExternalIedManualReportRequest) -> Iec61850ReportControlCandidate:
    parsed = _parse_report_reference(request.report_reference, request.report_kind)
    report_name = (request.report_name or parsed.report_name).strip()
    dataset_reference = (request.dataset_reference or "").strip() or None
    return Iec61850ReportControlCandidate(
        id=f"{parsed.domain}:{parsed.item}",
        ied_name="",
        access_point_name="AP1",
        logical_device_inst=parsed.domain,
        logical_node_name=parsed.logical_node,
        report_control_name=report_name,
        report_kind=parsed.report_kind,
        rpt_id=None,
        data_set_ref=dataset_reference,
        conf_rev=None,
        indexed=None,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(
            data_change=True,
            quality_change=True,
            data_update=True,
            periodic=False,
            general_interrogation=True,
        ),
        optional_fields=Iec61850OptionalFields(),
        signals=(Iec61850DataSetMember(reference=dataset_reference or parsed.domain, fc=None),),
    )


@dataclass(frozen=True, slots=True)
class _ParsedReportReference:
    domain: str
    item: str
    logical_node: str
    report_name: str
    report_kind: Iec61850ReportKind


def _parse_report_reference(reference: str, report_kind: str | None) -> _ParsedReportReference:
    text = reference.strip()
    if not text:
        raise ValueError("Report reference is required.")
    if ":" in text:
        domain, item = [part.strip() for part in text.split(":", 1)]
    elif " " in text and "$" in text:
        domain, item = [part.strip() for part in text.split(None, 1)]
    else:
        domain, item = _parse_path_report_reference(text, report_kind)
    if not domain or not item:
        raise ValueError("Report reference must include MMS domain and RCB item.")
    if "$BR$" not in item and "$RP$" not in item:
        raise ValueError("Report reference must identify a buffered or unbuffered RCB.")
    parts = item.split("$")
    logical_node = parts[0].strip() if parts else ""
    folder = parts[1].strip() if len(parts) > 1 else ""
    report_name = parts[2].strip() if len(parts) > 2 else ""
    if not logical_node or not report_name:
        raise ValueError("Report reference does not include LN and report control name.")
    kind = Iec61850ReportKind.UNBUFFERED if folder == "RP" else Iec61850ReportKind.BUFFERED
    return _ParsedReportReference(
        domain=domain,
        item=item,
        logical_node=logical_node,
        report_name=report_name,
        report_kind=kind,
    )


def _parse_path_report_reference(reference: str, report_kind: str | None) -> tuple[str, str]:
    value = reference.split("!", 1)[1] if "!" in reference else reference
    domain, separator, rest = value.partition("/")
    if separator != "/" or not domain.strip() or not rest.strip():
        raise ValueError("Report reference must be formatted as LD/LN.Report or LD:LN$BR$Report.")
    normalized = re.sub(r"[/\.]+", "$", rest.strip())
    parts = [part for part in normalized.split("$") if part]
    if len(parts) >= 3 and parts[1] in {"BR", "RP"}:
        return domain.strip(), "$".join(parts[:3])
    logical_node = parts[0] if parts else ""
    report_name = parts[-1] if parts else ""
    folder = "RP" if str(report_kind or "").lower().startswith("unbuffer") else "BR"
    if not logical_node or not report_name:
        raise ValueError("Report reference does not include LN and report control name.")
    return domain.strip(), f"{logical_node}${folder}${report_name}"


def _session_key(workspace_id: int, endpoint: Iec61850DeviceEndpoint, candidate_id: str) -> str:
    return f"{workspace_id}|{endpoint.host}:{endpoint.port}|{candidate_id}"


_manual_report_control_service = ExternalIedManualReportControlService()


def get_external_ied_manual_report_control_service() -> ExternalIedManualReportControlService:
    return _manual_report_control_service
