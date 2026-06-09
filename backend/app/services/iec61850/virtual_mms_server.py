from __future__ import annotations

import os
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from threading import RLock

from app.core.config import REPO_ROOT, get_settings

from .ied_simulator_process import wait_ied_native_wire_server_ready
from .report_runtime import Iec61850ReportRuntimeError
from .scl_import import Iec61850SclImportRecord


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
            self._source_dir = None
            self._source_path = None
            self._message = "Virtual MMS server stopped."
            return self._snapshot_locked()

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


_virtual_mms_server_service = Iec61850VirtualMmsServerService()


def get_virtual_mms_server_service() -> Iec61850VirtualMmsServerService:
    return _virtual_mms_server_service
