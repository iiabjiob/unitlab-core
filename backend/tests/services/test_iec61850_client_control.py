from __future__ import annotations

from datetime import UTC, datetime
import subprocess

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




class _ExternalMmsClientStdin:
    def __init__(self, commands: list[str], stdout: _QueuedStdout) -> None:
        self._commands = commands
        self._stdout = stdout

    def write(self, value: str) -> None:
        command = value.rstrip("\n")
        self._commands.append(command)
        if command.startswith("discover "):
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=discover rcb=KINTE13LVC01CTRL/LLN0.brcbA/<none> rcb-index=0 rptEna=false rptEna-invoke=0 giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command.startswith("rptena"):
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=rptena rcb=KINTE13LVC01CTRL/LLN0.brcbA/buffered rcb-index=0 rptEna=true rptEna-invoke=4 giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command == "gi":
            self._stdout.lines.append("native-wire-client: async-report\n")
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=async-report rcb=KINTE13LVC01CTRL/LLN0.brcbA/buffered rcb-index=0 rptEna=true rptEna-invoke=4 giRequested=true gi-invoke=5 lastReportReceived=true asyncReports=1 lastReportValues=1 lastReportDataRefs=1 lastReportMatchedDataRefs=1 lastReportReasons=1 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command in {"disconnect", "exit"}:
            self._stdout.lines.append("native-wire-client: disconnected\n")
            self._stdout.lines.append("native-wire-client: state=stopped\n")

    def flush(self) -> None:
        return None


class _ExternalMmsClientProcess:
    def __init__(self, command, commands: list[str]) -> None:
        self.command = tuple(command)
        self.stdout = _QueuedStdout()
        self.stdout.lines.extend([
            "native-wire-client: state=init\n",
            "native-wire-client: state=data-connected\n",
            "native-wire-client: state=associated\n",
            "native-wire-client: state=ready\n",
            "native-wire-client: ready\n",
        ])
        self.stdin = _ExternalMmsClientStdin(commands, self.stdout)
        self.stderr = _QueuedStdout()
        self.pid = 4242
        self._returncode = None

    def poll(self):
        return self._returncode

    def terminate(self) -> None:
        self._returncode = 0

    def kill(self) -> None:
        self._returncode = -9

    def wait(self, timeout=None):
        self._returncode = 0
        return self._returncode


class _FailingExternalMmsClientProcess:
    def __init__(self, command: list[str], fail_on_command: str) -> None:
        self.command = tuple(command)
        self.stdout = _QueuedStdout()
        self.stdout.lines.extend([
            "native-wire-client: state=init\n",
            "native-wire-client: state=data-connected\n",
            "native-wire-client: state=associated\n",
            "native-wire-client: state=ready\n",
            "native-wire-client: ready\n",
        ])
        self._commands = command
        self._fail_on_command = fail_on_command
        self.stderr = _QueuedStdout()
        self.pid = 4242
        self._returncode = None
        self.stdin = _FailingExternalMmsClientStdin(self, command)

    def poll(self):
        return self._returncode

    def terminate(self) -> None:
        self._returncode = 0

    def kill(self) -> None:
        self._returncode = -9

    def wait(self, timeout=None):
        self._returncode = 0
        return self._returncode


