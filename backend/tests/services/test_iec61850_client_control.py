from __future__ import annotations

from datetime import UTC, datetime

import pytest

from app.core.config import get_settings
from app.services.iec61850.client_control import Iec61850ClientControlService
import app.services.iec61850.client_control as client_control_module
from app.services.iec61850.report_runtime import Iec61850ReportReason, Iec61850ReportRuntimeError, Iec61850RuntimeStatus


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
        sockets: list[tuple[str, int]] = []
        created_sockets: list[object] = []
        cc_frame = bytes.fromhex("0300001611d00001000100c0010dc2020001c1020001")
        aare_frame = bytes.fromhex("0300001d02f080010001006110300e020103a009a107020102a5028100")
        read_request_frame = bytes.fromhex("0300003502f0800100010061286026020103a421301fa11da01b3019a017a1151a0558434252311a0c535424506f7324737456616c")
        read_response_frame = bytes.fromhex("0300001d02f080010001006110610e300c020103a407a105a0030201ff")
        report_frame = bytes.fromhex("0300001402f0800100010040076305a003810100")

        class _FakeStdin:
            def write(self, _value: str) -> None:
                return None

            def flush(self) -> None:
                return None

        class _FakeProcess:
            def __init__(self) -> None:
                self.stdin = _FakeStdin()

        class _FakeHandle:
            def __init__(self, spec) -> None:
                self.spec = spec
                self.endpoint = spec.endpoint
                self.process = _FakeProcess()
                self.pid = 4242

        class _FakeSocket:
            def __init__(self) -> None:
                self.address: tuple[str, int] | None = None
                self.closed = False
                self._buffer = bytearray(cc_frame + aare_frame + read_response_frame + report_frame)
                self.sent_frames: list[bytes] = []

            def settimeout(self, _timeout: float) -> None:
                return None

            def recv(self, size: int) -> bytes:
                if not self._buffer:
                    return b""
                chunk = bytes(self._buffer[:size])
                del self._buffer[:size]
                return chunk

            def sendall(self, value: bytes) -> None:
                self.sent_frames.append(value)

            def close(self) -> None:
                self.closed = True

        def fake_start_process(spec, **_kwargs):
            started_specs.append(spec)
            return _FakeHandle(spec)

        def fake_stop_process(_handle):
            return None

        def fake_create_connection(address, timeout=None):
            sockets.append(address)
            sock = _FakeSocket()
            sock.address = address
            created_sockets.append(sock)
            return sock

        monkeypatch.setattr(client_control_module.socket, "create_connection", fake_create_connection)
        monkeypatch.setattr(client_control_module, "start_ied_simulator_process", fake_start_process)
        monkeypatch.setattr(client_control_module, "stop_ied_simulator_process", fake_stop_process)

        state = service.start_live_wire_transport(mode="process")

        assert state.live_wire_mode == "process"
        assert started_specs
        assert started_specs[0].binary_path == "/bin/true"
        assert sockets == [("127.0.0.1", 12447)]
        assert created_sockets[0].sent_frames == [
            bytes.fromhex("0300001611e00000000100c0010dc2020001c1020001"),
            bytes.fromhex("030000b002f080010001006181a230819f020103a08199a18196020103ac8190800100a1818a302ba029a1271a144d793734366965644d6561737572656d656e74731a0f4262704d44494631245354244d6f64302ba029a1271a144d793734366965644d6561737572656d656e74731a0f4262704d4449463124535424426568302ea02ca12a1a144d793734366965644d6561737572656d656e74731a124262704d44494631245354244865616c7468"),
            bytes.fromhex("0300003502f0800100010061286026020103a421301fa11da01b3019a017a1151a0558434252311a0c535424506f7324737456616c"),
        ]
        assert state.live_wire_last_frame_length == len(read_response_frame)
        assert state.live_wire_last_frame_hex == read_response_frame.hex()
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

    emitted_commands: list[str] = []
    cc_frame = bytes.fromhex("0300001611d00001000100c0010dc2020001c1020001")
    aare_frame = bytes.fromhex("0300001d02f080010001006110300e020103a009a107020102a5028100")
    read_request_frame = bytes.fromhex("0300003502f0800100010061286026020103a421301fa11da01b3019a017a1151a0558434252311a0c535424506f7324737456616c")
    read_response_frame = bytes.fromhex("0300001d02f080010001006110610e300c020103a407a105a0030201ff")
    report_frame = bytes.fromhex("0300001402f0800100010040076305a003810100")

    class _FakeStdin:
        def write(self, value: str) -> None:
            emitted_commands.append(value)

        def flush(self) -> None:
            return None

    class _FakeProcess:
        def __init__(self) -> None:
            self.stdin = _FakeStdin()

    class _FakeHandle:
        def __init__(self) -> None:
            self.endpoint = service.snapshot().endpoint
            self.process = _FakeProcess()
            self.pid = 4242
            self.spec = None

    class _FakeSocket:
        def __init__(self) -> None:
            self._buffer = bytearray(cc_frame + aare_frame + read_response_frame + report_frame)
            self.closed = False
            self.sent_frames: list[bytes] = []

        def settimeout(self, _timeout: float) -> None:
            return None

        def recv(self, size: int) -> bytes:
            if not self._buffer:
                return b""
            chunk = bytes(self._buffer[:size])
            del self._buffer[:size]
            return chunk

        def sendall(self, value: bytes) -> None:
            self.sent_frames.append(value)

        def close(self) -> None:
            self.closed = True

    fake_socket = _FakeSocket()

    def fake_start_process(spec, **_kwargs):
        handle = _FakeHandle()
        handle.spec = spec
        handle.endpoint = spec.endpoint
        return handle

    monkeypatch.setattr(client_control_module, "start_ied_simulator_process", fake_start_process)
    monkeypatch.setattr(client_control_module.socket, "create_connection", lambda address, timeout=None: fake_socket)
    monkeypatch.setattr(client_control_module, "stop_ied_simulator_process", lambda handle: None)

    state = service.start_live_wire_transport(mode="process")
    assert state.live_wire_open is True
    assert fake_socket.sent_frames == [
        bytes.fromhex("0300001611e00000000100c0010dc2020001c1020001"),
        bytes.fromhex("030000b002f080010001006181a230819f020103a08199a18196020103ac8190800100a1818a302ba029a1271a144d793734366965644d6561737572656d656e74731a0f4262704d44494631245354244d6f64302ba029a1271a144d793734366965644d6561737572656d656e74731a0f4262704d4449463124535424426568302ea02ca12a1a144d793734366965644d6561737572656d656e74731a124262704d44494631245354244865616c7468"),
        bytes.fromhex("0300003502f0800100010061286026020103a421301fa11da01b3019a017a1151a0558434252311a0c535424506f7324737456616c"),
    ]

    state = service.emit_live_wire_report()
    assert state.live_wire_last_frame_length == len(report_frame)
    assert state.live_wire_last_frame_hex == report_frame.hex()
    assert [event.kind for event in state.transcript][-4:] == ["wire-session-open", "wire-associate", "wire-confirmed-read-frame", "wire-report-frame"]
    assert emitted_commands == ["emit-report\n"]

    state = service.stop_live_wire_transport()
    assert state.live_wire_open is False
    assert [event.kind for event in state.transcript][-1] == "wire-session-close"


