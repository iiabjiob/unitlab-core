from __future__ import annotations

from datetime import UTC, datetime
import io
import os
from pathlib import Path
import socket
import subprocess
import time
from collections.abc import Sequence
from typing import Any, Protocol, cast, final

import pytest

import app.services.iec61850.client_control as client_control_module
from app.core.config import get_settings
from app.services.iec61850.client_control import Iec61850ClientControlService
from app.services.iec61850.ied_simulator_process import Iec61850IedSimulatorProcessSpec
from app.services.iec61850.report_runtime import (
    Iec61850DataSetMember,
    Iec61850DeviceEndpoint,
    Iec61850OptionalFields,
    Iec61850ReportControlCandidate,
    Iec61850ReportKind,
    Iec61850ReportReason,
    Iec61850ReportRuntimeError,
    Iec61850RuntimeStatus,
    Iec61850RuntimeTriggerOptions,
)


READ_RESPONSE_FRAME = bytes.fromhex("0300001d02f080010001006110610e300c020103a407a105a0030201ff")
REPORT_FRAME = bytes.fromhex("0300001402f0800100010040076305a003810100")


def _payload(value: object) -> dict[str, Any]:  # pyright: ignore[reportExplicitAny]
    return cast(dict[str, Any], value)  # pyright: ignore[reportExplicitAny]


@pytest.fixture(autouse=True)
def _stub_external_mms_endpoint_probe(monkeypatch: pytest.MonkeyPatch) -> None:
    class _ConnectedSocket:
        def __enter__(self) -> "_ConnectedSocket":
            return self

        def __exit__(self, _exc_type: object, _exc: object, _traceback: object) -> bool:
            return False

    def create_connection(*_args: object, **_kwargs: object) -> _ConnectedSocket:
        return _ConnectedSocket()

    monkeypatch.setattr(
        socket,
        "create_connection",
        create_connection,
    )


def test_read_process_stdout_line_supports_zero_timeout_ready_fd() -> None:
    read_fd, write_fd = os.pipe()
    stdout = io.TextIOWrapper(os.fdopen(read_fd, "rb", buffering=0), encoding="utf-8")

    @final
    class _Process:
        def __init__(self, stdout: io.TextIOWrapper) -> None:
            self.stdout = stdout

    process = _Process(stdout)
    try:
        _ = os.write(write_fd, b"native-wire-client: async-report\n")
        line = client_control_module._read_process_stdout_line(  # pyright: ignore[reportPrivateUsage]
            cast(subprocess.Popen[str], cast(object, process)),
            timeout_deadline=time.monotonic(),
            line_buffer=bytearray(),
        )
        assert line == "native-wire-client: async-report\n"
    finally:
        os.close(write_fd)
        stdout.close()


def test_discovered_rcb_dataset_attr_refreshes_candidate_signals() -> None:
    service = Iec61850ClientControlService()
    _ = service.configure_target(
        client_control_module.Iec61850ClientTargetRequest(
            mode="external-mms",
            host="172.16.40.128",
            port=12447,
        )
    )

    service._apply_external_mms_client_line("native-wire-client: discovered-dataset[0] reference=KINTE15BCU01SYSTEM/LLN0.ST")  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    service._apply_external_mms_client_line(  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
        "native-wire-client: discovered-dataset-member[0.0] dataset=KINTE15BCU01SYSTEM/LLN0.ST ref=KINTE15BCU01SYSTEM/LLN0$ST$Beh"
    )
    service._apply_external_mms_client_line("native-wire-client: discovered-dataset[1] reference=KINTE15BCU01CTRL1/LLN0.ST")  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    service._apply_external_mms_client_line(  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
        "native-wire-client: discovered-dataset-member[1.0] dataset=KINTE15BCU01CTRL1/LLN0.ST ref=KINTE15BCU01CTRL1/CBCSWI1$ST$Pos"
    )
    service._apply_external_mms_client_line("native-wire-client: discovered-brcb[0] domain=KINTE15BCU01CTRL1 item=LLN0$BR$brcbST")  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    service._apply_external_mms_client_line(  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
        "native-wire-client: discovered-rcb-attr[0] domain=KINTE15BCU01CTRL1 item=LLN0$BR$brcbST field=DatSet value=KINTE15BCU01CTRL1/LLN0.ST"
    )

    candidate = next(item for item in service._available_candidates if item.id == "KINTE15BCU01CTRL1:LLN0$BR$brcbST")  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]

    assert candidate.data_set_ref == "KINTE15BCU01CTRL1/LLN0.ST"
    assert tuple(signal.reference for signal in candidate.signals) == ("CBCSWI1.Pos[ST]",)


def test_external_mms_uses_discovered_indexed_rcb_item_from_candidate_id() -> None:
    candidate = Iec61850ReportControlCandidate(
        id="KINTE15BCU01CTRL1:LLN0$BR$brcbST01",
        ied_name="KINTE15BCU01",
        access_point_name="AP1",
        logical_device_inst="CTRL1",
        logical_node_name="LLN0",
        report_control_name="brcbST",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id="KINTE15BCU01CTRL1/LLN0.brcbST",
        data_set_ref="KINTE15BCU01CTRL1/LLN0$LLN0BRptStDs",
        conf_rev=None,
        indexed=True,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(),
    )
    service = Iec61850ClientControlService(candidate=candidate)

    assert service._external_rcb_configuration_commands() == ()  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    assert service._external_rptena_command(True) == "write-bool KINTE15BCU01CTRL1 LLN0$BR$brcbST01$RptEna true"  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]


def test_external_mms_direct_unbuffered_rcb_reserves_before_rptena() -> None:
    candidate = Iec61850ReportControlCandidate(
        id="KINTE15BCU01CTRL1:RSYN1$RP$urcbMX01",
        ied_name="KINTE15BCU01",
        access_point_name="AP1",
        logical_device_inst="CTRL1",
        logical_node_name="RSYN1",
        report_control_name="urcbMX",
        report_kind=Iec61850ReportKind.UNBUFFERED,
        rpt_id="KINTE15BCU01CTRL1/RSYN1.urcbMX",
        data_set_ref="KINTE15BCU01CTRL1/RSYN1$RSYN1URptMxDs",
        conf_rev=None,
        indexed=True,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(),
    )
    service = Iec61850ClientControlService(candidate=candidate)

    assert service._external_urcb_reservation_command(True) == "write-bool KINTE15BCU01CTRL1 RSYN1$RP$urcbMX01$Resv true"  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    assert service._external_rptena_command(True) == "write-bool KINTE15BCU01CTRL1 RSYN1$RP$urcbMX01$RptEna true"  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]


def _wire_frame_response_line(frame: bytes) -> bytes:
    return f"wire-frame={frame.hex()}\n".encode("utf-8")


@final
class _QueuedStdout:
    def __init__(self) -> None:
        self.lines: list[str] = []

    def enqueue(self, frame: bytes) -> None:
        self.lines.append(_wire_frame_response_line(frame).decode("utf-8"))

    def readline(self) -> str:
        if not self.lines:
            return ""
        return self.lines.pop(0)


class _ProcessLike(Protocol):
    stdout: _QueuedStdout
    _returncode: int | None


@final
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




@final
class _ExternalMmsClientStdin:
    def __init__(self, commands: list[str], stdout: _QueuedStdout) -> None:
        self._commands = commands
        self._stdout = stdout

    def write(self, value: str) -> None:
        command = value.rstrip("\n")
        self._commands.append(command)
        if command == "discover" or command.startswith("discover "):
            self._stdout.lines.append("native-wire-client: discovered-logical-device[0] domain=KINTE13LVC01CTRL\n")
            self._stdout.lines.append("native-wire-client: discovered-logical-device[1] domain=KINTE13LVC01PROT\n")
            self._stdout.lines.append("native-wire-client: discovered-logical-node[0] domain=KINTE13LVC01CTRL name=LLN0\n")
            self._stdout.lines.append("native-wire-client: discovered-logical-node[1] domain=KINTE13LVC01PROT name=LLN0\n")
            self._stdout.lines.append("native-wire-client: discovered-dataset[0] reference=KINTE13LVC01CTRL/LLN0.RCB1\n")
            self._stdout.lines.append("native-wire-client: discovered-dataset[1] reference=KINTE13LVC01PROT/LLN0.RCB2\n")
            self._stdout.lines.append("native-wire-client: discovered-dataset-member[0.0] dataset=KINTE13LVC01CTRL/LLN0.RCB1 ref=KINTE13LVC01CTRL/XCBR1$ST$Pos\n")
            self._stdout.lines.append("native-wire-client: discovered-dataset-member[1.0] dataset=KINTE13LVC01PROT/LLN0.RCB2 ref=KINTE13LVC01PROT/PTOC1$ST$Str\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 kind=buffered\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb[1] domain=KINTE13LVC01PROT item=LLN0$RP$urcbB01 kind=unbuffered\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=RptID value=KINTE13LVC01CTRL/LLN0.brcbA\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=DatSet value=KINTE13LVC01CTRL/LLN0.RCB1\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=ConfRev value=10000\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=BufTm value=500\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=IntgPd value=0\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=OptFlds value=0x067f80\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=TrgOps value=0x0274\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb-attr[1] domain=KINTE13LVC01PROT item=LLN0$RP$urcbB01 field=RptID value=KINTE13LVC01PROT/LLN0.urcbB\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb-attr[1] domain=KINTE13LVC01PROT item=LLN0$RP$urcbB01 field=DatSet value=KINTE13LVC01PROT/LLN0.RCB2\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb-attr[1] domain=KINTE13LVC01PROT item=LLN0$RP$urcbB01 field=ConfRev value=7\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb-attr[1] domain=KINTE13LVC01PROT item=LLN0$RP$urcbB01 field=BufTm value=250\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb-attr[1] domain=KINTE13LVC01PROT item=LLN0$RP$urcbB01 field=IntgPd value=1000\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb-attr[1] domain=KINTE13LVC01PROT item=LLN0$RP$urcbB01 field=OptFlds value=0x067f80\n")
            self._stdout.lines.append("native-wire-client: discovered-rcb-attr[1] domain=KINTE13LVC01PROT item=LLN0$RP$urcbB01 field=TrgOps value=0x0274\n")
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=discover rcb=KINTE13LVC01CTRL/LLN0.brcbA/<none> rcb-index=0 rptEna=false rptEna-invoke=0 giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command in {
            "write-hex KINTE13LVC01CTRL LLN0$BR$brcbA$OptFlds 4 067f80",
            "write-hex KINTE13LVC01CTRL LLN0$BR$brcbA$TrgOps 4 0274",
            "write-hex KINTE13LVC01CTRL LLN0$BR$brcbA01$OptFlds 4 067f80",
            "write-hex KINTE13LVC01CTRL LLN0$BR$brcbA01$TrgOps 4 0274",
            "write-hex KINTE13LVC01PROT LLN0$RP$urcbB01$OptFlds 4 067f80",
            "write-hex KINTE13LVC01PROT LLN0$RP$urcbB01$TrgOps 4 0274",
        }:
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command in {"write-bool KINTE13LVC01CTRL LLN0$BR$brcbA$RptEna true", "rptena 0"}:
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=rptena rcb=KINTE13LVC01CTRL/LLN0.brcbA/buffered rcb-index=0 rptEna=true rptEna-invoke=4 giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command == "rptena 1":
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=rptena rcb=KINTE13LVC01PROT/LLN0.urcbB/unbuffered rcb-index=1 rptEna=true rptEna-invoke=4 giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command in {"write-bool KINTE13LVC01CTRL LLN0$BR$brcbA$GI true", "gi 0"}:
            self._stdout.lines.append(
                "native-wire-client: report-entry index=0 reference=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal dataRef=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal value=true kind=bool reason=general-interrogation datasetMatch=true discoveredMatch=true\n"
            )
            self._stdout.lines.append(
                "native-wire-client: report-entry index=1 reference=KINTE13LVC01CTRL/XCBR1$ST$Pos$q dataRef=KINTE13LVC01CTRL/XCBR1$ST$Pos$q value=0 kind=quality reason=general-interrogation datasetMatch=true discoveredMatch=true\n"
            )
            self._stdout.lines.append(
                "native-wire-client: report-entry index=2 reference=KINTE13LVC01CTRL/XCBR1$ST$Pos$t dataRef=KINTE13LVC01CTRL/XCBR1$ST$Pos$t value=<empty> kind=timestamp reason=general-interrogation datasetMatch=true discoveredMatch=true\n"
            )
            self._stdout.lines.append("native-wire-client: async-report\n")
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=async-report rcb=KINTE13LVC01CTRL/LLN0.brcbA/buffered rcb-index=0 rptEna=true rptEna-invoke=4 giRequested=true gi-invoke=5 lastReportReceived=true asyncReports=1 lastReportValues=3 lastReportDataRefs=3 lastReportMatchedDataRefs=3 lastReportReasons=3 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=gi rcb=KINTE13LVC01CTRL/LLN0.brcbA/buffered rcb-index=0 rptEna=true rptEna-invoke=4 giRequested=true gi-invoke=5 lastReportReceived=true asyncReports=1 lastReportValues=3 lastReportDataRefs=3 lastReportMatchedDataRefs=3 lastReportReasons=3 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command == "gi 1":
            self._stdout.lines.append(
                "native-wire-client: report-entry index=0 reference=KINTE13LVC01PROT/PTOC1$ST$Str$stVal dataRef=KINTE13LVC01PROT/PTOC1$ST$Str$stVal value=true kind=bool reason=general-interrogation datasetMatch=true discoveredMatch=true\n"
            )
            self._stdout.lines.append("native-wire-client: async-report\n")
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=gi rcb=KINTE13LVC01PROT/LLN0.urcbB/unbuffered rcb-index=1 rptEna=true rptEna-invoke=4 giRequested=true gi-invoke=5 lastReportReceived=true asyncReports=1 lastReportValues=1 lastReportDataRefs=1 lastReportMatchedDataRefs=1 lastReportReasons=1 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command == "write-bool KINTE13LVC01CTRL LLN0$BR$brcbA$RptEna false":
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command in {"disconnect", "exit"}:
            self._stdout.lines.append("native-wire-client: disconnected\n")
            self._stdout.lines.append("native-wire-client: state=stopped\n")

    def flush(self) -> None:
        return None


