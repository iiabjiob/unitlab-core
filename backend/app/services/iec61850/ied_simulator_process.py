from __future__ import annotations

import json
import socket
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
    Iec61850ReportRuntimeAdapter,
    Iec61850ReportRuntimeError,
    Iec61850ReportSubscriptionRunResult,
    Iec61850ReportSubscriptionPlan,
    Iec61850ReportSubscriptionPlanDevice,
    Iec61850RuntimeMode,
    run_report_subscription_plan,
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
    def base_command(self) -> tuple[str, ...]:
        return (
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

    @property
    def command(self) -> tuple[str, ...]:
        command = self.base_command
        if self.dry_run:
            return (*command, "--dry-run")
        return command

    @property
    def metadata_probe_command(self) -> tuple[str, ...]:
        return (*self.base_command, "--metadata-probe")

    @property
    def gi_probe_command(self) -> tuple[str, ...]:
        return (*self.base_command, "--gi-probe")

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


@dataclass(frozen=True, slots=True)
class Iec61850IedSimulatorProcessPlanRunResult:
    process_plan: Iec61850IedSimulatorProcessPlan
    subscription_run: Iec61850ReportSubscriptionRunResult
    stop_results: tuple[Iec61850IedSimulatorProcessStopResult, ...]


ProcessRunner = Callable[..., subprocess.CompletedProcess[str]]
ProcessFactory = Callable[..., subprocess.Popen[str]]
SleepFn = Callable[[float], None]
SocketConnector = Callable[[tuple[str, int], float], object]


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


def run_ied_simulator_metadata_probe(
    spec: Iec61850IedSimulatorProcessSpec,
    *,
    timeout_seconds: float = 5.0,
    runner: ProcessRunner = subprocess.run,
) -> Iec61850IedSimulatorProcessResult:
    if spec.dry_run:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_METADATA_PROBE_DRY_RUN_SPEC",
            "IEC 61850 IED simulator metadata probe requires a non-dry-run process spec.",
        )
    try:
        completed = runner(
            spec.metadata_probe_command,
            capture_output=True,
            text=True,
            timeout=timeout_seconds,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_METADATA_PROBE_TIMEOUT",
            f"IEC 61850 IED simulator metadata probe timed out after {timeout_seconds:g}s.",
        ) from exc

    result = Iec61850IedSimulatorProcessResult(
        command=spec.metadata_probe_command,
        return_code=completed.returncode,
        stdout=completed.stdout or "",
        stderr=completed.stderr or "",
    )
    if completed.returncode != 0:
        details = (result.stderr or result.stdout).strip()
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_METADATA_PROBE_FAILED",
            f"IEC 61850 IED simulator metadata probe failed with exit code {completed.returncode}: {details}",
        )
    return result


def run_ied_simulator_gi_probe(
    spec: Iec61850IedSimulatorProcessSpec,
    *,
    timeout_seconds: float = 5.0,
    runner: ProcessRunner = subprocess.run,
) -> Iec61850IedSimulatorProcessResult:
    if spec.dry_run:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_GI_PROBE_DRY_RUN_SPEC",
            "IEC 61850 IED simulator GI probe requires a non-dry-run process spec.",
        )
    try:
        completed = runner(
            spec.gi_probe_command,
            capture_output=True,
            text=True,
            timeout=timeout_seconds,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_GI_PROBE_TIMEOUT",
            f"IEC 61850 IED simulator GI probe timed out after {timeout_seconds:g}s.",
        ) from exc

    result = Iec61850IedSimulatorProcessResult(
        command=spec.gi_probe_command,
        return_code=completed.returncode,
        stdout=completed.stdout or "",
        stderr=completed.stderr or "",
    )
    if completed.returncode != 0:
        details = (result.stderr or result.stdout).strip()
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_GI_PROBE_FAILED",
            f"IEC 61850 IED simulator GI probe failed with exit code {completed.returncode}: {details}",
        )
    return result


def wait_ied_simulator_process_ready(
    spec: Iec61850IedSimulatorProcessSpec,
    process: subprocess.Popen[str],
    *,
    timeout_seconds: float = 5.0,
    retry_interval_seconds: float = 0.05,
    connector: SocketConnector = socket.create_connection,
    sleep: SleepFn = time.sleep,
) -> None:
    endpoint = spec.endpoint
    if endpoint.mode != Iec61850RuntimeMode.MMS:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_ENDPOINT_MODE_INVALID",
            "IEC 61850 IED simulator readiness requires an MMS endpoint.",
        )
    if not endpoint.host:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_ENDPOINT_HOST_INVALID",
            "IEC 61850 IED simulator readiness requires an endpoint host.",
        )
    if endpoint.port <= 0 or endpoint.port > 65535:
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_ENDPOINT_PORT_INVALID",
            f"IEC 61850 IED simulator readiness requires a valid TCP port, got {endpoint.port}.",
        )

    deadline = time.monotonic() + max(timeout_seconds, 0.0)
    last_error: OSError | None = None
    while True:
        return_code = process.poll()
        if return_code is not None:
            stdout, stderr = _communicate_finished_process(process)
            details = (stderr or stdout).strip()
            raise Iec61850ReportRuntimeError(
                "SIMULATOR_PROCESS_EXITED",
                f"IEC 61850 IED simulator process exited before endpoint readiness with code {return_code}: {details}",
            )

        remaining = max(deadline - time.monotonic(), 0.0)
        try:
            connection = connector((endpoint.host, endpoint.port), min(remaining, 1.0))
            close = getattr(connection, "close", None)
            if callable(close):
                close()
            return
        except OSError as exc:
            last_error = exc

        remaining = deadline - time.monotonic()
        if remaining <= 0:
            details = f": {last_error}" if last_error is not None else ""
            raise Iec61850ReportRuntimeError(
                "SIMULATOR_ENDPOINT_READY_TIMEOUT",
                f"IEC 61850 IED simulator endpoint {endpoint.host}:{endpoint.port} did not become reachable within {timeout_seconds:g}s{details}.",
            )
        sleep(min(retry_interval_seconds, remaining))


