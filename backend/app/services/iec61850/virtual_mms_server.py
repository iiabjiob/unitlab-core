from __future__ import annotations

import json
import os
import socket
import subprocess
import tempfile
from collections import deque
from dataclasses import dataclass
from pathlib import Path
from threading import RLock, Thread

from app.core.config import REPO_ROOT, get_settings

from .ied_simulator_process import wait_ied_native_wire_server_ready
from .report_runtime import Iec61850ReportRuntimeError
from .scl_import import Iec61850SclImportRecord



@dataclass(frozen=True, slots=True)
class Iec61850VirtualMmsRuntimeStatus:
    running: bool
    data_client_connected: bool
    data_client: str
    report_enabled: bool
    active_report: str
    active_report_key: str
    report_kind: str
    report_id_reference: str
    data_set_ref: str
    data_set_reference: str
    owner: str
    pending_report_kind: str
    pending_report_queue_count: int
    reports_sent: int
    report_events_queued: int


@dataclass(frozen=True, slots=True)
class Iec61850VirtualMmsSignalUpdateResult:
    ok: bool
    object_reference: str
    value_kind: str
    value: str
    report_queued: bool
    report_sent: bool
    pending_report_kind: str
    message: str

@dataclass(frozen=True, slots=True)
class Iec61850VirtualMmsServerSnapshot:
    running: bool
    import_id: str | None
    selected_ied: str | None
    source_hash: str | None
    host: str | None
    port: int | None
    pid: int | None
    fixture_path: str | None
    binary_path: str | None
    message: str | None = None


