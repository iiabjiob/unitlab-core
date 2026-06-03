from __future__ import annotations

from datetime import UTC, datetime

import pytest

from app.core.config import get_settings
from app.services.iec61850.client_control import Iec61850ClientControlService
import app.services.iec61850.client_control as client_control_module
from app.services.iec61850.report_runtime import Iec61850ReportReason, Iec61850ReportRuntimeError, Iec61850RuntimeStatus


COTP_CONNECT_REQUEST_FRAME = bytes.fromhex("0300001611e00000000100c0010dc2020001c1020001")
ASSOCIATION_REQUEST_FRAME = bytes.fromhex("030000b002f080010001006181a230819f020103a08199a18196020103ac8190800100a1818a302ba029a1271a144d793734366965644d6561737572656d656e74731a0f4262704d44494631245354244d6f64302ba029a1271a144d793734366965644d6561737572656d656e74731a0f4262704d4449463124535424426568302ea02ca12a1a144d793734366965644d6561737572656d656e74731a124262704d44494631245354244865616c7468")
CONFIRMED_READ_REQUEST_FRAME = bytes.fromhex("0300003502f0800100010061286026020103a421301fa11da01b3019a017a1151a0558434252311a0c535424506f7324737456616c")
COTP_CONNECT_RESPONSE_FRAME = bytes.fromhex("0300001611d00001000100c0010dc2020001c1020001")
AARE_FRAME = bytes.fromhex("0300001d02f080010001006110300e020103a009a107020102a5028100")
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
        if command == "emit-wire-frame cotp-connect-request":
            self._stdout.enqueue(COTP_CONNECT_REQUEST_FRAME)
        elif command == "emit-wire-frame association-request":
            self._stdout.enqueue(ASSOCIATION_REQUEST_FRAME)
        elif command == "emit-wire-frame confirmed-read-request XCBR1 ST$Pos$stVal 3":
            self._stdout.enqueue(CONFIRMED_READ_REQUEST_FRAME)

    def flush(self) -> None:
        return None


class _CommandDrivenControlSocket:
    def __init__(self, responses: dict[str, bytes]) -> None:
        self.responses = responses
        self.sent_commands: list[str] = []
        self.sent_frames: list[bytes] = []
        self._buffer = bytearray()
        self.address: tuple[str, int] | None = None
        self.closed = False

    def sendall(self, value: bytes) -> None:
        self.sent_frames.append(value)
        command = value.decode("utf-8").strip()
        self.sent_commands.append(command)
        if command.startswith("emit-wire-frame "):
            frame_kind = command.split(" ", 2)[1]
            frame = self.responses.get(frame_kind)
            if frame is None:
                raise AssertionError(f"unexpected wire frame request: {command}")
            self._buffer.extend(_wire_frame_response_line(frame))

    def recv(self, size: int) -> bytes:
        if not self._buffer:
            return b""
        chunk = bytes(self._buffer[:size])
        del self._buffer[:size]
        return chunk

    def settimeout(self, _timeout: float) -> None:
        return None

    def close(self) -> None:
        self.closed = True


class _CommandDrivenDataSocket:
    def __init__(self, frames: bytes) -> None:
        self.address: tuple[str, int] | None = None
        self.closed = False
        self._buffer = bytearray(frames)
        self.sent_frames: list[bytes] = []

    def sendall(self, value: bytes) -> None:
        self.sent_frames.append(value)

    def recv(self, size: int) -> bytes:
        if not self._buffer:
            return b""
        chunk = bytes(self._buffer[:size])
        del self._buffer[:size]
        return chunk

    def settimeout(self, _timeout: float) -> None:
        return None

    def close(self) -> None:
        self.closed = True


class _CommandDrivenSelect:
    @staticmethod
    def select(readable, writable, exceptional, timeout=None):
        ready = []
        for item in readable:
            if hasattr(item, "_buffer") and len(getattr(item, "_buffer")) > 0:
                ready.append(item)
            elif hasattr(item, "lines") and getattr(item, "lines"):
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
        )

        started_specs: list[object] = []
        sockets: list[object] = []
        process_commands: list[str] = []
        stdout = _QueuedStdout()

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

        def fake_stop_process(_handle):
            return None

        def fake_create_connection(address, timeout=None):
            sockets.append(address)
            sock = _CommandDrivenDataSocket(COTP_CONNECT_RESPONSE_FRAME + AARE_FRAME + READ_RESPONSE_FRAME + REPORT_FRAME)
            sock.address = address
            return sock

        monkeypatch.setattr(client_control_module, "select", _CommandDrivenSelect)
        monkeypatch.setattr(client_control_module.socket, "create_connection", fake_create_connection)
        monkeypatch.setattr(client_control_module, "start_ied_simulator_process", fake_start_process)
        monkeypatch.setattr(client_control_module, "stop_ied_simulator_process", fake_stop_process)
        monkeypatch.setattr(client_control_module, "write_ied_simulator_process_command", lambda handle, command: handle.process.stdin.write(command))

        state = service.start_live_wire_transport(mode="process")

        assert state.live_wire_mode == "process"
        assert started_specs
        assert started_specs[0].binary_path == "/bin/true"
        assert sockets == [("127.0.0.1", 12447)]
        assert process_commands == [
            "emit-wire-frame cotp-connect-request",
            "emit-wire-frame association-request",
            "emit-wire-frame confirmed-read-request XCBR1 ST$Pos$stVal 3",
        ]
        assert state.live_wire_last_frame_length == len(READ_RESPONSE_FRAME)
        assert state.live_wire_last_frame_hex == READ_RESPONSE_FRAME.hex()
        assert [event.kind for event in state.transcript][-3:] == ["wire-session-open", "wire-associate", "wire-confirmed-read-frame"]
    finally:
        get_settings.cache_clear()

