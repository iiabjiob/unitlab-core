from __future__ import annotations

from dataclasses import dataclass, replace
from datetime import UTC, datetime
import os
import time
from pathlib import Path
import select
from threading import RLock
from typing import Callable, Sequence
import tempfile

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
        self._last_read: Iec61850ReportControlReadResult | None = None
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

    def _start_live_wire_process_transport(self) -> None:
        subscription_plan = _build_subscription_plan(self._candidate)
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
            ied_name=self._candidate.ied_name,
            bind_address=self._live_wire_service_host,
            port=self._live_wire_data_port,
            native_wire_client_start=True,
        )
        process_handle: Iec61850IedSimulatorProcessHandle | None = None
        try:
            process_handle = start_ied_simulator_process(spec)
            self._live_wire_process = process_handle
            self._live_wire_fixture_dir = fixture_dir
            self._live_wire_endpoint = spec.endpoint
            self._transcript_wire_endpoint_id = spec.endpoint.id
            self._runtime._append_event(
                kind="wire-session-open",
                session_id=self._session_id,
                endpoint_id=spec.endpoint.id,
                client_id=self._client_id,
                outcome="connected",
            )
            self._runtime._append_event(
                kind="wire-associate",
                session_id=self._session_id,
                endpoint_id=spec.endpoint.id,
                client_id=self._client_id,
                outcome="associated",
            )
            self._live_wire_last_frame = None
            self._live_wire_last_diagnostic = None
            self._runtime._append_event(
                kind="wire-client-ready",
                session_id=self._session_id,
                endpoint_id=spec.endpoint.id,
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

    def _drain_live_wire_process_stdout(self) -> None:
        if self._live_wire_process is None:
            return
        deadline = time.monotonic()
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