class Iec61850VirtualMmsServerService:
    def __init__(self) -> None:
        self._lock = RLock()
        self._logs: deque[str] = deque(maxlen=5000)
        self._log_threads: list[Thread] = []
        self._process: subprocess.Popen[str] | None = None
        self._source_dir: tempfile.TemporaryDirectory[str] | None = None
        self._source_path: str | None = None
        self._binary_path: str | None = None
        self._import_id: str | None = None
        self._selected_ied: str | None = None
        self._source_hash: str | None = None
        self._host: str | None = None
        self._port: int | None = None
        self._message: str | None = None

    def snapshot(self) -> Iec61850VirtualMmsServerSnapshot:
        with self._lock:
            return self._snapshot_locked()

    def start(
        self,
        record: Iec61850SclImportRecord,
        *,
        source_bytes: bytes,
        host: str = "0.0.0.0",
        port: int = 12447,
    ) -> Iec61850VirtualMmsServerSnapshot:
        with self._lock:
            if self._process is not None:
                self.stop()
            if not source_bytes:
                raise Iec61850ReportRuntimeError("VIRTUAL_MMS_SOURCE_EMPTY", "SCL source bytes are required to start a virtual MMS server.")

            source_dir = tempfile.TemporaryDirectory(prefix="unitlab-iec61850-virtual-mms-")
            try:
                source_path = Path(source_dir.name) / f"{record.selected_ied}.scd"
                source_path.write_bytes(source_bytes)
                binary_path = _resolve_ied_simulator_binary_path()
                _validate_native_binary_path(binary_path)
                command = _scl_native_wire_command(binary_path, source_path, record.selected_ied, host, port)
                _run_startup_check(command)
                try:
                    process = subprocess.Popen(
                        command,
                        stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                        text=True,
                        bufsize=1,
                    )
                except OSError as exc:
                    raise Iec61850ReportRuntimeError("VIRTUAL_MMS_PROCESS_START_FAILED", f"Virtual MMS server process could not be started: {exc}") from exc
                wait_ied_native_wire_server_ready(None, process, timeout_seconds=10.0)  # type: ignore[arg-type]
            except Exception:
                source_dir.cleanup()
                raise

            self._process = process
            self._start_log_drain(process)
            self._source_dir = source_dir
            self._source_path = str(source_path)
            self._binary_path = str(binary_path)
            self._import_id = record.import_id
            self._selected_ied = record.selected_ied
            self._source_hash = record.source_hash
            self._host = host
            self._port = port
            self._message = f"Virtual MMS server listening on {host}:{port}."
            return self._snapshot_locked()

    def stop(self) -> Iec61850VirtualMmsServerSnapshot:
        with self._lock:
            process = self._process
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5.0)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5.0)
            if self._source_dir is not None:
                self._source_dir.cleanup()
            self._process = None
            self._log_threads = []
            self._source_dir = None
            self._source_path = None
            self._message = "Virtual MMS server stopped."
            return self._snapshot_locked()


    def logs(self) -> tuple[str, ...]:
        with self._lock:
            return tuple(self._logs)

    def runtime_status(self) -> Iec61850VirtualMmsRuntimeStatus:
        payload = self._send_control_json("status")
        return _runtime_status_from_payload(payload)

    def update_signal(self, *, object_reference: str, value_kind: str, value: object) -> Iec61850VirtualMmsSignalUpdateResult:
        normalized_reference = object_reference.strip()
        normalized_kind = _normalize_signal_value_kind(value_kind)
        normalized_value = _normalize_signal_value(normalized_kind, value)
        if not normalized_reference:
            raise Iec61850ReportRuntimeError("VIRTUAL_MMS_SIGNAL_REFERENCE_EMPTY", "Signal object reference is required.")
        command = "update-signal {kind} {reference_hex} {value_hex}".format(
            kind=normalized_kind,
            reference_hex=normalized_reference.encode("utf-8").hex(),
            value_hex=normalized_value.encode("utf-8").hex(),
        )
        payload = self._send_control_json(command)
        if not bool(payload.get("ok")):
            raise Iec61850ReportRuntimeError(
                str(payload.get("code") or "VIRTUAL_MMS_SIGNAL_UPDATE_FAILED"),
                str(payload.get("message") or "Virtual MMS signal update failed."),
            )
        return Iec61850VirtualMmsSignalUpdateResult(
            ok=True,
            object_reference=str(payload.get("objectReference") or normalized_reference),
            value_kind=str(payload.get("valueKind") or normalized_kind),
            value=str(payload.get("value") or normalized_value),
            report_queued=bool(payload.get("reportQueued")),
            report_sent=bool(payload.get("reportSent")),
            pending_report_kind=str(payload.get("pendingReportKind") or "none"),
            message=str(payload.get("message") or "signal updated"),
        )

    def _send_control_json(self, command: str, *, timeout_seconds: float = 3.0) -> dict:
        response = self._send_control_command(command, timeout_seconds=timeout_seconds)
        try:
            payload = json.loads(response)
        except json.JSONDecodeError as exc:
            raise Iec61850ReportRuntimeError("VIRTUAL_MMS_CONTROL_RESPONSE_INVALID", f"Virtual MMS control response is not JSON: {response}") from exc
        if not isinstance(payload, dict):
            raise Iec61850ReportRuntimeError("VIRTUAL_MMS_CONTROL_RESPONSE_INVALID", "Virtual MMS control response must be a JSON object.")
        if not bool(payload.get("ok")):
            raise Iec61850ReportRuntimeError(
                str(payload.get("code") or "VIRTUAL_MMS_CONTROL_FAILED"),
                str(payload.get("message") or "Virtual MMS control command failed."),
            )
        return payload

    def _send_control_command(self, command: str, *, timeout_seconds: float) -> str:
        with self._lock:
            process = self._process
            if process is None or process.poll() is not None:
                raise Iec61850ReportRuntimeError("VIRTUAL_MMS_NOT_RUNNING", "Virtual MMS server is not running.")
            host = _control_connect_host(self._host)
            port = _control_port(self._port)

        if port is None:
            raise Iec61850ReportRuntimeError("VIRTUAL_MMS_CONTROL_PORT_UNAVAILABLE", "Virtual MMS control port is not available.")

        try:
            with socket.create_connection((host, port), timeout=timeout_seconds) as control_socket:
                control_socket.settimeout(timeout_seconds)
                control_socket.sendall(command.encode("utf-8") + b"\n")
                try:
                    control_socket.shutdown(socket.SHUT_WR)
                except OSError:
                    pass
                chunks: list[bytes] = []
                while True:
                    chunk = control_socket.recv(4096)
                    if not chunk:
                        break
                    chunks.append(chunk)
                    if b"\n" in chunk:
                        break
        except OSError as exc:
            raise Iec61850ReportRuntimeError("VIRTUAL_MMS_CONTROL_REQUEST_FAILED", f"Virtual MMS control command failed: {exc}") from exc

        raw = b"".join(chunks).split(b"\n", 1)[0].decode("utf-8", errors="replace").strip()
        if not raw:
            raise Iec61850ReportRuntimeError("VIRTUAL_MMS_CONTROL_RESPONSE_EMPTY", "Virtual MMS control command returned an empty response.")
        return raw

    def _start_log_drain(self, process: subprocess.Popen[str]) -> None:
        self._logs.clear()
        self._log_threads = []
        if process.stdout is not None:
            self._log_threads.append(self._start_stream_drain(process.stdout, "stdout"))
        if process.stderr is not None:
            self._log_threads.append(self._start_stream_drain(process.stderr, "stderr"))

    def _start_stream_drain(self, stream, name: str) -> Thread:
        thread = Thread(target=self._drain_stream, args=(stream, name), daemon=True)
        thread.start()
        return thread

    def _drain_stream(self, stream, name: str) -> None:
        try:
            for line in iter(stream.readline, ""):
                text = line.rstrip("\r\n")
                if not text:
                    continue
                with self._lock:
                    self._logs.append(f"{name}: {text}")
        except Exception as exc:  # pragma: no cover - defensive runtime logging path
            with self._lock:
                self._logs.append(f"{name}: <log-drain-error {exc}>")

    def _snapshot_locked(self) -> Iec61850VirtualMmsServerSnapshot:
        process = self._process
        running = process is not None and process.poll() is None
        if process is not None and not running:
            self._message = f"Virtual MMS server process exited with code {process.returncode}."
        return Iec61850VirtualMmsServerSnapshot(
            running=running,
            import_id=self._import_id,
            selected_ied=self._selected_ied if running else None,
            source_hash=self._source_hash,
            host=self._host if running else None,
            port=self._port if running else None,
            pid=process.pid if process is not None and running else None,
            fixture_path=self._source_path if running else None,
            binary_path=self._binary_path if running else None,
            message=self._message,
        )