def test_client_control_service_can_drive_a_live_wire_transport_smoke(monkeypatch: pytest.MonkeyPatch) -> None:
    service = Iec61850ClientControlService(
        now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC),
        live_wire_binary_path="/bin/true",
        live_wire_bind_address="127.0.0.1",
        live_wire_data_port=12346,
    )

    process_commands: list[str] = []
    stdout = _QueuedStdout()

    class _FakeProcess:
        def __init__(self) -> None:
            self.stdin = _CommandDrivenProcessStdin(process_commands, stdout)
            self.stdout = stdout

    class _FakeHandle:
        def __init__(self) -> None:
            self.endpoint = service.snapshot().endpoint
            self.process = _FakeProcess()
            self.pid = 4242
            self.spec = None

    def fake_start_process(spec, **_kwargs):
        handle = _FakeHandle()
        handle.spec = spec
        handle.endpoint = spec.endpoint
        return handle

    monkeypatch.setattr(client_control_module, "select", _CommandDrivenSelect)
    monkeypatch.setattr(client_control_module, "start_ied_simulator_process", fake_start_process)
    monkeypatch.setattr(client_control_module.socket, "create_connection", lambda address, timeout=None: _CommandDrivenDataSocket(COTP_CONNECT_RESPONSE_FRAME + AARE_FRAME + READ_RESPONSE_FRAME + REPORT_FRAME))
    monkeypatch.setattr(client_control_module, "stop_ied_simulator_process", lambda handle: None)
    monkeypatch.setattr(client_control_module, "write_ied_simulator_process_command", lambda handle, command: handle.process.stdin.write(command))

    state = service.start_live_wire_transport(mode="process")
    assert state.live_wire_open is True
    assert process_commands == [
        "emit-wire-frame cotp-connect-request",
        "emit-wire-frame association-request",
        "emit-wire-frame confirmed-read-request XCBR1 ST$Pos$stVal 3",
    ]

    state = service.emit_live_wire_report()
    assert state.live_wire_last_frame_length == len(REPORT_FRAME)
    assert state.live_wire_last_frame_hex == REPORT_FRAME.hex()
    assert [event.kind for event in state.transcript][-4:] == ["wire-session-open", "wire-associate", "wire-confirmed-read-frame", "wire-report-frame"]
    assert process_commands[-1] == "emit-report"

    state = service.stop_live_wire_transport()
    assert state.live_wire_open is False
    assert [event.kind for event in state.transcript][-1] == "wire-session-close"

def test_client_control_service_can_drive_a_live_wire_host_debug_smoke(monkeypatch: pytest.MonkeyPatch) -> None:
    service = Iec61850ClientControlService(
        now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC),
        live_wire_binary_path="",
    )

    sockets: list[object] = []
    data_socket = _CommandDrivenDataSocket(COTP_CONNECT_RESPONSE_FRAME + AARE_FRAME + READ_RESPONSE_FRAME + REPORT_FRAME)
    control_socket = _CommandDrivenControlSocket(
        {
            "cotp-connect-request": COTP_CONNECT_REQUEST_FRAME,
            "association-request": ASSOCIATION_REQUEST_FRAME,
            "confirmed-read-request": CONFIRMED_READ_REQUEST_FRAME,
        }
    )

    def fake_create_connection(address, timeout=None):
        role = len(sockets)
        sock = data_socket if role == 0 else control_socket
        sock.address = address
        sockets.append(sock)
        return sock

    monkeypatch.setattr(client_control_module, "select", _CommandDrivenSelect)
    monkeypatch.setattr(client_control_module.socket, "create_connection", fake_create_connection)

    state = service.start_live_wire_transport(mode="host")
    assert state.live_wire_mode == "host"
    assert state.live_wire_last_frame_length == len(READ_RESPONSE_FRAME)
    assert state.live_wire_last_frame_hex == READ_RESPONSE_FRAME.hex()
    assert sockets[0].address == ("host.docker.internal", 12447)
    assert sockets[1].address == ("host.docker.internal", 12448)
    assert data_socket.sent_frames == [
        COTP_CONNECT_REQUEST_FRAME,
        ASSOCIATION_REQUEST_FRAME,
        CONFIRMED_READ_REQUEST_FRAME,
    ]
    assert control_socket.sent_commands == [
        "emit-wire-frame cotp-connect-request",
        "emit-wire-frame association-request",
        "emit-wire-frame confirmed-read-request XCBR1 ST$Pos$stVal 3",
    ]

    state = service.stop_live_wire_transport()
    assert state.live_wire_open is False