def test_client_control_service_can_drive_a_live_wire_host_debug_smoke(monkeypatch: pytest.MonkeyPatch) -> None:
    service = Iec61850ClientControlService(
        now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC),
        live_wire_binary_path="",
    )

    sockets: list[_FakeSocket] = []
    cc_frame = bytes.fromhex("0300001611d00001000100c0010dc2020001c1020001")
    aare_frame = bytes.fromhex("0300001d02f080010001006110300e020103a009a107020102a5028100")
    read_request_frame = bytes.fromhex("0300003502f0800100010061286026020103a421301fa11da01b3019a017a1151a0558434252311a0c535424506f7324737456616c")
    read_response_frame = bytes.fromhex("0300001d02f080010001006110610e300c020103a407a105a0030201ff")
    report_frame = bytes.fromhex("0300001402f0800100010040076305a003810100")

    class _FakeSocket:
        def __init__(self, role: str) -> None:
            self.role = role
            self.address: tuple[str, int] | None = None
            self.closed = False
            self._buffer = bytearray(cc_frame + aare_frame + read_response_frame + report_frame if role == "data" else b"")
            self.sent_frames: list[bytes] = []

        def settimeout(self, _timeout: float) -> None:
            return None

        def recv(self, size: int) -> bytes:
            if self.role == "data":
                if not self._buffer:
                    return b""
                chunk = bytes(self._buffer[:size])
                del self._buffer[:size]
                return chunk
            return b""

        def sendall(self, value: bytes) -> None:
            if self.role == "data":
                self.sent_frames.append(value)
            return None

        def close(self) -> None:
            self.closed = True

    def fake_create_connection(address, timeout=None):
        role = "data" if len(sockets) == 0 else "control"
        sock = _FakeSocket(role)
        sock.address = address
        sockets.append(sock)
        return sock

    monkeypatch.setattr(client_control_module.socket, "create_connection", fake_create_connection)

    state = service.start_live_wire_transport(mode="host")
    assert state.live_wire_mode == "host"
    assert state.live_wire_last_frame_length == len(read_response_frame)
    assert state.live_wire_last_frame_hex == read_response_frame.hex()
    assert sockets[0].address == ("host.docker.internal", 12447)
    assert sockets[1].address == ("host.docker.internal", 12448)
    assert sockets[0].sent_frames == [
        bytes.fromhex("0300001611e00000000100c0010dc2020001c1020001"),
        bytes.fromhex("030000b002f080010001006181a230819f020103a08199a18196020103ac8190800100a1818a302ba029a1271a144d793734366965644d6561737572656d656e74731a0f4262704d44494631245354244d6f64302ba029a1271a144d793734366965644d6561737572656d656e74731a0f4262704d4449463124535424426568302ea02ca12a1a144d793734366965644d6561737572656d656e74731a124262704d44494631245354244865616c7468"),
        bytes.fromhex("0300003502f0800100010061286026020103a421301fa11da01b3019a017a1151a0558434252311a0c535424506f7324737456616c"),
    ]

    state = service.stop_live_wire_transport()
    assert state.live_wire_open is False