def _scl_native_wire_command(binary_path: Path, source_path: Path, selected_ied: str, host: str, port: int) -> tuple[str, ...]:
    return (
        str(binary_path),
        "--scl",
        str(source_path),
        "--ied",
        selected_ied,
        "--bind",
        host,
        "--port",
        str(port),
        "--native-wire-start",
    )


def _run_startup_check(command: tuple[str, ...]) -> None:
    dry_run_command = tuple(part for part in command if part != "--native-wire-start") + ("--dry-run",)
    try:
        completed = subprocess.run(dry_run_command, capture_output=True, text=True, timeout=15.0, check=False)
    except subprocess.TimeoutExpired as exc:
        raise Iec61850ReportRuntimeError("VIRTUAL_MMS_STARTUP_CHECK_TIMEOUT", "Virtual MMS server SCL dry-run check timed out.") from exc
    except OSError as exc:
        raise Iec61850ReportRuntimeError("VIRTUAL_MMS_STARTUP_CHECK_FAILED", f"Virtual MMS server SCL dry-run check could not be started: {exc}") from exc
    if completed.returncode != 0:
        details = (completed.stderr or completed.stdout).strip()
        raise Iec61850ReportRuntimeError("VIRTUAL_MMS_STARTUP_CHECK_FAILED", f"Virtual MMS server SCL dry-run check failed: {details}")


def _validate_native_binary_path(binary_path: Path) -> None:
    if not binary_path.exists():
        raise Iec61850ReportRuntimeError(
            "VIRTUAL_MMS_BINARY_NOT_FOUND",
            f"IEC 61850 virtual MMS binary was not found: {binary_path}",
        )
    if not binary_path.is_file():
        raise Iec61850ReportRuntimeError(
            "VIRTUAL_MMS_BINARY_INVALID",
            f"IEC 61850 virtual MMS binary path is not a file: {binary_path}",
        )
    if not os.access(binary_path, os.X_OK):
        raise Iec61850ReportRuntimeError(
            "VIRTUAL_MMS_BINARY_NOT_EXECUTABLE",
            f"IEC 61850 virtual MMS binary is not executable: {binary_path}",
        )


def _resolve_ied_simulator_binary_path() -> Path:
    settings = get_settings()
    configured = (getattr(settings, "iec61850_ied_live_wire_binary_path", None) or "").strip()
    if configured:
        return Path(configured)
    return REPO_ROOT.parent / "iec61850_ied" / "build" / "unitlab-iec61850-ied-sim"