@final
class _ExternalMmsClientProcess:
    def __init__(self, command: Sequence[str], commands: list[str]) -> None:
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

    def poll(self) -> int | None:
        return self._returncode

    def terminate(self) -> None:
        self._returncode = 0

    def kill(self) -> None:
        self._returncode = -9

    def wait(self, timeout: float | None = None) -> int | None:  # pyright: ignore[reportUnusedParameter]
        self._returncode = 0
        return self._returncode


@final
class _DelayedGiExternalMmsClientStdin:
    def __init__(self, commands: list[str], stdout: _QueuedStdout) -> None:
        self._commands = commands
        self._stdout = stdout

    def write(self, value: str) -> None:
        command = value.rstrip("\n")
        self._commands.append(command)
        if command == "write-bool KINTE13LVC01CTRL LLN0$BR$brcbA01$RptEna true":
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=rptena rcb=KINTE13LVC01CTRL/LLN0$BR$brcbA01 rcb-index=0 rptEna=true rptEna-invoke=1 giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command == "write-bool KINTE13LVC01CTRL LLN0$BR$brcbA01$GI true":
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=gi rcb=KINTE13LVC01CTRL/LLN0$BR$brcbA01 rcb-index=0 rptEna=true rptEna-invoke=1 giRequested=true gi-invoke=2 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command.startswith("poll-reports "):
            self._stdout.lines.append(
                "native-wire-client: report-entry index=0 reference=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal dataRef=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal value=true kind=bool reason=general-interrogation datasetMatch=true discoveredMatch=true\n"
            )
            self._stdout.lines.append("native-wire-client: async-report\n")
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=async-report rcb=KINTE13LVC01CTRL/LLN0$BR$brcbA01 rcb-index=0 rptEna=true rptEna-invoke=1 giRequested=true gi-invoke=2 lastReportReceived=true asyncReports=1 lastReportValues=1 lastReportDataRefs=1 lastReportMatchedDataRefs=1 lastReportReasons=1 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command in {"write-bool KINTE13LVC01CTRL LLN0$BR$brcbA01$RptEna false", "disconnect", "exit"}:
            self._stdout.lines.append("native-wire-client: state=ready\n")

    def flush(self) -> None:
        return None


@final
class _DelayedGiExternalMmsClientProcess:
    def __init__(self, command: Sequence[str], commands: list[str]) -> None:
        self.command = tuple(command)
        self.stdout = _QueuedStdout()
        self.stdout.lines.extend([
            "native-wire-client: state=init\n",
            "native-wire-client: state=data-connected\n",
            "native-wire-client: state=associated\n",
            "native-wire-client: state=ready\n",
            "native-wire-client: ready\n",
        ])
        self.stdin = _DelayedGiExternalMmsClientStdin(commands, self.stdout)
        self.stderr = _QueuedStdout()
        self.pid = 4242
        self._returncode = None

    def poll(self) -> int | None:
        return self._returncode

    def terminate(self) -> None:
        self._returncode = 0

    def kill(self) -> None:
        self._returncode = -9

    def wait(self, timeout: float | None = None) -> int | None:  # pyright: ignore[reportUnusedParameter]
        self._returncode = 0
        return self._returncode


@final
class _RptenaImmediateReportStdin:
    def __init__(self, commands: list[str], stdout: _QueuedStdout) -> None:
        self._commands = commands
        self._stdout = stdout

    def write(self, value: str) -> None:
        command = value.rstrip("\n")
        self._commands.append(command)
        if command == "write-bool KINTE13LVC01CTRL LLN0$BR$brcbA01$RptEna true":
            self._stdout.lines.append(
                "native-wire-client: report-entry index=0 reference=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal dataRef=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal value=true kind=bool reason=data-change datasetMatch=true discoveredMatch=true\n"
            )
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=report rcb=KINTE13LVC01CTRL/LLN0$BR$brcbA01 rcb-index=0 rptEna=true rptEna-invoke=1 giRequested=false gi-invoke=0 lastReportReceived=true asyncReports=0 lastReportValues=1 lastReportDataRefs=1 lastReportMatchedDataRefs=1 lastReportReasons=1 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command in {"disconnect", "exit"}:
            self._stdout.lines.append("native-wire-client: state=stopped\n")

    def flush(self) -> None:
        return None


@final
class _RptenaImmediateReportProcess:
    def __init__(self, command: Sequence[str], commands: list[str]) -> None:
        self.command = tuple(command)
        self.stdout = _QueuedStdout()
        self.stdout.lines.extend([
            "native-wire-client: state=init\n",
            "native-wire-client: state=data-connected\n",
            "native-wire-client: state=associated\n",
            "native-wire-client: state=ready\n",
            "native-wire-client: ready\n",
        ])
        self.stdin = _RptenaImmediateReportStdin(commands, self.stdout)
        self.stderr = _QueuedStdout()
        self.pid = 4242
        self._returncode = None

    def poll(self) -> int | None:
        return self._returncode

    def terminate(self) -> None:
        self._returncode = 0

    def kill(self) -> None:
        self._returncode = -9

    def wait(self, timeout: float | None = None) -> int | None:  # pyright: ignore[reportUnusedParameter]
        self._returncode = 0
        return self._returncode