class _FailingExternalMmsClientStdin:
    def __init__(self, process: _FailingExternalMmsClientProcess, commands: list[str]) -> None:
        self._process = process
        self._commands = commands

    def write(self, value: str) -> None:
        command = value.rstrip("\n")
        self._commands.append(command)
        if command.startswith("discover "):
            self._process.stdout.lines.append(
                "native-wire-client: subscription-summary phase=discover rcb=KINTE13LVC01CTRL/LLN0.brcbA/<none> rcb-index=0 rptEna=false giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._process.stdout.lines.append("native-wire-client: state=ready\n")
        elif command.startswith("rptena"):
            self._process.stdout.lines.append(
                "native-wire-client: subscription-summary phase=rptena rcb=KINTE13LVC01CTRL/LLN0.brcbA/buffered rcb-index=0 rptEna=true rptEna-invoke=4 giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._process.stdout.lines.append("native-wire-client: state=ready\n")
        elif command == self._process._fail_on_command:
            self._process.stdout.lines.append("native-wire-client: state=failed\n")
            self._process._returncode = 1
        elif command in {"disconnect", "exit"}:
            self._process.stdout.lines.append("native-wire-client: disconnected\n")
            self._process.stdout.lines.append("native-wire-client: state=stopped\n")

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


def test_debug_discover_opens_session_and_projects_structure():
    service = Iec61850ClientControlService()

    snapshot = service.discover_ied()

    assert snapshot.session_open is True
    assert snapshot.last_discovery is not None
    assert snapshot.last_discovery["schema"] == "unitlab.iec61850.client.discovery.v1"
    assert snapshot.last_discovery["logicalDevices"][0]["inst"] == "LD0"
    assert snapshot.last_discovery["logicalNodes"][0]["name"] == "LLN0"
    assert snapshot.last_discovery["dataSets"][0]["memberCount"] == 1
    assert snapshot.last_discovery["reportControls"][0]["name"] == "brcbEvents"
    assert snapshot.transcript[-1].kind == "ied-discover"


def test_debug_close_ied_removes_in_memory_state():
    service = Iec61850ClientControlService()
    service.discover_ied()
    snapshot = service.close_ied()

    assert snapshot.session_open is False
    assert snapshot.last_discovery is None
    assert snapshot.last_read is None
    assert snapshot.last_state is None
    assert snapshot.last_report is None
    assert snapshot.transcript[-1].kind == "ied-close"


def test_debug_rptena_reserves_and_enables_report_control():
    service = Iec61850ClientControlService()
    service.connect_ied()

    snapshot = service.enable_reporting()

    assert snapshot.last_state is not None
    assert snapshot.last_state.enabled is True
    assert [event.kind for event in snapshot.transcript[-2:]] == ["report-control-reserve", "report-control-enable"]


def test_debug_connect_disconnect_are_idempotent_for_debug_view():
    service = Iec61850ClientControlService()

    connected = service.connect_ied()
    connected_again = service.connect_ied()
    disconnected = service.disconnect_ied()
    disconnected_again = service.disconnect_ied()

    assert connected.session_open is True
    assert connected_again.session_open is True
    assert connected_again.transcript[-1].outcome == "already-connected"
    assert disconnected.session_open is False
    assert disconnected_again.session_open is False
    assert disconnected_again.transcript[-1].outcome == "already-disconnected"


def test_client_control_snapshot_exposes_operator_ui_state_contract() -> None:
    service = Iec61850ClientControlService(now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC))

    initial = service.snapshot().ui_state
    assert initial["schema"] == "unitlab.iec61850.client.ui-state.v1"
    assert initial["session"]["phase"] == "idle"
    assert initial["discovery"]["discovered"] is False
    assert initial["subscription"]["subscribed"] is False
    assert initial["report"]["received"] is False

    discovered = service.discover_ied().ui_state
    assert discovered["session"]["phase"] == "discovered"
    assert discovered["session"]["associated"] is True
    assert discovered["discovery"]["logical_devices"] == 1
    assert discovered["discovery"]["logical_nodes"] == 1
    assert discovered["discovery"]["data_sets"] == 1
    assert discovered["discovery"]["data_set_members"] == 1
    assert discovered["discovery"]["report_controls"] == 1
    assert discovered["discovery"]["selected_rcb_ref"] == "IED1/AP1/LD0/LLN0/brcbEvents/buffered"
    assert discovered["actions"]["can_rptena"] is True

    subscribed = service.enable_reporting().ui_state
    assert subscribed["session"]["phase"] == "subscribed"
    assert subscribed["subscription"]["subscribed"] is True
    assert subscribed["subscription"]["rptena_enabled"] is True
    assert subscribed["subscription"]["owner"] == "unitlab-test-client"
    assert subscribed["actions"]["can_gi"] is True

    reporting = service.send_general_interrogation().ui_state
    assert reporting["session"]["phase"] == "reporting"
    assert reporting["report"]["received"] is True
    assert reporting["report"]["reason"] == "general-interrogation"
    assert reporting["report"]["value_count"] == 1
    assert reporting["report"]["values"][0]["reference"] == "LD0/XCBR1.Pos.stVal[ST]"

    closed = service.close_ied().ui_state
    assert closed["session"]["phase"] == "idle"
    assert closed["session"]["associated"] is False
    assert closed["discovery"]["discovered"] is False
    assert closed["subscription"]["subscribed"] is False
    assert closed["report"]["received"] is False


def test_client_control_ui_state_exposes_active_diagnostic() -> None:
    service = Iec61850ClientControlService(now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC))
    service.open_session()

    with pytest.raises(Iec61850ReportRuntimeError):
        service.open_session()

    ui_state = service.snapshot().ui_state
    assert ui_state["session"]["phase"] == "failed"
    assert ui_state["diagnostic"]["active"] is True
    assert ui_state["diagnostic"]["action"] == "open-session"
    assert ui_state["diagnostic"]["code"] == "SESSION_EXISTS"



