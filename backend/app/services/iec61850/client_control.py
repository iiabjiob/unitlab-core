from __future__ import annotations

from dataclasses import dataclass, replace
from datetime import UTC, datetime
import os
import re
import socket
import subprocess
import time
from pathlib import Path
import select
from threading import RLock
from typing import Callable, Literal, Sequence
import tempfile
import xml.etree.ElementTree as ET

from app.core.config import get_settings

from .client_runtime import Iec61850MmsClientEvent, Iec61850MmsClientRuntime
from .ied_simulator_fixture import build_ied_simulator_fixture_from_subscription_plan
from .mms_adapter import Iec61850MmsEndpointCatalog
from .ied_simulator_process import (
    Iec61850IedSimulatorProcessHandle,
    build_ied_simulator_process_spec,
    start_ied_simulator_process,
    stop_ied_simulator_process,
    write_ied_simulator_fixture_file,
    write_ied_simulator_process_command,
)
from .report_runtime import (
    Iec61850DataSetMember,
    Iec61850DeviceEndpoint,
    Iec61850OptionalFields,
    Iec61850ReportControlCandidate,
    Iec61850ReportControlReadResult,
    Iec61850ReportControlState,
    Iec61850ReportEvent,
    Iec61850ReportEventValue,
    Iec61850ReportKind,
    Iec61850ReportReason,
    Iec61850ReportRuntimeError,
    Iec61850ReportSubscriptionPlan,
    Iec61850ReportSubscriptionPlanDevice,
    Iec61850ReportSubscriptionPlanReport,
    Iec61850ReportSubscriptionPlanSignal,
    Iec61850RuntimeMode,
    Iec61850RuntimeStatus,
    Iec61850RuntimeTriggerOptions,
    Iec61850SelectedSignal,
    create_iec61850_simulator_adapter,
    report_control_key,
    to_report_control_ref,
)


@dataclass(frozen=True, slots=True)
class Iec61850ClientControlDiagnostic:
    action: str
    code: str
    message: str


@dataclass(frozen=True, slots=True)
class _ExternalDiscoveredReportControl:
    index: int
    domain: str
    item: str


@dataclass(frozen=True, slots=True)
class Iec61850EndpointResolution:
    transport_source: Literal["simulator", "explicit_request"]
    model_source: Literal["simulator", "scd-first", "discovery-fallback"]
    requested_host: str | None
    requested_port: int | None
    resolved_host: str | None
    resolved_port: int | None
    override_host: str | None
    override_port: int | None
    override_applied: bool
    scl_path: str | None
    notes: tuple[str, ...] = ()


_EXTERNAL_RCB_OPTFLDS_HEX = "067f80"
_EXTERNAL_RCB_TRGOPS_HEX = "0274"
_EXTERNAL_MMS_CLIENT_STARTUP_TIMEOUT_SECONDS = 1.5
_EXTERNAL_MMS_CLIENT_DISCOVER_TIMEOUT_SECONDS = 3.0
_EXTERNAL_MMS_CLIENT_REPORTING_TIMEOUT_SECONDS = 1.5
_EXTERNAL_MMS_CLIENT_DISCONNECT_TIMEOUT_SECONDS = 1.5
_EXTERNAL_MMS_ENDPOINT_PROBE_TIMEOUT_SECONDS = 0.35


@dataclass(frozen=True, slots=True)
class Iec61850ClientTargetRequest:
    mode: str
    host: str
    port: int
    ied_name: str = ""
    scl_path: str | None = None
    access_point_name: str = "AP1"
    selected_rcb_ref: str | None = None
    transport_override_host: str | None = None
    transport_override_port: int | None = None


def _candidate_is_unselected(candidate: Iec61850ReportControlCandidate) -> bool:
    return (
        candidate.logical_device_inst.strip() == ""
        or candidate.logical_node_name.strip() == ""
        or candidate.report_control_name.strip() == ""
    )


@dataclass(frozen=True, slots=True)
class Iec61850ClientControlSnapshot:
    session_id: str
    client_id: str
    session_open: bool
    endpoint: Iec61850DeviceEndpoint
    endpoint_resolution: Iec61850EndpointResolution
    candidate: Iec61850ReportControlCandidate
    last_read: Iec61850ReportControlReadResult | None
    last_discovery: dict | None
    last_state: Iec61850ReportControlState | None
    last_report: Iec61850ReportEvent | None
    last_plan: Iec61850ReportSubscriptionPlan | None
    transcript: tuple[Iec61850MmsClientEvent, ...]
    last_diagnostic: Iec61850ClientControlDiagnostic | None
    live_wire_open: bool
    live_wire_control_open: bool
    live_wire_endpoint: Iec61850DeviceEndpoint | None
    live_wire_last_frame_length: int | None
    live_wire_last_frame_hex: str | None
    live_wire_last_diagnostic: Iec61850ClientControlDiagnostic | None
    ui_state: dict


@dataclass(frozen=True, slots=True)
class _ExternalProbeUiState:
    phase: str
    runtime_status: str
    last_command: str
    command_accepted: bool


