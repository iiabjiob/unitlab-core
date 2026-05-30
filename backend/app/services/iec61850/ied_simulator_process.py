from __future__ import annotations

import json
import subprocess
import time
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Callable, Sequence

from .ied_simulator_fixture import (
    Iec61850IedSimulatorFixture,
    Iec61850IedSimulatorFixtureDevice,
    ied_simulator_fixture_to_payload,
)
from .report_runtime import (
    Iec61850DeviceEndpoint,
    Iec61850ReportRuntimeError,
    Iec61850RuntimeMode,
)


@dataclass(frozen=True, slots=True)
class Iec61850IedSimulatorProcessSpec:
    binary_path: str
    fixture_path: str
    ied_name: str
    access_point_name: str
    bind_address: str
    port: int
    dry_run: bool = False

    @property
    def command(self) -> tuple[str, ...]:
        command: tuple[str, ...] = (
            self.binary_path,
            "--fixture",
            self.fixture_path,
            "--ied",
            self.ied_name,
            "--bind",
            self.bind_address,
            "--port",
            str(self.port),
        )
        if self.dry_run:
            return (*command, "--dry-run")
        return command

    @property
    def endpoint(self) -> Iec61850DeviceEndpoint:
        return Iec61850DeviceEndpoint(
            id=f"mms-simulator:{self.ied_name}/{self.access_point_name}@{self.bind_address}:{self.port}",
            mode=Iec61850RuntimeMode.MMS,
            ied_name=self.ied_name,
            access_point_name=self.access_point_name,
            host=self.bind_address,
            port=self.port,
        )


@dataclass(frozen=True, slots=True)
class Iec61850IedSimulatorProcessResult:
    command: tuple[str, ...]
    return_code: int
    stdout: str
    stderr: str


@dataclass(frozen=True, slots=True)
class Iec61850IedSimulatorProcessHandle:
    spec: Iec61850IedSimulatorProcessSpec
    endpoint: Iec61850DeviceEndpoint
    pid: int
    process: subprocess.Popen[str]


@dataclass(frozen=True, slots=True)
class Iec61850IedSimulatorProcessStopResult:
    pid: int
    return_code: int | None
    killed: bool


ProcessRunner = Callable[..., subprocess.CompletedProcess[str]]
ProcessFactory = Callable[..., subprocess.Popen[str]]
SleepFn = Callable[[float], None]


def write_ied_simulator_fixture_file(
    fixture: Iec61850IedSimulatorFixture,
    fixture_path: str | Path,
) -> Path:
    path = Path(fixture_path)
    if not path.name:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_FIXTURE_PATH_INVALID",
            "IEC 61850 IED simulator fixture path must include a file name.",
        )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(ied_simulator_fixture_to_payload(fixture), indent=2, sort_keys=True),
        encoding="utf-8",
    )
    return path


def build_ied_simulator_process_spec(
    *,
    fixture: Iec61850IedSimulatorFixture,
    binary_path: str | Path,
    fixture_path: str | Path,
    ied_name: str,
    bind_address: str = "127.0.0.1",
    port: int = 1102,
    dry_run: bool = False,
) -> Iec61850IedSimulatorProcessSpec:
    binary_text = str(binary_path).strip()
    if not binary_text:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_BINARY_PATH_INVALID",
            "IEC 61850 IED simulator binary path is required.",
        )
    binary = Path(binary_text)
    if not binary.is_file():
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_BINARY_NOT_FOUND",
            f'IEC 61850 IED simulator binary "{binary}" was not found.',
        )
    if not str(fixture_path).strip():
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_FIXTURE_PATH_INVALID",
            "IEC 61850 IED simulator fixture path is required.",
        )
    if not ied_name.strip():
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_IED_NAME_INVALID",
            "IEC 61850 IED simulator requires a selected IED name.",
        )
    if not bind_address.strip():
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_BIND_ADDRESS_INVALID",
            "IEC 61850 IED simulator bind address is required.",
        )
    if port <= 0 or port > 65535:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_PORT_INVALID",
            f"IEC 61850 IED simulator port {port} is outside range 1..65535.",
        )

    device = _find_fixture_device(fixture.devices, ied_name)
    return Iec61850IedSimulatorProcessSpec(
        binary_path=str(binary),
        fixture_path=str(Path(fixture_path)),
        ied_name=device.ied_name,
        access_point_name=device.access_point_name,
        bind_address=bind_address,
        port=port,
        dry_run=dry_run,
    )


