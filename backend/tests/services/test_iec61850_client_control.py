from __future__ import annotations

from datetime import UTC, datetime

import pytest

import app.services.iec61850.client_control as client_control_module
from app.core.config import get_settings
from app.services.iec61850.client_control import Iec61850ClientControlService
from app.services.iec61850.report_runtime import Iec61850ReportReason, Iec61850ReportRuntimeError, Iec61850RuntimeStatus


READ_RESPONSE_FRAME = bytes.fromhex("0300001d02f080010001006110610e300c020103a407a105a0030201ff")
REPORT_FRAME = bytes.fromhex("0300001402f0800100010040076305a003810100")


def _wire_frame_response_line(frame: bytes) -> bytes:
    return f"wire-frame={frame.hex()}\n".encode("utf-8")


class _QueuedStdout:
    def __init__(self) -> None:
        self.lines: list[str] = []

    def enqueue(self, frame: bytes) -> None:
        self.lines.append(_wire_frame_response_line(frame).decode("utf-8"))

    def readline(self) -> str:
        if not self.lines:
            return ""
        return self.lines.pop(0)


class _CommandDrivenProcessStdin:
    def __init__(self, commands: list[str], stdout: _QueuedStdout) -> None:
        self._commands = commands
        self._stdout = stdout

    def write(self, value: str) -> None:
        command = value.rstrip("\n")
        self._commands.append(command)
        if command == "emit-report":
            self._stdout.lines.append("native-wire-client: state=report-requested\n")
            self._stdout.enqueue(REPORT_FRAME)
            self._stdout.lines.append("native-wire-client: state=ready\n")

    def flush(self) -> None:
        return None


class _CommandDrivenSelect:
    @staticmethod
    def select(readable, writable, exceptional, timeout=None):
        ready = []
        for item in readable:
            if hasattr(item, "lines") and getattr(item, "lines"):
                ready.append(item)
        return ready, [], []


def test_client_control_service_runs_full_demo_report_loop() -> None:
    service = Iec61850ClientControlService(now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC))

    state = service.snapshot()
    assert state.session_open is False
    assert state.last_diagnostic is None

    state = service.open_session()
    assert state.session_open is True

    state = service.read_report_control()
    assert state.last_read is not None
    assert state.last_state is not None
    assert state.last_state.runtime_status == Iec61850RuntimeStatus.READ

    state = service.reserve_report_control()
    assert state.last_state is not None
    assert state.last_state.runtime_status == Iec61850RuntimeStatus.RESERVED

    state = service.enable_report_control()
    assert state.last_state is not None
    assert state.last_state.runtime_status == Iec61850RuntimeStatus.ENABLED

    state = service.send_general_interrogation()
    assert state.last_report is not None
    assert state.last_report.reason == Iec61850ReportReason.GENERAL_INTERROGATION

    state = service.disable_report_control()
    assert state.last_state is not None
    assert state.last_state.runtime_status == Iec61850RuntimeStatus.DISABLED

    state = service.release_report_control()
    assert state.last_state is not None
    assert state.last_state.runtime_status == Iec61850RuntimeStatus.RELEASED

    state = service.close_session()
    assert state.session_open is False
    assert [event.kind for event in state.transcript] == [
        "session-open",
        "report-control-read",
        "report-control-reserve",
        "report-control-enable",
        "report-control-gi",
        "report-control-disable",
        "report-control-release",
        "session-close",
    ]


def test_client_control_service_surfaces_last_diagnostic_on_duplicate_open() -> None:
    service = Iec61850ClientControlService(now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC))
    service.open_session()

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        service.open_session()

    assert error.value.code == "SESSION_EXISTS"
    assert service.snapshot().last_diagnostic is not None
    assert service.snapshot().last_diagnostic.code == "SESSION_EXISTS"