def _control_connect_host(host: str | None) -> str:
    normalized = (host or "").strip()
    if normalized in {"", "0.0.0.0", "::", "[::]"}:
        return "127.0.0.1"
    return normalized


def _control_port(port: int | None) -> int | None:
    if port is None or port <= 0 or port >= 65535:
        return None
    return port + 1


def _normalize_signal_value_kind(value_kind: str) -> str:
    normalized = value_kind.strip().lower().replace("_", "-")
    aliases = {
        "bool": "boolean",
        "boolean": "boolean",
        "int": "integer",
        "int32": "integer",
        "integer": "integer",
        "enum": "enum",
        "real": "real",
        "float": "real",
        "float32": "real",
        "string": "string",
        "visible-string": "string",
        "visible_string": "string",
    }
    resolved = aliases.get(normalized)
    if resolved is None:
        raise Iec61850ReportRuntimeError("VIRTUAL_MMS_SIGNAL_VALUE_KIND_UNSUPPORTED", f"Unsupported signal value kind: {value_kind}")
    return resolved


def _normalize_signal_value(value_kind: str, value: object) -> str:
    if value_kind == "boolean":
        if isinstance(value, bool):
            return "true" if value else "false"
        normalized = str(value).strip().lower()
        if normalized in {"true", "1", "on"}:
            return "true"
        if normalized in {"false", "0", "off"}:
            return "false"
        raise Iec61850ReportRuntimeError("VIRTUAL_MMS_SIGNAL_BOOLEAN_INVALID", "Boolean signal value must be true/false or 1/0.")
    if value_kind in {"integer", "enum"}:
        try:
            parsed = int(str(value).strip(), 10)
        except ValueError as exc:
            raise Iec61850ReportRuntimeError("VIRTUAL_MMS_SIGNAL_INTEGER_INVALID", "Integer signal value must be a base-10 integer.") from exc
        if parsed < -(2**31) or parsed > 2**31 - 1:
            raise Iec61850ReportRuntimeError("VIRTUAL_MMS_SIGNAL_INTEGER_INVALID", "Integer signal value must fit int32.")
        return str(parsed)
    if value_kind == "real":
        text = str(value).strip()
        try:
            parsed = float(text)
        except ValueError as exc:
            raise Iec61850ReportRuntimeError("VIRTUAL_MMS_SIGNAL_REAL_INVALID", "Real signal value must be numeric.") from exc
        if parsed != parsed or parsed in {float("inf"), float("-inf")}:
            raise Iec61850ReportRuntimeError("VIRTUAL_MMS_SIGNAL_REAL_INVALID", "Real signal value must be finite.")
        return text
    return str(value)


def _runtime_status_from_payload(payload: dict) -> Iec61850VirtualMmsRuntimeStatus:
    return Iec61850VirtualMmsRuntimeStatus(
        running=True,
        data_client_connected=bool(payload.get("dataClientConnected")),
        data_client=str(payload.get("dataClient") or ""),
        report_enabled=bool(payload.get("reportEnabled")),
        active_report=str(payload.get("activeReport") or ""),
        active_report_key=str(payload.get("activeReportKey") or ""),
        report_kind=str(payload.get("reportKind") or ""),
        report_id_reference=str(payload.get("reportIdReference") or ""),
        data_set_ref=str(payload.get("dataSetRef") or ""),
        data_set_reference=str(payload.get("dataSetReference") or ""),
        owner=str(payload.get("owner") or ""),
        pending_report_kind=str(payload.get("pendingReportKind") or "none"),
        pending_report_queue_count=_int_payload(payload.get("pendingReportQueueCount")),
        reports_sent=_int_payload(payload.get("reportsSent")),
        report_events_queued=_int_payload(payload.get("reportEventsQueued")),
    )


def _int_payload(value: object) -> int:
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, int):
        return value
    try:
        return int(str(value), 10)
    except (TypeError, ValueError):
        return 0

_virtual_mms_server_service = Iec61850VirtualMmsServerService()


def get_virtual_mms_server_service() -> Iec61850VirtualMmsServerService:
    return _virtual_mms_server_service
