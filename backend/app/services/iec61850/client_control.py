from __future__ import annotations

from dataclasses import dataclass, replace
from datetime import UTC, datetime
import os
import subprocess
import time
from pathlib import Path
import select
from threading import RLock
from typing import Callable, Sequence
import tempfile
import xml.etree.ElementTree as ET

from app.core.config import get_settings

from .client_runtime import Iec61850MmsClientEvent, Iec61850MmsClientRuntime
from .ied_simulator_fixture import build_ied_simulator_fixture_from_subscription_plan
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
class Iec61850ClientTargetRequest:
    mode: str
    host: str
    port: int
    ied_name: str
    scl_path: str | None = None
    access_point_name: str = "AP1"


@dataclass(frozen=True, slots=True)
class Iec61850ClientControlSnapshot:
    session_id: str
    client_id: str
    session_open: bool
    endpoint: Iec61850DeviceEndpoint
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


class Iec61850ClientControlService:
    def __init__(
        self,
        *,
        now: Callable[[], datetime] | None = None,
        session_id: str = "iec61850-client-test",
        client_id: str = "unitlab-test-client",
        endpoint: Iec61850DeviceEndpoint | None = None,
        candidate: Iec61850ReportControlCandidate | None = None,
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
        self._candidate = candidate or _default_candidate()
        self._target_scl_path: str | None = None
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
        self._live_wire_process: Iec61850IedSimulatorProcessHandle | None = None
        self._live_wire_fixture_dir: tempfile.TemporaryDirectory[str] | None = None
        self._live_wire_endpoint: Iec61850DeviceEndpoint | None = None
        self._transcript_wire_endpoint_id: str | None = None
        self._live_wire_last_frame: bytes | None = None
        self._live_wire_last_diagnostic: Iec61850ClientControlDiagnostic | None = None
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
                last_discovery=self._last_discovery,
                last_state=self._last_state,
                last_report=self._last_report,
                last_plan=self._last_plan,
                transcript=self._project_transcript(self._runtime.transcript()),
                last_diagnostic=self._last_diagnostic,
                live_wire_open=self._live_wire_process is not None,
                live_wire_control_open=(self._live_wire_process is not None and self._live_wire_process.process.stdin is not None),
                live_wire_endpoint=self._live_wire_endpoint,
                live_wire_last_frame_length=len(self._live_wire_last_frame) if self._live_wire_last_frame is not None else None,
                live_wire_last_frame_hex=self._live_wire_last_frame.hex() if self._live_wire_last_frame is not None else None,
                live_wire_last_diagnostic=self._live_wire_last_diagnostic,
            )

    def configure_target(self, request: Iec61850ClientTargetRequest) -> Iec61850ClientControlSnapshot:
        with self._lock:
            self._reset_runtime_state_for_target_change()
            endpoint, candidate = _build_target_endpoint_and_candidate(request)
            self._endpoint = endpoint
            self._candidate = candidate
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
                message=f"Configured IEC 61850 client target {endpoint.ied_name}@{endpoint.host}:{endpoint.port}.",
            )
            return self.snapshot()

    def open_session(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            return self._run(
                "open-session",
                lambda: self._runtime.open_session(session_id=self._session_id, endpoint=self._endpoint, candidates=[self._candidate]),
                post=lambda _result: setattr(self, "_session_open", True),
            )

    def discover_ied(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
            if self._endpoint.mode == Iec61850RuntimeMode.MMS:
                return self._run("external-discover-ied", self._discover_external_mms_ied)
            return self._run("discover-ied", self._discover_ied)

    def connect_ied(self) -> Iec61850ClientControlSnapshot:
        with self._lock:
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
                lambda: self._runtime.open_session(session_id=self._session_id, endpoint=self._endpoint, candidates=[self._candidate]),
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
            if self._live_wire_process is not None:
                self._stop_live_wire_transport()
            if self._session_open:
                self._runtime.close_session(self._session_id)
            self._session_open = False
            self._last_read = None
            self._last_discovery = None
            self._last_state = None
            self._last_report = None
            self._last_plan = None
            self._last_diagnostic = None
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
            self._runtime.open_session(session_id=self._session_id, endpoint=self._endpoint, candidates=[self._candidate])
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
        result = self._run_external_probe("metadata")
        self._last_discovery = _build_wire_discovery_structure(self._endpoint, self._candidate, "metadata-probe")
        self._runtime._append_event(
            kind="external-ied-discover",
            session_id=self._session_id,
            endpoint_id=self._endpoint.id,
            candidate_id=self._candidate.id,
            report_control_name=self._candidate.report_control_name,
            client_id=self._client_id,
            outcome="accepted",
            code=str(result.returncode),
            message=_compact_probe_output(result),
        )

    def _enable_external_mms_reporting(self) -> None:
        result = self._run_external_probe("metadata")
        self._runtime._append_event(
            kind="external-report-control-precheck",
            session_id=self._session_id,
            endpoint_id=self._endpoint.id,
            candidate_id=self._candidate.id,
            report_control_name=self._candidate.report_control_name,
            client_id=self._client_id,
            outcome="accepted",
            code=str(result.returncode),
            message=_compact_probe_output(result),
        )

    def _send_external_mms_general_interrogation(self) -> None:
        result = self._run_external_probe("gi")
        self._runtime._append_event(
            kind="external-report-control-gi",
            session_id=self._session_id,
            endpoint_id=self._endpoint.id,
            candidate_id=self._candidate.id,
            report_control_name=self._candidate.report_control_name,
            client_id=self._client_id,
            outcome="accepted",
            code=str(result.returncode),
            message=_compact_probe_output(result),
        )

    def _disconnect_external_mms_ied(self) -> None:
        self._runtime._append_event(
            kind="external-ied-disconnect",
            session_id=self._session_id,
            endpoint_id=self._endpoint.id,
            candidate_id=self._candidate.id,
            report_control_name=self._candidate.report_control_name,
            client_id=self._client_id,
            outcome="not-persistent",
            message="External MMS probe commands open and close their own client association.",
        )

    def _external_probe_binary_path(self) -> str:
        preferred = Path("/workspace/iec61850_ied/build-libiec61850/unitlab-iec61850-ied-sim")
        if preferred.is_file():
            return str(preferred)
        relative_preferred = Path("iec61850_ied/build-libiec61850/unitlab-iec61850-ied-sim")
        if relative_preferred.is_file():
            return str(relative_preferred)
        if self._live_wire_binary_path is not None and self._live_wire_binary_path.strip():
            return self._live_wire_binary_path
        return str(relative_preferred)

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
            command.extend(("--report-key", self._candidate.id))
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
        if self._live_wire_process is not None:
            self._stop_live_wire_transport()
        if self._session_open:
            self._runtime.close_session(self._session_id)
        self._session_open = False
        self._last_read = None
        self._last_discovery = None
        self._last_state = None
        self._last_report = None
        self._last_plan = None
        self._last_diagnostic = None
        self._live_wire_last_frame = None
        self._live_wire_last_diagnostic = None

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


def get_iec61850_client_control_service() -> Iec61850ClientControlService:
    return _CLIENT_CONTROL_SERVICE

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
        if remaining <= 0:
            return None

        readable, _, _ = select.select([raw_fd], [], [], min(remaining, 1.0))
        if not readable:
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


def _build_target_endpoint_and_candidate(request: Iec61850ClientTargetRequest) -> tuple[Iec61850DeviceEndpoint, Iec61850ReportControlCandidate]:
    mode = request.mode.strip().lower()
    if mode == "simulator":
        return _default_endpoint(), _default_candidate()
    if mode not in {"mms", "external-mms"}:
        raise Iec61850ReportRuntimeError("CLIENT_TARGET_MODE_INVALID", "IEC 61850 client target mode must be simulator or external-mms.")
    host = request.host.strip()
    ied_name = request.ied_name.strip()
    if not host:
        raise Iec61850ReportRuntimeError("CLIENT_TARGET_HOST_REQUIRED", "IEC 61850 external MMS target host is required.")
    if request.port <= 0 or request.port > 65535:
        raise Iec61850ReportRuntimeError("CLIENT_TARGET_PORT_INVALID", "IEC 61850 external MMS target port must be in range 1..65535.")
    if not ied_name:
        raise Iec61850ReportRuntimeError("CLIENT_TARGET_IED_REQUIRED", "IEC 61850 external MMS target IED name is required.")
    if request.scl_path is None or not request.scl_path.strip():
        raise Iec61850ReportRuntimeError("CLIENT_TARGET_SCL_REQUIRED", "IEC 61850 external MMS target requires an SCD/SCL path.")
    scl_path = Path(request.scl_path.strip())
    if not scl_path.is_file():
        raise Iec61850ReportRuntimeError("CLIENT_TARGET_SCL_NOT_FOUND", f"IEC 61850 SCD/SCL file was not found: {scl_path}.")
    candidate = _candidate_from_scd(scl_path, ied_name, request.access_point_name.strip() or "AP1")
    endpoint = Iec61850DeviceEndpoint(
        id=f"mms:{ied_name}@{host}:{request.port}",
        mode=Iec61850RuntimeMode.MMS,
        ied_name=ied_name,
        access_point_name=candidate.access_point_name,
        host=host,
        port=request.port,
    )
    return endpoint, candidate


def _candidate_from_scd(scl_path: Path, ied_name: str, fallback_access_point: str) -> Iec61850ReportControlCandidate:
    try:
        root = ET.parse(scl_path).getroot()
    except ET.ParseError as exc:
        raise Iec61850ReportRuntimeError("CLIENT_TARGET_SCL_PARSE_FAILED", f"IEC 61850 SCD/SCL parse failed: {exc}.") from exc

    ied = _find_child_by_attr(root, "IED", "name", ied_name)
    if ied is None:
        raise Iec61850ReportRuntimeError("CLIENT_TARGET_IED_NOT_FOUND", f'IED "{ied_name}" was not found in {scl_path}.')

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
                    return Iec61850ReportControlCandidate(
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
                    )
    raise Iec61850ReportRuntimeError("CLIENT_TARGET_REPORT_CONTROL_NOT_FOUND", f'IED "{ied_name}" has no ReportControl in {scl_path}.')


def _build_wire_discovery_structure(endpoint: Iec61850DeviceEndpoint, candidate: Iec61850ReportControlCandidate, command: str) -> dict:
    signal_items = [{"reference": signal.reference, "fc": signal.fc} for signal in candidate.signals]
    return {
        "schema": "unitlab.iec61850.client.wire-discovery.v1",
        "command": command,
        "endpoint": {
            "id": endpoint.id,
            "mode": endpoint.mode.value,
            "iedName": endpoint.ied_name,
            "accessPointName": endpoint.access_point_name,
            "host": endpoint.host,
            "port": endpoint.port,
        },
        "logicalDevices": [{"iedName": candidate.ied_name, "inst": candidate.logical_device_inst, "reference": f"{candidate.ied_name}{candidate.logical_device_inst}"}],
        "logicalNodes": [{"logicalDeviceInst": candidate.logical_device_inst, "name": candidate.logical_node_name, "reference": f"{candidate.ied_name}{candidate.logical_device_inst}/{candidate.logical_node_name}"}],
        "dataSets": [{"reference": candidate.data_set_ref, "members": signal_items, "memberCount": len(signal_items)}],
        "reportControls": [{"id": candidate.id, "name": candidate.report_control_name, "kind": candidate.report_kind.value, "rptId": candidate.rpt_id, "dataSetRef": candidate.data_set_ref, "confRev": candidate.conf_rev, "indexed": candidate.indexed, "bufferTimeMs": candidate.buffer_time_ms, "integrityPeriodMs": candidate.integrity_period_ms}],
        "signals": signal_items,
    }


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