def test_client_control_configures_external_mms_target_from_scd(tmp_path) -> None:
    scl_path = tmp_path / "target.scd"
    scl_path.write_text(
        """<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <IED name="KINTE13LVC01">
    <AccessPoint name="AP1">
      <Server>
        <LDevice inst="CTRL">
          <LN0 lnClass="LLN0" inst="" lnType="T_CTRL">
            <DataSet name="RCB1">
              <FCDA ldInst="CTRL" lnClass="XCBR" lnInst="1" doName="Pos" daName="stVal" fc="ST" />
            </DataSet>
            <ReportControl name="brcbA" datSet="RCB1" buffered="true" indexed="true" rptID="KINTE13LVC01CTRL/LLN0.brcbA" confRev="10000" bufTime="500" intgPd="0">
              <TrgOps dchg="true" qchg="true" gi="true" />
              <OptFields seqNum="true" timeStamp="true" reasonCode="true" dataSet="true" dataRef="true" entryID="true" configRef="true" bufOvfl="true" />
            </ReportControl>
          </LN0>
        </LDevice>
      </Server>
    </AccessPoint>
  </IED>
</SCL>
""",
        encoding="utf-8",
    )
    service = Iec61850ClientControlService()

    snapshot = service.configure_target(
        client_control_module.Iec61850ClientTargetRequest(
            mode="external-mms",
            host="host.docker.internal",
            port=12447,
            ied_name="KINTE13LVC01",
            scl_path=str(scl_path),
        )
    )

    assert snapshot.endpoint.id == "mms:KINTE13LVC01@host.docker.internal:12447"
    assert snapshot.endpoint.mode.value == "mms"
    assert snapshot.candidate.logical_device_inst == "CTRL"
    assert snapshot.candidate.logical_node_name == "LLN0"
    assert snapshot.candidate.report_control_name == "brcbA"
    assert snapshot.candidate.data_set_ref == "KINTE13LVC01CTRL/LLN0.RCB1"
    assert snapshot.candidate.signals[0].reference == "CTRL/XCBR1.Pos.stVal[ST]"
    assert snapshot.transcript[-1].kind == "target-configured"