class Iec61850ClientControlService:
    def __init__(
        self,
        *,
        now: Callable[[], datetime] | None = None,
        session_id: str = "iec61850-client-test",
        client_id: str = "unitlab-test-client",
        endpoint: Iec61850DeviceEndpoint | None = None,
        candidate: Iec61850ReportControlCandidate | None = None,
        available_candidates: Sequence[Iec61850ReportControlCandidate] | None = None,
        endpoint_catalog: Iec61850MmsEndpointCatalog | None = None,
        target_scl_path: str | None = None,
        live_wire_binary_path: str | None = None,
        live_wire_service_host: str | None = None,
        live_wire_data_port: int | None = None,
    ) -> None:
        adapter = create_iec61850_simulator_adapter(now=now or _utc_now)
        settings = get_settings()
        self._runtime = Iec61850MmsClientRuntime(adapter)
        self._session_id = session_id
        self._client_id = client_id
        self._endpoint = endpoint or _default_endpoint()
        self._endpoint_resolution = _default_endpoint_resolution(self._endpoint)
        self._endpoint_catalog = endpoint_catalog
        self._candidate = candidate or _default_candidate()
        self._target_scl_path: str | None = target_scl_path.strip() if target_scl_path is not None and target_scl_path.strip() else None
        self._last_read: Iec61850ReportControlReadResult | None = None
        self._last_discovery: dict | None = None
        self._last_state: Iec61850ReportControlState | None = None
        self._last_report: Iec61850ReportEvent | None = None
        self._last_plan: Iec61850ReportSubscriptionPlan | None = None
        self._last_diagnostic: Iec61850ClientControlDiagnostic | None = None
        self._session_open = False
        if live_wire_binary_path is not None:
            self._live_wire_binary_path = live_wire_binary_path.strip() or None
        else:
            self._live_wire_binary_path = getattr(settings, "iec61850_ied_live_wire_binary_path", None) or None
        self._live_wire_service_host = live_wire_service_host or getattr(settings, "iec61850_ied_live_wire_host", "iec61850-ied")
        self._live_wire_data_port = live_wire_data_port if live_wire_data_port is not None else int(getattr(settings, "iec61850_ied_live_wire_port", 12447))
        if available_candidates:
            self._available_candidates = tuple(available_candidates)
        elif candidate is not None:
            self._available_candidates = (candidate,)
        else:
            self._available_candidates = (_default_candidate(),)
        self._live_wire_process: Iec61850IedSimulatorProcessHandle | None = None
        self._live_wire_fixture_dir: tempfile.TemporaryDirectory[str] | None = None
        self._live_wire_endpoint: Iec61850DeviceEndpoint | None = None
        self._transcript_wire_endpoint_id: str | None = None
        self._live_wire_last_frame: bytes | None = None
        self._live_wire_last_diagnostic: Iec61850ClientControlDiagnostic | None = None
        self._external_mms_process: subprocess.Popen[str] | None = None
        self._external_mms_stdout_buffer = bytearray()
        self._external_discovered_rcbs: list[_ExternalDiscoveredReportControl] = []
        self._external_discover_summary_seen = False
        self._external_live_discovery: dict | None = None
        self._pending_external_report_entries: list[dict[str, str]] = []
        self._current_external_report_values: dict[str, Iec61850ReportEventValue] = {}
        self._lock = RLock()

    @property
    def session_id(self) -> str:
        return self._session_id

    @property
    def client_id(self) -> str:
        return self._client_id

    def snapshot(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            self._drain_external_mms_process_stdout(timeout_seconds=0.0)
            transcript = self._project_transcript(self._runtime.transcript())
            live_wire_open = self._live_wire_process is not None
            live_wire_control_open = self._live_wire_process is not None and self._live_wire_process.process.stdin is not None
            live_wire_last_frame_length = len(self._live_wire_last_frame) if self._live_wire_last_frame is not None else None
            live_wire_last_frame_hex = self._live_wire_last_frame.hex() if self._live_wire_last_frame is not None else None
            ui_state = _build_ui_state(
                session_id=self._session_id,
                client_id=self._client_id,
                session_open=self._session_open,
                endpoint=self._endpoint,
                endpoint_resolution=self._endpoint_resolution,
                candidate=self._candidate,
                available_candidates=self._available_candidates,
                last_discovery=self._last_discovery,
                last_state=self._last_state,
                last_report=self._last_report,
                current_report_values=tuple(self._current_external_report_values.values()),
                last_diagnostic=self._last_diagnostic,
                live_wire_open=live_wire_open,
                live_wire_control_open=live_wire_control_open,
                live_wire_endpoint=self._live_wire_endpoint,
                live_wire_last_frame_length=live_wire_last_frame_length,
                live_wire_last_diagnostic=self._live_wire_last_diagnostic,
                transcript=transcript,
            )
            return Iec61850ClientControlSnapshot(
                session_id=self._session_id,
                client_id=self._client_id,
                session_open=self._session_open,
                endpoint=self._endpoint,
                endpoint_resolution=self._endpoint_resolution,
                candidate=self._candidate,
                last_read=self._last_read,
                last_discovery=self._last_discovery,
                last_state=self._last_state,
                last_report=self._last_report,
                last_plan=self._last_plan,
                transcript=transcript,
                last_diagnostic=self._last_diagnostic,
                live_wire_open=live_wire_open,
                live_wire_control_open=live_wire_control_open,
                live_wire_endpoint=self._live_wire_endpoint,
                live_wire_last_frame_length=live_wire_last_frame_length,
                live_wire_last_frame_hex=live_wire_last_frame_hex,
                live_wire_last_diagnostic=self._live_wire_last_diagnostic,
                ui_state=ui_state,
            )

    def configure_target(self, request: Iec61850ClientTargetRequest) -> Iec61850ClientControlSnapshot:
        with self._lock:
            self._reset_runtime_state_for_target_change()
            endpoint, candidate, available_candidates, endpoint_resolution = _build_target_endpoint_and_candidate(
                request,
                endpoint_catalog=self._endpoint_catalog,
            )
            self._endpoint = endpoint
            self._endpoint_resolution = endpoint_resolution
            self._candidate = candidate
            self._available_candidates = available_candidates
            self._target_scl_path = request.scl_path.strip() if request.scl_path is not None and request.scl_path.strip() else None
            if endpoint.mode == Iec61850RuntimeMode.MMS and endpoint.host is not None:
                self._live_wire_service_host = endpoint.host
                self._live_wire_data_port = endpoint.port
            self._runtime._append_event(
                kind="target-configured",
                session_id=self._session_id,
                endpoint_id=endpoint.id,
                candidate_id=candidate.id,
                report_control_name=candidate.report_control_name,
                client_id=self._client_id,
                outcome="configured",
                message=f"Configured IEC 61850 client target {_endpoint_label(endpoint)}.",
            )
            return self.snapshot()

    def select_report_control(self, selected_rcb_ref: str) -> Iec61850ClientControlSnapshot:
        with self._lock:
            selected = _find_candidate_by_rcb_reference(
                candidates=self._available_candidates,
                selected_rcb_ref=selected_rcb_ref,
            )
            if selected is None:
                raise Iec61850ReportRuntimeError(
                    "CLIENT_REPORT_CONTROL_NOT_FOUND",
                    f"Selected IEC 61850 report control was not found: {selected_rcb_ref}.",
                )
            if selected == self._candidate:
                return self.snapshot()
            self._candidate = selected
            self._last_read = None
            self._last_discovery = None
            self._external_discovered_rcbs.clear()
            self._external_live_discovery = None
            self._last_state = None
            self._last_report = None
            self._last_plan = None
            self._last_diagnostic = None
            self._runtime._append_event(
                kind="report-control-select",
                session_id=self._session_id,
                endpoint_id=self._endpoint.id,
                candidate_id=selected.id,
                report_control_name=selected.report_control_name,
                client_id=self._client_id,
                outcome="selected",
                message=f"Selected IEC 61850 report control {selected.report_control_name}.",
            )
            return self.snapshot()

    def open_session(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run(
                "open-session",
                lambda: self._runtime.open_session(session_id=self._session_id, endpoint=self._endpoint, candidates=self._available_candidates),
                post=lambda _result: setattr(self, "_session_open", True),
            )

    def discover_ied(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            if self._endpoint.mode == Iec61850RuntimeMode.MMS:
                return self._run("external-discover-ied", self._discover_external_mms_ied)
            return self._run("discover-ied", self._discover_ied)

    def connect_ied(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            if self._endpoint.mode == Iec61850RuntimeMode.MMS:
                return self._run("external-connect-ied", self._ensure_external_mms_client_started)
            if self._session_open:
                self._runtime._append_event(
                    kind="ied-connect",
                    session_id=self._session_id,
                    endpoint_id=self._endpoint.id,
                    client_id=self._client_id,
                    outcome="already-connected",
                )
                return self.snapshot()
            return self._run(
                "connect-ied",
                lambda: self._runtime.open_session(session_id=self._session_id, endpoint=self._endpoint, candidates=self._available_candidates),
                post=lambda _result: setattr(self, "_session_open", True),
            )

    def close_session(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run("close-session", lambda: self._runtime.close_session(self._session_id), post=lambda _result: setattr(self, "_session_open", False))

    def disconnect_ied(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            if self._endpoint.mode == Iec61850RuntimeMode.MMS:
                return self._run("external-disconnect-ied", self._disconnect_external_mms_ied)
            if not self._session_open:
                self._runtime._append_event(
                    kind="ied-disconnect",
                    session_id=self._session_id,
                    endpoint_id=self._endpoint.id,
                    client_id=self._client_id,
                    outcome="already-disconnected",
                )
                return self.snapshot()
            return self._run("disconnect-ied", lambda: self._runtime.close_session(self._session_id), post=lambda _result: setattr(self, "_session_open", False))

    def close_ied(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            if self._external_mms_process is not None:
                self._stop_external_mms_client_process()
            if self._live_wire_process is not None:
                self._stop_live_wire_transport()
            if self._session_open:
                self._runtime.close_session(self._session_id)
            self._session_open = False
            self._last_read = None
            self._last_discovery = None
            self._external_live_discovery = None
            self._external_discover_summary_seen = False
            self._last_state = None
            self._last_report = None
            self._last_plan = None
            self._last_diagnostic = None
            self._current_external_report_values.clear()
            self._live_wire_last_frame = None
            self._live_wire_last_diagnostic = None
            self._runtime._append_event(
                kind="ied-close",
                session_id=self._session_id,
                endpoint_id=self._endpoint.id,
                client_id=self._client_id,
                outcome="removed-from-memory",
            )
            return self.snapshot()

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

    def enable_reporting(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            if self._endpoint.mode == Iec61850RuntimeMode.MMS:
                return self._run("external-rptena-precheck", self._enable_external_mms_reporting)
            return self._run("rptena", self._enable_reporting)

    def send_general_interrogation(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            if self._endpoint.mode == Iec61850RuntimeMode.MMS:
                return self._run("external-gi", self._send_external_mms_general_interrogation)
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
            self._transcript_wire_endpoint_id = None
            self._last_diagnostic = None
            return self.snapshot()

    def run_subscription_plan(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run(
                "run-subscription-plan",
                lambda: self._runtime.run_simulator_report_subscription_plan(plan=_build_subscription_plan(self._candidate), client_id=self._client_id),
                post=self._capture_subscription_run,
            )

    def start_live_wire_transport(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run("wire-start", self._start_live_wire_transport)

    def emit_live_wire_report(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run("wire-emit-report", self._emit_live_wire_report)

    def stop_live_wire_transport(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run("wire-stop", self._stop_live_wire_transport)

    def _start_live_wire_transport(self) -> None:
        if self._live_wire_process is not None:
            raise Iec61850ReportRuntimeError(
                "LIVE_WIRE_SESSION_EXISTS",
                "IEC 61850 live wire transport is already running.",
            )

        self._live_wire_last_frame = None
        self._live_wire_last_diagnostic = None
        self._start_live_wire_process_transport()

    def _discover_ied(self) -> None:
        if not self._session_open:
            self._runtime.open_session(session_id=self._session_id, endpoint=self._endpoint, candidates=self._available_candidates)
            self._session_open = True
        read_result = self._runtime.read_report_control(session_id=self._session_id, endpoint=self._endpoint, candidate=self._candidate)
        self._last_read = read_result
        self._last_state = read_result.state
        self._last_discovery = _build_discovery_structure(self._endpoint, self._candidate, read_result)
        self._runtime._append_event(
            kind="ied-discover",
            session_id=self._session_id,
            endpoint_id=self._endpoint.id,
            candidate_id=self._candidate.id,
            report_control_name=self._candidate.report_control_name,
            client_id=self._client_id,
            outcome="structure-ready",
            message="Discovered LD/LN/DataSet/ReportControl structure from the active in-memory IED.",
        )

    def _enable_reporting(self) -> None:
        reserved = self._runtime.reserve_report_control(session_id=self._session_id, candidate=self._candidate, client_id=self._client_id)
        enabled = self._runtime.enable_report_control(session_id=self._session_id, candidate=self._candidate, client_id=self._client_id)
        self._last_state = enabled if enabled is not None else reserved

    def _start_live_wire_process_transport(self) -> None:
        process_candidate = self._candidate
        process_ied_name = self._candidate.ied_name
        process_endpoint: Iec61850DeviceEndpoint | None = None
        if self._endpoint.mode == Iec61850RuntimeMode.MMS:
            process_candidate = _default_candidate()
            process_ied_name = process_candidate.ied_name
            process_endpoint = self._endpoint
        subscription_plan = _build_subscription_plan(process_candidate)
        fixture = build_ied_simulator_fixture_from_subscription_plan(subscription_plan)
        fixture_dir: tempfile.TemporaryDirectory[str] | None = None
        fixture_path = Path("/workspace/iec61850_ied/examples/single-report.fixture.json")
        if not fixture_path.is_file():
            fixture_dir = tempfile.TemporaryDirectory(prefix="unitlab-iec61850-wire-")
            fixture_path = Path(fixture_dir.name) / "wire-fixture.json"
            write_ied_simulator_fixture_file(fixture, fixture_path)
        spec = build_ied_simulator_process_spec(
            fixture=fixture,
            binary_path=self._live_wire_binary_path,
            fixture_path=fixture_path,
            ied_name=process_ied_name,
            bind_address=self._live_wire_service_host,
            port=self._live_wire_data_port,
            native_wire_client_start=True,
        )
        process_handle: Iec61850IedSimulatorProcessHandle | None = None
        try:
            process_handle = start_ied_simulator_process(spec)
            self._live_wire_process = process_handle
            self._live_wire_fixture_dir = fixture_dir
            self._live_wire_endpoint = process_endpoint or spec.endpoint
            self._transcript_wire_endpoint_id = self._live_wire_endpoint.id
            self._runtime._append_event(
                kind="wire-session-open",
                session_id=self._session_id,
                endpoint_id=self._live_wire_endpoint.id,
                client_id=self._client_id,
                outcome="connected",
            )
            self._runtime._append_event(
                kind="wire-associate",
                session_id=self._session_id,
                endpoint_id=self._live_wire_endpoint.id,
                client_id=self._client_id,
                outcome="associated",
            )
            self._live_wire_last_frame = None
            self._live_wire_last_diagnostic = None
            self._runtime._append_event(
                kind="wire-client-ready",
                session_id=self._session_id,
                endpoint_id=self._live_wire_endpoint.id,
                client_id=self._client_id,
                outcome="ready",
                code=None,
                message=None,
            )
        except Exception:
            if process_handle is not None:
                stop_ied_simulator_process(process_handle)
            if fixture_dir is not None:
                fixture_dir.cleanup()
            self._live_wire_process = None
            self._live_wire_fixture_dir = None
            self._live_wire_endpoint = None
            self._transcript_wire_endpoint_id = None
            raise

    def _emit_live_wire_report(self) -> None:
        if self._live_wire_endpoint is None:
            raise Iec61850ReportRuntimeError(
                "LIVE_WIRE_SESSION_NOT_OPEN",
                "IEC 61850 live wire transport is not open.",
            )
        if self._live_wire_process is None or self._live_wire_process.process.stdin is None:
            raise Iec61850ReportRuntimeError(
                "LIVE_WIRE_PROCESS_NOT_OPEN",
                "IEC 61850 live wire transport process is not open.",
            )
        self._drain_live_wire_process_stdout()
        write_ied_simulator_process_command(self._live_wire_process, "emit-report")
        frame = self._read_live_wire_process_frame_response("report")
        self._live_wire_last_frame = frame
        self._live_wire_last_diagnostic = None
        self._runtime._append_event(
            kind="wire-report-frame",
            session_id=self._session_id,
            endpoint_id=self._live_wire_endpoint.id,
            client_id=self._client_id,
            outcome="received",
            code=str(len(frame)),
            message=frame.hex(),
        )

    def _discover_external_mms_ied(self) -> None:
        self._ensure_external_mms_client_started()
        self._external_discovered_rcbs.clear()
        self._external_discover_summary_seen = False
        self._external_live_discovery = _empty_wire_discovery_structure(self._endpoint, self._build_external_discover_command(), source="live")
        self._write_external_mms_command(self._build_external_discover_command())
        try:
            self._drain_external_mms_process_stdout(timeout_seconds=_EXTERNAL_MMS_CLIENT_DISCOVER_TIMEOUT_SECONDS, stop_on="native-wire-client: state=ready", refresh_timeout_on_activity=True)
        except Iec61850ReportRuntimeError as exc:
            if exc.code != "EXTERNAL_MMS_CLIENT_COMMAND_FAILED" or not self._external_discover_summary_seen:
                raise
        self._last_discovery = self._external_live_discovery or _empty_wire_discovery_structure(self._endpoint, self._build_external_discover_command(), source="live")
        self._runtime._append_event(
            kind="external-ied-discover",
            session_id=self._session_id,
            endpoint_id=self._endpoint.id,
            candidate_id=self._candidate.id,
            report_control_name=self._candidate.report_control_name,
            client_id=self._client_id,
            outcome="accepted",
            message="Persistent external MMS client discovery completed.",
        )

    def _enable_external_mms_reporting(self) -> None:
        self._ensure_external_mms_client_started()
        for command in self._external_rcb_configuration_commands():
            self._write_external_mms_command(command)
            self._drain_external_mms_process_stdout(timeout_seconds=_EXTERNAL_MMS_CLIENT_REPORTING_TIMEOUT_SECONDS, stop_on="native-wire-client: state=ready")
        self._write_external_mms_command(self._external_rptena_command(True))
        self._drain_external_mms_process_stdout(timeout_seconds=_EXTERNAL_MMS_CLIENT_REPORTING_TIMEOUT_SECONDS, stop_on="native-wire-client: state=ready")
        self._last_state = self._external_state(Iec61850RuntimeStatus.ENABLED, enabled=True)
        self._runtime._append_event(
            kind="external-report-control-enable",
            session_id=self._session_id,
            endpoint_id=self._endpoint.id,
            candidate_id=self._candidate.id,
            report_control_name=self._candidate.report_control_name,
            client_id=self._client_id,
            outcome="accepted",
            message="Persistent external MMS client enabled RptEna.",
        )

    def _send_external_mms_general_interrogation(self) -> None:
        self._ensure_external_mms_client_started()
        self._write_external_mms_command(self._external_gi_command())
        self._drain_external_mms_process_stdout(timeout_seconds=_EXTERNAL_MMS_CLIENT_REPORTING_TIMEOUT_SECONDS, stop_on="native-wire-client: state=ready")
        if self._last_report is None:
            self._last_state = self._external_state(Iec61850RuntimeStatus.GI_PENDING, enabled=True, gi_in_progress=True)
        self._runtime._append_event(
            kind="external-report-control-gi",
            session_id=self._session_id,
            endpoint_id=self._endpoint.id,
            candidate_id=self._candidate.id,
            report_control_name=self._candidate.report_control_name,
            client_id=self._client_id,
            outcome="accepted",
            message="Persistent external MMS client requested GI.",
        )
        self._drain_external_mms_process_stdout(timeout_seconds=1.0)

    def _disconnect_external_mms_ied(self) -> None:
        if self._external_mms_process is not None:
            if self._last_state is not None and self._last_state.enabled:
                if self._selected_external_discovered_rcb_index() is None:
                    self._write_external_mms_command(self._external_rptena_command(False))
                    self._drain_external_mms_process_stdout(timeout_seconds=_EXTERNAL_MMS_CLIENT_DISCONNECT_TIMEOUT_SECONDS, stop_on="native-wire-client: state=ready")
            self._write_external_mms_command("disconnect")
            self._drain_external_mms_process_stdout(timeout_seconds=_EXTERNAL_MMS_CLIENT_DISCONNECT_TIMEOUT_SECONDS, stop_on="native-wire-client: state=stopped")
            self._stop_external_mms_client_process()
        self._session_open = False
        self._last_state = self._external_state(Iec61850RuntimeStatus.DISCONNECTED, enabled=False)
        self._runtime._append_event(
            kind="external-ied-disconnect",
            session_id=self._session_id,
            endpoint_id=self._endpoint.id,
            candidate_id=self._candidate.id,
            report_control_name=self._candidate.report_control_name,
            client_id=self._client_id,
            outcome="disconnected",
            message="Persistent external MMS client disconnected and cleaned up the selected RCB.",
        )

    def _build_external_discover_command(self) -> str:
        return "discover"

    def _external_rptena_command(self, value: bool) -> str:
        selected_index = self._selected_external_discovered_rcb_index()
        if value and selected_index is not None:
            return f"rptena {selected_index}"
        return _external_rcb_bool_command(self._candidate, "RptEna", value)

    def _external_gi_command(self) -> str:
        selected_index = self._selected_external_discovered_rcb_index()
        if selected_index is not None:
            return f"gi {selected_index}"
        return _external_rcb_bool_command(self._candidate, "GI", True)

    def _selected_external_discovered_rcb_index(self) -> int | None:
        discovered = self._selected_external_discovered_rcb()
        return discovered.index if discovered is not None else None

    def _selected_external_discovered_rcb(self) -> _ExternalDiscoveredReportControl | None:
        if _candidate_is_unselected(self._candidate):
            return None
        expected_domain = f"{self._candidate.ied_name}{self._candidate.logical_device_inst}"
        report_folder = "BR" if self._candidate.report_kind == Iec61850ReportKind.BUFFERED else "RP"
        expected_prefix = f"{self._candidate.logical_node_name}${report_folder}$"
        expected_name = self._candidate.report_control_name
        for discovered in self._external_discovered_rcbs:
            if discovered.domain != expected_domain or not discovered.item.startswith(expected_prefix):
                continue
            discovered_name = discovered.item[len(expected_prefix):]
            if discovered_name == expected_name:
                return discovered
            suffix = discovered_name[len(expected_name):]
            if discovered_name.startswith(expected_name) and suffix.isdigit():
                return discovered
        return None

    def _external_rcb_configuration_commands(self) -> tuple[str, str]:
        if _candidate_is_unselected(self._candidate):
            raise Iec61850ReportRuntimeError(
                "CLIENT_REPORT_CONTROL_NOT_SELECTED",
                "IEC 61850 report control must be discovered before RptEna or GI.",
            )
        discovered = self._selected_external_discovered_rcb()
        if discovered is not None:
            return (
                f"write-hex {discovered.domain} {discovered.item}$OptFlds 4 {_EXTERNAL_RCB_OPTFLDS_HEX}",
                f"write-hex {discovered.domain} {discovered.item}$TrgOps 4 {_EXTERNAL_RCB_TRGOPS_HEX}",
            )
        return (
            _external_rcb_hex_command(self._candidate, "OptFlds", _EXTERNAL_RCB_OPTFLDS_HEX),
            _external_rcb_hex_command(self._candidate, "TrgOps", _EXTERNAL_RCB_TRGOPS_HEX),
        )

    def _ensure_external_mms_client_started(self) -> None:
        if self._external_mms_process is not None and self._external_mms_process.poll() is None:
            self._session_open = True
            return
        if self._endpoint.host is None or not self._endpoint.host.strip():
            raise Iec61850ReportRuntimeError("EXTERNAL_MMS_HOST_REQUIRED", "IEC 61850 external MMS target host is required.")
        self._probe_external_mms_endpoint()
        command = [
            self._external_probe_binary_path(),
            "--bind",
            self._endpoint.host,
            "--port",
            str(self._endpoint.port),
            "--mms-client-start",
        ]
        if self._endpoint.ied_name.strip():
            command[1:1] = ["--ied", self._endpoint.ied_name]
        if self._target_scl_path is not None and self._target_scl_path.strip():
            command[1:1] = ["--scl", self._target_scl_path]
        try:
            process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
        except OSError as exc:
            raise Iec61850ReportRuntimeError("EXTERNAL_MMS_CLIENT_START_FAILED", f"IEC 61850 external MMS client could not be started: {exc}") from exc
        self._external_mms_process = process
        self._external_mms_stdout_buffer.clear()
        self._external_discovered_rcbs.clear()
        try:
            self._drain_external_mms_process_stdout(
                timeout_seconds=_EXTERNAL_MMS_CLIENT_STARTUP_TIMEOUT_SECONDS,
                stop_on="native-wire-client: state=ready",
            )
        except Exception:
            self._stop_external_mms_client_process()
            raise
        if process.poll() is not None:
            stderr = process.stderr.read() if process.stderr is not None else ""
            raise Iec61850ReportRuntimeError("EXTERNAL_MMS_CLIENT_EXITED", f"IEC 61850 external MMS client exited during startup: {stderr.strip()}")
        self._session_open = True
        self._last_state = self._external_state(Iec61850RuntimeStatus.CONNECTED, enabled=False)
        self._runtime._append_event(
            kind="external-session-open",
            session_id=self._session_id,
            endpoint_id=self._endpoint.id,
            candidate_id=self._candidate.id,
            report_control_name=self._candidate.report_control_name,
            client_id=self._client_id,
            outcome="associated",
            message="Persistent external MMS client associated with the target IED.",
        )

    def _probe_external_mms_endpoint(self) -> None:
        host = str(self._endpoint.host or "").strip()
        if not host:
            return
        try:
            with socket.create_connection((host, int(self._endpoint.port)), timeout=_EXTERNAL_MMS_ENDPOINT_PROBE_TIMEOUT_SECONDS):
                return
        except OSError as exc:
            raise Iec61850ReportRuntimeError(
                "EXTERNAL_MMS_ENDPOINT_UNREACHABLE",
                f"IEC 61850 endpoint {host}:{self._endpoint.port} is unreachable: {exc}",
            ) from exc

    def _write_external_mms_command(self, command: str) -> None:
        if self._external_mms_process is None or self._external_mms_process.stdin is None or self._external_mms_process.poll() is not None:
            raise Iec61850ReportRuntimeError("EXTERNAL_MMS_CLIENT_NOT_OPEN", "IEC 61850 external MMS client process is not open.")
        self._external_mms_process.stdin.write(command.rstrip("\n") + "\n")
        self._external_mms_process.stdin.flush()

    def _drain_external_mms_process_stdout(
        self,
        *,
        timeout_seconds: float,
        stop_on: str | None = None,
        require_stop: bool | None = None,
        refresh_timeout_on_activity: bool = False,
    ) -> None:
        process = self._external_mms_process
        if process is None:
            return
        if require_stop is None:
            require_stop = stop_on is not None
        deadline = time.monotonic() + max(timeout_seconds, 0.0)
        saw_stop = stop_on is None
        saw_failed = False
        while True:
            line = _read_process_stdout_line(process, timeout_deadline=deadline, line_buffer=self._external_mms_stdout_buffer)
            if line is None:
                if require_stop and not saw_stop:
                    returncode = process.poll()
                    if returncode is not None:
                        self._stop_external_mms_client_process()
                    stderr = ""
                    if returncode is not None and process.stderr is not None:
                        read = getattr(process.stderr, "read", None)
                        if callable(read):
                            stderr = str(read()).strip()
                    if stderr:
                        raise Iec61850ReportRuntimeError("EXTERNAL_MMS_CLIENT_COMMAND_FAILED", stderr)
                    if saw_failed:
                        try:
                            process.wait(timeout=0.25)
                        except subprocess.TimeoutExpired:
                            pass
                        if process.poll() is not None and process.stderr is not None:
                            read = getattr(process.stderr, "read", None)
                            if callable(read):
                                stderr = str(read()).strip()
                        if stderr:
                            raise Iec61850ReportRuntimeError("EXTERNAL_MMS_CLIENT_COMMAND_FAILED", stderr)
                        raise Iec61850ReportRuntimeError(
                            "EXTERNAL_MMS_CLIENT_COMMAND_FAILED",
                            "IEC 61850 external MMS client reported state=failed.",
                        )
                    if returncode is not None:
                        if returncode < 0:
                            raise Iec61850ReportRuntimeError(
                                "EXTERNAL_MMS_CLIENT_COMMAND_FAILED",
                                f"IEC 61850 external MMS client exited by signal {-returncode} before completion.",
                            )
                        raise Iec61850ReportRuntimeError(
                            "EXTERNAL_MMS_CLIENT_COMMAND_FAILED",
                            f"IEC 61850 external MMS client exited with code {returncode} before completion.",
                        )
                    raise Iec61850ReportRuntimeError("EXTERNAL_MMS_CLIENT_COMMAND_FAILED", "IEC 61850 external MMS client command ended before completion.")
                return
            stripped = line.strip()
            if not stripped:
                continue
            self._apply_external_mms_client_line(stripped)
            if refresh_timeout_on_activity and timeout_seconds > 0.0:
                deadline = time.monotonic() + timeout_seconds
            if stripped.startswith("native-wire-client: state=failed"):
                saw_failed = True
            if stop_on is not None and stripped.startswith(stop_on):
                saw_stop = True
                return

    def _apply_external_mms_client_line(self, line: str) -> None:
        if line.startswith("native-wire-client: subscription-summary "):
            fields = _parse_space_kv_line(line.removeprefix("native-wire-client: subscription-summary "))
            phase = fields.get("phase")
            if phase == "rptena":
                self._last_state = self._external_state(Iec61850RuntimeStatus.ENABLED, enabled=True)
            elif phase == "gi":
                if self._last_report is None:
                    self._last_state = self._external_state(Iec61850RuntimeStatus.GI_PENDING, enabled=True, gi_in_progress=True)
            elif phase in {"async-report", "report"}:
                self._last_report = self._external_report_event(fields)
                self._last_state = self._external_state(Iec61850RuntimeStatus.REPORTING, enabled=True, gi_in_progress=False)
        elif line.startswith("native-wire-client: report-entry "):
            self._pending_external_report_entries.append(_parse_space_kv_line(line.removeprefix("native-wire-client: report-entry ")))
        elif line.startswith("native-wire-client: discovered-logical-device["):
            self._apply_external_discovered_logical_device_line(line)
        elif line.startswith("native-wire-client: discovered-logical-node["):
            self._apply_external_discovered_logical_node_line(line)
        elif line.startswith("native-wire-client: discovered-dataset["):
            self._apply_external_discovered_dataset_line(line)
        elif line.startswith("native-wire-client: discovered-dataset-member["):
            self._apply_external_discovered_dataset_member_line(line)
        elif line.startswith("native-wire-client: discovered-rcb[") or line.startswith("native-wire-client: discovered-brcb["):
            self._apply_external_discovered_rcb_line(line)
        elif line.startswith("native-wire-client: discovered-rcb-attr["):
            self._apply_external_discovered_rcb_attr_line(line)
        elif line.startswith("native-wire-client: model-summary phase=discover"):
            self._external_discover_summary_seen = True
        elif line.startswith("native-wire-client: async-report"):
            self._last_report = self._external_report_event({})
            self._last_state = self._external_state(Iec61850RuntimeStatus.REPORTING, enabled=True, gi_in_progress=False)
        elif line.startswith("native-wire-client: state=failed"):
            self._session_open = False
            self._last_state = self._external_state(Iec61850RuntimeStatus.FAILED, enabled=False)
        elif line.startswith("native-wire-client: disconnected"):
            self._session_open = False
            self._last_state = self._external_state(Iec61850RuntimeStatus.DISCONNECTED, enabled=False)

    def _apply_external_discovered_rcb_line(self, line: str) -> None:
        if line.startswith("native-wire-client: discovered-rcb["):
            payload = line.removeprefix("native-wire-client: discovered-rcb[")
        else:
            payload = line.removeprefix("native-wire-client: discovered-brcb[")
        index_text, separator, fields_text = payload.partition("] ")
        if separator != "] ":
            return
        try:
            index = int(index_text)
        except ValueError:
            return
        fields = _parse_space_kv_line(fields_text)
        domain = fields.get("domain")
        item = fields.get("item")
        if not domain or not item:
            return
        discovered = _ExternalDiscoveredReportControl(index=index, domain=domain, item=item)
        self._external_discovered_rcbs = [rcb for rcb in self._external_discovered_rcbs if rcb.index != index]
        self._external_discovered_rcbs.append(discovered)
        discovery = self._ensure_external_live_discovery()
        report_controls = discovery.setdefault("reportControls", [])
        if not any(isinstance(item_, dict) and item_.get("domain") == domain and item_.get("item") == item for item_ in report_controls):
            name = item.rsplit("$", 1)[-1]
            base_name = name.rstrip("0123456789") or name
            data_set_ref = _first_live_discovery_dataset_ref(discovery, domain)
            report_controls.append({
                "id": f"{domain}:{item}",
                "domain": domain,
                "item": item,
                "name": base_name,
                "kind": "buffered" if "$BR$" in item else "unbuffered",
                "indexed": name != base_name,
                "dataSetRef": data_set_ref,
            })
            candidate = _candidate_from_live_discovered_rcb(
                endpoint=self._endpoint,
                discovered=discovered,
                report_control_name=base_name,
                data_set_ref=data_set_ref,
                discovery=discovery,
            )
            self._available_candidates = _replace_or_append_candidate(self._available_candidates, candidate)
            if _candidate_is_unselected(self._candidate) or self._candidate.report_control_name == base_name:
                self._candidate = candidate
            if not self._endpoint.ied_name.strip() and candidate.ied_name.strip():
                self._set_external_live_identity(candidate.ied_name)

    def _ensure_external_live_discovery(self) -> dict:
        if self._external_live_discovery is None:
            self._external_live_discovery = _empty_wire_discovery_structure(self._endpoint, self._build_external_discover_command(), source="live")
        return self._external_live_discovery

    def _set_external_live_identity(self, ied_name: str) -> None:
        normalized = ied_name.strip()
        if not normalized or normalized == self._endpoint.ied_name:
            return
        self._endpoint = replace(
            self._endpoint,
            id=f"mms:{normalized}@{self._endpoint.host}:{self._endpoint.port}",
            ied_name=normalized,
        )
        discovery = self._external_live_discovery
        if discovery is None:
            return
        endpoint = discovery.setdefault("endpoint", {})
        if isinstance(endpoint, dict):
            endpoint["id"] = self._endpoint.id
            endpoint["iedName"] = normalized
        logical_devices = discovery.get("logicalDevices")
        if isinstance(logical_devices, list):
            for item in logical_devices:
                if not isinstance(item, dict):
                    continue
                domain = item.get("domain")
                if isinstance(domain, str):
                    item["iedName"] = normalized
                    item["inst"] = _live_logical_device_inst(normalized, domain)

    def _apply_external_discovered_logical_device_line(self, line: str) -> None:
        fields = _parse_indexed_space_kv_line(line, "native-wire-client: discovered-logical-device[")
        domain = fields.get("domain")
        if not domain:
            return
        discovery = self._ensure_external_live_discovery()
        logical_devices = discovery.setdefault("logicalDevices", [])
        if not any(isinstance(item, dict) and item.get("reference") == domain for item in logical_devices):
            logical_devices.append({
                "iedName": self._endpoint.ied_name,
                "inst": _live_logical_device_inst(self._endpoint.ied_name, domain),
                "reference": domain,
                "domain": domain,
            })

    def _apply_external_discovered_logical_node_line(self, line: str) -> None:
        fields = _parse_indexed_space_kv_line(line, "native-wire-client: discovered-logical-node[")
        domain = fields.get("domain")
        name = fields.get("name")
        if not domain or not name:
            return
        discovery = self._ensure_external_live_discovery()
        logical_nodes = discovery.setdefault("logicalNodes", [])
        reference = f"{domain}/{name}"
        if not any(isinstance(item, dict) and item.get("reference") == reference for item in logical_nodes):
            logical_nodes.append({"logicalDeviceRef": domain, "name": name, "reference": reference})

    def _apply_external_discovered_dataset_line(self, line: str) -> None:
        fields = _parse_indexed_space_kv_line(line, "native-wire-client: discovered-dataset[")
        reference = fields.get("reference")
        if not reference:
            return
        discovery = self._ensure_external_live_discovery()
        _ensure_live_discovery_dataset(discovery, reference)

    def _apply_external_discovered_dataset_member_line(self, line: str) -> None:
        fields = _parse_indexed_space_kv_line(line, "native-wire-client: discovered-dataset-member[")
        data_set_ref = fields.get("dataset")
        member_ref = fields.get("ref")
        if not data_set_ref or not member_ref:
            return
        discovery = self._ensure_external_live_discovery()
        data_set = _ensure_live_discovery_dataset(discovery, data_set_ref)
        members = data_set.setdefault("members", [])
        signal = _live_member_signal(member_ref)
        if not any(isinstance(item, dict) and item.get("reference") == signal["reference"] for item in members):
            members.append(signal)
            data_set["memberCount"] = len(members)
        signals = discovery.setdefault("signals", [])
        if not any(isinstance(item, dict) and item.get("reference") == signal["reference"] for item in signals):
            signals.append(signal)

    def _apply_external_discovered_rcb_attr_line(self, line: str) -> None:
        fields = _parse_indexed_space_kv_line(line, "native-wire-client: discovered-rcb-attr[")
        domain = fields.get("domain")
        item = fields.get("item")
        field = fields.get("field")
        value = fields.get("value")
        if not domain or not item or not field or value is None:
            return
        discovery = self._ensure_external_live_discovery()
        report_controls = discovery.setdefault("reportControls", [])
        for report_control in report_controls:
            if isinstance(report_control, dict) and report_control.get("domain") == domain and report_control.get("item") == item:
                _apply_live_rcb_attr(report_control, field, value)
                break
        candidate = next((candidate for candidate in self._available_candidates if candidate.id == f"{domain}:{item}"), None)
        if candidate is None:
            return
        updated = _candidate_with_live_rcb_attr(candidate, field, value)
        self._available_candidates = _replace_or_append_candidate(self._available_candidates, updated)
        if self._candidate.id == candidate.id:
            self._candidate = updated

    def _external_state(self, runtime_status: Iec61850RuntimeStatus, *, enabled: bool, gi_in_progress: bool = False) -> Iec61850ReportControlState:
        return Iec61850ReportControlState(
            reference=to_report_control_ref(self._candidate),
            runtime_status=runtime_status,
            rpt_id=self._candidate.rpt_id,
            data_set_ref=self._candidate.data_set_ref,
            conf_rev=self._candidate.conf_rev,
            indexed=self._candidate.indexed,
            buffer_time_ms=self._candidate.buffer_time_ms,
            integrity_period_ms=self._candidate.integrity_period_ms,
            trigger_options=self._candidate.trigger_options,
            optional_fields=self._candidate.optional_fields,
            signal_count=self._candidate.signal_count,
            enabled=enabled,
            reserved_by=self._client_id if enabled and self._candidate.report_kind == Iec61850ReportKind.BUFFERED else None,
            owner=self._client_id if enabled else None,
            sequence_number=self._last_state.sequence_number + 1 if self._last_state is not None else 0,
            gi_in_progress=gi_in_progress,
        )

    def _external_report_event(self, fields: dict[str, str]) -> Iec61850ReportEvent:
        now = _utc_now().isoformat().replace("+00:00", "Z")
        sequence_number = _parse_int_or_none(fields.get("asyncReports") or fields.get("count"))
        reason = Iec61850ReportReason.GENERAL_INTERROGATION
        values = tuple(_external_report_values(self._candidate, self._pending_external_report_entries, now))
        for value in values:
            self._current_external_report_values[value.data_reference or value.reference] = value
        if not values and self._last_report is not None:
            values = self._last_report.values
        self._pending_external_report_entries.clear()
        return Iec61850ReportEvent(
            id=f"{self._session_id}:external-report:{sequence_number or 0}",
            endpoint_id=self._endpoint.id,
            received_at=now,
            report_control=to_report_control_ref(self._candidate),
            rpt_id=self._candidate.rpt_id,
            data_set_ref=self._candidate.data_set_ref,
            conf_rev=self._candidate.conf_rev,
            sequence_number=sequence_number,
            time_of_entry=now,
            entry_id=f"{self._endpoint.id}:{report_control_key(to_report_control_ref(self._candidate))}:{sequence_number or 0}",
            buffer_overflow=False,
            reason=reason,
            values=values,
        )

    def _stop_external_mms_client_process(self) -> None:
        process = self._external_mms_process
        self._external_mms_process = None
        self._external_mms_stdout_buffer.clear()
        self._pending_external_report_entries.clear()
        self._current_external_report_values.clear()
        if process is None:
            return
        if process.poll() is None:
            try:
                if process.stdin is not None:
                    process.stdin.write("exit\n")
                    process.stdin.flush()
            except (BrokenPipeError, OSError):
                pass
            try:
                process.terminate()
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=2)

    def _external_probe_binary_path(self) -> str:
        preferred_paths = (
            Path("/workspace/iec61850_ied/build/unitlab-iec61850-ied-sim"),
            Path("iec61850_ied/build/unitlab-iec61850-ied-sim"),
            Path("/workspace/iec61850_ied/build-libiec61850/unitlab-iec61850-ied-sim"),
            Path("iec61850_ied/build-libiec61850/unitlab-iec61850-ied-sim"),
        )
        for path in preferred_paths:
            if path.is_file():
                return str(path)
        return str(preferred_paths[0])

    def _run_external_probe(self, probe: str):
        if self._endpoint.host is None or not self._endpoint.host.strip():
            raise Iec61850ReportRuntimeError("EXTERNAL_MMS_HOST_REQUIRED", "IEC 61850 external MMS target host is required.")
        if self._target_scl_path is None or not self._target_scl_path.strip():
            raise Iec61850ReportRuntimeError("EXTERNAL_MMS_SCL_REQUIRED", "IEC 61850 external MMS target SCD path is required.")
        binary_path = self._external_probe_binary_path()
        command = [
            binary_path,
            "--scl",
            self._target_scl_path,
            "--ied",
            self._endpoint.ied_name,
            "--bind",
            self._endpoint.host,
            "--port",
            str(self._endpoint.port),
        ]
        if probe == "metadata":
            command.append("--metadata-probe")
        elif probe == "gi":
            command.append("--gi-probe")
            command.extend(("--report-key", report_control_key(to_report_control_ref(self._candidate))))
        else:
            raise Iec61850ReportRuntimeError("EXTERNAL_MMS_PROBE_INVALID", "IEC 61850 external MMS probe kind is invalid.")
        try:
            return subprocess.run(command, capture_output=True, text=True, timeout=30, check=True)
        except subprocess.TimeoutExpired as exc:
            raise Iec61850ReportRuntimeError("EXTERNAL_MMS_PROBE_TIMEOUT", f"IEC 61850 external MMS {probe} probe timed out after 30s.") from exc
        except subprocess.CalledProcessError as exc:
            details = ((exc.stderr or "") + (exc.stdout or "")).strip()
            raise Iec61850ReportRuntimeError("EXTERNAL_MMS_PROBE_FAILED", f"IEC 61850 external MMS {probe} probe failed with exit code {exc.returncode}: {details}") from exc

    def _discover_live_wire_ied(self) -> None:
        command = f"discover {self._candidate.ied_name}{self._candidate.logical_device_inst}"
        self._write_live_wire_command(command)
        self._drain_live_wire_process_stdout(timeout_seconds=2.0)
        self._last_read = None
        self._last_state = None
        self._last_discovery = _build_wire_discovery_structure(self._endpoint, self._candidate, command)
        self._runtime._append_event(
            kind="wire-ied-discover",
            session_id=self._session_id,
            endpoint_id=self._wire_endpoint_id(),
            candidate_id=self._candidate.id,
            report_control_name=self._candidate.report_control_name,
            client_id=self._client_id,
            outcome="command-sent",
            message=command,
        )

    def _enable_live_wire_reporting(self) -> None:
        self._write_live_wire_command("rptena")
        self._drain_live_wire_process_stdout(timeout_seconds=2.0)
        self._runtime._append_event(
            kind="wire-report-control-enable",
            session_id=self._session_id,
            endpoint_id=self._wire_endpoint_id(),
            candidate_id=self._candidate.id,
            report_control_name=self._candidate.report_control_name,
            client_id=self._client_id,
            outcome="command-sent",
            message="rptena",
        )

    def _send_live_wire_general_interrogation(self) -> None:
        self._write_live_wire_command("gi")
        self._drain_live_wire_process_stdout(timeout_seconds=2.0)
        self._runtime._append_event(
            kind="wire-report-control-gi",
            session_id=self._session_id,
            endpoint_id=self._wire_endpoint_id(),
            candidate_id=self._candidate.id,
            report_control_name=self._candidate.report_control_name,
            client_id=self._client_id,
            outcome="command-sent",
            message="gi",
        )

    def _disconnect_live_wire_ied(self) -> None:
        self._write_live_wire_command("disconnect")
        self._drain_live_wire_process_stdout(timeout_seconds=1.0)
        self._runtime._append_event(
            kind="wire-ied-disconnect",
            session_id=self._session_id,
            endpoint_id=self._wire_endpoint_id(),
            candidate_id=self._candidate.id,
            report_control_name=self._candidate.report_control_name,
            client_id=self._client_id,
            outcome="command-sent",
            message="disconnect",
        )

    def _write_live_wire_command(self, command: str) -> None:
        if self._live_wire_endpoint is None:
            raise Iec61850ReportRuntimeError(
                "LIVE_WIRE_SESSION_NOT_OPEN",
                "IEC 61850 live wire transport is not open.",
            )
        if self._live_wire_process is None or self._live_wire_process.process.stdin is None:
            raise Iec61850ReportRuntimeError(
                "LIVE_WIRE_PROCESS_NOT_OPEN",
                "IEC 61850 live wire transport process is not open.",
            )
        self._drain_live_wire_process_stdout(timeout_seconds=0.0)
        write_ied_simulator_process_command(self._live_wire_process, command)

    def _drain_live_wire_process_stdout(self, timeout_seconds: float = 0.0) -> None:
        if self._live_wire_process is None:
            return
        deadline = time.monotonic() + max(timeout_seconds, 0.0)
        line_buffer = bytearray()
        while True:
            line = _read_process_stdout_line(self._live_wire_process.process, timeout_deadline=deadline, line_buffer=line_buffer)
            if line is None:
                return

    def _stop_live_wire_transport(self) -> None:
        if self._live_wire_process is None:
            return
        process_handle = self._live_wire_process
        fixture_dir = self._live_wire_fixture_dir
        endpoint_id = self._live_wire_endpoint.id if self._live_wire_endpoint is not None else self._session_id
        self._live_wire_process = None
        self._live_wire_fixture_dir = None
        self._live_wire_endpoint = None
        try:
            if process_handle.process.stdin is not None:
                try:
                    write_ied_simulator_process_command(process_handle, "exit")
                except Iec61850ReportRuntimeError:
                    pass
        finally:
            stop_ied_simulator_process(process_handle)
            if fixture_dir is not None:
                fixture_dir.cleanup()
        self._live_wire_last_diagnostic = None
        self._runtime._append_event(
            kind="wire-session-close",
            session_id=self._session_id,
            endpoint_id=endpoint_id,
            client_id=self._client_id,
            outcome="closed",
        )

    def _uses_live_wire_client(self) -> bool:
        return self._endpoint.mode == Iec61850RuntimeMode.MMS and self._live_wire_process is not None

    def _wire_endpoint_id(self) -> str:
        if self._live_wire_endpoint is not None:
            return self._live_wire_endpoint.id
        return self._endpoint.id

    def _reset_runtime_state_for_target_change(self) -> None:
        if self._external_mms_process is not None:
            self._stop_external_mms_client_process()
        if self._live_wire_process is not None:
            self._stop_live_wire_transport()
        if self._session_open:
            try:
                self._runtime.close_session(self._session_id)
            except Iec61850ReportRuntimeError as exc:
                if exc.code != "SESSION_NOT_FOUND":
                    raise
        self._session_open = False
        self._last_read = None
        self._last_discovery = None
        self._external_discovered_rcbs.clear()
        self._external_live_discovery = None
        self._last_state = None
        self._last_report = None
        self._last_plan = None
        self._last_diagnostic = None
        self._live_wire_last_frame = None
        self._live_wire_last_diagnostic = None
        self._endpoint_resolution = _default_endpoint_resolution(self._endpoint)

    def _read_live_wire_process_frame_response(self, frame_kind: str, timeout_seconds: float = 5.0) -> bytes:
        if self._live_wire_process is None or self._live_wire_process.process.stdout is None:
            raise Iec61850ReportRuntimeError(
                "LIVE_WIRE_PROCESS_NOT_OPEN",
                "IEC 61850 live wire transport process is not open.",
            )
        deadline = time.monotonic() + max(timeout_seconds, 0.0)
        line_buffer = bytearray()
        while True:
            line = _read_process_stdout_line(self._live_wire_process.process, timeout_deadline=deadline, line_buffer=line_buffer)
            if line is None:
                raise Iec61850ReportRuntimeError(
                    "LIVE_WIRE_FRAME_TIMEOUT",
                    f"IEC 61850 live wire transport timed out while waiting for a {frame_kind} frame response.",
                )
            line = line.strip()
            if not line:
                continue
            if line.startswith("wire-frame="):
                return self._decode_live_wire_frame_response(line, frame_kind)

    def _decode_live_wire_frame_response(self, response: str, frame_kind: str) -> bytes:
        if not response.startswith("wire-frame="):
            raise Iec61850ReportRuntimeError(
                "LIVE_WIRE_FRAME_INVALID",
                f"IEC 61850 live wire transport received an invalid {frame_kind} response.",
            )
        hex_payload = response[len("wire-frame="):].strip()
        try:
            return bytes.fromhex(hex_payload)
        except ValueError as exc:
            raise Iec61850ReportRuntimeError(
                "LIVE_WIRE_FRAME_INVALID",
                f"IEC 61850 live wire transport received a malformed {frame_kind} response.",
            ) from exc

    def _project_transcript(self, transcript: tuple[Iec61850MmsClientEvent, ...]) -> tuple[Iec61850MmsClientEvent, ...]:
        if self._transcript_wire_endpoint_id is None:
            return transcript
        projected: list[Iec61850MmsClientEvent] = []
        for event in transcript:
            projected.append(self._project_transcript_event(event))
        return tuple(projected)

    def _project_transcript_event(self, event: Iec61850MmsClientEvent) -> Iec61850MmsClientEvent:
        if self._transcript_wire_endpoint_id is None:
            return event
        kind_map = {
            "session-open": "mms-associate",
            "session-close": "mms-disassociate",
            "report-control-read": "wire-report-control-read",
            "report-control-reserve": "wire-report-control-reserve",
            "report-control-enable": "wire-report-control-enable",
            "report-control-disable": "wire-report-control-disable",
            "report-control-release": "wire-report-control-release",
            "report-control-gi": "wire-report-control-gi",
        }
        kind = kind_map.get(event.kind, event.kind)
        return replace(event, kind=kind, endpoint_id=self._transcript_wire_endpoint_id)

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


def _build_ui_state(
    *,
    session_id: str,
    client_id: str,
    session_open: bool,
    endpoint: Iec61850DeviceEndpoint,
    endpoint_resolution: Iec61850EndpointResolution,
    candidate: Iec61850ReportControlCandidate,
    available_candidates: tuple[Iec61850ReportControlCandidate, ...],
    last_discovery: dict | None,
    last_state: Iec61850ReportControlState | None,
    last_report: Iec61850ReportEvent | None,
    current_report_values: tuple[Iec61850ReportEventValue, ...],
    last_diagnostic: Iec61850ClientControlDiagnostic | None,
    live_wire_open: bool,
    live_wire_control_open: bool,
    live_wire_endpoint: Iec61850DeviceEndpoint | None,
    live_wire_last_frame_length: int | None,
    live_wire_last_diagnostic: Iec61850ClientControlDiagnostic | None,
    transcript: tuple[Iec61850MmsClientEvent, ...],
) -> dict:
    candidate_ready = not _candidate_is_unselected(candidate)
    selected_rcb_ref = _candidate_rcb_reference(candidate) if candidate_ready else None
    selected_dataset_ref = candidate.data_set_ref if candidate_ready else None
    discovery_counts = _discovery_counts(last_discovery, candidate)
    rptena_enabled = bool(last_state.enabled) if last_state is not None else False
    associated = session_open or live_wire_open
    last_event = transcript[-1] if transcript else None
    external_probe = (
        _external_probe_ui_state(endpoint=endpoint, last_event=last_event)
        if last_state is None and not session_open
        else None
    )
    runtime_status = (
        external_probe.runtime_status
        if external_probe is not None
        else last_state.runtime_status.value if last_state is not None else "disconnected"
    )
    subscribed = rptena_enabled or runtime_status in {"enabled", "gi-pending", "reporting"}
    active_diagnostic = last_diagnostic or live_wire_last_diagnostic
    phase = (
        external_probe.phase
        if active_diagnostic is None and external_probe is not None
        else _ui_phase(
            session_open=session_open,
            live_wire_open=live_wire_open,
            discovered=last_discovery is not None,
            subscribed=subscribed,
            last_report=last_report,
            diagnostic=active_diagnostic,
        )
    )
    external_mms = endpoint.mode == Iec61850RuntimeMode.MMS
    can_external_probe = external_mms and last_discovery is not None and candidate_ready
    can_external_gi = (
        can_external_probe
        and external_probe is not None
        and external_probe.last_command in {"rptena", "gi"}
    )
    report_values = []
    if last_report is not None:
        candidate_signal_refs = {signal.reference for signal in candidate.signals}
        for value in last_report.values:
            matched = (
                value.reference in candidate_signal_refs
                or value.data_reference is None
                or value.data_reference == value.reference
            )
            report_values.append({
                "index": value.data_set_index,
                "reference": value.reference,
                "data_reference": value.data_reference,
                "value": value.value,
                "reason": value.reason_code.value,
                "timestamp": value.timestamp,
                "matched": matched,
            })
    report_signal_states = _report_signal_states(candidate, last_report, current_report_values)

    return {
        "schema": "unitlab.iec61850.client.ui-state.v1",
        "session": {
            "id": session_id,
            "client_id": client_id,
            "phase": phase,
            "connected": session_open,
            "associated": associated,
            "mode": endpoint.mode.value,
            "endpoint_id": endpoint.id,
            "endpoint_label": _endpoint_label(endpoint),
            "last_event_kind": last_event.kind if last_event is not None else None,
            "last_event_outcome": last_event.outcome if last_event is not None else None,
        },
        "endpoint_resolution": {
            "transport_source": endpoint_resolution.transport_source,
            "model_source": endpoint_resolution.model_source,
            "requested_host": endpoint_resolution.requested_host,
            "requested_port": endpoint_resolution.requested_port,
            "resolved_host": endpoint_resolution.resolved_host,
            "resolved_port": endpoint_resolution.resolved_port,
            "override_host": endpoint_resolution.override_host,
            "override_port": endpoint_resolution.override_port,
            "override_applied": endpoint_resolution.override_applied,
            "scl_path": endpoint_resolution.scl_path,
            "notes": list(endpoint_resolution.notes),
        },
        "discovery": {
            "discovered": last_discovery is not None,
            "logical_devices": discovery_counts["logical_devices"],
            "logical_nodes": discovery_counts["logical_nodes"],
            "data_sets": discovery_counts["data_sets"],
            "data_set_members": discovery_counts["data_set_members"],
            "report_controls": discovery_counts["report_controls"],
            "signals": discovery_counts["signals"],
            "available_report_controls": _available_report_controls_payload(available_candidates),
            "selected_dataset_ref": selected_dataset_ref,
            "selected_rcb_ref": selected_rcb_ref,
        },
        "subscription": {
            "subscribed": subscribed,
            "runtime_status": runtime_status,
            "rptena_enabled": rptena_enabled,
            "reserved_by": last_state.reserved_by if last_state is not None else None,
            "owner": last_state.owner if last_state is not None else None,
            "gi_in_progress": bool(last_state.gi_in_progress) if last_state is not None else False,
            "sequence_number": last_state.sequence_number if last_state is not None else None,
            "last_command": external_probe.last_command if external_probe is not None else None,
            "command_accepted": external_probe.command_accepted if external_probe is not None else False,
            "external_probe": external_probe is not None,
            "selected_rcb_ref": selected_rcb_ref,
            "selected_dataset_ref": selected_dataset_ref,
        },
        "report": {
            "received": last_report is not None,
            "rpt_id": last_report.rpt_id if last_report is not None else None,
            "data_set_ref": last_report.data_set_ref if last_report is not None else None,
            "conf_rev": last_report.conf_rev if last_report is not None else None,
            "sequence_number": last_report.sequence_number if last_report is not None else None,
            "reason": last_report.reason.value if last_report is not None else None,
            "value_count": len(last_report.values) if last_report is not None else 0,
            "matched_value_count": sum(1 for value in report_values if value["matched"]),
            "unmatched_value_count": sum(1 for value in report_values if not value["matched"]),
            "signal_state_count": len(report_signal_states),
            "signal_states": report_signal_states,
            "values": report_values,
        },
        "wire": {
            "open": live_wire_open,
            "control_open": live_wire_control_open,
            "endpoint_id": live_wire_endpoint.id if live_wire_endpoint is not None else None,
            "endpoint_label": _endpoint_label(live_wire_endpoint) if live_wire_endpoint is not None else None,
            "last_frame_length": live_wire_last_frame_length,
        },
        "diagnostic": {
            "active": active_diagnostic is not None,
            "action": active_diagnostic.action if active_diagnostic is not None else None,
            "code": active_diagnostic.code if active_diagnostic is not None else None,
            "message": active_diagnostic.message if active_diagnostic is not None else None,
        },
        "actions": {
            "can_connect": not session_open and not live_wire_open,
            "can_discover": associated or external_mms,
            "can_rptena": (associated and candidate_ready and (last_discovery is not None or external_mms)) or can_external_probe,
            "can_gi": (associated and subscribed and candidate_ready) or can_external_gi,
            "can_disconnect": associated,
            "can_close_ied": session_open or live_wire_open or last_discovery is not None or last_report is not None,
        },
    }


def _external_probe_ui_state(
    *,
    endpoint: Iec61850DeviceEndpoint,
    last_event: Iec61850MmsClientEvent | None,
) -> _ExternalProbeUiState | None:
    if endpoint.mode != Iec61850RuntimeMode.MMS or last_event is None or last_event.outcome != "accepted":
        return None
    if last_event.kind == "external-report-control-precheck":
        return _ExternalProbeUiState(
            phase="rptena-accepted",
            runtime_status="rptena-accepted",
            last_command="rptena",
            command_accepted=True,
        )
    if last_event.kind == "external-report-control-gi":
        return _ExternalProbeUiState(
            phase="gi-accepted",
            runtime_status="gi-accepted",
            last_command="gi",
            command_accepted=True,
        )
    return None


def _ui_phase(
    *,
    session_open: bool,
    live_wire_open: bool,
    discovered: bool,
    subscribed: bool,
    last_report: Iec61850ReportEvent | None,
    diagnostic: Iec61850ClientControlDiagnostic | None,
) -> str:
    if diagnostic is not None:
        return "failed"
    if last_report is not None:
        return "reporting"
    if subscribed:
        return "subscribed"
    if discovered:
        return "discovered"
    if session_open or live_wire_open:
        return "associated"
    return "idle"


def _available_report_controls_payload(candidates: tuple[Iec61850ReportControlCandidate, ...]) -> list[dict[str, str | bool | dict | None]]:
    return [
        {
            "rcb_ref": _candidate_rcb_reference(candidate),
            "report_control_id": candidate.id,
            "report_control_name": candidate.report_control_name,
            "report_kind": candidate.report_kind.value,
            "rpt_id": candidate.rpt_id,
            "data_set_ref": candidate.data_set_ref,
            "trigger_options": _trigger_options_payload(candidate.trigger_options),
            "optional_fields": _optional_fields_payload(candidate.optional_fields),
        }
        for candidate in candidates
    ]


def _discovery_counts(discovery: dict | None, candidate: Iec61850ReportControlCandidate) -> dict[str, int]:
    if discovery is None:
        return {
            "logical_devices": 0,
            "logical_nodes": 0,
            "data_sets": 0,
            "data_set_members": 0,
            "report_controls": 0,
            "signals": 0,
        }
    data_sets = discovery.get("dataSets") if isinstance(discovery, dict) else None
    return {
        "logical_devices": _list_count(discovery.get("logicalDevices")),
        "logical_nodes": _list_count(discovery.get("logicalNodes")),
        "data_sets": _list_count(data_sets),
        "data_set_members": _data_set_member_count(data_sets),
        "report_controls": _list_count(discovery.get("reportControls")),
        "signals": _list_count(discovery.get("signals")) or candidate.signal_count,
    }


def _list_count(value) -> int:
    return len(value) if isinstance(value, list) else 0


def _data_set_member_count(value) -> int:
    if not isinstance(value, list):
        return 0
    total = 0
    for item in value:
        if isinstance(item, dict) and isinstance(item.get("memberCount"), int):
            total += item["memberCount"]
        elif isinstance(item, dict) and isinstance(item.get("members"), list):
            total += len(item["members"])
    return total


def _candidate_rcb_reference(candidate: Iec61850ReportControlCandidate) -> str:
    logical_device_inst = _normalized_live_logical_device_inst(candidate.ied_name, candidate.logical_device_inst)
    return f"{candidate.ied_name}/{candidate.access_point_name}/{logical_device_inst}/{candidate.logical_node_name}/{candidate.report_control_name}/{candidate.report_kind.value}"


def _endpoint_label(endpoint: Iec61850DeviceEndpoint) -> str:
    if endpoint.host:
        if endpoint.ied_name:
            return f"{endpoint.ied_name}@{endpoint.host}:{endpoint.port}"
        return f"{endpoint.host}:{endpoint.port}"
    return f"{endpoint.ied_name}/{endpoint.access_point_name}" if endpoint.ied_name else endpoint.access_point_name


def _default_endpoint_resolution(endpoint: Iec61850DeviceEndpoint) -> Iec61850EndpointResolution:
    transport_source: Literal["simulator", "explicit_request"] = "simulator" if endpoint.mode == Iec61850RuntimeMode.SIMULATOR else "explicit_request"
    model_source: Literal["simulator", "scd-first", "discovery-fallback"] = "simulator" if endpoint.mode == Iec61850RuntimeMode.SIMULATOR else "discovery-fallback"
    return Iec61850EndpointResolution(
        transport_source=transport_source,
        model_source=model_source,
        requested_host=endpoint.host,
        requested_port=endpoint.port,
        resolved_host=endpoint.host,
        resolved_port=endpoint.port,
        override_host=None,
        override_port=None,
        override_applied=False,
        scl_path=None,
        notes=(),
    )

def get_iec61850_client_control_service() -> Iec61850ClientControlService:
    return _CLIENT_CONTROL_SERVICE

def _parse_space_kv_line(text: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for token in text.split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        fields[key] = value
    return fields


def _parse_indexed_space_kv_line(text: str, prefix: str) -> dict[str, str]:
    payload = text.removeprefix(prefix)
    _index_text, separator, fields_text = payload.partition("] ")
    if separator != "] ":
        return {}
    return _parse_space_kv_line(fields_text)


def _none_if_placeholder(value: str | None) -> str | None:
    if value is None or value == "<none>":
        return None
    return value


def _parse_int_or_none(value: str | None) -> int | None:
    if value is None:
        return None
    try:
        return int(value)
    except ValueError:
        return None


def _parse_external_report_value(value: str | None) -> bool | int | float | str | None:
    if value is None or value == "<none>":
        return None
    normalized = value.strip()
    if len(normalized) >= 2 and normalized[0] == '"' and normalized[-1] == '"':
        normalized = normalized[1:-1]
    if normalized == "true":
        return True
    if normalized == "false":
        return False
    try:
        return int(normalized, 10)
    except ValueError:
        pass
    try:
        return float(normalized)
    except ValueError:
        return normalized


def _external_rcb_bool_command(candidate: Iec61850ReportControlCandidate, field: str, value: bool) -> str:
    rcb_kind = "BR" if candidate.report_kind == Iec61850ReportKind.BUFFERED else "RP"
    domain = _external_mms_domain(candidate)
    item = f"{candidate.logical_node_name}${rcb_kind}${candidate.report_control_name}${field}"
    return f"write-bool {domain} {item} {'true' if value else 'false'}"


def _external_rcb_hex_command(candidate: Iec61850ReportControlCandidate, field: str, value_hex: str) -> str:
    rcb_kind = "BR" if candidate.report_kind == Iec61850ReportKind.BUFFERED else "RP"
    domain = _external_mms_domain(candidate)
    item = f"{candidate.logical_node_name}${rcb_kind}${candidate.report_control_name}${field}"
    return f"write-hex {domain} {item} 4 {value_hex}"


def _external_report_reason(value: str | None) -> Iec61850ReportReason:
    labels = set((value or "").split(","))
    if "general-interrogation" in labels:
        return Iec61850ReportReason.GENERAL_INTERROGATION
    if "data-change" in labels:
        return Iec61850ReportReason.DATA_CHANGE
    if "quality-change" in labels:
        return Iec61850ReportReason.QUALITY_CHANGE
    if "data-update" in labels:
        return Iec61850ReportReason.DATA_UPDATE
    if "integrity" in labels:
        return Iec61850ReportReason.INTEGRITY
    return Iec61850ReportReason.GENERAL_INTERROGATION


def _external_report_values(
    candidate: Iec61850ReportControlCandidate,
    entries: Sequence[dict[str, str]],
    timestamp: str,
) -> list[Iec61850ReportEventValue]:
    values: list[Iec61850ReportEventValue] = []
    for fallback_index, entry in enumerate(entries):
        data_reference = _none_if_placeholder(entry.get("dataRef")) or _none_if_placeholder(entry.get("reference"))
        matched_reference, matched_index = _match_external_report_reference(candidate, data_reference)
        data_set_index = _parse_int_or_none(entry.get("index"))
        if data_set_index is None:
            data_set_index = matched_index if matched_index is not None else fallback_index
        values.append(
            Iec61850ReportEventValue(
                data_set_index=data_set_index,
                reference=matched_reference or data_reference or "<unknown>",
                data_reference=data_reference,
                value=_parse_external_report_value(entry.get("value")),
                reason_code=_external_report_reason(entry.get("reason")),
                timestamp=timestamp,
            )
        )
    return values


def _match_external_report_reference(candidate: Iec61850ReportControlCandidate, data_reference: str | None) -> tuple[str | None, int | None]:
    if data_reference is None:
        return None, None
    for index, signal in enumerate(candidate.signals):
        prefix = _signal_mms_prefix(candidate.ied_name, candidate.logical_device_inst, signal.reference)
        if prefix is not None and (data_reference == prefix or data_reference.startswith(prefix + "$")):
            return signal.reference, index
    return None, None


def _signal_mms_prefix(ied_name: str, logical_device_inst: str, reference: str) -> str | None:
    if "." not in reference or "[" not in reference or not reference.endswith("]"):
        return None
    if "/" in reference:
        logical_device, rest = reference.split("/", 1)
    else:
        logical_device = logical_device_inst
        rest = reference
    logical_node, object_and_fc = rest.split(".", 1)
    object_path, fc = object_and_fc.rsplit("[", 1)
    fc = fc[:-1]
    object_mms = object_path.replace(".", "$")
    return f"{ied_name}{logical_device}/{logical_node}${fc}${object_mms}"


def _report_signal_states(
    candidate: Iec61850ReportControlCandidate,
    report: Iec61850ReportEvent | None,
    current_values: Sequence[Iec61850ReportEventValue] = (),
) -> list[dict]:
    values = tuple(current_values) if current_values else report.values if report is not None else ()
    if not values:
        return []

    states: list[dict] = []
    for signal_index, signal in enumerate(candidate.signals):
        prefix = _signal_mms_prefix(candidate.ied_name, candidate.logical_device_inst, signal.reference)
        if prefix is None:
            continue

        signal_values = [
            value
            for value in values
            if value.data_reference is not None
            and (value.data_reference == prefix or value.data_reference.startswith(prefix + "$"))
        ]
        if not signal_values:
            continue

        primary = _select_signal_primary_value(prefix, signal_values)
        quality = _select_signal_leaf_value(prefix, signal_values, "$q")
        source_timestamp = _select_signal_leaf_value(prefix, signal_values, "$t")
        states.append({
            "index": signal_index,
            "reference": signal.reference,
            "fc": signal.fc,
            "value": primary.value if primary is not None else None,
            "value_data_reference": primary.data_reference if primary is not None else None,
            "quality": quality.value if quality is not None else None,
            "quality_data_reference": quality.data_reference if quality is not None else None,
            "source_timestamp": source_timestamp.value if source_timestamp is not None else None,
            "source_timestamp_data_reference": source_timestamp.data_reference if source_timestamp is not None else None,
            "timestamp": primary.timestamp if primary is not None else signal_values[-1].timestamp,
            "reason": primary.reason_code.value if primary is not None else signal_values[-1].reason_code.value,
            "leaf_count": len(signal_values),
        })
    return states


def _external_mms_domain(candidate: Iec61850ReportControlCandidate) -> str:
    logical_device_inst = candidate.logical_device_inst.strip()
    ied_name = candidate.ied_name.strip()
    if not ied_name or logical_device_inst.startswith(ied_name):
        return logical_device_inst or ied_name
    return f"{ied_name}{logical_device_inst}"


def _select_signal_primary_value(
    prefix: str,
    values: Sequence[Iec61850ReportEventValue],
) -> Iec61850ReportEventValue | None:
    for suffix in ("$stVal", "$general", ""):
        selected = _select_signal_leaf_value(prefix, values, suffix)
        if selected is not None:
            return selected
    for value in values:
        if value.data_reference is None:
            continue
        if not value.data_reference.endswith("$q") and not value.data_reference.endswith("$t"):
            return value
    return values[0] if values else None


def _select_signal_leaf_value(
    prefix: str,
    values: Sequence[Iec61850ReportEventValue],
    suffix: str,
) -> Iec61850ReportEventValue | None:
    expected = f"{prefix}{suffix}"
    for value in values:
        if value.data_reference == expected:
            return value
    return None


def _read_process_stdout_line(process, *, timeout_deadline: float, line_buffer: bytearray) -> str | None:
    stdout = process.stdout
    if stdout is None:
        return None

    raw_fd = None
    raw_stream = getattr(getattr(stdout, "buffer", None), "raw", None)
    if raw_stream is not None:
        fileno = getattr(raw_stream, "fileno", None)
        if callable(fileno):
            raw_fd = fileno()

    if raw_fd is None:
        line = stdout.readline()
        return line or None

    while True:
        newline_index = line_buffer.find(b"\n")
        if newline_index >= 0:
            line = bytes(line_buffer[: newline_index + 1])
            del line_buffer[: newline_index + 1]
            return line.decode("utf-8", errors="replace")

        remaining = timeout_deadline - time.monotonic()
        select_timeout = min(remaining, 1.0) if remaining > 0 else 0.0

        readable, _, _ = select.select([raw_fd], [], [], select_timeout)
        if not readable:
            if remaining <= 0:
                return None
            continue

        try:
            chunk = os.read(raw_fd, 4096)
        except BlockingIOError:
            continue
        if not chunk:
            if line_buffer:
                line = bytes(line_buffer)
                line_buffer.clear()
                return line.decode("utf-8", errors="replace")
            return None
        line_buffer.extend(chunk)




def _build_discovery_structure(
    endpoint: Iec61850DeviceEndpoint,
    candidate: Iec61850ReportControlCandidate,
    read_result: Iec61850ReportControlReadResult,
) -> dict:
    signal_items = [
        {
            "reference": signal.reference,
            "fc": signal.fc,
        }
        for signal in candidate.signals
    ]
    data_set_ref = candidate.data_set_ref or f"{candidate.ied_name}{candidate.logical_device_inst}/{candidate.logical_node_name}.dsEvents"
    return {
        "schema": "unitlab.iec61850.client.discovery.v1",
        "endpoint": {
            "id": endpoint.id,
            "mode": endpoint.mode.value,
            "iedName": endpoint.ied_name,
            "accessPointName": endpoint.access_point_name,
            "host": endpoint.host,
            "port": endpoint.port,
        },
        "logicalDevices": [
            {
                "iedName": candidate.ied_name,
                "inst": candidate.logical_device_inst,
                "reference": f"{candidate.ied_name}{candidate.logical_device_inst}",
            }
        ],
        "logicalNodes": [
            {
                "logicalDeviceInst": candidate.logical_device_inst,
                "name": candidate.logical_node_name,
                "reference": f"{candidate.ied_name}{candidate.logical_device_inst}/{candidate.logical_node_name}",
            }
        ],
        "dataSets": [
            {
                "reference": data_set_ref,
                "members": signal_items,
                "memberCount": len(signal_items),
            }
        ],
        "reportControls": [
            {
                "id": candidate.id,
                "name": candidate.report_control_name,
                "kind": candidate.report_kind.value,
                "rptId": candidate.rpt_id,
                "dataSetRef": data_set_ref,
                "confRev": candidate.conf_rev,
                "indexed": candidate.indexed,
                "bufferTimeMs": candidate.buffer_time_ms,
                "integrityPeriodMs": candidate.integrity_period_ms,
                "runtimeStatus": read_result.state.runtime_status.value,
            }
        ],
        "signals": signal_items,
    }


def _build_target_endpoint_and_candidate(
    request: Iec61850ClientTargetRequest,
    *,
    endpoint_catalog: Iec61850MmsEndpointCatalog | None = None,
) -> tuple[
    Iec61850DeviceEndpoint,
    Iec61850ReportControlCandidate,
    tuple[Iec61850ReportControlCandidate, ...],
    Iec61850EndpointResolution,
]:
    mode = request.mode.strip().lower()
    if mode == "simulator":
        endpoint = _default_endpoint()
        return endpoint, _default_candidate(), (_default_candidate(),), _default_endpoint_resolution(endpoint)
    if mode not in {"mms", "external-mms"}:
        raise Iec61850ReportRuntimeError("CLIENT_TARGET_MODE_INVALID", "IEC 61850 client target mode must be simulator or external-mms.")
    ied_name = request.ied_name.strip()
    access_point_name = request.access_point_name.strip() or "AP1"
    if (request.scl_path is not None and request.scl_path.strip()) and not ied_name:
        raise Iec61850ReportRuntimeError("CLIENT_TARGET_IED_REQUIRED", "IEC 61850 external MMS target IED name is required.")
    requested_host = request.host.strip() if request.host is not None else ""
    requested_port = request.port if request.port > 0 else None
    override_host = request.transport_override_host.strip() if request.transport_override_host is not None and request.transport_override_host.strip() else ""
    override_port = request.transport_override_port if request.transport_override_port is not None and request.transport_override_port > 0 else None
    transport_host = override_host or requested_host
    transport_port = override_port if override_port is not None else requested_port
    notes: tuple[str, ...] = ()
    if override_host or override_port is not None:
        notes = ("test-only transport override applied",)
    if endpoint_catalog is not None and ied_name:
        try:
            endpoint, notes = endpoint_catalog.resolve_transport_endpoint(
                ied_name=ied_name,
                access_point_name=access_point_name,
                requested_host=transport_host or None,
                requested_port=transport_port,
            )
        except Iec61850ReportRuntimeError:
            if not transport_host:
                raise
            endpoint = Iec61850DeviceEndpoint(
                id=f"mms:{ied_name}@{transport_host}:{transport_port}" if ied_name else f"mms:{transport_host}:{transport_port}",
                mode=Iec61850RuntimeMode.MMS,
                ied_name=ied_name,
                access_point_name=access_point_name,
                host=transport_host,
                port=transport_port or 102,
            )
            notes = ("explicit transport request",)
    else:
        if not transport_host:
            raise Iec61850ReportRuntimeError("CLIENT_TARGET_HOST_REQUIRED", "IEC 61850 external MMS target host is required.")
        if transport_port is None:
            raise Iec61850ReportRuntimeError("CLIENT_TARGET_PORT_INVALID", "IEC 61850 external MMS target port must be in range 1..65535.")
        endpoint = Iec61850DeviceEndpoint(
            id=f"mms:{ied_name}@{transport_host}:{transport_port}" if ied_name else f"mms:{transport_host}:{transport_port}",
            mode=Iec61850RuntimeMode.MMS,
            ied_name=ied_name,
            access_point_name=access_point_name,
            host=transport_host,
            port=transport_port,
        )
    if request.scl_path is None or not request.scl_path.strip():
        return (
            endpoint,
            _external_unselected_candidate(endpoint),
            (),
            Iec61850EndpointResolution(
                transport_source="explicit_request",
                model_source="discovery-fallback",
                requested_host=requested_host or None,
                requested_port=requested_port,
                resolved_host=endpoint.host,
                resolved_port=endpoint.port,
                override_host=override_host or None,
                override_port=override_port,
                override_applied=bool(override_host or override_port is not None),
                scl_path=None,
                notes=("discovery required for model binding",) + notes,
            ),
        )
    scl_path = Path(request.scl_path.strip())
    if not scl_path.is_file():
        raise Iec61850ReportRuntimeError("CLIENT_TARGET_SCL_NOT_FOUND", f"IEC 61850 SCD/SCL file was not found: {scl_path}.")
    candidates = _candidates_from_scd(scl_path, ied_name, access_point_name)
    if not candidates:
        raise Iec61850ReportRuntimeError("CLIENT_TARGET_REPORT_CONTROL_NOT_FOUND", f'IED "{ied_name}" has no ReportControl in {scl_path}.')
    candidate = _find_candidate_by_rcb_reference(candidates, request.selected_rcb_ref)
    if candidate is None:
        candidate = next((item for item in candidates if item.access_point_name == access_point_name), candidates[0])
    return (
        endpoint,
        candidate,
        candidates,
        Iec61850EndpointResolution(
            transport_source="explicit_request",
            model_source="scd-first",
            requested_host=requested_host or None,
            requested_port=requested_port,
            resolved_host=endpoint.host,
            resolved_port=endpoint.port,
            override_host=override_host or None,
            override_port=override_port,
            override_applied=bool(override_host or override_port is not None),
            scl_path=str(scl_path),
            notes=("loaded SCD used for model binding",) + notes,
        ),
    )


def _candidates_from_scd(scl_path: Path, ied_name: str, fallback_access_point: str) -> tuple[Iec61850ReportControlCandidate, ...]:
    try:
        root = ET.parse(scl_path).getroot()
    except ET.ParseError as exc:
        raise Iec61850ReportRuntimeError("CLIENT_TARGET_SCL_PARSE_FAILED", f"IEC 61850 SCD/SCL parse failed: {exc}.") from exc

    ied = _find_child_by_attr(root, "IED", "name", ied_name)
    if ied is None:
        raise Iec61850ReportRuntimeError("CLIENT_TARGET_IED_NOT_FOUND", f'IED "{ied_name}" was not found in {scl_path}.')

    candidates: list[Iec61850ReportControlCandidate] = []
    for access_point in _iter_children(ied, "AccessPoint"):
        access_point_name = access_point.attrib.get("name") or fallback_access_point
        server = _first_child(access_point, "Server")
        if server is None:
            continue
        for ldevice in _iter_children(server, "LDevice"):
            logical_device_inst = ldevice.attrib.get("inst", "").strip()
            if not logical_device_inst:
                continue
            for logical_node in _iter_children(ldevice, None):
                if _local_name(logical_node.tag) not in {"LN0", "LN"}:
                    continue
                logical_node_name = _logical_node_name(logical_node)
                data_sets = {item.attrib.get("name"): item for item in _iter_children(logical_node, "DataSet") if item.attrib.get("name")}
                for report in _iter_children(logical_node, "ReportControl"):
                    report_name = report.attrib.get("name", "").strip()
                    if not report_name:
                        continue
                    data_set_name = report.attrib.get("datSet", "").strip()
                    data_set = data_sets.get(data_set_name)
                    signals = _data_set_members(logical_device_inst, data_set) if data_set is not None else ()
                    candidates.append(
                        Iec61850ReportControlCandidate(
                            id=f"{ied_name}:{logical_device_inst}/{logical_node_name}.{report_name}",
                            ied_name=ied_name,
                            access_point_name=access_point_name,
                            logical_device_inst=logical_device_inst,
                            logical_node_name=logical_node_name,
                            report_control_name=report_name,
                            report_kind=Iec61850ReportKind.BUFFERED if _bool_attr(report, "buffered", False) else Iec61850ReportKind.UNBUFFERED,
                            rpt_id=report.attrib.get("rptID"),
                            data_set_ref=f"{ied_name}{logical_device_inst}/{logical_node_name}.{data_set_name}" if data_set_name else None,
                            conf_rev=report.attrib.get("confRev"),
                            indexed=_bool_attr(report, "indexed", True),
                            buffer_time_ms=_int_attr(report, "bufTime"),
                            integrity_period_ms=_int_attr(report, "intgPd"),
                            trigger_options=_trigger_options(report),
                            optional_fields=_optional_fields(report),
                            signals=signals or (Iec61850DataSetMember(reference=f"{logical_device_inst}/{logical_node_name}", fc=None),),
                        ),
                    )
    return tuple(candidates)


def _find_candidate_by_rcb_reference(
    candidates: tuple[Iec61850ReportControlCandidate, ...],
    selected_rcb_ref: str | None,
) -> Iec61850ReportControlCandidate | None:
    if selected_rcb_ref is None:
        return None
    selected = selected_rcb_ref.strip()
    if not selected:
        return None
    for candidate in candidates:
        if candidate.id == selected or _candidate_rcb_reference(candidate) == selected:
            return candidate
    return None


def _build_wire_discovery_structure(endpoint: Iec61850DeviceEndpoint, candidate: Iec61850ReportControlCandidate, command: str) -> dict:
    logical_device_inst = _normalized_live_logical_device_inst(candidate.ied_name, candidate.logical_device_inst)
    signal_items = [{"reference": signal.reference, "fc": signal.fc} for signal in candidate.signals]
    return {
        "schema": "unitlab.iec61850.client.wire-discovery.v1",
        "source": "scd-derived",
        "command": command,
        "endpoint": {
            "id": endpoint.id,
            "mode": endpoint.mode.value,
            "iedName": endpoint.ied_name,
            "accessPointName": endpoint.access_point_name,
            "host": endpoint.host,
            "port": endpoint.port,
        },
        "logicalDevices": [{"iedName": candidate.ied_name, "inst": logical_device_inst, "reference": f"{candidate.ied_name}{logical_device_inst}"}],
        "logicalNodes": [{"logicalDeviceInst": logical_device_inst, "name": candidate.logical_node_name, "reference": f"{candidate.ied_name}{logical_device_inst}/{candidate.logical_node_name}"}],
        "dataSets": [{"reference": candidate.data_set_ref, "members": signal_items, "memberCount": len(signal_items)}],
        "reportControls": [{"id": candidate.id, "name": candidate.report_control_name, "kind": candidate.report_kind.value, "rptId": candidate.rpt_id, "dataSetRef": candidate.data_set_ref, "confRev": candidate.conf_rev, "indexed": candidate.indexed, "bufferTimeMs": candidate.buffer_time_ms, "integrityPeriodMs": candidate.integrity_period_ms}],
        "signals": signal_items,
    }


def _empty_wire_discovery_structure(endpoint: Iec61850DeviceEndpoint, command: str, *, source: str) -> dict:
    return {
        "schema": "unitlab.iec61850.client.wire-discovery.v1",
        "source": source,
        "command": command,
        "endpoint": {
            "id": endpoint.id,
            "mode": endpoint.mode.value,
            "iedName": endpoint.ied_name,
            "accessPointName": endpoint.access_point_name,
            "host": endpoint.host,
            "port": endpoint.port,
        },
        "logicalDevices": [],
        "logicalNodes": [],
        "dataSets": [],
        "reportControls": [],
        "signals": [],
    }


def _ensure_live_discovery_dataset(discovery: dict, reference: str) -> dict:
    data_sets = discovery.setdefault("dataSets", [])
    for item in data_sets:
        if isinstance(item, dict) and item.get("reference") == reference:
            return item
    data_set = {"reference": reference, "members": [], "memberCount": 0}
    data_sets.append(data_set)
    return data_set


def _live_member_signal(member_ref: str) -> dict[str, str]:
    domain, separator, item = member_ref.partition("/")
    reference_item = item if separator else member_ref
    fc = ""
    if "$" in reference_item:
        parts = reference_item.split("$")
        if len(parts) >= 2:
            fc = parts[1]
            path = ".".join(part for part in (parts[0], ".".join(parts[2:])) if part)
        else:
            path = reference_item.replace("$", ".")
    else:
        path = reference_item
    reference = path
    if fc:
        reference = f"{reference}[{fc}]"
    return {"reference": reference, "mmsReference": member_ref, "domain": domain if separator else "", "fc": fc}


def _first_live_discovery_dataset_ref(discovery: dict, domain: str) -> str | None:
    data_sets = discovery.get("dataSets")
    if not isinstance(data_sets, list):
        return None
    for item in data_sets:
        if isinstance(item, dict):
            reference = item.get("reference")
            if isinstance(reference, str) and reference.startswith(f"{domain}/"):
                return reference
    return None


def _infer_live_logical_device_inst_from_discovery(discovery: dict) -> str:
    for key in ("signals", "dataSets"):
        items = discovery.get(key)
        if not isinstance(items, list):
            continue
        for item in items:
            if not isinstance(item, dict):
                continue
            if key == "signals":
                for reference_key in ("mmsReference", "reference"):
                    reference = item.get(reference_key)
                    if not isinstance(reference, str) or "/" not in reference:
                        continue
                    logical_device_inst = _split_live_domain_identity(reference.split("/", 1)[0].strip())[1]
                    if logical_device_inst:
                        return logical_device_inst
            else:
                members = item.get("members")
                if not isinstance(members, list):
                    continue
                for member in members:
                    if not isinstance(member, dict):
                        continue
                    for reference_key in ("mmsReference", "reference"):
                        reference = member.get(reference_key)
                        if not isinstance(reference, str) or "/" not in reference:
                            continue
                        logical_device_inst = _split_live_domain_identity(reference.split("/", 1)[0].strip())[1]
                        if logical_device_inst:
                            return logical_device_inst
    return ""


def _infer_live_ied_name_from_discovery(discovery: dict, logical_device_inst: str, discovered_domain: str) -> str:
    if logical_device_inst and discovered_domain.endswith(logical_device_inst):
        candidate = discovered_domain[: -len(logical_device_inst)]
        if candidate:
            return candidate
    prefix, suffix = _split_live_domain_identity(discovered_domain)
    if prefix and suffix:
        return prefix
    logical_devices = discovery.get("logicalDevices")
    if isinstance(logical_devices, list):
        device_references = [
            item.get("reference")
            for item in logical_devices
            if isinstance(item, dict) and isinstance(item.get("reference"), str)
        ]
        if device_references:
            common_prefix = os.path.commonprefix(device_references).rstrip("._-/")
            if common_prefix:
                return common_prefix
    return ""


def _split_live_domain_identity(domain: str) -> tuple[str, str]:
    normalized = domain.strip()
    if not normalized:
        return "", ""
    match = re.match(r"^(.*?)([A-Z][A-Z0-9]{1,})$", normalized)
    if match is None:
        return "", normalized
    prefix = match.group(1).strip()
    suffix = match.group(2).strip()
    if not prefix:
        return "", normalized
    return prefix, suffix


def _infer_live_identity(endpoint_ied_name: str, discovered_domain: str, discovery: dict) -> tuple[str, str]:
    normalized_endpoint_ied_name = endpoint_ied_name.strip()
    if normalized_endpoint_ied_name:
        return normalized_endpoint_ied_name, _live_logical_device_inst(normalized_endpoint_ied_name, discovered_domain)
    logical_device_inst = _infer_live_logical_device_inst_from_discovery(discovery)
    inferred_ied_name = _infer_live_ied_name_from_discovery(discovery, logical_device_inst, discovered_domain)
    if inferred_ied_name:
        return inferred_ied_name, logical_device_inst or _live_logical_device_inst(inferred_ied_name, discovered_domain)
    if logical_device_inst and discovered_domain.endswith(logical_device_inst):
        inferred_ied_name = discovered_domain[: -len(logical_device_inst)]
        if inferred_ied_name:
            return inferred_ied_name, logical_device_inst
    return discovered_domain, logical_device_inst or discovered_domain


def _candidate_from_live_discovered_rcb(
    *,
    endpoint: Iec61850DeviceEndpoint,
    discovered: _ExternalDiscoveredReportControl,
    report_control_name: str,
    data_set_ref: str | None,
    discovery: dict,
) -> Iec61850ReportControlCandidate:
    logical_node_name, report_kind = _live_rcb_logical_node_and_kind(discovered.item)
    inferred_ied_name, logical_device_inst = _infer_live_identity(endpoint.ied_name, discovered.domain, discovery)
    logical_device_inst = _normalized_live_logical_device_inst(inferred_ied_name, logical_device_inst)
    signals = _live_candidate_signals(discovery, data_set_ref)
    return Iec61850ReportControlCandidate(
        id=f"{discovered.domain}:{discovered.item}",
        ied_name=inferred_ied_name,
        access_point_name=endpoint.access_point_name,
        logical_device_inst=logical_device_inst,
        logical_node_name=logical_node_name,
        report_control_name=report_control_name,
        report_kind=report_kind,
        rpt_id=f"{discovered.domain}/{logical_node_name}.{report_control_name}",
        data_set_ref=data_set_ref,
        conf_rev=None,
        indexed=discovered.item.rsplit("$", 1)[-1] != report_control_name,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(
            data_change=True,
            quality_change=True,
            data_update=True,
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
        signals=signals,
    )


def _normalized_live_logical_device_inst(ied_name: str, logical_device_inst: str) -> str:
    normalized = logical_device_inst.strip()
    normalized_ied_name = ied_name.strip()
    if normalized_ied_name and normalized.startswith(normalized_ied_name):
        suffix = normalized[len(normalized_ied_name):]
        return suffix or normalized
    return normalized


def _apply_live_rcb_attr(report_control: dict, field: str, value: str) -> None:
    if field == "RptID":
        report_control["rptId"] = value
    elif field == "DatSet":
        report_control["dataSetRef"] = value
    elif field == "ConfRev":
        report_control["confRev"] = value
    elif field == "BufTm":
        report_control["bufferTimeMs"] = _parse_int_or_none(value)
    elif field == "IntgPd":
        report_control["integrityPeriodMs"] = _parse_int_or_none(value)
    elif field == "OptFlds":
        report_control["optFlds"] = value
        report_control["optionalFields"] = _optional_fields_payload(_optional_fields_from_mms_bit_string(value))
    elif field == "TrgOps":
        report_control["trgOps"] = value
        report_control["triggerOptions"] = _trigger_options_payload(_trigger_options_from_mms_bit_string(value))


def _candidate_with_live_rcb_attr(
    candidate: Iec61850ReportControlCandidate,
    field: str,
    value: str,
) -> Iec61850ReportControlCandidate:
    kwargs = {
        "id": candidate.id,
        "ied_name": candidate.ied_name,
        "access_point_name": candidate.access_point_name,
        "logical_device_inst": candidate.logical_device_inst,
        "logical_node_name": candidate.logical_node_name,
        "report_control_name": candidate.report_control_name,
        "report_kind": candidate.report_kind,
        "rpt_id": candidate.rpt_id,
        "data_set_ref": candidate.data_set_ref,
        "conf_rev": candidate.conf_rev,
        "indexed": candidate.indexed,
        "buffer_time_ms": candidate.buffer_time_ms,
        "integrity_period_ms": candidate.integrity_period_ms,
        "trigger_options": candidate.trigger_options,
        "optional_fields": candidate.optional_fields,
        "signals": candidate.signals,
    }
    if field == "RptID":
        kwargs["rpt_id"] = value
    elif field == "DatSet":
        kwargs["data_set_ref"] = value
    elif field == "ConfRev":
        kwargs["conf_rev"] = value
    elif field == "BufTm":
        kwargs["buffer_time_ms"] = _parse_int_or_none(value)
    elif field == "IntgPd":
        kwargs["integrity_period_ms"] = _parse_int_or_none(value)
    elif field == "OptFlds":
        kwargs["optional_fields"] = _optional_fields_from_mms_bit_string(value)
    elif field == "TrgOps":
        kwargs["trigger_options"] = _trigger_options_from_mms_bit_string(value)
    return Iec61850ReportControlCandidate(**kwargs)


def _trigger_options_payload(options: Iec61850RuntimeTriggerOptions) -> dict[str, bool | None]:
    return {
        "data_change": options.data_change,
        "quality_change": options.quality_change,
        "data_update": options.data_update,
        "periodic": options.periodic,
        "general_interrogation": options.general_interrogation,
    }


def _optional_fields_payload(fields: Iec61850OptionalFields) -> dict[str, bool | None]:
    return {
        "sequence_number": fields.sequence_number,
        "timestamp": fields.timestamp,
        "reason_code": fields.reason_code,
        "data_set_name": fields.data_set_name,
        "data_reference": fields.data_reference,
        "entry_id": fields.entry_id,
        "config_revision": fields.config_revision,
        "buffer_overflow": fields.buffer_overflow,
    }


def _trigger_options_from_mms_bit_string(value: str) -> Iec61850RuntimeTriggerOptions:
    bytes_value = _parse_mms_hex_value(value)
    mask = bytes_value[1] if len(bytes_value) >= 2 else 0
    return Iec61850RuntimeTriggerOptions(
        data_change=(mask & 0x40) != 0,
        quality_change=(mask & 0x20) != 0,
        data_update=(mask & 0x10) != 0,
        periodic=(mask & 0x08) != 0,
        general_interrogation=(mask & 0x04) != 0,
    )


def _optional_fields_from_mms_bit_string(value: str) -> Iec61850OptionalFields:
    bytes_value = _parse_mms_hex_value(value)
    first = bytes_value[1] if len(bytes_value) >= 2 else 0
    second = bytes_value[2] if len(bytes_value) >= 3 else 0
    return Iec61850OptionalFields(
        sequence_number=(first & 0x40) != 0,
        timestamp=(first & 0x20) != 0,
        reason_code=(first & 0x10) != 0,
        data_set_name=(first & 0x08) != 0,
        data_reference=(first & 0x04) != 0,
        buffer_overflow=(first & 0x02) != 0,
        entry_id=(first & 0x01) != 0,
        config_revision=(second & 0x80) != 0,
    )


def _parse_mms_hex_value(value: str) -> bytes:
    normalized = value.strip()
    if normalized.startswith("0x"):
        normalized = normalized[2:]
    if len(normalized) % 2 != 0:
        return b""
    try:
        return bytes.fromhex(normalized)
    except ValueError:
        return b""


def _live_rcb_logical_node_and_kind(item: str) -> tuple[str, Iec61850ReportKind]:
    parts = item.split("$")
    logical_node_name = parts[0] if parts else "LLN0"
    report_kind = Iec61850ReportKind.UNBUFFERED if len(parts) > 1 and parts[1] == "RP" else Iec61850ReportKind.BUFFERED
    return logical_node_name, report_kind


def _live_logical_device_inst(ied_name: str, domain: str) -> str:
    if ied_name and domain.startswith(ied_name):
        suffix = domain[len(ied_name):]
        return suffix or domain
    return domain


def _live_candidate_signals(discovery: dict, data_set_ref: str | None) -> tuple[Iec61850DataSetMember, ...]:
    if data_set_ref is not None:
        data_sets = discovery.get("dataSets")
        if isinstance(data_sets, list):
            for data_set in data_sets:
                if not isinstance(data_set, dict) or data_set.get("reference") != data_set_ref:
                    continue
                members = data_set.get("members")
                if isinstance(members, list):
                    signals = tuple(
                        Iec61850DataSetMember(
                            reference=str(member.get("reference")),
                            fc=str(member.get("fc")) if member.get("fc") else None,
                        )
                        for member in members
                        if isinstance(member, dict) and member.get("reference")
                    )
                    if signals:
                        return signals
    return (Iec61850DataSetMember(reference=data_set_ref or "<live-discovered-dataset>", fc=None),)


def _replace_or_append_candidate(
    candidates: tuple[Iec61850ReportControlCandidate, ...],
    candidate: Iec61850ReportControlCandidate,
) -> tuple[Iec61850ReportControlCandidate, ...]:
    replaced: list[Iec61850ReportControlCandidate] = []
    found = False
    for item in candidates:
        if item.id == candidate.id or _candidate_rcb_reference(item) == _candidate_rcb_reference(candidate):
            if not found:
                replaced.append(candidate)
                found = True
            continue
        replaced.append(item)
    if not found:
        replaced.append(candidate)
    return tuple(replaced)


def _data_set_members(default_ld_inst: str, data_set: ET.Element | None) -> tuple[Iec61850DataSetMember, ...]:
    if data_set is None:
        return ()
    members: list[Iec61850DataSetMember] = []
    for fcda in _iter_children(data_set, "FCDA"):
        ld_inst = fcda.attrib.get("ldInst") or default_ld_inst
        ln_class = fcda.attrib.get("lnClass", "")
        ln_inst = fcda.attrib.get("lnInst", "")
        prefix = fcda.attrib.get("prefix", "")
        ln_name = f"{prefix}{ln_class}{ln_inst}"
        do_name = fcda.attrib.get("doName", "")
        da_name = fcda.attrib.get("daName", "")
        fc = fcda.attrib.get("fc")
        object_name = do_name if not da_name else f"{do_name}.{da_name}"
        reference = f"{ld_inst}/{ln_name}.{object_name}[{fc}]" if object_name and fc else f"{ld_inst}/{ln_name}"
        members.append(Iec61850DataSetMember(reference=reference, fc=fc))
    return tuple(members)


def _trigger_options(report: ET.Element) -> Iec61850RuntimeTriggerOptions:
    trg_ops = _first_child(report, "TrgOps")
    return Iec61850RuntimeTriggerOptions(
        data_change=_bool_attr(trg_ops, "dchg", None),
        quality_change=_bool_attr(trg_ops, "qchg", None),
        data_update=_bool_attr(trg_ops, "dupd", None),
        periodic=_bool_attr(trg_ops, "period", None),
        general_interrogation=_bool_attr(trg_ops, "gi", None),
    )


def _optional_fields(report: ET.Element) -> Iec61850OptionalFields:
    opt_fields = _first_child(report, "OptFields")
    return Iec61850OptionalFields(
        sequence_number=_bool_attr(opt_fields, "seqNum", None),
        timestamp=_bool_attr(opt_fields, "timeStamp", None),
        reason_code=_bool_attr(opt_fields, "reasonCode", None),
        data_set_name=_bool_attr(opt_fields, "dataSet", None),
        data_reference=_bool_attr(opt_fields, "dataRef", None),
        entry_id=_bool_attr(opt_fields, "entryID", None),
        config_revision=_bool_attr(opt_fields, "configRef", None),
        buffer_overflow=_bool_attr(opt_fields, "bufOvfl", None),
    )


def _selected_signal_address(candidate: Iec61850ReportControlCandidate) -> str:
    first = candidate.signals[0].reference if candidate.signals else f"{candidate.logical_device_inst}/{candidate.logical_node_name}"
    return f"{candidate.ied_name}{first}"


def _logical_node_name(element: ET.Element) -> str:
    if _local_name(element.tag) == "LN0":
        return "LLN0"
    return f"{element.attrib.get('prefix', '')}{element.attrib.get('lnClass', '')}{element.attrib.get('inst', '')}"


def _bool_attr(element: ET.Element | None, name: str, default: bool | None) -> bool | None:
    if element is None or name not in element.attrib:
        return default
    value = element.attrib[name].strip().lower()
    if value in {"true", "1"}:
        return True
    if value in {"false", "0"}:
        return False
    return default


def _int_attr(element: ET.Element, name: str) -> int | None:
    value = element.attrib.get(name)
    if value is None or not value.strip():
        return None
    try:
        return int(value)
    except ValueError:
        return None


def _find_child_by_attr(element: ET.Element, tag: str, attr: str, value: str) -> ET.Element | None:
    for child in element.iter():
        if _local_name(child.tag) == tag and child.attrib.get(attr) == value:
            return child
    return None


def _first_child(element: ET.Element, tag: str) -> ET.Element | None:
    for child in _iter_children(element, tag):
        return child
    return None


def _iter_children(element: ET.Element, tag: str | None):
    for child in list(element):
        if tag is None or _local_name(child.tag) == tag:
            yield child


def _local_name(tag: str) -> str:
    if "}" in tag:
        return tag.rsplit("}", 1)[1]
    return tag


def _compact_probe_output(result) -> str:
    output = ((result.stdout or "") + (result.stderr or "")).strip()
    if not output:
        return "probe completed"
    lines = [line.strip() for line in output.splitlines() if line.strip()]
    return " | ".join(lines[:8])


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


def _external_unselected_candidate(endpoint: Iec61850DeviceEndpoint) -> Iec61850ReportControlCandidate:
    return Iec61850ReportControlCandidate(
        id=f"{endpoint.id}:unselected",
        ied_name=endpoint.ied_name,
        access_point_name=endpoint.access_point_name,
        logical_device_inst="",
        logical_node_name="",
        report_control_name="",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id=None,
        data_set_ref=None,
        conf_rev=None,
        indexed=None,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(),
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
                                selected_signal=Iec61850SelectedSignal(id="sig-1", address=_selected_signal_address(candidate)),
                                model_reference=candidate.signals[0].reference if candidate.signals else f"{candidate.logical_device_inst}/{candidate.logical_node_name}",
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