def run_ied_simulator_startup_check(
    spec: Iec61850IedSimulatorProcessSpec,
    *,
    timeout_seconds: float = 5.0,
    runner: ProcessRunner = subprocess.run,
) -> Iec61850IedSimulatorProcessResult:
    check_spec = spec if spec.dry_run else replace(spec, dry_run=True)
    try:
        completed = runner(
            check_spec.command,
            capture_output=True,
            text=True,
            timeout=timeout_seconds,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_PROCESS_CHECK_TIMEOUT",
            f"IEC 61850 IED simulator dry-run check timed out after {timeout_seconds:g}s.",
        ) from exc

    result = Iec61850IedSimulatorProcessResult(
        command=check_spec.command,
        return_code=completed.returncode,
        stdout=completed.stdout or "",
        stderr=completed.stderr or "",
    )
    if completed.returncode != 0:
        details = (result.stderr or result.stdout).strip()
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_PROCESS_CHECK_FAILED",
            f"IEC 61850 IED simulator dry-run check failed with exit code {completed.returncode}: {details}",
        )
    return result


def start_ied_simulator_process(
    spec: Iec61850IedSimulatorProcessSpec,
    *,
    startup_check_timeout_seconds: float = 5.0,
    startup_grace_seconds: float = 0.1,
    runner: ProcessRunner = subprocess.run,
    process_factory: ProcessFactory = subprocess.Popen,
    sleep: SleepFn = time.sleep,
) -> Iec61850IedSimulatorProcessHandle:
    if spec.dry_run:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_PROCESS_START_DRY_RUN_SPEC",
            "IEC 61850 IED simulator process start requires a non-dry-run process spec.",
        )

    run_ied_simulator_startup_check(
        spec,
        timeout_seconds=startup_check_timeout_seconds,
        runner=runner,
    )
    try:
        process = process_factory(
            spec.command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
    except OSError as exc:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_PROCESS_START_FAILED",
            f"IEC 61850 IED simulator process could not be started: {exc}",
        ) from exc

    if startup_grace_seconds > 0:
        sleep(startup_grace_seconds)
    return_code = process.poll()
    if return_code is not None:
        stdout, stderr = _communicate_finished_process(process)
        details = (stderr or stdout).strip()
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_PROCESS_EXITED",
            f"IEC 61850 IED simulator process exited during startup with code {return_code}: {details}",
        )

    return Iec61850IedSimulatorProcessHandle(
        spec=spec,
        endpoint=spec.endpoint,
        pid=process.pid,
        process=process,
    )


def stop_ied_simulator_process(
    handle: Iec61850IedSimulatorProcessHandle,
    *,
    terminate_timeout_seconds: float = 5.0,
) -> Iec61850IedSimulatorProcessStopResult:
    return_code = handle.process.poll()
    killed = False
    if return_code is None:
        handle.process.terminate()
        try:
            return_code = handle.process.wait(timeout=terminate_timeout_seconds)
        except subprocess.TimeoutExpired:
            handle.process.kill()
            killed = True
            return_code = handle.process.wait(timeout=terminate_timeout_seconds)

    return Iec61850IedSimulatorProcessStopResult(
        pid=handle.pid,
        return_code=return_code,
        killed=killed,
    )


def _communicate_finished_process(process: subprocess.Popen[str]) -> tuple[str, str]:
    stdout, stderr = process.communicate()
    return stdout or "", stderr or ""


def _find_fixture_device(
    devices: Sequence[Iec61850IedSimulatorFixtureDevice],
    ied_name: str,
) -> Iec61850IedSimulatorFixtureDevice:
    requested_name = ied_name.strip().lower()
    for device in devices:
        if device.ied_name.strip().lower() == requested_name:
            return device
    raise Iec61850ReportRuntimeError(
        "SIMULATOR_DEVICE_NOT_IN_FIXTURE",
        f'IEC 61850 IED simulator fixture does not contain selected IED "{ied_name}".',
    )