def test_client_control_configures_external_target_with_multiple_report_controls_and_select(tmp_path) -> None:
    scl_path = tmp_path / "target.scd"
    scl_path.write_text(
        """<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <IED name="KINTE13LVC01">
    <AccessPoint name="AP1">
      <Server>
        <LDevice inst="CTRL">
          <LN0 lnClass="LLN0" inst="" lnType="T_CTRL">
            <DataSet name="RCB1">
              <FCDA ldInst="CTRL" lnClass="XCBR" lnInst="1" doName="Pos" daName="stVal" fc="ST" />
            </DataSet>
            <DataSet name="RCB2">
              <FCDA ldInst="CTRL" lnClass="XCBR" lnInst="2" doName="Pos" daName="stVal" fc="ST" />
            </DataSet>
            <ReportControl name="brcbA" datSet="RCB1" buffered="true" indexed="true" rptID="KINTE13LVC01CTRL/LLN0.brcbA" confRev="10000" />
            <ReportControl name="brcbB" datSet="RCB2" buffered="false" indexed="true" rptID="KINTE13LVC01CTRL/LLN0.brcbB" confRev="10000" />
          </LN0>
        </LDevice>
      </Server>
    </AccessPoint>
  </IED>
</SCL>
""",
        encoding="utf-8",
    )
    service = Iec61850ClientControlService()
    snapshot = service.configure_target(
        client_control_module.Iec61850ClientTargetRequest(
            mode="external-mms",
            host="host.docker.internal",
            port=12447,
            ied_name="KINTE13LVC01",
            scl_path=str(scl_path),
        )
    )

    available_controls = snapshot.ui_state["discovery"]["available_report_controls"]
    assert len(available_controls) == 2
    assert available_controls[0]["report_control_name"] == "brcbA"
    assert available_controls[1]["report_control_name"] == "brcbB"
    assert snapshot.candidate.report_control_name == "brcbA"

    selection = service.select_report_control(available_controls[1]["rcb_ref"])
    assert selection.candidate.report_control_name == "brcbB"
    assert selection.ui_state["discovery"]["selected_rcb_ref"] == available_controls[1]["rcb_ref"]
    assert selection.last_state is None
    assert selection.last_read is None
    assert selection.transcript[-1].kind == "report-control-select"


def test_external_mms_target_routes_discover_rptena_gi_to_external_probes(monkeypatch: pytest.MonkeyPatch, tmp_path) -> None:
    scl_path = tmp_path / "target.scd"
    scl_path.write_text(
        """<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <IED name="KINTE13LVC01"><AccessPoint name="AP1"><Server><LDevice inst="CTRL"><LN0 lnClass="LLN0" inst="" lnType="T_CTRL">
    <DataSet name="RCB1"><FCDA ldInst="CTRL" lnClass="XCBR" lnInst="1" doName="Pos" daName="stVal" fc="ST" /></DataSet>
    <ReportControl name="brcbA" datSet="RCB1" buffered="true" indexed="true" rptID="KINTE13LVC01CTRL/LLN0.brcbA" confRev="10000" />
  </LN0></LDevice></Server></AccessPoint></IED>
</SCL>""",
        encoding="utf-8",
    )
    service = Iec61850ClientControlService(
        live_wire_binary_path="/bin/true",
        live_wire_service_host="host.docker.internal",
        live_wire_data_port=12447,
    )
    service.configure_target(
        client_control_module.Iec61850ClientTargetRequest(
            mode="external-mms",
            host="host.docker.internal",
            port=12447,
            ied_name="KINTE13LVC01",
            scl_path=str(scl_path),
        )
    )

    process_commands: list[tuple[str, ...]] = []
    stdin_commands: list[str] = []

    def fake_popen(command, **_kwargs):
        process_commands.append(tuple(command))
        return _ExternalMmsClientProcess(command, stdin_commands)

    monkeypatch.setattr(client_control_module.subprocess, "Popen", fake_popen)

    discover_snapshot = service.discover_ied()
    rptena_snapshot = service.enable_reporting()
    gi_snapshot = service.send_general_interrogation()
    disconnect_snapshot = service.disconnect_ied()

    assert discover_snapshot.ui_state["session"]["phase"] == "discovered"
    assert discover_snapshot.ui_state["session"]["associated"] is True
    assert rptena_snapshot.ui_state["session"]["phase"] == "subscribed"
    assert rptena_snapshot.ui_state["subscription"]["runtime_status"] == "enabled"
    assert rptena_snapshot.ui_state["subscription"]["rptena_enabled"] is True
    assert gi_snapshot.ui_state["session"]["phase"] == "reporting"
    assert gi_snapshot.ui_state["subscription"]["runtime_status"] == "reporting"
    assert gi_snapshot.ui_state["report"]["received"] is True
    assert gi_snapshot.ui_state["report"]["rpt_id"] == "KINTE13LVC01CTRL/LLN0.brcbA"
    assert gi_snapshot.ui_state["report"]["reason"] == "general-interrogation"
    assert gi_snapshot.ui_state["subscription"]["selected_rcb_ref"] == "KINTE13LVC01/AP1/CTRL/LLN0/brcbA/buffered"
    assert gi_snapshot.ui_state["actions"]["can_rptena"] is True
    assert gi_snapshot.ui_state["actions"]["can_gi"] is True
    assert disconnect_snapshot.ui_state["session"]["associated"] is False

    assert process_commands[0] == (
        "/workspace/iec61850_ied/build-libiec61850/unitlab-iec61850-ied-sim",
        "--scl",
        str(scl_path),
        "--ied",
        "KINTE13LVC01",
        "--bind",
        "host.docker.internal",
        "--port",
        "12447",
        "--mms-client-start",
    )
    assert stdin_commands[:4] == [
        "discover KINTE13LVC01CTRL",
        "rptena",
        "gi",
        "disconnect",
    ]
    assert [event.kind for event in service.snapshot().transcript[-5:]] == [
        "external-session-open",
        "external-ied-discover",
        "external-report-control-enable",
        "external-report-control-gi",
        "external-ied-disconnect",
    ]


