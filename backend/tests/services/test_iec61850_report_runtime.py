from __future__ import annotations

from datetime import UTC, datetime

import pytest

from app.services.iec61850 import (
    Iec61850DataSetMember,
    Iec61850DeviceEndpoint,
    Iec61850OptionalFields,
    Iec61850ReportControlCandidate,
    Iec61850ReportKind,
    Iec61850ReportReason,
    Iec61850ReportRuntimeError,
    Iec61850ReportRuntimeService,
    Iec61850RuntimeMode,
    Iec61850RuntimeStatus,
    Iec61850RuntimeTriggerOptions,
    create_iec61850_simulator_adapter,
)


def test_backend_runtime_service_owns_simulator_session_flow() -> None:
    candidate = _candidate()
    endpoint = _endpoint()
    adapter = create_iec61850_simulator_adapter(now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC))
    service = Iec61850ReportRuntimeService(adapter)

    service.open_session(session_id="session-1", endpoint=endpoint, candidates=[candidate])
    read_result = service.read_report_control(session_id="session-1", endpoint=endpoint, candidate=candidate)

    assert read_result.diagnostics == ()
    assert read_result.state.runtime_status == Iec61850RuntimeStatus.READ
    assert service.reserve_report_control(session_id="session-1", candidate=candidate, client_id="unitlab").runtime_status == Iec61850RuntimeStatus.RESERVED
    assert service.enable_report_control(session_id="session-1", candidate=candidate, client_id="unitlab").runtime_status == Iec61850RuntimeStatus.ENABLED

    report = service.send_general_interrogation(session_id="session-1", candidate=candidate, client_id="unitlab")

    assert report.reason == Iec61850ReportReason.GENERAL_INTERROGATION
    assert report.sequence_number == 1
    assert [value.reference for value in report.values] == [
        "LD0/XCBR1.Pos.stVal[ST]",
        "LD0/PGGIO1.Ind1[ST]",
    ]
    assert service.disable_report_control(session_id="session-1", candidate=candidate, client_id="unitlab").runtime_status == Iec61850RuntimeStatus.DISABLED
    assert service.release_report_control(session_id="session-1", candidate=candidate, client_id="unitlab").runtime_status == Iec61850RuntimeStatus.RELEASED

    service.close_session("session-1")

    assert [event.kind for event in adapter.get_event_log()] == [
        "connect",
        "read",
        "reserve",
        "enable",
        "general-interrogation",
        "report",
        "disable",
        "release",
        "disconnect",
    ]


def test_backend_runtime_service_surfaces_deterministic_simulator_failures() -> None:
    candidate = _candidate()
    endpoint = _endpoint()
    adapter = create_iec61850_simulator_adapter(now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC))
    service = Iec61850ReportRuntimeService(adapter)
    service.open_session(session_id="session-1", endpoint=endpoint, candidates=[candidate])

    with pytest.raises(Iec61850ReportRuntimeError) as enable_error:
        service.enable_report_control(session_id="session-1", candidate=candidate, client_id="unitlab")
    assert enable_error.value.code == "ENABLE_WITHOUT_RESERVATION"

    with pytest.raises(Iec61850ReportRuntimeError) as gi_error:
        service.send_general_interrogation(session_id="session-1", candidate=candidate, client_id="unitlab")
    assert gi_error.value.code == "GI_WHILE_DISABLED"

    service.reserve_report_control(session_id="session-1", candidate=candidate, client_id="unitlab")
    with pytest.raises(Iec61850ReportRuntimeError) as conflict_error:
        service.reserve_report_control(session_id="session-1", candidate=candidate, client_id="other-client")
    assert conflict_error.value.code == "RESERVATION_CONFLICT"


def test_backend_runtime_service_compares_live_state_before_activation() -> None:
    candidate = _candidate(conf_rev="7")
    stale_candidate = _candidate(id="report-stale", conf_rev="8")
    endpoint = _endpoint()
    adapter = create_iec61850_simulator_adapter(now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC))
    service = Iec61850ReportRuntimeService(adapter)
    service.open_session(session_id="session-1", endpoint=endpoint, candidates=[stale_candidate])

    read_result = service.read_report_control(session_id="session-1", endpoint=endpoint, candidate=candidate)

    assert [diagnostic.code for diagnostic in read_result.diagnostics] == ["CONFREV_MISMATCH"]


def test_backend_runtime_service_can_enforce_enabled_disconnect_failure() -> None:
    candidate = _candidate()
    endpoint = _endpoint()
    adapter = create_iec61850_simulator_adapter(
        now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC),
        strict_disconnect_while_enabled=True,
    )
    service = Iec61850ReportRuntimeService(adapter)
    service.open_session(session_id="session-1", endpoint=endpoint, candidates=[candidate])
    service.reserve_report_control(session_id="session-1", candidate=candidate, client_id="unitlab")
    service.enable_report_control(session_id="session-1", candidate=candidate, client_id="unitlab")

    with pytest.raises(Iec61850ReportRuntimeError) as disconnect_error:
        service.close_session("session-1")

    assert disconnect_error.value.code == "DISCONNECT_WHILE_ENABLED"
    assert adapter.get_event_log()[-1].code == "DISCONNECT_WHILE_ENABLED"
    with pytest.raises(Iec61850ReportRuntimeError) as missing_error:
        service.read_report_control(session_id="session-1", endpoint=endpoint, candidate=candidate)
    assert missing_error.value.code == "SESSION_NOT_FOUND"


def _endpoint() -> Iec61850DeviceEndpoint:
    return Iec61850DeviceEndpoint(
        id="sim:IED1/AP1",
        mode=Iec61850RuntimeMode.SIMULATOR,
        ied_name="IED1",
        access_point_name="AP1",
        host=None,
        port=102,
    )


def _candidate(id: str = "report-1", conf_rev: str = "7") -> Iec61850ReportControlCandidate:
    return Iec61850ReportControlCandidate(
        id=id,
        ied_name="IED1",
        access_point_name="AP1",
        logical_device_inst="LD0",
        logical_node_name="LLN0",
        report_control_name="brcbEvents",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id="IED1LD0/LLN0.BR.Events",
        data_set_ref="IED1/AP1/LD0/LLN0.dsEvents",
        conf_rev=conf_rev,
        indexed=True,
        buffer_time_ms=100,
        integrity_period_ms=1000,
        trigger_options=Iec61850RuntimeTriggerOptions(
            data_change=True,
            quality_change=True,
            data_update=False,
            periodic=False,
            general_interrogation=True,
        ),
        optional_fields=Iec61850OptionalFields(
            sequence_number=True,
            timestamp=True,
            reason_code=True,
            data_set_name=True,
            data_reference=True,
            entry_id=True,
            config_revision=True,
            buffer_overflow=True,
        ),
        signals=(
            Iec61850DataSetMember(reference="LD0/XCBR1.Pos.stVal[ST]", fc="ST"),
            Iec61850DataSetMember(reference="LD0/PGGIO1.Ind1[ST]", fc="ST"),
        ),
    )
