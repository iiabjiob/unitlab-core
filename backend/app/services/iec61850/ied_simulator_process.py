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
    build_ied_simulator_fixture_from_subscription_plan,
    ied_simulator_fixture_to_payload,
)
from .report_runtime import (
    Iec61850DeviceEndpoint,
    Iec61850ReportRuntimeError,
    Iec61850ReportSubscriptionPlan,
    Iec61850ReportSubscriptionPlanDevice,
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


@dataclass(frozen=True, slots=True)
class Iec61850IedSimulatorProcessPlan:
    fixture_path: str
    specs: tuple[Iec61850IedSimulatorProcessSpec, ...]

    @property
    def endpoints(self) -> tuple[Iec61850DeviceEndpoint, ...]:
        return tuple(spec.endpoint for spec in self.specs)

    def endpoint_for_plan_device(self, device: Iec61850ReportSubscriptionPlanDevice) -> Iec61850DeviceEndpoint:
        matches = tuple(
            spec.endpoint
            for spec in self.specs
            if _endpoint_key(spec.ied_name, spec.access_point_name) == _endpoint_key(device.ied_name, device.access_point_name)
        )
        if not matches:
            raise Iec61850ReportRuntimeError(
                "SIMULATOR_PROCESS_ENDPOINT_NOT_CONFIGURED",
                f'IEC 61850 simulator process endpoint for "{device.ied_name}/{device.access_point_name}" is not configured.',
            )
        if len(matches) > 1:
            raise Iec61850ReportRuntimeError(
                "SIMULATOR_PROCESS_ENDPOINT_DUPLICATE",
                f'IEC 61850 simulator process endpoint for "{device.ied_name}/{device.access_point_name}" is configured more than once.',
            )
        return matches[0]


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


def prepare_ied_simulator_process_plan(
    *,
    fixture: Iec61850IedSimulatorFixture,
    binary_path: str | Path,
    fixture_path: str | Path,
    bind_address: str = "127.0.0.1",
    base_port: int = 1102,
    dry_run: bool = False,
) -> Iec61850IedSimulatorProcessPlan:
    if not fixture.devices:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_FIXTURE_EMPTY",
            "IEC 61850 IED simulator process plan requires at least one fixture device.",
        )
    if base_port <= 0 or base_port > 65535:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_PORT_INVALID",
            f"IEC 61850 IED simulator base port {base_port} is outside range 1..65535.",
        )
    final_port = base_port + len(fixture.devices) - 1
    if final_port > 65535:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_PORT_RANGE_INVALID",
            f"IEC 61850 IED simulator port range {base_port}..{final_port} exceeds 65535.",
        )

    written_fixture_path = write_ied_simulator_fixture_file(fixture, fixture_path)
    specs = tuple(
        build_ied_simulator_process_spec(
            fixture=fixture,
            binary_path=binary_path,
            fixture_path=written_fixture_path,
            ied_name=device.ied_name,
            bind_address=bind_address,
            port=base_port + index,
            dry_run=dry_run,
        )
        for index, device in enumerate(fixture.devices)
    )
    return Iec61850IedSimulatorProcessPlan(
        fixture_path=str(written_fixture_path),
        specs=specs,
    )


def prepare_ied_simulator_process_plan_from_subscription_plan(
    *,
    subscription_plan: Iec61850ReportSubscriptionPlan,
    binary_path: str | Path,
    fixture_path: str | Path,
    bind_address: str = "127.0.0.1",
    base_port: int = 1102,
    dry_run: bool = False,
) -> Iec61850IedSimulatorProcessPlan:
    fixture = build_ied_simulator_fixture_from_subscription_plan(subscription_plan)
    return prepare_ied_simulator_process_plan(
        fixture=fixture,
        binary_path=binary_path,
        fixture_path=fixture_path,
        bind_address=bind_address,
        base_port=base_port,
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


def run_ied_simulator_process_plan_startup_checks(
    plan: Iec61850IedSimulatorProcessPlan,
    *,
    timeout_seconds: float = 5.0,
    runner: ProcessRunner = subprocess.run,
) -> tuple[Iec61850IedSimulatorProcessResult, ...]:
    return tuple(
        run_ied_simulator_startup_check(
            spec,
            timeout_seconds=timeout_seconds,
            runner=runner,
        )
        for spec in plan.specs
    )


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


def start_ied_simulator_process_plan(
    plan: Iec61850IedSimulatorProcessPlan,
    *,
    startup_check_timeout_seconds: float = 5.0,
    startup_grace_seconds: float = 0.1,
    terminate_timeout_seconds: float = 5.0,
    runner: ProcessRunner = subprocess.run,
    process_factory: ProcessFactory = subprocess.Popen,
    sleep: SleepFn = time.sleep,
) -> tuple[Iec61850IedSimulatorProcessHandle, ...]:
    handles: list[Iec61850IedSimulatorProcessHandle] = []
    try:
        for spec in plan.specs:
            handles.append(
                start_ied_simulator_process(
                    spec,
                    startup_check_timeout_seconds=startup_check_timeout_seconds,
                    startup_grace_seconds=startup_grace_seconds,
                    runner=runner,
                    process_factory=process_factory,
                    sleep=sleep,
                )
            )
    except Exception:
        stop_ied_simulator_processes(
            tuple(reversed(handles)),
            terminate_timeout_seconds=terminate_timeout_seconds,
        )
        raise
    return tuple(handles)


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


def stop_ied_simulator_processes(
    handles: Sequence[Iec61850IedSimulatorProcessHandle],
    *,
    terminate_timeout_seconds: float = 5.0,
) -> tuple[Iec61850IedSimulatorProcessStopResult, ...]:
    return tuple(
        stop_ied_simulator_process(
            handle,
            terminate_timeout_seconds=terminate_timeout_seconds,
        )
        for handle in handles
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


def _endpoint_key(ied_name: str, access_point_name: str) -> tuple[str, str]:
    return (ied_name.strip().lower(), access_point_name.strip().lower())