@final
class _GiInterleavedDataChangeStdin:
    def __init__(self, commands: list[str], stdout: _QueuedStdout) -> None:
        self._commands = commands
        self._stdout = stdout

    def write(self, value: str) -> None:
        command = value.rstrip("\n")
        self._commands.append(command)
        if command == "write-bool KINTE13LVC01CTRL LLN0$BR$brcbA01$RptEna true":
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=rptena rcb=KINTE13LVC01CTRL/LLN0$BR$brcbA01 rcb-index=0 rptEna=true rptEna-invoke=1 giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command == "write-bool KINTE13LVC01CTRL LLN0$BR$brcbA01$GI true":
            self._stdout.lines.append(
                "native-wire-client: report-entry index=0 reference=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal dataRef=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal value=true kind=bool reason=data-change datasetMatch=true discoveredMatch=true\n"
            )
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=async-report rcb=KINTE13LVC01CTRL/LLN0$BR$brcbA01 rcb-index=0 rptEna=true rptEna-invoke=1 giRequested=true gi-invoke=2 lastReportReceived=true asyncReports=1 lastReportValues=1 lastReportDataRefs=1 lastReportMatchedDataRefs=1 lastReportReasons=1 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append("native-wire-client: state=ready\n")
        elif command.startswith("poll-reports "):
            self._stdout.lines.append(
                "native-wire-client: report-entry index=0 reference=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal dataRef=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal value=false kind=bool reason=general-interrogation datasetMatch=true discoveredMatch=true\n"
            )
            self._stdout.lines.append(
                "native-wire-client: report-entry index=1 reference=KINTE13LVC01CTRL/XCBR2$ST$Pos$stVal dataRef=KINTE13LVC01CTRL/XCBR2$ST$Pos$stVal value=true kind=bool reason=general-interrogation datasetMatch=true discoveredMatch=true\n"
            )
            self._stdout.lines.append(
                "native-wire-client: subscription-summary phase=async-report rcb=KINTE13LVC01CTRL/LLN0$BR$brcbA01 rcb-index=0 rptEna=true rptEna-invoke=1 giRequested=true gi-invoke=2 lastReportReceived=true asyncReports=2 lastReportValues=2 lastReportDataRefs=2 lastReportMatchedDataRefs=2 lastReportReasons=2 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._stdout.lines.append("native-wire-client: state=ready\n")

    def flush(self) -> None:
        return None


@final
class _GiInterleavedDataChangeProcess:
    def __init__(self, command: Sequence[str], commands: list[str]) -> None:
        self.command = tuple(command)
        self.stdout = _QueuedStdout()
        self.stdout.lines.extend([
            "native-wire-client: state=init\n",
            "native-wire-client: state=data-connected\n",
            "native-wire-client: state=associated\n",
            "native-wire-client: state=ready\n",
            "native-wire-client: ready\n",
        ])
        self.stdin = _GiInterleavedDataChangeStdin(commands, self.stdout)
        self.stderr = _QueuedStdout()
        self.pid = 4242
        self._returncode = None

    def poll(self) -> int | None:
        return self._returncode

    def terminate(self) -> None:
        self._returncode = 0

    def kill(self) -> None:
        self._returncode = -9

    def wait(self, timeout: float | None = None) -> int | None:  # pyright: ignore[reportUnusedParameter]
        self._returncode = 0
        return self._returncode


@final
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

    def poll(self) -> int | None:
        return self._returncode

    def terminate(self) -> None:
        self._returncode = 0

    def kill(self) -> None:
        self._returncode = -9

    def wait(self, timeout: float | None = None) -> int | None:  # pyright: ignore[reportUnusedParameter]
        self._returncode = 0
        return self._returncode


@final
class _FailingExternalMmsClientStdin:
    def __init__(self, process: _FailingExternalMmsClientProcess, commands: list[str]) -> None:
        self._process = process
        self._commands = commands

    def write(self, value: str) -> None:
        command = value.rstrip("\n")
        self._commands.append(command)
        if command == "discover" or command.startswith("discover "):
            self._process.stdout.lines.append("native-wire-client: discovered-logical-device[0] domain=KINTE13LVC01CTRL\n")
            self._process.stdout.lines.append("native-wire-client: discovered-logical-node[0] domain=KINTE13LVC01CTRL name=LLN0\n")
            self._process.stdout.lines.append("native-wire-client: discovered-dataset[0] reference=KINTE13LVC01CTRL/LLN0.RCB1\n")
            self._process.stdout.lines.append("native-wire-client: discovered-dataset-member[0.0] dataset=KINTE13LVC01CTRL/LLN0.RCB1 ref=KINTE13LVC01CTRL/XCBR1$ST$Pos\n")
            self._process.stdout.lines.append("native-wire-client: discovered-brcb[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01\n")
            self._process.stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=RptID value=KINTE13LVC01CTRL/LLN0.brcbA\n")
            self._process.stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=DatSet value=KINTE13LVC01CTRL/LLN0.RCB1\n")
            self._process.stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=ConfRev value=10000\n")
            self._process.stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=BufTm value=500\n")
            self._process.stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=IntgPd value=0\n")
            self._process.stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=OptFlds value=0x067f80\n")
            self._process.stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=TrgOps value=0x0274\n")
            self._process.stdout.lines.append(
                "native-wire-client: subscription-summary phase=discover rcb=KINTE13LVC01CTRL/LLN0.brcbA/<none> rcb-index=0 rptEna=false giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._process.stdout.lines.append(
                "native-wire-client: model-summary phase=discover domain=KINTE13LVC01CTRL logical-devices=1 logical-nodes=1 data-names=1 typed-data-names=1 data-components=3 typed-data-components=3 typed-data-nodes=3 leaf-refs=1 datasets=1 dataset-members=1 brcbs=1 last-report-entries=0 last-report-dataRefs=0 last-report-values=0 last-report-reasons=0 last-report-matched-dataRefs=0 last-report-dataset-mismatches=0 last-report-missing-values=0 last-report-extra-values=0 last-report-missing-reasons=0 last-report-extra-reasons=0 last-report-unsupported-values=0 last-report-rptId=<none> last-report-datSet=<none>\n"
            )
            self._process.stdout.lines.append("native-wire-client: state=ready\n")
        elif command in {
            "write-hex KINTE13LVC01CTRL LLN0$BR$brcbA$OptFlds 4 067f80",
            "write-hex KINTE13LVC01CTRL LLN0$BR$brcbA$TrgOps 4 0274",
            "write-hex KINTE13LVC01CTRL LLN0$BR$brcbA01$OptFlds 4 067f80",
            "write-hex KINTE13LVC01CTRL LLN0$BR$brcbA01$TrgOps 4 0274",
        }:
            self._process.stdout.lines.append("native-wire-client: state=ready\n")
        elif command in {"write-bool KINTE13LVC01CTRL LLN0$BR$brcbA$RptEna true", "rptena 0"}:
            self._process.stdout.lines.append(
                "native-wire-client: subscription-summary phase=rptena rcb=KINTE13LVC01CTRL/LLN0.brcbA/buffered rcb-index=0 rptEna=true rptEna-invoke=4 giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._process.stdout.lines.append("native-wire-client: state=ready\n")
        elif command == self._process._fail_on_command:  # pyright: ignore[reportPrivateUsage]
            self._process.stdout.lines.append("native-wire-client: state=failed\n")
            self._process._returncode = 1  # pyright: ignore[reportPrivateUsage]
        elif command in {"disconnect", "exit"}:
            self._process.stdout.lines.append("native-wire-client: disconnected\n")
            self._process.stdout.lines.append("native-wire-client: state=stopped\n")

    def flush(self) -> None:
        return None


@final
class _NoReadyExternalMmsClientStdin:
    def __init__(self, process: _ProcessLike, commands: list[str]) -> None:
        self._process = process
        self._commands = commands

    def write(self, value: str) -> None:
        command = value.rstrip("\n")
        self._commands.append(command)
        if command == "discover" or command.startswith("discover "):
            self._process.stdout.lines.append("native-wire-client: discovered-logical-device[0] domain=KINTE13LVC01CTRL\n")
            self._process.stdout.lines.append("native-wire-client: discovered-logical-node[0] domain=KINTE13LVC01CTRL name=LLN0\n")
            self._process.stdout.lines.append("native-wire-client: discovered-dataset[0] reference=KINTE13LVC01CTRL/LLN0.RCB1\n")
            self._process.stdout.lines.append("native-wire-client: discovered-dataset-member[0.0] dataset=KINTE13LVC01CTRL/LLN0.RCB1 ref=KINTE13LVC01CTRL/XCBR1$ST$Pos\n")
            self._process.stdout.lines.append("native-wire-client: discovered-brcb[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01\n")
            self._process.stdout.lines.append(
                "native-wire-client: subscription-summary phase=discover rcb=KINTE13LVC01CTRL/LLN0.brcbA/<none> rcb-index=0 rptEna=false giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
        elif command in {"write-hex KINTE13LVC01CTRL LLN0$BR$brcbA01$OptFlds 4 067f80", "write-hex KINTE13LVC01CTRL LLN0$BR$brcbA01$TrgOps 4 0274"}:
            self._process._returncode = 0  # pyright: ignore[reportPrivateUsage]
        elif command in {"write-bool KINTE13LVC01CTRL LLN0$BR$brcbA01$RptEna true", "rptena 0"}:
            self._process.stdout.lines.append(
                "native-wire-client: subscription-summary phase=rptena rcb=KINTE13LVC01CTRL/LLN0.brcbA/buffered rcb-index=0 rptEna=true rptEna-invoke=4 giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._process._returncode = 0  # pyright: ignore[reportPrivateUsage]
        elif command in {
            "write-bool KINTE13LVC01CTRL LLN0$BR$brcbA$GI true",
            "write-bool KINTE13LVC01CTRL LLN0$BR$brcbA01$GI true",
            "gi 0",
        }:
            self._process.stdout.lines.append(
                "native-wire-client: report-entry index=0 reference=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal dataRef=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal value=true kind=bool reason=general-interrogation datasetMatch=true discoveredMatch=true\n"
            )
            self._process.stdout.lines.append(
                "native-wire-client: subscription-summary phase=gi rcb=KINTE13LVC01CTRL/LLN0.brcbA/buffered rcb-index=0 rptEna=true rptEna-invoke=4 giRequested=true gi-invoke=5 lastReportReceived=true asyncReports=1 lastReportValues=1 lastReportDataRefs=1 lastReportMatchedDataRefs=1 lastReportReasons=1 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._process._returncode = 0  # pyright: ignore[reportPrivateUsage]
        elif command == "disconnect":
            self._process.stdout.lines.append("native-wire-client: disconnected\n")
            self._process.stdout.lines.append("native-wire-client: state=stopped\n")

    def flush(self) -> None:
        return None


@final
class _NoReadyExternalMmsClientProcess:
    def __init__(self, command: Sequence[str], commands: list[str]) -> None:
        self.command = tuple(command)
        self.stdout = _QueuedStdout()
        self.stdout.lines.extend([
            "native-wire-client: state=init\n",
            "native-wire-client: state=data-connected\n",
            "native-wire-client: state=associated\n",
            "native-wire-client: state=ready\n",
            "native-wire-client: ready\n",
        ])
        self.stdin = _NoReadyExternalMmsClientStdin(self, commands)
        self.stderr = _QueuedStdout()
        self.pid = 4242
        self._returncode = None

    def poll(self) -> int | None:
        return self._returncode

    def terminate(self) -> None:
        self._returncode = 0

    def kill(self) -> None:
        self._returncode = -9

    def wait(self, timeout: float | None = None) -> int | None:  # pyright: ignore[reportUnusedParameter]
        self._returncode = 0
        return self._returncode


@final
class _BrokenPipeOnceExternalMmsClientStdin:
    def __init__(self, process: _ProcessLike, commands: list[str], broken_command: str) -> None:
        self._process = process
        self._commands = commands
        self._broken_command = broken_command
        self._broken = False

    def write(self, value: str) -> None:
        command = value.rstrip("\n")
        self._commands.append(command)
        if not self._broken and command == self._broken_command:
            self._broken = True
            raise BrokenPipeError(32, "Broken pipe")
        if command == "discover" or command.startswith("discover "):
            self._process.stdout.lines.append("native-wire-client: discovered-logical-device[0] domain=KINTE13LVC01CTRL\n")
            self._process.stdout.lines.append("native-wire-client: discovered-logical-node[0] domain=KINTE13LVC01CTRL name=LLN0\n")
            self._process.stdout.lines.append("native-wire-client: discovered-dataset[0] reference=KINTE13LVC01CTRL/LLN0.RCB1\n")
            self._process.stdout.lines.append("native-wire-client: discovered-dataset-member[0.0] dataset=KINTE13LVC01CTRL/LLN0.RCB1 ref=KINTE13LVC01CTRL/XCBR1$ST$Pos\n")
            self._process.stdout.lines.append("native-wire-client: discovered-brcb[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01\n")
            self._process.stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=RptID value=KINTE13LVC01CTRL/LLN0.brcbA\n")
            self._process.stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=DatSet value=KINTE13LVC01CTRL/LLN0.RCB1\n")
            self._process.stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=ConfRev value=10000\n")
            self._process.stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=BufTm value=500\n")
            self._process.stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=IntgPd value=0\n")
            self._process.stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=OptFlds value=0x067f80\n")
            self._process.stdout.lines.append("native-wire-client: discovered-rcb-attr[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 field=TrgOps value=0x0274\n")
            self._process.stdout.lines.append("native-wire-client: subscription-summary phase=discover rcb=KINTE13LVC01CTRL/LLN0.brcbA/<none> rcb-index=0 rptEna=false giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n")
            self._process.stdout.lines.append("native-wire-client: state=ready\n")
        elif command in {"write-hex KINTE13LVC01CTRL LLN0$BR$brcbA01$OptFlds 4 067f80", "write-hex KINTE13LVC01CTRL LLN0$BR$brcbA01$TrgOps 4 0274", "rptena 0"}:
            self._process.stdout.lines.append("native-wire-client: state=ready\n")
        elif command in {"write-bool KINTE13LVC01CTRL LLN0$BR$brcbA01$RptEna true", "write-bool KINTE13LVC01CTRL LLN0$BR$brcbA$RptEna true"}:
            self._process.stdout.lines.append("native-wire-client: subscription-summary phase=rptena rcb=KINTE13LVC01CTRL/LLN0.brcbA/buffered rcb-index=0 rptEna=true rptEna-invoke=4 giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n")
        elif command in {"write-bool KINTE13LVC01CTRL LLN0$BR$brcbA01$GI true", "gi 0"}:
            self._process.stdout.lines.append("native-wire-client: subscription-summary phase=gi rcb=KINTE13LVC01CTRL/LLN0.brcbA/buffered rcb-index=0 rptEna=true rptEna-invoke=4 giRequested=true gi-invoke=5 lastReportReceived=true asyncReports=1 lastReportValues=1 lastReportDataRefs=1 lastReportMatchedDataRefs=1 lastReportReasons=1 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n")

    def flush(self) -> None:
        return None


@final
class _BrokenPipeOnceExternalMmsClientProcess:
    def __init__(self, command: Sequence[str], commands: list[str], broken_command: str = "rptena 0") -> None:
        self.command = tuple(command)
        self.stdout = _QueuedStdout()
        self.stdout.lines.extend([
            "native-wire-client: state=init\n",
            "native-wire-client: state=data-connected\n",
            "native-wire-client: state=associated\n",
            "native-wire-client: state=ready\n",
            "native-wire-client: ready\n",
        ])
        self.stdin = _BrokenPipeOnceExternalMmsClientStdin(self, commands, broken_command)
        self.stderr = _QueuedStdout()
        self.pid = 4242
        self._returncode = None

    def poll(self) -> int | None:
        return self._returncode

    def terminate(self) -> None:
        self._returncode = 0

    def kill(self) -> None:
        self._returncode = -9

    def wait(self, timeout: float | None = None) -> int | None:  # pyright: ignore[reportUnusedParameter]
        self._returncode = 0
        return self._returncode


@final
class _SummaryOnlyExternalMmsClientStdin:
    def __init__(self, process: _ProcessLike, commands: list[str]) -> None:
        self._process = process
        self._commands = commands

    def write(self, value: str) -> None:
        command = value.rstrip("\n")
        self._commands.append(command)
        if command == "discover" or command.startswith("discover "):
            self._process.stdout.lines.append("native-wire-client: discovered-logical-device[0] domain=KINTE13LVC01CTRL\n")
            self._process.stdout.lines.append("native-wire-client: discovered-logical-node[0] domain=KINTE13LVC01CTRL name=LLN0\n")
            self._process.stdout.lines.append("native-wire-client: discovered-dataset[0] reference=KINTE13LVC01CTRL/LLN0.RCB1\n")
            self._process.stdout.lines.append("native-wire-client: discovered-dataset-member[0.0] dataset=KINTE13LVC01CTRL/LLN0.RCB1 ref=KINTE13LVC01CTRL/XCBR1$ST$Pos\n")
            self._process.stdout.lines.append("native-wire-client: discovered-brcb[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01\n")
            self._process.stdout.lines.append(
                "native-wire-client: subscription-summary phase=discover rcb=KINTE13LVC01CTRL/LLN0.brcbA/<none> rcb-index=0 rptEna=false giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._process.stdout.lines.append(
                "native-wire-client: model-summary phase=discover domain=KINTE13LVC01CTRL logical-devices=1 logical-nodes=1 data-names=1 typed-data-names=1 data-components=3 typed-data-components=3 typed-data-nodes=3 leaf-refs=1 datasets=1 dataset-members=1 brcbs=1 last-report-entries=0 last-report-dataRefs=0 last-report-values=0 last-report-reasons=0 last-report-matched-dataRefs=0 last-report-dataset-mismatches=0 last-report-missing-values=0 last-report-extra-values=0 last-report-missing-reasons=0 last-report-extra-reasons=0 last-report-unsupported-values=0 last-report-rptId=<none> last-report-datSet=<none>\n"
            )
        elif command in {"rptena 0", "gi 0"}:
            self._process.stdout.lines.append("native-wire-client: state=ready\n")
        elif command == "disconnect":
            self._process.stdout.lines.append("native-wire-client: disconnected\n")
            self._process.stdout.lines.append("native-wire-client: state=stopped\n")

    def flush(self) -> None:
        return None


@final
class _SummaryOnlyExternalMmsClientProcess:
    def __init__(self, command: Sequence[str], commands: list[str]) -> None:
        self.command = tuple(command)
        self.stdout = _QueuedStdout()
        self.stdout.lines.extend([
            "native-wire-client: state=init\n",
            "native-wire-client: state=data-connected\n",
            "native-wire-client: state=associated\n",
            "native-wire-client: state=ready\n",
            "native-wire-client: ready\n",
        ])
        self.stdin = _SummaryOnlyExternalMmsClientStdin(self, commands)
        self.stderr = _QueuedStdout()
        self.pid = 4242
        self._returncode = None

    def poll(self) -> int | None:
        return self._returncode

    def terminate(self) -> None:
        self._returncode = 0

    def kill(self) -> None:
        self._returncode = -9

    def wait(self, timeout: float | None = None) -> int | None:  # pyright: ignore[reportUnusedParameter]
        self._returncode = 0
        return self._returncode


@final
class _MixedDomainExternalMmsClientStdin:
    def __init__(self, process: _ProcessLike, commands: list[str]) -> None:
        self._process = process
        self._commands = commands

    def write(self, value: str) -> None:
        command = value.rstrip("\n")
        self._commands.append(command)
        if command == "discover" or command.startswith("discover "):
            self._process.stdout.lines.append("native-wire-client: discovered-logical-device[0] domain=KINTE13LVC01CTRL\n")
            self._process.stdout.lines.append("native-wire-client: discovered-logical-node[0] domain=KINTE13LVC01CTRL name=LLN0\n")
            self._process.stdout.lines.append("native-wire-client: discovered-dataset[0] reference=KINTE13LVC01CTRL/LLN0.RCB1\n")
            self._process.stdout.lines.append("native-wire-client: discovered-dataset-member[0.0] dataset=KINTE13LVC01CTRL/LLN0.RCB1 ref=KINTE13LVC01CTRL/XCBR1$ST$Pos\n")
            self._process.stdout.lines.append("native-wire-client: discovered-brcb[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01\n")
            self._process.stdout.lines.append("native-wire-client: discovered-logical-device[1] domain=KINTE13LVC01PROT\n")
            self._process.stdout.lines.append("native-wire-client: discover-skip=domain domain=KINTE13LVC01PROT reason=domain-sequence-failed\n")
            self._process.stdout.lines.append(
                "native-wire-client: subscription-summary phase=discover rcb=KINTE13LVC01CTRL/LLN0.brcbA/<none> rcb-index=0 rptEna=false giRequested=false gi-invoke=0 lastReportReceived=false asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n"
            )
            self._process.stdout.lines.append(
                "native-wire-client: model-summary phase=discover domain=KINTE13LVC01CTRL logical-devices=1 logical-nodes=1 data-names=1 typed-data-names=1 data-components=3 typed-data-components=3 typed-data-nodes=3 leaf-refs=1 datasets=1 dataset-members=1 brcbs=1 last-report-entries=0 last-report-dataRefs=0 last-report-values=0 last-report-reasons=0 last-report-matched-dataRefs=0 last-report-dataset-mismatches=0 last-report-missing-values=0 last-report-extra-values=0 last-report-missing-reasons=0 last-report-extra-reasons=0 last-report-unsupported-values=0 last-report-rptId=<none> last-report-datSet=<none>\n"
            )
            self._process.stdout.lines.append("native-wire-client: state=ready\n")
        elif command in {"rptena 0", "gi 0"}:
            self._process.stdout.lines.append("native-wire-client: state=ready\n")
        elif command == "disconnect":
            self._process.stdout.lines.append("native-wire-client: disconnected\n")
            self._process.stdout.lines.append("native-wire-client: state=stopped\n")

    def flush(self) -> None:
        return None


@final
class _MixedDomainExternalMmsClientProcess:
    def __init__(self, command: Sequence[str], commands: list[str]) -> None:
        self.command = tuple(command)
        self.stdout = _QueuedStdout()
        self.stdout.lines.extend([
            "native-wire-client: state=init\n",
            "native-wire-client: state=data-connected\n",
            "native-wire-client: state=associated\n",
            "native-wire-client: state=ready\n",
            "native-wire-client: ready\n",
        ])
        self.stdin = _MixedDomainExternalMmsClientStdin(self, commands)
        self.stderr = _QueuedStdout()
        self.pid = 4242
        self._returncode = None

    def poll(self) -> int | None:
        return self._returncode

    def terminate(self) -> None:
        self._returncode = 0

    def kill(self) -> None:
        self._returncode = -9

    def wait(self, timeout: float | None = None) -> int | None:  # pyright: ignore[reportUnusedParameter]
        self._returncode = 0
        return self._returncode


@final
class _CommandDrivenSelect:
    @staticmethod
    def select(readable: Sequence[object], _writable: Sequence[object], _exceptional: Sequence[object], timeout: float | None = None) -> tuple[list[object], list[object], list[object]]:  # pyright: ignore[reportUnusedParameter]
        ready: list[object] = []
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
    _ = service.open_session()

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        _ = service.open_session()

    assert error.value.code == "SESSION_EXISTS"
    diagnostic = service.snapshot().last_diagnostic
    assert diagnostic is not None
    assert diagnostic.code == "SESSION_EXISTS"


def test_external_mms_uses_configured_production_binary(monkeypatch: pytest.MonkeyPatch) -> None:
    binary = "/opt/unitlab/bin/unitlab-iec61850-ied-sim"
    monkeypatch.setenv("IEC61850_IED_LIVE_WIRE_BINARY_PATH", binary)
    get_settings.cache_clear()
    try:
        service = Iec61850ClientControlService()
        assert service._external_probe_binary_path() == binary  # pyright: ignore[reportPrivateUsage]
    finally:
        get_settings.cache_clear()


def test_external_mms_missing_binary_reports_installation_error(monkeypatch: pytest.MonkeyPatch) -> None:
    service = Iec61850ClientControlService(live_wire_binary_path="")
    def missing_path(_path: Path) -> bool:
        return False

    monkeypatch.setattr(Path, "is_file", missing_path)
    with pytest.raises(Iec61850ReportRuntimeError) as error:
        _ = service._external_probe_binary_path()  # pyright: ignore[reportPrivateUsage]
    assert error.value.code == "EXTERNAL_MMS_BINARY_UNAVAILABLE"


def test_client_control_service_uses_env_live_wire_binary_path(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("IEC61850_IED_LIVE_WIRE_BINARY_PATH", "/bin/true")
    get_settings.cache_clear()
    try:
        service = Iec61850ClientControlService(
            now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC),
            live_wire_service_host="wire-host",
        )

        started_specs: list[Iec61850IedSimulatorProcessSpec] = []
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

        @final
        class _FakeProcess:
            def __init__(self) -> None:
                self.stdin = _CommandDrivenProcessStdin(process_commands, stdout)
                self.stdout = stdout

        @final
        class _FakeHandle:
            def __init__(self, spec: Iec61850IedSimulatorProcessSpec) -> None:
                self.spec = spec
                self.endpoint = spec.endpoint
                self.process = _FakeProcess()
                self.pid = 4242

        def fake_start_process(spec: Iec61850IedSimulatorProcessSpec, **_kwargs: object) -> _FakeHandle:
            started_specs.append(spec)
            return _FakeHandle(spec)

        monkeypatch.setattr(client_control_module, "select", _CommandDrivenSelect)
        monkeypatch.setattr(client_control_module, "start_ied_simulator_process", fake_start_process)
        def stop_process(_handle: _FakeHandle) -> None:
            return None

        monkeypatch.setattr(client_control_module, "stop_ied_simulator_process", stop_process)

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

    @final
    class _FakeProcess:
        def __init__(self) -> None:
            self.stdin = _CommandDrivenProcessStdin(process_commands, stdout)
            self.stdout = stdout

    @final
    class _FakeHandle:
        spec: Iec61850IedSimulatorProcessSpec | None
        endpoint: Iec61850DeviceEndpoint

        def __init__(self) -> None:
            self.process = _FakeProcess()
            self.pid = 4242
            self.spec = None
            self.endpoint = service.snapshot().endpoint

    def fake_start_process(spec: Iec61850IedSimulatorProcessSpec, **_kwargs: object) -> _FakeHandle:
        handle = _FakeHandle()
        handle.spec = spec
        handle.endpoint = spec.endpoint
        return handle

    monkeypatch.setattr(client_control_module, "select", _CommandDrivenSelect)
    monkeypatch.setattr(client_control_module, "start_ied_simulator_process", fake_start_process)
    def stop_process(_handle: _FakeHandle) -> None:
        return None

    monkeypatch.setattr(client_control_module, "stop_ied_simulator_process", stop_process)

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
    assert _payload(snapshot.last_discovery)["schema"] == "unitlab.iec61850.client.discovery.v1"
    assert _payload(snapshot.last_discovery)["logicalDevices"][0]["inst"] == "LD0"
    assert _payload(snapshot.last_discovery)["logicalNodes"][0]["name"] == "LLN0"
    assert _payload(snapshot.last_discovery)["dataSets"][0]["memberCount"] == 1
    assert _payload(snapshot.last_discovery)["reportControls"][0]["name"] == "brcbEvents"
    assert snapshot.transcript[-1].kind == "ied-discover"


def test_debug_close_ied_removes_in_memory_state():
    service = Iec61850ClientControlService()
    _ = service.discover_ied()
    snapshot = service.close_ied()

    assert snapshot.session_open is False
    assert snapshot.last_discovery is None
    assert snapshot.last_read is None
    assert snapshot.last_state is None
    assert snapshot.last_report is None
    assert snapshot.transcript[-1].kind == "ied-close"


def test_debug_rptena_reserves_and_enables_report_control():
    service = Iec61850ClientControlService()
    _ = service.connect_ied()

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

    initial = _payload(service.snapshot().ui_state)
    assert initial["schema"] == "unitlab.iec61850.client.ui-state.v1"
    assert initial["session"]["phase"] == "idle"
    assert initial["discovery"]["discovered"] is False
    assert initial["subscription"]["subscribed"] is False
    assert initial["report"]["received"] is False

    discovered = _payload(service.discover_ied().ui_state)
    assert discovered["session"]["phase"] == "discovered"
    assert discovered["session"]["associated"] is True
    assert discovered["discovery"]["logical_devices"] == 1
    assert discovered["discovery"]["logical_nodes"] == 1
    assert discovered["discovery"]["data_sets"] == 1
    assert discovered["discovery"]["data_set_members"] == 1
    assert discovered["discovery"]["report_controls"] == 1
    assert discovered["discovery"]["selected_rcb_ref"] == "IED1/AP1/LD0/LLN0/brcbEvents/buffered"
    assert discovered["actions"]["can_rptena"] is True

    subscribed = _payload(service.enable_reporting().ui_state)
    assert subscribed["session"]["phase"] == "subscribed"
    assert subscribed["subscription"]["subscribed"] is True
    assert subscribed["subscription"]["rptena_enabled"] is True
    assert subscribed["subscription"]["owner"] == "unitlab-test-client"
    assert subscribed["actions"]["can_gi"] is True

    reporting = _payload(service.send_general_interrogation().ui_state)
    assert reporting["session"]["phase"] == "reporting"
    assert reporting["report"]["received"] is True
    assert reporting["report"]["reason"] == "general-interrogation"
    assert reporting["report"]["value_count"] == 1
    assert reporting["report"]["values"][0]["reference"] == "LD0/XCBR1.Pos.stVal[ST]"

    closed = _payload(service.close_ied().ui_state)
    assert closed["session"]["phase"] == "idle"
    assert closed["session"]["associated"] is False
    assert closed["discovery"]["discovered"] is False
    assert closed["subscription"]["subscribed"] is False
    assert closed["report"]["received"] is False


def test_client_control_ui_state_exposes_active_diagnostic() -> None:
    service = Iec61850ClientControlService(now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC))
    _ = service.open_session()

    with pytest.raises(Iec61850ReportRuntimeError):
        _ = service.open_session()

    ui_state = _payload(service.snapshot().ui_state)
    assert ui_state["session"]["phase"] == "failed"
    assert ui_state["diagnostic"]["active"] is True
    assert ui_state["diagnostic"]["action"] == "open-session"
    assert ui_state["diagnostic"]["code"] == "SESSION_EXISTS"



def test_client_control_configures_external_mms_target_from_scd(tmp_path: Path) -> None:
    scl_path = tmp_path / "target.scd"
    _ = scl_path.write_text(
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
    assert snapshot.endpoint_resolution.transport_source == "explicit_request"
    assert snapshot.endpoint_resolution.model_source == "scd-first"
    assert snapshot.endpoint_resolution.requested_host == "host.docker.internal"
    assert snapshot.endpoint_resolution.resolved_host == "host.docker.internal"
    assert snapshot.candidate.logical_device_inst == "CTRL"
    assert snapshot.candidate.logical_node_name == "LLN0"
    assert snapshot.candidate.report_control_name == "brcbA"
    assert snapshot.candidate.data_set_ref == "KINTE13LVC01CTRL/LLN0.RCB1"
    assert snapshot.candidate.signals[0].reference == "CTRL/XCBR1.Pos.stVal[ST]"
    assert snapshot.transcript[-1].kind == "target-configured"


def test_client_control_can_remap_a_real_device_ip_to_a_virtual_endpoint(tmp_path: Path) -> None:
    scl_path = tmp_path / "target.scd"
    _ = scl_path.write_text(
        """<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <IED name="C264_BCU_01">
    <AccessPoint name="AP1">
      <Server>
        <LDevice inst="CTRL">
          <LN0 lnClass="LLN0" inst="" lnType="T_CTRL">
            <DataSet name="RCB1">
              <FCDA ldInst="CTRL" lnClass="XCBR" lnInst="1" doName="Pos" daName="stVal" fc="ST" />
            </DataSet>
            <ReportControl name="brcbA" datSet="RCB1" buffered="true" indexed="true" rptID="C264_BCU_01CTRL/LLN0.brcbA" confRev="10000" />
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
            host="172.16.40.128",
            port=12447,
            ied_name="C264_BCU_01",
            scl_path=str(scl_path),
            transport_override_host="10.10.10.250",
        )
    )

    assert snapshot.endpoint.id == "mms:C264_BCU_01@10.10.10.250:12447"
    assert snapshot.endpoint_resolution.requested_host == "172.16.40.128"
    assert snapshot.endpoint_resolution.override_host == "10.10.10.250"
    assert snapshot.endpoint_resolution.override_applied is True
    assert snapshot.endpoint_resolution.transport_source == "explicit_request"
    assert snapshot.endpoint_resolution.resolved_host == "10.10.10.250"
    assert snapshot.endpoint_resolution.model_source == "scd-first"
    assert _payload(snapshot.ui_state)["endpoint_resolution"]["resolved_host"] == "10.10.10.250"


def test_client_control_configures_external_target_with_multiple_report_controls_and_select(tmp_path: Path) -> None:
    scl_path = tmp_path / "target.scd"
    _ = scl_path.write_text(
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

    available_controls = cast(list[dict[str, str]], _payload(snapshot.ui_state)["discovery"]["available_report_controls"])
    assert len(available_controls) == 2
    assert available_controls[0]["report_control_name"] == "brcbA"
    assert available_controls[1]["report_control_name"] == "brcbB"
    assert snapshot.candidate.report_control_name == "brcbA"

    selection = service.select_report_control(available_controls[1]["rcb_ref"])
    assert selection.candidate.report_control_name == "brcbB"
    assert _payload(selection.ui_state)["discovery"]["selected_rcb_ref"] == available_controls[1]["rcb_ref"]
    assert selection.last_state is None
    assert selection.last_read is None
    assert selection.transcript[-1].kind == "report-control-select"


def test_client_control_configure_target_recovers_from_stale_session_flag(tmp_path: Path) -> None:
    scl_path = tmp_path / "target.scd"
    _ = scl_path.write_text(
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
            <ReportControl name="brcbA" datSet="RCB1" buffered="true" indexed="true" rptID="KINTE13LVC01CTRL/LLN0.brcbA" confRev="10000" />
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
    service._session_open = True  # pyright: ignore[reportPrivateUsage]

    snapshot = service.configure_target(
        client_control_module.Iec61850ClientTargetRequest(
            mode="external-mms",
            host="host.docker.internal",
            port=12447,
            ied_name="KINTE13LVC01",
            scl_path=str(scl_path),
        )
    )

    assert snapshot.session_open is False
    assert snapshot.endpoint.id == "mms:KINTE13LVC01@host.docker.internal:12447"
    assert snapshot.transcript[-1].kind == "target-configured"


def test_external_mms_target_can_connect_and_discover_without_scd(monkeypatch: pytest.MonkeyPatch) -> None:
    service = Iec61850ClientControlService(
        live_wire_binary_path="/bin/true",
        live_wire_service_host="host.docker.internal",
        live_wire_data_port=12447,
    )
    snapshot = service.configure_target(
        client_control_module.Iec61850ClientTargetRequest(
            mode="external-mms",
            host="host.docker.internal",
            port=12447,
        )
    )
    assert snapshot.endpoint.ied_name == ""
    assert snapshot.endpoint_resolution.transport_source == "explicit_request"
    assert snapshot.endpoint_resolution.model_source == "discovery-fallback"
    assert _payload(snapshot.ui_state)["session"]["endpoint_label"] == "host.docker.internal:12447"
    assert snapshot.candidate.report_control_name == ""
    assert _payload(snapshot.ui_state)["discovery"]["available_report_controls"] == []
    assert _payload(snapshot.ui_state)["actions"]["can_rptena"] is False

    process_commands: list[tuple[str, ...]] = []
    stdin_commands: list[str] = []
    def fake_popen(command: Sequence[str], **_kwargs: object):
        process_commands.append(tuple(command))
        process = _ExternalMmsClientProcess(command, stdin_commands)
        return process

    monkeypatch.setattr(subprocess, "Popen", fake_popen)

    connect_snapshot = service.connect_ied()
    discover_snapshot = service.discover_ied()
    rptena_snapshot = service.enable_reporting()
    gi_snapshot = service.send_general_interrogation()
    closed_snapshot = service.close_ied()

    assert connect_snapshot.session_open is True
    assert _payload(connect_snapshot.ui_state)["session"]["phase"] == "associated"
    assert connect_snapshot.endpoint.ied_name == ""
    assert _payload(connect_snapshot.ui_state)["actions"]["can_rptena"] is False
    assert discover_snapshot.last_discovery is not None
    assert _payload(discover_snapshot.last_discovery)["endpoint"]["iedName"] == "KINTE13LVC01"
    assert discover_snapshot.endpoint.ied_name == "KINTE13LVC01"
    assert discover_snapshot.endpoint.id == "mms:KINTE13LVC01@host.docker.internal:12447"
    assert discover_snapshot.endpoint_resolution.model_source == "discovery-fallback"
    assert _payload(discover_snapshot.ui_state)["session"]["endpoint_label"] == "KINTE13LVC01@host.docker.internal:12447"
    assert discover_snapshot.candidate.report_control_name == "brcbA"
    assert discover_snapshot.candidate.ied_name == "KINTE13LVC01"
    assert _payload(discover_snapshot.ui_state)["discovery"]["available_report_controls"][0]["report_control_name"] == "brcbA"
    assert _payload(discover_snapshot.ui_state)["discovery"]["selected_rcb_ref"] == "KINTE13LVC01/AP1/CTRL/LLN0/brcbA/buffered"
    assert _payload(discover_snapshot.ui_state)["actions"]["can_rptena"] is True
    assert _payload(rptena_snapshot.ui_state)["subscription"]["runtime_status"] == "enabled"
    assert _payload(gi_snapshot.ui_state)["session"]["phase"] == "reporting"
    assert _payload(gi_snapshot.ui_state)["report"]["received"] is True
    assert _payload(gi_snapshot.ui_state)["report"]["value_count"] == 3
    assert closed_snapshot.session_open is False
    assert _payload(closed_snapshot.ui_state)["session"]["phase"] == "idle"

    assert process_commands[0][0] == "/bin/true"
    assert "--ied" not in process_commands[0]
    assert process_commands[0][-5:] == (
        "--bind",
        "host.docker.internal",
        "--port",
        "12447",
        "--mms-client-start",
    )
    assert stdin_commands[0] == "discover"
    assert not any(cmd.startswith("write-hex ") for cmd in stdin_commands)
    assert "rptena 0" in stdin_commands
    assert "gi 0" in stdin_commands


def test_external_mms_gi_polls_native_client_for_delayed_report(monkeypatch: pytest.MonkeyPatch) -> None:
    candidate = Iec61850ReportControlCandidate(
        id="KINTE13LVC01CTRL:LLN0$BR$brcbA01",
        ied_name="KINTE13LVC01",
        access_point_name="AP1",
        logical_device_inst="CTRL",
        logical_node_name="LLN0",
        report_control_name="brcbA",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id="KINTE13LVC01CTRL/LLN0.brcbA",
        data_set_ref="KINTE13LVC01CTRL/LLN0.RCB1",
        conf_rev=None,
        indexed=True,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(Iec61850DataSetMember(reference="XCBR1.Pos[ST]", fc="ST"),),
    )
    service = Iec61850ClientControlService(
        candidate=candidate,
        available_candidates=(candidate,),
        live_wire_binary_path="/bin/true",
        live_wire_service_host="host.docker.internal",
        live_wire_data_port=12447,
    )
    _ = service.configure_target(
        client_control_module.Iec61850ClientTargetRequest(
            mode="external-mms",
            host="host.docker.internal",
            port=12447,
        )
    )
    service._candidate = candidate  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    service._available_candidates = (candidate,)  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    stdin_commands: list[str] = []

    def fake_popen(command: Sequence[str], **_kwargs: object):
        return _DelayedGiExternalMmsClientProcess(command, stdin_commands)

    monkeypatch.setattr(subprocess, "Popen", fake_popen)

    _ = service.enable_reporting()
    gi_snapshot = service.send_general_interrogation()

    assert "write-bool KINTE13LVC01CTRL LLN0$BR$brcbA01$GI true" in stdin_commands
    assert any(command.startswith("poll-reports ") for command in stdin_commands)
    assert _payload(gi_snapshot.ui_state)["session"]["phase"] == "reporting"
    assert _payload(gi_snapshot.ui_state)["report"]["received"] is True
    assert _payload(gi_snapshot.ui_state)["report"]["value_count"] == 1
    assert _payload(gi_snapshot.ui_state)["report"]["values"][0]["reason"] == "general-interrogation"


def test_external_mms_rptena_accepts_immediate_report_before_rptena_summary(monkeypatch: pytest.MonkeyPatch) -> None:
    candidate = Iec61850ReportControlCandidate(
        id="KINTE13LVC01CTRL:LLN0$BR$brcbA01",
        ied_name="KINTE13LVC01",
        access_point_name="AP1",
        logical_device_inst="CTRL",
        logical_node_name="LLN0",
        report_control_name="brcbA",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id="KINTE13LVC01CTRL/LLN0.brcbA",
        data_set_ref="KINTE13LVC01CTRL/LLN0.RCB1",
        conf_rev=None,
        indexed=True,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(Iec61850DataSetMember(reference="XCBR1.Pos[ST]", fc="ST"),),
    )
    service = Iec61850ClientControlService(
        candidate=candidate,
        available_candidates=(candidate,),
        live_wire_binary_path="/bin/true",
        live_wire_service_host="host.docker.internal",
        live_wire_data_port=12447,
    )
    _ = service.configure_target(
        client_control_module.Iec61850ClientTargetRequest(
            mode="external-mms",
            host="host.docker.internal",
            port=12447,
        )
    )
    service._candidate = candidate  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    service._available_candidates = (candidate,)  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    stdin_commands: list[str] = []

    def fake_popen(command: Sequence[str], **_kwargs: object):
        return _RptenaImmediateReportProcess(command, stdin_commands)

    monkeypatch.setattr(subprocess, "Popen", fake_popen)

    snapshot = service.enable_reporting()

    assert "write-bool KINTE13LVC01CTRL LLN0$BR$brcbA01$RptEna true" in stdin_commands
    assert _payload(snapshot.ui_state)["subscription"]["runtime_status"] == "enabled"
    assert _payload(snapshot.ui_state)["report"]["received"] is False
    assert _payload(snapshot.ui_state)["report"]["value_count"] == 0


def test_external_mms_gi_keeps_richest_report_when_data_change_arrives_first(monkeypatch: pytest.MonkeyPatch) -> None:
    candidate = Iec61850ReportControlCandidate(
        id="KINTE13LVC01CTRL:LLN0$BR$brcbA01",
        ied_name="KINTE13LVC01",
        access_point_name="AP1",
        logical_device_inst="CTRL",
        logical_node_name="LLN0",
        report_control_name="brcbA",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id="KINTE13LVC01CTRL/LLN0.brcbA",
        data_set_ref="KINTE13LVC01CTRL/LLN0.RCB1",
        conf_rev=None,
        indexed=True,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(
            Iec61850DataSetMember(reference="XCBR1.Pos[ST]", fc="ST"),
            Iec61850DataSetMember(reference="XCBR2.Pos[ST]", fc="ST"),
        ),
    )
    service = Iec61850ClientControlService(
        candidate=candidate,
        available_candidates=(candidate,),
        live_wire_binary_path="/bin/true",
        live_wire_service_host="host.docker.internal",
        live_wire_data_port=12447,
    )
    _ = service.configure_target(
        client_control_module.Iec61850ClientTargetRequest(
            mode="external-mms",
            host="host.docker.internal",
            port=12447,
        )
    )
    service._candidate = candidate  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    service._available_candidates = (candidate,)  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    stdin_commands: list[str] = []

    def fake_popen(command: Sequence[str], **_kwargs: object):
        return _GiInterleavedDataChangeProcess(command, stdin_commands)

    monkeypatch.setattr(subprocess, "Popen", fake_popen)

    _ = service.enable_reporting()
    snapshot = service.send_general_interrogation()

    assert any(command.startswith("poll-reports ") for command in stdin_commands)
    assert _payload(snapshot.ui_state)["report"]["value_count"] == 2
    assert _payload(snapshot.ui_state)["report"]["values"][0]["reason"] == "general-interrogation"
    assert _payload(snapshot.ui_state)["report"]["values"][1]["reason"] == "general-interrogation"


def test_external_mms_target_can_discover_when_summary_arrives_before_ready(monkeypatch: pytest.MonkeyPatch) -> None:
    service = Iec61850ClientControlService(
        live_wire_binary_path="/bin/true",
        live_wire_service_host="host.docker.internal",
        live_wire_data_port=12447,
    )
    _ = service.configure_target(
        client_control_module.Iec61850ClientTargetRequest(
            mode="external-mms",
            host="host.docker.internal",
            port=12447,
            ied_name="KINTE13LVC01",
        )
    )

    process_commands: list[tuple[str, ...]] = []
    stdin_commands: list[str] = []

    def fake_popen(command: Sequence[str], **_kwargs: object):
        process_commands.append(tuple(command))
        return _SummaryOnlyExternalMmsClientProcess(command, stdin_commands)

    monkeypatch.setattr(subprocess, "Popen", fake_popen)

    snapshot = service.discover_ied()

    assert snapshot.last_discovery is not None
    assert _payload(snapshot.ui_state)["session"]["phase"] == "discovered"
    assert snapshot.endpoint_resolution.model_source == "discovery-fallback"
    assert _payload(snapshot.ui_state)["discovery"]["logical_devices"] == 1
    assert stdin_commands[0] == "discover"
    assert process_commands[0][-1] == "--mms-client-start"


def test_external_mms_target_keeps_partial_discovery_when_one_domain_is_skipped(monkeypatch: pytest.MonkeyPatch) -> None:
    service = Iec61850ClientControlService(
        live_wire_binary_path="/bin/true",
        live_wire_service_host="host.docker.internal",
        live_wire_data_port=12447,
    )
    _ = service.configure_target(
        client_control_module.Iec61850ClientTargetRequest(
            mode="external-mms",
            host="host.docker.internal",
            port=12447,
        )
    )

    process_commands: list[tuple[str, ...]] = []
    stdin_commands: list[str] = []

    def fake_popen(command: Sequence[str], **_kwargs: object):
        process_commands.append(tuple(command))
        return _MixedDomainExternalMmsClientProcess(command, stdin_commands)

    monkeypatch.setattr(subprocess, "Popen", fake_popen)

    snapshot = service.discover_ied()

    assert snapshot.last_diagnostic is None
    assert _payload(snapshot.ui_state)["session"]["phase"] == "discovered"
    assert _payload(snapshot.ui_state)["discovery"]["logical_devices"] == 2
    assert _payload(snapshot.ui_state)["discovery"]["logical_nodes"] == 1
    assert _payload(snapshot.ui_state)["discovery"]["data_sets"] == 1
    assert _payload(snapshot.ui_state)["discovery"]["report_controls"] == 1
    assert _payload(snapshot.ui_state)["discovery"]["signals"] == 1
    assert _payload(snapshot.ui_state)["discovery"]["discovered"] is True
    assert _payload(snapshot.ui_state)["discovery"]["available_report_controls"][0]["report_control_name"] == "brcbA"
    assert stdin_commands[0] == "discover"
    assert process_commands[0][-1] == "--mms-client-start"


def test_external_mms_target_can_connect_and_enable_reporting_from_scd_without_discovery(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    scl_path = tmp_path / "target.scd"
    _ = scl_path.write_text(
        """<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <IED name="KINTE13LVC01"><AccessPoint name="AP1"><Server><LDevice inst="CTRL"><LN0 lnClass="LLN0" inst="" lnType="T_CTRL">
    <DataSet name="RCB1"><FCDA ldInst="CTRL" lnClass="XCBR" lnInst="1" doName="Pos" fc="ST" /></DataSet>
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
    _ = service.configure_target(
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

    def fake_popen(command: Sequence[str], **_kwargs: object):
        process_commands.append(tuple(command))
        return _ExternalMmsClientProcess(command, stdin_commands)

    monkeypatch.setattr(subprocess, "Popen", fake_popen)

    connect_snapshot = service.connect_ied()
    rptena_snapshot = service.enable_reporting()

    assert connect_snapshot.session_open is True
    assert _payload(connect_snapshot.ui_state)["session"]["phase"] == "associated"
    assert _payload(connect_snapshot.ui_state)["actions"]["can_rptena"] is True
    assert _payload(rptena_snapshot.ui_state)["subscription"]["runtime_status"] == "enabled"
    assert _payload(rptena_snapshot.ui_state)["session"]["phase"] == "subscribed"
    assert "discover" not in stdin_commands
    assert any(cmd.startswith("write-bool KINTE13LVC01CTRL LLN0$BR$brcbA$RptEna true") for cmd in stdin_commands)
    assert process_commands[0][-1] == "--mms-client-start"


def test_external_mms_target_routes_discover_rptena_gi_to_external_probes(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    scl_path = tmp_path / "target.scd"
    _ = scl_path.write_text(
        """<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <IED name="KINTE13LVC01"><AccessPoint name="AP1"><Server><LDevice inst="CTRL"><LN0 lnClass="LLN0" inst="" lnType="T_CTRL">
    <DataSet name="RCB1"><FCDA ldInst="CTRL" lnClass="XCBR" lnInst="1" doName="Pos" fc="ST" /></DataSet>
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
    _ = service.configure_target(
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
    processes: list[_ExternalMmsClientProcess] = []

    def fake_popen(command: Sequence[str], **_kwargs: object):
        process_commands.append(tuple(command))
        process = _ExternalMmsClientProcess(command, stdin_commands)
        processes.append(process)
        return process

    monkeypatch.setattr(subprocess, "Popen", fake_popen)

    discover_snapshot = service.discover_ied()
    rptena_snapshot = service.enable_reporting()
    gi_snapshot = service.send_general_interrogation()

    assert _payload(discover_snapshot.ui_state)["session"]["phase"] == "discovered"
    assert _payload(discover_snapshot.ui_state)["session"]["associated"] is True
    assert _payload(rptena_snapshot.ui_state)["session"]["phase"] == "subscribed"
    assert _payload(rptena_snapshot.ui_state)["subscription"]["runtime_status"] == "enabled"
    assert _payload(rptena_snapshot.ui_state)["subscription"]["rptena_enabled"] is True
    assert _payload(gi_snapshot.ui_state)["session"]["phase"] == "reporting"
    assert _payload(gi_snapshot.ui_state)["subscription"]["runtime_status"] == "reporting"
    assert _payload(gi_snapshot.ui_state)["report"]["received"] is True
    assert _payload(gi_snapshot.ui_state)["report"]["rpt_id"] == "KINTE13LVC01CTRL/LLN0.brcbA"
    assert _payload(gi_snapshot.ui_state)["report"]["reason"] == "general-interrogation"
    assert _payload(gi_snapshot.ui_state)["report"]["value_count"] == 3
    assert _payload(gi_snapshot.ui_state)["report"]["matched_value_count"] == 3
    assert _payload(gi_snapshot.ui_state)["report"]["unmatched_value_count"] == 0
    assert _payload(gi_snapshot.ui_state)["report"]["signal_state_count"] == 1
    assert _payload(gi_snapshot.ui_state)["report"]["signal_states"][0]["reference"] == "XCBR1.Pos[ST]"
    assert _payload(gi_snapshot.ui_state)["report"]["signal_states"][0]["value"] is True
    assert _payload(gi_snapshot.ui_state)["report"]["signal_states"][0]["value_data_reference"] == "KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal"
    assert _payload(gi_snapshot.ui_state)["report"]["signal_states"][0]["quality"] == 0
    assert _payload(gi_snapshot.ui_state)["report"]["signal_states"][0]["source_timestamp"] == "<empty>"
    assert _payload(gi_snapshot.ui_state)["report"]["signal_states"][0]["leaf_count"] == 3
    assert _payload(gi_snapshot.ui_state)["report"]["values"][0]["reference"] == "XCBR1.Pos[ST]"
    assert _payload(gi_snapshot.ui_state)["report"]["values"][0]["data_reference"] == "KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal"
    assert _payload(gi_snapshot.ui_state)["report"]["values"][0]["value"] is True

    processes[0].stdout.lines.extend([
        "native-wire-client: report-entry index=0 reference=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal dataRef=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal value=false kind=bool reason=data-change datasetMatch=true discoveredMatch=true\n",
        "native-wire-client: async-report\n",
        "native-wire-client: subscription-summary phase=async-report rcb=KINTE13LVC01CTRL/LLN0.brcbA/buffered rcb-index=0 rptEna=true rptEna-invoke=4 giRequested=true gi-invoke=5 lastReportReceived=true asyncReports=2 lastReportValues=1 lastReportDataRefs=1 lastReportMatchedDataRefs=1 lastReportReasons=1 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n",
    ])
    async_snapshot = service.snapshot()
    assert _payload(async_snapshot.ui_state)["report"]["value_count"] == 1
    assert _payload(async_snapshot.ui_state)["report"]["signal_state_count"] == 1
    assert _payload(async_snapshot.ui_state)["report"]["signal_states"][0]["value"] is False
    assert _payload(async_snapshot.ui_state)["report"]["signal_states"][0]["quality"] == 0
    assert _payload(async_snapshot.ui_state)["report"]["signal_states"][0]["source_timestamp"] == "<empty>"
    assert _payload(async_snapshot.ui_state)["report"]["signal_states"][0]["leaf_count"] == 3
    assert _payload(async_snapshot.ui_state)["report"]["signal_states"][0]["reason"] == "data-change"
    processes[0].stdout.lines.extend([
        "native-wire-client: report-entry index=0 reference=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal dataRef=KINTE13LVC01CTRL/XCBR1$ST$Pos$stVal value=true kind=bool reason=quality-change datasetMatch=true discoveredMatch=true\n",
        "native-wire-client: async-report\n",
        "native-wire-client: subscription-summary phase=async-report rcb=KINTE13LVC01CTRL/LLN0.brcbA/buffered rcb-index=0 rptEna=true rptEna-invoke=4 giRequested=true gi-invoke=5 lastReportReceived=true asyncReports=3 lastReportValues=1 lastReportDataRefs=1 lastReportMatchedDataRefs=1 lastReportReasons=1 lastReportDatasetMismatches=0 lastReportMissingValues=0 lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0\n",
    ])
    quality_snapshot = service.snapshot()
    assert _payload(quality_snapshot.ui_state)["report"]["signal_states"][0]["value"] is True
    assert _payload(quality_snapshot.ui_state)["report"]["signal_states"][0]["reason"] == "quality-change"
    disconnect_snapshot = service.disconnect_ied()
    assert _payload(gi_snapshot.ui_state)["subscription"]["selected_rcb_ref"] == "KINTE13LVC01/AP1/CTRL/LLN0/brcbA/buffered"
    assert _payload(gi_snapshot.ui_state)["actions"]["can_rptena"] is True
    assert _payload(gi_snapshot.ui_state)["actions"]["can_gi"] is True
    assert _payload(disconnect_snapshot.ui_state)["session"]["associated"] is False

    assert process_commands[0] == (
        "/bin/true",
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
    assert discover_snapshot.last_discovery is not None
    assert _payload(discover_snapshot.last_discovery)["source"] == "live"
    logical_devices = cast(list[dict[str, str]], _payload(discover_snapshot.last_discovery)["logicalDevices"])
    assert [item["reference"] for item in logical_devices] == [
        "KINTE13LVC01CTRL",
        "KINTE13LVC01PROT",
    ]
    assert logical_devices[0]["reference"] == "KINTE13LVC01CTRL"
    assert _payload(discover_snapshot.last_discovery)["logicalNodes"][0]["reference"] == "KINTE13LVC01CTRL/LLN0"
    assert _payload(discover_snapshot.last_discovery)["logicalNodes"][1]["reference"] == "KINTE13LVC01PROT/LLN0"
    assert _payload(discover_snapshot.last_discovery)["dataSets"][0]["reference"] == "KINTE13LVC01CTRL/LLN0.RCB1"
    assert _payload(discover_snapshot.last_discovery)["dataSets"][1]["reference"] == "KINTE13LVC01PROT/LLN0.RCB2"
    assert _payload(discover_snapshot.last_discovery)["dataSets"][0]["memberCount"] == 1
    assert _payload(discover_snapshot.last_discovery)["reportControls"][0]["item"] == "LLN0$BR$brcbA01"
    assert _payload(discover_snapshot.last_discovery)["reportControls"][1]["item"] == "LLN0$RP$urcbB01"
    assert _payload(discover_snapshot.last_discovery)["reportControls"][1]["kind"] == "unbuffered"
    assert _payload(discover_snapshot.ui_state)["discovery"]["logical_devices"] == 2
    assert _payload(discover_snapshot.ui_state)["discovery"]["logical_nodes"] == 2
    assert _payload(discover_snapshot.ui_state)["discovery"]["data_sets"] == 2
    assert _payload(discover_snapshot.ui_state)["discovery"]["report_controls"] == 2
    assert discover_snapshot.candidate.id == "KINTE13LVC01CTRL:LLN0$BR$brcbA01"
    assert discover_snapshot.candidate.rpt_id == "KINTE13LVC01CTRL/LLN0.brcbA"
    assert discover_snapshot.candidate.data_set_ref == "KINTE13LVC01CTRL/LLN0.RCB1"
    assert discover_snapshot.candidate.conf_rev == "10000"
    assert discover_snapshot.candidate.buffer_time_ms == 500
    assert discover_snapshot.candidate.integrity_period_ms == 0
    assert discover_snapshot.candidate.trigger_options.data_change is True
    assert discover_snapshot.candidate.trigger_options.quality_change is True
    assert discover_snapshot.candidate.trigger_options.data_update is True
    assert discover_snapshot.candidate.trigger_options.periodic is False
    assert discover_snapshot.candidate.trigger_options.general_interrogation is True
    assert discover_snapshot.candidate.optional_fields.sequence_number is True
    assert discover_snapshot.candidate.optional_fields.timestamp is True
    assert discover_snapshot.candidate.optional_fields.reason_code is True
    assert discover_snapshot.candidate.optional_fields.data_set_name is True
    assert discover_snapshot.candidate.optional_fields.data_reference is True
    assert discover_snapshot.candidate.optional_fields.entry_id is True
    assert discover_snapshot.candidate.optional_fields.config_revision is True
    assert discover_snapshot.candidate.optional_fields.buffer_overflow is True
    assert discover_snapshot.candidate.signals[0].reference == "XCBR1.Pos[ST]"
    assert _payload(discover_snapshot.ui_state)["discovery"]["available_report_controls"][0]["report_control_id"] == "KINTE13LVC01CTRL:LLN0$BR$brcbA01"
    assert _payload(discover_snapshot.ui_state)["discovery"]["available_report_controls"][0]["rpt_id"] == "KINTE13LVC01CTRL/LLN0.brcbA"
    assert _payload(discover_snapshot.ui_state)["discovery"]["available_report_controls"][1]["report_control_id"] == "KINTE13LVC01PROT:LLN0$RP$urcbB01"
    assert _payload(discover_snapshot.ui_state)["discovery"]["available_report_controls"][1]["report_control_name"] == "urcbB"
    assert _payload(discover_snapshot.ui_state)["discovery"]["available_report_controls"][1]["report_kind"] == "unbuffered"
    assert _payload(discover_snapshot.ui_state)["discovery"]["available_report_controls"][1]["data_set_ref"] == "KINTE13LVC01PROT/LLN0.RCB2"
    assert _payload(discover_snapshot.ui_state)["discovery"]["available_report_controls"][0]["trigger_options"]["data_change"] is True
    assert _payload(discover_snapshot.ui_state)["discovery"]["available_report_controls"][0]["trigger_options"]["periodic"] is False
    assert _payload(discover_snapshot.ui_state)["discovery"]["available_report_controls"][0]["optional_fields"]["config_revision"] is True

    assert stdin_commands[:4] == [
        "discover",
        "rptena 0",
        "gi 0",
        "poll-reports 1000",
    ]
    assert "disconnect" in stdin_commands
    assert [event.kind for event in service.snapshot().transcript[-5:]] == [
        "external-session-open",
        "external-ied-discover",
        "external-report-control-enable",
        "external-report-control-gi",
        "external-ied-disconnect",
    ]


def test_external_mms_selection_preserves_discovered_rcb_for_indexed_gi(monkeypatch: pytest.MonkeyPatch) -> None:
    service = Iec61850ClientControlService(
        live_wire_binary_path="/bin/true",
        live_wire_service_host="host.docker.internal",
        live_wire_data_port=12447,
    )
    _ = service.configure_target(
        client_control_module.Iec61850ClientTargetRequest(
            mode="external-mms",
            host="host.docker.internal",
            port=12447,
        )
    )

    process_commands: list[tuple[str, ...]] = []
    stdin_commands: list[str] = []

    def fake_popen(command: Sequence[str], **_kwargs: object):
        process_commands.append(tuple(command))
        return _ExternalMmsClientProcess(command, stdin_commands)

    monkeypatch.setattr(subprocess, "Popen", fake_popen)

    _ = service.connect_ied()
    discover_snapshot = service.discover_ied()
    available_controls = cast(list[dict[str, str]], _payload(discover_snapshot.ui_state)["discovery"]["available_report_controls"])
    selected = service.select_report_control(available_controls[1]["rcb_ref"])
    rptena_snapshot = service.enable_reporting()
    gi_snapshot = service.send_general_interrogation()

    assert selected.last_discovery is not None
    assert selected.candidate.id == "KINTE13LVC01PROT:LLN0$RP$urcbB01"
    assert _payload(rptena_snapshot.ui_state)["subscription"]["runtime_status"] == "enabled"
    assert _payload(gi_snapshot.ui_state)["session"]["phase"] == "reporting"
    assert "rptena 1" in stdin_commands
    assert "gi 1" in stdin_commands
    assert "write-bool KINTE13LVC01PROT LLN0$RP$urcbB$GI true" not in stdin_commands
    assert process_commands[0][-1] == "--mms-client-start"


def test_external_mms_rptena_summary_must_match_selected_rcb() -> None:
    service = Iec61850ClientControlService()
    _ = service.configure_target(
        client_control_module.Iec61850ClientTargetRequest(
            mode="external-mms",
            host="host.docker.internal",
            port=12447,
        )
    )
    service._apply_external_mms_client_line("native-wire-client: discovered-rcb[0] domain=KINTE13LVC01CTRL item=LLN0$BR$brcbA01 kind=buffered")  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    service._apply_external_mms_client_line("native-wire-client: discovered-rcb[1] domain=KINTE13LVC01PROT item=LLN0$RP$urcbB01 kind=unbuffered")  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    _ = service.select_report_control("KINTE13LVC01PROT:LLN0$RP$urcbB01")
    service._last_state = service._external_state(Iec61850RuntimeStatus.READ, enabled=False)  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]

    service._apply_external_mms_client_line(  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
        "native-wire-client: subscription-summary phase=rptena rcb=KINTE13LVC01CTRL/LLN0.brcbA/buffered "
        + "rcb-index=0 rptEna=true rptEna-invoke=4 giRequested=false gi-invoke=0 lastReportReceived=false "
        + "asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 "
        + "lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 "
        + "lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0"
    )

    assert service._last_state is not None  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    assert service._last_state.runtime_status == Iec61850RuntimeStatus.READ  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]

    service._apply_external_mms_client_line(  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
        "native-wire-client: subscription-summary phase=rptena rcb=KINTE13LVC01PROT/LLN0.urcbB/unbuffered "
        + "rcb-index=1 rptEna=true rptEna-invoke=5 giRequested=false gi-invoke=0 lastReportReceived=false "
        + "asyncReports=0 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 "
        + "lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 "
        + "lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 lastReportUnsupportedValues=0"
    )

    assert service._last_state.runtime_status == Iec61850RuntimeStatus.ENABLED  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]


def test_external_mms_ignores_empty_async_report_summary() -> None:
    service = Iec61850ClientControlService()

    service._apply_external_mms_client_line("native-wire-client: async-report")  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    service._apply_external_mms_client_line(  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
        "native-wire-client: subscription-summary phase=async-report rcb=<none>/<none> rcb-index=0 "
        + "rptEna=false rptEna-invoke=0 giRequested=false gi-invoke=0 lastReportReceived=false "
        + "asyncReports=1 lastReportValues=0 lastReportDataRefs=0 lastReportMatchedDataRefs=0 "
        + "lastReportReasons=0 lastReportDatasetMismatches=0 lastReportMissingValues=0 "
        + "lastReportExtraValues=0 lastReportMissingReasons=0 lastReportExtraReasons=0 "
        + "lastReportUnsupportedValues=0"
    )

    snapshot = service.snapshot()
    assert snapshot.last_report is None
    assert snapshot.last_state is None
    assert _payload(snapshot.ui_state)["session"]["phase"] == "idle"
    assert _payload(snapshot.ui_state)["report"]["received"] is False
    assert _payload(snapshot.ui_state)["report"]["value_count"] == 0
    assert _payload(snapshot.ui_state)["report"]["signal_state_count"] == 0


def test_external_mms_snapshot_surfaces_pending_report_entries_without_summary() -> None:
    candidate = Iec61850ReportControlCandidate(
        id="KINTE15BCU01CTRL1:LLN0$BR$brcbST01",
        ied_name="KINTE15BCU01",
        access_point_name="AP1",
        logical_device_inst="CTRL1",
        logical_node_name="LLN0",
        report_control_name="brcbST",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id="KINTE15BCU01CTRL1/LLN0.brcbST",
        data_set_ref="KINTE15BCU01CTRL1/LLN0$LLN0BRptStDs",
        conf_rev=None,
        indexed=True,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(Iec61850DataSetMember(reference="CBCSWI1.Pos[ST]", fc="ST"),),
    )
    service = Iec61850ClientControlService(candidate=candidate)
    service._last_state = service._external_state(Iec61850RuntimeStatus.ENABLED, enabled=True)  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]

    service._apply_external_mms_client_line(  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
        "native-wire-client: report-entry index=0 "
        + "reference=KINTE15BCU01CTRL1/CBCSWI1$ST$Pos$stVal "
        + "dataRef=KINTE15BCU01CTRL1/CBCSWI1$ST$Pos$stVal "
        + "value=true kind=bool reason=general-interrogation datasetMatch=true discoveredMatch=true"
    )
    snapshot = service.snapshot()

    assert snapshot.last_report is not None
    assert _payload(snapshot.ui_state)["session"]["phase"] == "reporting"
    assert _payload(snapshot.ui_state)["report"]["value_count"] == 1
    assert _payload(snapshot.ui_state)["report"]["signal_state_count"] == 1
    assert _payload(snapshot.ui_state)["report"]["signal_states"][0]["value"] is True


def test_external_mms_report_entries_keep_native_reason() -> None:
    candidate = Iec61850ReportControlCandidate(
        id="KINTE15BCU01CTRL1:LLN0$BR$brcbST01",
        ied_name="KINTE15BCU01",
        access_point_name="AP1",
        logical_device_inst="CTRL1",
        logical_node_name="LLN0",
        report_control_name="brcbST",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id="KINTE15BCU01CTRL1/LLN0.brcbST",
        data_set_ref="KINTE15BCU01CTRL1/LLN0$LLN0BRptStDs",
        conf_rev=None,
        indexed=True,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(Iec61850DataSetMember(reference="CBCSWI1.Pos[ST]", fc="ST"),),
    )
    service = Iec61850ClientControlService(candidate=candidate)
    service._last_state = service._external_state(Iec61850RuntimeStatus.ENABLED, enabled=True)  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]

    service._apply_external_mms_client_line(  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
        "native-wire-client: report-entry index=0 "
        + "reference=KINTE15BCU01CTRL1/CBCSWI1$ST$Pos$stVal "
        + "dataRef=KINTE15BCU01CTRL1/CBCSWI1$ST$Pos$stVal "
        + "value=false kind=bool reason=data-change datasetMatch=true discoveredMatch=true"
    )
    service._finalize_pending_external_report_entries()  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    snapshot = service.snapshot()

    assert snapshot.last_report is not None
    assert _payload(snapshot.ui_state)["report"]["values"][0]["reason"] == "data-change"
    assert _payload(snapshot.ui_state)["report"]["signal_states"][0]["reason"] == "data-change"
    assert _payload(snapshot.ui_state)["report"]["signal_states"][0]["value"] is False


def test_external_mms_report_entry_matches_do_level_st_to_stval_signal() -> None:
    candidate = Iec61850ReportControlCandidate(
        id="KINTE15BCU01CTRL2:LLN0$BR$brcbST01",
        ied_name="KINTE15BCU01",
        access_point_name="AP1",
        logical_device_inst="CTRL2",
        logical_node_name="LLN0",
        report_control_name="brcbST01",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id="KINTE15BCU01CTRL2/LLN0.brcbST01",
        data_set_ref="KINTE15BCU01CTRL2/LLN0$LLN0BRptStDs",
        conf_rev=None,
        indexed=True,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(
            Iec61850DataSetMember(
                reference="KINTE15BCU01CTRL2/SlotHGGIO12/Ind3/stVal[ST]",
                fc="ST",
            ),
        ),
    )
    service = Iec61850ClientControlService(candidate=candidate)
    service._last_state = service._external_state(Iec61850RuntimeStatus.ENABLED, enabled=True)  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]

    service._apply_external_mms_client_line(  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
        "native-wire-client: report-entry index=0 "
        + "reference=KINTE15BCU01CTRL2/SlotHGGIO12$ST$Ind3 "
        + "dataRef=KINTE15BCU01CTRL2/SlotHGGIO12$ST$Ind3 "
        + "value=true kind=bool reason=data-change datasetMatch=false discoveredMatch=true"
    )
    service._finalize_pending_external_report_entries()  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
    snapshot = service.snapshot()

    assert snapshot.last_report is not None
    assert _payload(snapshot.ui_state)["report"]["values"][0]["matched"] is True
    assert _payload(snapshot.ui_state)["report"]["values"][0]["reference"] == "KINTE15BCU01CTRL2/SlotHGGIO12/Ind3/stVal[ST]"
    assert _payload(snapshot.ui_state)["report"]["signal_states"][0]["value"] is True
    assert _payload(snapshot.ui_state)["report"]["signal_states"][0]["reason"] == "data-change"


def test_external_mms_report_events_use_local_sequence_when_native_count_repeats() -> None:
    candidate = Iec61850ReportControlCandidate(
        id="KINTE15BCU01CTRL2:LLN0$BR$brcbST01",
        ied_name="KINTE15BCU01",
        access_point_name="AP1",
        logical_device_inst="CTRL2",
        logical_node_name="LLN0",
        report_control_name="brcbST01",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id="KINTE15BCU01CTRL2/LLN0.brcbST01",
        data_set_ref="KINTE15BCU01CTRL2/LLN0$LLN0BRptStDs",
        conf_rev=None,
        indexed=True,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(
            Iec61850DataSetMember(
                reference="KINTE15BCU01CTRL2/SlotHGGIO12/Ind3/stVal[ST]",
                fc="ST",
            ),
        ),
    )
    service = Iec61850ClientControlService(candidate=candidate)
    service._last_state = service._external_state(Iec61850RuntimeStatus.ENABLED, enabled=True)  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]

    service._apply_external_mms_client_line(  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
        "native-wire-client: report-entry index=0 "
        + "reference=KINTE15BCU01CTRL2/SlotHGGIO12$ST$Ind3 "
        + "dataRef=KINTE15BCU01CTRL2/SlotHGGIO12$ST$Ind3 "
        + "value=false kind=bool reason=general-interrogation datasetMatch=false discoveredMatch=true"
    )
    service._apply_external_mms_client_line(  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
        "native-wire-client: subscription-summary phase=gi rcb=KINTE15BCU01CTRL2/LLN0$BR$brcbST01 "
        + "rptEna=true giRequested=true lastReportReceived=true asyncReports=29 "
        + "lastReportValues=1 lastReportDataRefs=1 lastReportMatchedDataRefs=0 lastReportReasons=1 "
        + "lastReportDatasetMismatches=1"
    )
    first_report = service.snapshot().last_report

    service._apply_external_mms_client_line(  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
        "native-wire-client: report-entry index=0 "
        + "reference=KINTE15BCU01CTRL2/SlotHGGIO12$ST$Ind3 "
        + "dataRef=KINTE15BCU01CTRL2/SlotHGGIO12$ST$Ind3 "
        + "value=true kind=bool reason=data-change datasetMatch=false discoveredMatch=true"
    )
    service._apply_external_mms_client_line(  # noqa: SLF001  # pyright: ignore[reportPrivateUsage]
        "native-wire-client: subscription-summary phase=report rcb=KINTE15BCU01CTRL2/LLN0$BR$brcbST01 "
        + "rptEna=true giRequested=true lastReportReceived=true asyncReports=29 "
        + "lastReportValues=1 lastReportDataRefs=1 lastReportMatchedDataRefs=0 lastReportReasons=1 "
        + "lastReportDatasetMismatches=1"
    )
    second_report = service.snapshot().last_report

    assert first_report is not None
    assert second_report is not None
    assert second_report.id != first_report.id
    assert second_report.sequence_number == cast(int, first_report.sequence_number) + 1
    assert second_report.reason == Iec61850ReportReason.DATA_CHANGE


def test_external_mms_target_connects_and_reports_from_scd_without_discover(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    scl_path = tmp_path / "target.scd"
    _ = scl_path.write_text(
        """<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <IED name="KINTE13LVC01"><AccessPoint name="AP1"><Server><LDevice inst="CTRL"><LN0 lnClass="LLN0" inst="" lnType="T_CTRL">
    <DataSet name="RCB1"><FCDA ldInst="CTRL" lnClass="XCBR" lnInst="1" doName="Pos" fc="ST" /></DataSet>
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
    _ = service.configure_target(
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

    def fake_popen(command: Sequence[str], **_kwargs: object):
        process_commands.append(tuple(command))
        return _ExternalMmsClientProcess(command, stdin_commands)

    monkeypatch.setattr(subprocess, "Popen", fake_popen)

    connect_snapshot = service.connect_ied()
    rptena_snapshot = service.enable_reporting()
    gi_snapshot = service.send_general_interrogation()
    disconnect_snapshot = service.disconnect_ied()

    assert connect_snapshot.session_open is True
    assert connect_snapshot.last_discovery is None
    assert connect_snapshot.endpoint_resolution.model_source == "scd-first"
    assert _payload(connect_snapshot.ui_state)["session"]["phase"] == "associated"
    assert _payload(connect_snapshot.ui_state)["discovery"]["discovered"] is False
    assert _payload(connect_snapshot.ui_state)["actions"]["can_rptena"] is True
    assert rptena_snapshot.last_discovery is None
    assert _payload(rptena_snapshot.ui_state)["session"]["phase"] == "subscribed"
    assert _payload(rptena_snapshot.ui_state)["subscription"]["runtime_status"] == "enabled"
    assert gi_snapshot.last_discovery is None
    assert _payload(gi_snapshot.ui_state)["session"]["phase"] == "reporting"
    assert _payload(gi_snapshot.ui_state)["report"]["signal_state_count"] == 1
    assert _payload(disconnect_snapshot.ui_state)["session"]["associated"] is False

    assert stdin_commands[:4] == [
        "write-bool KINTE13LVC01CTRL LLN0$BR$brcbA$RptEna true",
        "write-bool KINTE13LVC01CTRL LLN0$BR$brcbA$GI true",
        "poll-reports 1000",
        "write-bool KINTE13LVC01CTRL LLN0$BR$brcbA$RptEna false",
    ]
    assert "disconnect" in stdin_commands
    assert [event.kind for event in service.snapshot().transcript[-4:]] == [
        "external-session-open",
        "external-report-control-enable",
        "external-report-control-gi",
        "external-ied-disconnect",
    ]
    assert process_commands[0][-1] == "--mms-client-start"


def test_external_mms_target_accepts_rptena_and_gi_without_final_ready(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    scl_path = tmp_path / "target.scd"
    _ = scl_path.write_text(
        """<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <IED name="KINTE13LVC01"><AccessPoint name="AP1"><Server><LDevice inst="CTRL"><LN0 lnClass="LLN0" inst="" lnType="T_CTRL">
    <DataSet name="RCB1"><FCDA ldInst="CTRL" lnClass="XCBR" lnInst="1" doName="Pos" fc="ST" /></DataSet>
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
    _ = service.configure_target(
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

    def fake_popen(command: Sequence[str], **_kwargs: object):
        process_commands.append(tuple(command))
        return _NoReadyExternalMmsClientProcess(command, stdin_commands)

    monkeypatch.setattr(subprocess, "Popen", fake_popen)

    _ = service.connect_ied()
    _ = service.discover_ied()
    rptena_snapshot = service.enable_reporting()
    gi_snapshot = service.send_general_interrogation()

    assert _payload(rptena_snapshot.ui_state)["subscription"]["runtime_status"] == "enabled"
    assert _payload(gi_snapshot.ui_state)["subscription"]["runtime_status"] == "reporting"
    assert _payload(gi_snapshot.ui_state)["report"]["value_count"] == 1
    assert "rptena 0" in stdin_commands
    assert "write-bool KINTE13LVC01CTRL LLN0$BR$brcbA01$GI true" in stdin_commands
    assert "gi -1" not in stdin_commands
    assert process_commands[0][-1] == "--mms-client-start"


def test_external_mms_target_recovers_from_broken_pipe_on_rptena(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    scl_path = tmp_path / "target.scd"
    _ = scl_path.write_text(
        """<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <IED name="KINTE13LVC01"><AccessPoint name="AP1"><Server><LDevice inst="CTRL"><LN0 lnClass="LLN0" inst="" lnType="T_CTRL">
    <DataSet name="RCB1"><FCDA ldInst="CTRL" lnClass="XCBR" lnInst="1" doName="Pos" fc="ST" /></DataSet>
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
    _ = service.configure_target(
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
    process_count = {"value": 0}

    def fake_popen(command: Sequence[str], **_kwargs: object):
        process_commands.append(tuple(command))
        process_count["value"] += 1
        if process_count["value"] == 1:
            return _BrokenPipeOnceExternalMmsClientProcess(command, stdin_commands, broken_command="rptena 0")
        return _NoReadyExternalMmsClientProcess(command, stdin_commands)

    monkeypatch.setattr(subprocess, "Popen", fake_popen)

    _ = service.connect_ied()
    _ = service.discover_ied()
    rptena_snapshot = service.enable_reporting()

    assert _payload(rptena_snapshot.ui_state)["subscription"]["runtime_status"] == "enabled"
    assert stdin_commands.count("rptena 0") == 2
    assert process_count["value"] == 2


def test_external_mms_state_failed_aborts_general_interrogation(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    scl_path = tmp_path / "target.scd"
    _ = scl_path.write_text(
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
    _ = service.configure_target(
        client_control_module.Iec61850ClientTargetRequest(
            mode="external-mms",
            host="host.docker.internal",
            port=12447,
            ied_name="KINTE13LVC01",
            scl_path=str(scl_path),
        )
    )

    process_commands: list[str] = []

    def fake_popen(_command: Sequence[str], **_kwargs: object):
        return _FailingExternalMmsClientProcess(
            process_commands,
            fail_on_command="gi 0",
        )

    monkeypatch.setattr(subprocess, "Popen", fake_popen)

    _ = service.discover_ied()
    _ = service.enable_reporting()

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        _ = service.send_general_interrogation()

    assert error.value.code == "EXTERNAL_MMS_CLIENT_COMMAND_FAILED"
    snapshot = service.snapshot()
    assert snapshot.last_diagnostic is not None
    assert snapshot.last_diagnostic.code == "EXTERNAL_MMS_CLIENT_COMMAND_FAILED"
    assert snapshot.last_diagnostic.action == "external-gi"
    assert snapshot.last_state is not None
    assert snapshot.last_state.runtime_status == Iec61850RuntimeStatus.FAILED
    assert _payload(snapshot.ui_state)["subscription"]["runtime_status"] == "failed"
    assert _payload(snapshot.ui_state)["session"]["phase"] == "failed"
    assert process_commands[:3] == [
        "discover",
        "rptena 0",
        "gi 0",
    ]