def start_ied_simulator_process(
    spec: Iec61850IedSimulatorProcessSpec,
    *,
    startup_check_timeout_seconds: float = 5.0,
    startup_grace_seconds: float = 0.1,
    readiness_timeout_seconds: float = 5.0,
    readiness_retry_interval_seconds: float = 0.05,
    metadata_probe_timeout_seconds: float = 5.0,
    runner: ProcessRunner = subprocess.run,
    process_factory: ProcessFactory = subprocess.Popen,
    readiness_connector: SocketConnector = socket.create_connection,
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

    handle = Iec61850IedSimulatorProcessHandle(
        spec=spec,
        endpoint=spec.endpoint,
        pid=process.pid,
        process=process,
    )
    try:
        wait_ied_simulator_process_ready(
            spec,
            process,
            timeout_seconds=readiness_timeout_seconds,
            retry_interval_seconds=readiness_retry_interval_seconds,
            connector=readiness_connector,
            sleep=sleep,
        )
        run_ied_simulator_metadata_probe(
            spec,
            timeout_seconds=metadata_probe_timeout_seconds,
            runner=runner,
        )
    except Exception:
        stop_ied_simulator_process(handle)
        raise

    return handle


def start_ied_simulator_process_plan(
    plan: Iec61850IedSimulatorProcessPlan,
    *,
    startup_check_timeout_seconds: float = 5.0,
    startup_grace_seconds: float = 0.1,
    readiness_timeout_seconds: float = 5.0,
    readiness_retry_interval_seconds: float = 0.05,
    metadata_probe_timeout_seconds: float = 5.0,
    terminate_timeout_seconds: float = 5.0,
    runner: ProcessRunner = subprocess.run,
    process_factory: ProcessFactory = subprocess.Popen,
    readiness_connector: SocketConnector = socket.create_connection,
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
                    readiness_timeout_seconds=readiness_timeout_seconds,
                    readiness_retry_interval_seconds=readiness_retry_interval_seconds,
                    metadata_probe_timeout_seconds=metadata_probe_timeout_seconds,
                    runner=runner,
                    process_factory=process_factory,
                    readiness_connector=readiness_connector,
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


def run_report_subscription_plan_with_external_ied_simulators(
    *,
    subscription_plan: Iec61850ReportSubscriptionPlan,
    adapter: Iec61850ReportRuntimeAdapter,
    client_id: str,
    binary_path: str | Path,
    fixture_path: str | Path,
    bind_address: str = "127.0.0.1",
    base_port: int = 1102,
    startup_check_timeout_seconds: float = 5.0,
    startup_grace_seconds: float = 0.1,
    readiness_timeout_seconds: float = 5.0,
    readiness_retry_interval_seconds: float = 0.05,
    metadata_probe_timeout_seconds: float = 5.0,
    terminate_timeout_seconds: float = 5.0,
    runner: ProcessRunner = subprocess.run,
    process_factory: ProcessFactory = subprocess.Popen,
    readiness_connector: SocketConnector = socket.create_connection,
    sleep: SleepFn = time.sleep,
) -> Iec61850IedSimulatorProcessPlanRunResult:
    process_plan = prepare_ied_simulator_process_plan_from_subscription_plan(
        subscription_plan=subscription_plan,
        binary_path=binary_path,
        fixture_path=fixture_path,
        bind_address=bind_address,
        base_port=base_port,
    )
    handles = start_ied_simulator_process_plan(
        process_plan,
        startup_check_timeout_seconds=startup_check_timeout_seconds,
        startup_grace_seconds=startup_grace_seconds,
        readiness_timeout_seconds=readiness_timeout_seconds,
        readiness_retry_interval_seconds=readiness_retry_interval_seconds,
        metadata_probe_timeout_seconds=metadata_probe_timeout_seconds,
        terminate_timeout_seconds=terminate_timeout_seconds,
        runner=runner,
        process_factory=process_factory,
        readiness_connector=readiness_connector,
        sleep=sleep,
    )
    stop_results: tuple[Iec61850IedSimulatorProcessStopResult, ...] = ()
    try:
        subscription_run = run_report_subscription_plan(
            plan=subscription_plan,
            adapter=adapter,
            client_id=client_id,
            endpoint_for_device=process_plan.endpoint_for_plan_device,
        )
    finally:
        stop_results = stop_ied_simulator_processes(
            tuple(reversed(handles)),
            terminate_timeout_seconds=terminate_timeout_seconds,
        )
    return Iec61850IedSimulatorProcessPlanRunResult(
        process_plan=process_plan,
        subscription_run=subscription_run,
        stop_results=stop_results,
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