def test_external_mms_state_failed_aborts_general_interrogation(monkeypatch: pytest.MonkeyPatch, tmp_path) -> None:
    scl_path = tmp_path / "target.scd"
    scl_path.write_text(
        """<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <IED name="KINTE13LVC01"><AccessPoint name="AP1"><Server><LDevice inst="CTRL"><LN0 lnClass="LLN0" inst="" lnType="T_CTRL">
    <DataSet name="RCB1"><FCDA ldInst="CTRL" lnClass="XCBR" lnInst="1" doName="Pos" daName="stVal" fc="ST" /></DataSet>
    <ReportControl name="brcbA" datSet="RCB1" buffered="true" indexed="true" rptID="KINTE13LVC01CTRL/LLN0.brcbA" confRev="10000" />
  </LN0></LDevice></Server></AccessPoint></IED>
</SCL>""",
        encoding="utf-8",
    )
    service = Iec61850ClientControlService(
        live_wire_binary_path="/bin/true",
        live_wire_service_host="host.docker.internal",
        live_wire_data_port=12447,
    )
    service.configure_target(
        client_control_module.Iec61850ClientTargetRequest(
            mode="external-mms",
            host="host.docker.internal",
            port=12447,
            ied_name="KINTE13LVC01",
            scl_path=str(scl_path),
        )
    )

    process_commands: list[str] = []

    def fake_popen(command, **_kwargs):
        return _FailingExternalMmsClientProcess(process_commands, fail_on_command="gi")

    monkeypatch.setattr(client_control_module.subprocess, "Popen", fake_popen)

    service.discover_ied()
    service.enable_reporting()

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        service.send_general_interrogation()

    assert error.value.code == "EXTERNAL_MMS_CLIENT_COMMAND_FAILED"
    snapshot = service.snapshot()
    assert snapshot.last_diagnostic is not None
    assert snapshot.last_diagnostic.code == "EXTERNAL_MMS_CLIENT_COMMAND_FAILED"
    assert snapshot.last_diagnostic.action == "external-gi"
    assert snapshot.last_state is not None
    assert snapshot.last_state.runtime_status == Iec61850RuntimeStatus.FAILED
    assert snapshot.ui_state["subscription"]["runtime_status"] == "failed"
    assert snapshot.ui_state["session"]["phase"] == "failed"
    assert process_commands[:3] == ["discover KINTE13LVC01CTRL", "rptena", "gi"]