def test_client_control_service_uses_env_live_wire_binary_path(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("IEC61850_IED_LIVE_WIRE_BINARY_PATH", "/bin/true")
    get_settings.cache_clear()
    try:
        service = Iec61850ClientControlService(
            now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC),
            live_wire_service_host="wire-host",
        )

        started_specs: list[object] = []
        process_commands: list[str] = []
        stdout = _QueuedStdout()
        stdout.lines.extend([
            "native-wire-client: state=init\n",
            "native-wire-client: state=data-connected\n",
            "native-wire-client: state=control-connected\n",
            "native-wire-client: state=cotp-connected\n",
            "native-wire-client: state=associating\n",
            "native-wire-client: state=associated\n",
            "native-wire-client: state=ready\n",
            "native-wire-client: ready\n",
            _wire_frame_response_line(READ_RESPONSE_FRAME).decode("utf-8"),
        ])

        class _FakeProcess:
            def __init__(self) -> None:
                self.stdin = _CommandDrivenProcessStdin(process_commands, stdout)
                self.stdout = stdout

        class _FakeHandle:
            def __init__(self, spec) -> None:
                self.spec = spec
                self.endpoint = spec.endpoint
                self.process = _FakeProcess()
                self.pid = 4242

        def fake_start_process(spec, **_kwargs):
            started_specs.append(spec)
            return _FakeHandle(spec)

        monkeypatch.setattr(client_control_module, "select", _CommandDrivenSelect)
        monkeypatch.setattr(client_control_module, "start_ied_simulator_process", fake_start_process)
        monkeypatch.setattr(client_control_module, "stop_ied_simulator_process", lambda handle: None)

        state = service.start_live_wire_transport()

        assert started_specs
        assert started_specs[0].binary_path == "/bin/true"
        assert started_specs[0].native_wire_client_start is True
        assert started_specs[0].bind_address == "wire-host"
        assert state.live_wire_open is True
        assert process_commands == []
        assert state.live_wire_last_frame_length is None
        assert state.live_wire_last_frame_hex is None
        assert [event.kind for event in state.transcript][-3:] == ["wire-session-open", "wire-associate", "wire-client-ready"]

        state = service.emit_live_wire_report()
        assert process_commands == ["emit-report"]
        assert state.live_wire_last_frame_length == len(REPORT_FRAME)
        assert state.live_wire_last_frame_hex == REPORT_FRAME.hex()
    finally:
        get_settings.cache_clear()


def test_client_control_service_can_drive_a_live_wire_transport_smoke(monkeypatch: pytest.MonkeyPatch) -> None:
    service = Iec61850ClientControlService(
        now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC),
        live_wire_binary_path="/bin/true",
        live_wire_service_host="wire-host",
        live_wire_data_port=12346,
    )

    process_commands: list[str] = []
    stdout = _QueuedStdout()
    stdout.lines.extend([
        "native-wire-client: state=init\n",
        "native-wire-client: state=data-connected\n",
        "native-wire-client: state=control-connected\n",
        "native-wire-client: state=cotp-connected\n",
        "native-wire-client: state=associating\n",
        "native-wire-client: state=associated\n",
        "native-wire-client: state=ready\n",
        "native-wire-client: ready\n",
        _wire_frame_response_line(READ_RESPONSE_FRAME).decode("utf-8"),
    ])

    class _FakeProcess:
        def __init__(self) -> None:
            self.stdin = _CommandDrivenProcessStdin(process_commands, stdout)
            self.stdout = stdout

    class _FakeHandle:
        def __init__(self) -> None:
            self.process = _FakeProcess()
            self.pid = 4242
            self.spec = None
            self.endpoint = service.snapshot().endpoint

    def fake_start_process(spec, **_kwargs):
        handle = _FakeHandle()
        handle.spec = spec
        handle.endpoint = spec.endpoint
        return handle

    monkeypatch.setattr(client_control_module, "select", _CommandDrivenSelect)
    monkeypatch.setattr(client_control_module, "start_ied_simulator_process", fake_start_process)
    monkeypatch.setattr(client_control_module, "stop_ied_simulator_process", lambda handle: None)

    state = service.start_live_wire_transport()
    assert state.live_wire_open is True
    assert process_commands == []
    assert state.live_wire_last_frame_length is None
    assert state.live_wire_last_frame_hex is None
    assert state.live_wire_endpoint is not None
    assert state.live_wire_endpoint.host == "wire-host"

    state = service.emit_live_wire_report()
    assert state.live_wire_last_frame_length == len(REPORT_FRAME)
    assert state.live_wire_last_frame_hex == REPORT_FRAME.hex()
    assert [event.kind for event in state.transcript][-4:] == ["wire-session-open", "wire-associate", "wire-client-ready", "wire-report-frame"]
    assert process_commands == ["emit-report"]

    state = service.stop_live_wire_transport()
    assert state.live_wire_open is False
    assert [event.kind for event in state.transcript][-1] == "wire-session-close"
