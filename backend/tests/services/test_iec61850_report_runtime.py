from __future__ import annotations

import json
import subprocess
from datetime import UTC, datetime

import pytest

from app.services.iec61850 import (
    Iec61850DataSetMember,
    Iec61850DeviceEndpoint,
    Iec61850OptionalFields,
    Iec61850ReportControlCandidate,
    Iec61850ReportControlRef,
    Iec61850ReportControlState,
    Iec61850ReportEvent,
    Iec61850ReportEventValue,
    Iec61850ReportKind,
    Iec61850ReportReason,
    Iec61850ReportRuntimeError,
    Iec61850ReportRuntimeService,
    Iec61850ReportSubscriptionPlan,
    Iec61850ReportSubscriptionPlanDevice,
    Iec61850ReportSubscriptionPlanReport,
    Iec61850ReportSubscriptionPlanSignal,
    Iec61850RuntimeMode,
    Iec61850RuntimeStatus,
    Iec61850RuntimeTriggerOptions,
    Iec61850SelectedSignal,
    build_ied_simulator_process_spec,
    build_ied_simulator_fixture_from_subscription_plan,
    build_mms_endpoint_catalog,
    create_iec61850_simulator_adapter,
    create_unavailable_mms_adapter,
    ied_simulator_fixture_to_payload,
    Iec61850MmsEndpointCatalogEntry,
    map_report_event_to_subscription_plan_observations,
    map_report_event_to_signal_observations,
    normalize_report_data_reference,
    prepare_ied_simulator_process_plan,
    prepare_ied_simulator_process_plan_from_subscription_plan,
    run_ied_simulator_gi_probe,
    run_ied_simulator_metadata_probe,
    run_ied_simulator_process_plan_gi_probes,
    run_ied_simulator_process_plan_gi_validation,
    run_ied_simulator_process_plan_startup_checks,
    run_ied_simulator_startup_check,
    run_report_subscription_plan,
    run_report_subscription_plan_with_external_ied_simulators,
    run_simulator_report_subscription_plan,
    start_ied_simulator_process,
    start_ied_simulator_process_plan,
    stop_ied_simulator_process,
    to_report_control_ref,
    validate_report_subscription_plan_with_external_ied_simulators,
    wait_ied_simulator_process_ready,
    write_ied_simulator_fixture_file,
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


def test_backend_runtime_maps_gi_report_to_signal_observations() -> None:
    candidate = _candidate()
    endpoint = _endpoint()
    adapter = create_iec61850_simulator_adapter(now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC))
    service = Iec61850ReportRuntimeService(adapter)
    service.open_session(session_id="session-1", endpoint=endpoint, candidates=[candidate])
    service.reserve_report_control(session_id="session-1", candidate=candidate, client_id="unitlab")
    service.enable_report_control(session_id="session-1", candidate=candidate, client_id="unitlab")

    report = service.send_general_interrogation(session_id="session-1", candidate=candidate, client_id="unitlab")
    result = map_report_event_to_signal_observations(
        candidate=candidate,
        matched_signals=_matched_signals(),
        event=report,
    )

    assert result.diagnostics == ()
    assert [(observation.selected_signal_id, observation.model_reference, observation.value) for observation in result.observations] == [
        ("sig-1", "LD0/XCBR1.Pos.stVal[ST]", 0),
        ("sig-2", "LD0/PGGIO1.Ind1[ST]", 1),
    ]
    assert result.unselected_values == ()


def test_backend_runtime_maps_subset_report_to_signal_observation_diagnostics() -> None:
    candidate = _candidate()
    report = Iec61850ReportEvent(
        id="event-1",
        endpoint_id="sim:IED1/AP1",
        received_at="2026-05-29T12:00:03Z",
        report_control=to_report_control_ref(candidate),
        rpt_id=candidate.rpt_id,
        data_set_ref=candidate.data_set_ref,
        conf_rev=candidate.conf_rev,
        sequence_number=3,
        time_of_entry="2026-05-29T12:00:03Z",
        entry_id="entry-3",
        buffer_overflow=False,
        reason=Iec61850ReportReason.DATA_CHANGE,
        values=(
            Iec61850ReportEventValue(
                data_set_index=0,
                reference="LD0/XCBR1.Pos.stVal[ST]",
                data_reference="IED1LD0/XCBR1$ST$Pos$stVal",
                value=True,
                reason_code=Iec61850ReportReason.DATA_CHANGE,
                timestamp="2026-05-29T12:00:03Z",
            ),
        ),
    )

    result = map_report_event_to_signal_observations(
        candidate=candidate,
        matched_signals=_matched_signals(),
        event=report,
    )

    assert normalize_report_data_reference("IED1LD0/XCBR1$ST$Pos$stVal", candidate) == "LD0/XCBR1.Pos.stVal[ST]"
    assert len(result.observations) == 1
    assert result.observations[0].selected_signal_id == "sig-1"
    assert result.observations[0].reason_code == Iec61850ReportReason.DATA_CHANGE
    assert result.diagnostics[0].code == "SIGNAL_NOT_INCLUDED_IN_REPORT_EVENT"
    assert result.diagnostics[0].signal_id == "sig-2"


def test_backend_runtime_routes_report_events_through_subscription_plan() -> None:
    candidate = _candidate()
    event = _data_change_event(candidate)

    result = map_report_event_to_subscription_plan_observations(
        plan=_subscription_plan(candidate),
        event=event,
    )

    assert result.report_candidate_id == candidate.id
    assert [(observation.selected_signal_id, observation.value) for observation in result.observations] == [("sig-1", True)]
    assert result.diagnostics[0].code == "SIGNAL_NOT_INCLUDED_IN_REPORT_EVENT"

    unplanned_event = Iec61850ReportEvent(
        id="event-unplanned",
        endpoint_id="sim:IED1/AP1",
        received_at="2026-05-29T12:00:04Z",
        report_control=Iec61850ReportControlRef(
            ied_name="IED1",
            access_point_name="AP1",
            logical_device_inst="LD0",
            logical_node_name="LLN0",
            report_control_name="unknownReport",
            report_kind=Iec61850ReportKind.BUFFERED,
        ),
        rpt_id=None,
        data_set_ref=None,
        conf_rev=None,
        sequence_number=4,
        time_of_entry="2026-05-29T12:00:04Z",
        entry_id="entry-4",
        buffer_overflow=False,
        reason=Iec61850ReportReason.DATA_CHANGE,
        values=event.values,
    )
    unplanned_result = map_report_event_to_subscription_plan_observations(
        plan=_subscription_plan(candidate),
        event=unplanned_event,
    )

    assert unplanned_result.report_candidate_id is None
    assert unplanned_result.observations == ()
    assert [value.reference for value in unplanned_result.unselected_values] == ["LD0/XCBR1.Pos.stVal[ST]"]
    assert unplanned_result.diagnostics[0].code == "REPORT_NOT_IN_PLAN"


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


def test_backend_runtime_runs_simulator_subscription_plan_with_observations() -> None:
    candidate = _candidate()
    run = run_simulator_report_subscription_plan(
        plan=_subscription_plan(candidate),
        client_id="unitlab",
        now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC),
    )

    assert run.diagnostics == ()
    assert len(run.reports) == 1
    assert run.reports[0].runtime_status == Iec61850RuntimeStatus.RELEASED
    assert run.reports[0].error_code is None
    assert [(observation.selected_signal_id, observation.model_reference, observation.value) for observation in run.reports[0].observations] == [
        ("sig-1", "LD0/XCBR1.Pos.stVal[ST]", 0),
        ("sig-2", "LD0/PGGIO1.Ind1[ST]", 1),
    ]
    assert [event.kind for event in run.event_log] == [
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


def test_backend_runtime_subscription_plan_runner_releases_after_activation_failure() -> None:
    candidate = _candidate()
    adapter = _EnableFailureAdapter(candidate)

    run = run_report_subscription_plan(
        plan=_subscription_plan(candidate),
        adapter=adapter,
        client_id="unitlab",
        endpoint_for_device=lambda _: _endpoint(),
        now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC),
    )

    assert len(run.reports) == 1
    assert run.reports[0].error_code == "ENABLE_FAILED"
    assert run.reports[0].runtime_status == Iec61850RuntimeStatus.RELEASED
    assert run.reports[0].event is None
    assert run.reports[0].observations == ()
    assert adapter.release_called is True


def test_backend_runtime_subscription_plan_runner_blocks_activation_on_read_mismatch() -> None:
    candidate = _candidate()
    adapter = _ReadMismatchAdapter(candidate)

    run = run_report_subscription_plan(
        plan=_subscription_plan(candidate),
        adapter=adapter,
        client_id="unitlab",
        endpoint_for_device=lambda _: _endpoint(),
        now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC),
    )

    assert len(run.reports) == 1
    assert run.reports[0].error_code == "REPORT_CONTROL_PRECHECK_FAILED"
    assert run.reports[0].runtime_status == Iec61850RuntimeStatus.READ
    assert run.reports[0].event is None
    assert run.reports[0].observations == ()
    assert [diagnostic.code for diagnostic in run.reports[0].diagnostics] == ["DATASET_MISMATCH"]
    assert adapter.reserve_called is False


def test_backend_runtime_builds_mms_endpoint_from_catalog() -> None:
    candidate = _candidate()
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED1",
            access_point_name="AP1",
            host="127.0.0.1",
            port=1102,
        ),
    ))

    endpoint = catalog.endpoint_for_plan_device(_subscription_plan(candidate).devices[0])

    assert endpoint.id == "mms:IED1/AP1@127.0.0.1:1102"
    assert endpoint.mode == Iec61850RuntimeMode.MMS
    assert endpoint.ied_name == "IED1"
    assert endpoint.access_point_name == "AP1"
    assert endpoint.host == "127.0.0.1"
    assert endpoint.port == 1102


def test_backend_runtime_rejects_missing_mms_endpoint() -> None:
    catalog = build_mms_endpoint_catalog(())

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        catalog.endpoint_for_plan_device(_subscription_plan(_candidate()).devices[0])

    assert error.value.code == "MMS_ENDPOINT_NOT_CONFIGURED"


def test_backend_runtime_rejects_invalid_mms_endpoint_catalog_entries() -> None:
    with pytest.raises(Iec61850ReportRuntimeError) as duplicate_error:
        build_mms_endpoint_catalog((
            Iec61850MmsEndpointCatalogEntry(ied_name="IED1", access_point_name="AP1", host="127.0.0.1"),
            Iec61850MmsEndpointCatalogEntry(ied_name="ied1", access_point_name="ap1", host="127.0.0.2"),
        ))
    assert duplicate_error.value.code == "DUPLICATE_MMS_ENDPOINT"

    with pytest.raises(Iec61850ReportRuntimeError) as host_error:
        build_mms_endpoint_catalog((
            Iec61850MmsEndpointCatalogEntry(ied_name="IED1", access_point_name="AP1", host=" "),
        ))
    assert host_error.value.code == "INVALID_MMS_ENDPOINT"

    with pytest.raises(Iec61850ReportRuntimeError) as port_error:
        build_mms_endpoint_catalog((
            Iec61850MmsEndpointCatalogEntry(ied_name="IED1", access_point_name="AP1", host="127.0.0.1", port=70000),
        ))
    assert port_error.value.code == "INVALID_MMS_ENDPOINT"


def test_backend_runtime_mms_adapter_fails_closed_until_implemented() -> None:
    candidate = _candidate()
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED1",
            access_point_name="AP1",
            host="127.0.0.1",
            port=1102,
        ),
    ))

    run = run_report_subscription_plan(
        plan=_subscription_plan(candidate),
        adapter=create_unavailable_mms_adapter(),
        client_id="unitlab",
        endpoint_for_device=catalog.endpoint_for_plan_device,
        now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC),
    )

    assert len(run.reports) == 1
    assert run.reports[0].runtime_status == Iec61850RuntimeStatus.FAILED
    assert run.reports[0].error_code == "MMS_ADAPTER_NOT_IMPLEMENTED"
    assert run.reports[0].event is None
    assert run.reports[0].observations == ()
    assert [diagnostic.code for diagnostic in run.reports[0].diagnostics] == ["MMS_ADAPTER_NOT_IMPLEMENTED"]


def test_backend_runtime_builds_ied_simulator_fixture_from_required_reports() -> None:
    candidate = _candidate()
    ignored_candidate = _candidate(id="report-ignored")
    plan = Iec61850ReportSubscriptionPlan(
        selected_signal_count=2,
        matched_signal_count=2,
        unmatched_signal_count=0,
        ambiguous_signal_count=0,
        required_report_count=1,
        devices=(
            Iec61850ReportSubscriptionPlanDevice(
                ied_name=candidate.ied_name,
                access_point_name=candidate.access_point_name,
                reports=(
                    Iec61850ReportSubscriptionPlanReport(
                        status="required",
                        candidate=candidate,
                        matched_signals=_matched_signals(),
                    ),
                    Iec61850ReportSubscriptionPlanReport(
                        status="candidate",
                        candidate=ignored_candidate,
                        matched_signals=(),
                    ),
                ),
            ),
        ),
    )

    fixture = build_ied_simulator_fixture_from_subscription_plan(plan)
    payload = ied_simulator_fixture_to_payload(fixture)

    assert payload["schema"] == "unitlab.iec61850.ied-simulator-fixture.v1"
    assert payload["devices"] == [
        {
            "iedName": "IED1",
            "accessPointName": "AP1",
            "dataSets": [
                {
                    "reference": "IED1/AP1/LD0/LLN0.dsEvents",
                    "members": [
                        {
                            "dataSetIndex": 0,
                            "reference": "LD0/XCBR1.Pos.stVal[ST]",
                            "kind": "FCDA",
                            "fc": "ST",
                            "initialValue": 0,
                        },
                        {
                            "dataSetIndex": 1,
                            "reference": "LD0/PGGIO1.Ind1[ST]",
                            "kind": "FCDA",
                            "fc": "ST",
                            "initialValue": 1,
                        },
                    ],
                },
            ],
            "reports": [
                {
                    "key": "IED1/AP1/LD0/LLN0/brcbEvents/buffered",
                    "logicalDeviceInst": "LD0",
                    "logicalNodeName": "LLN0",
                    "reportControlName": "brcbEvents",
                    "reportKind": "buffered",
                    "rptId": "IED1LD0/LLN0.BR.Events",
                    "dataSetRef": "IED1/AP1/LD0/LLN0.dsEvents",
                    "confRev": "7",
                    "indexed": True,
                    "bufferTimeMs": 100,
                    "integrityPeriodMs": 1000,
                    "triggerOptions": {
                        "dataChange": True,
                        "qualityChange": True,
                        "dataUpdate": False,
                        "periodic": False,
                        "generalInterrogation": True,
                    },
                    "optionalFields": {
                        "sequenceNumber": True,
                        "timestamp": True,
                        "reasonCode": True,
                        "dataSetName": True,
                        "dataReference": True,
                        "entryId": True,
                        "configRevision": True,
                        "bufferOverflow": True,
                    },
                },
            ],
        },
    ]
    assert "sig-1" not in repr(payload)


def test_backend_runtime_rejects_ied_simulator_fixture_without_dataset_ref() -> None:
    candidate = _candidate(data_set_ref=None)

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        build_ied_simulator_fixture_from_subscription_plan(_subscription_plan(candidate))

    assert error.value.code == "SIMULATOR_FIXTURE_DATASET_MISSING"


def test_backend_runtime_prepares_external_ied_simulator_process(tmp_path) -> None:
    fixture = build_ied_simulator_fixture_from_subscription_plan(_subscription_plan(_candidate()))
    fixture_path = write_ied_simulator_fixture_file(fixture, tmp_path / "ied1.fixture.json")
    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")

    spec = build_ied_simulator_process_spec(
        fixture=fixture,
        binary_path=binary_path,
        fixture_path=fixture_path,
        ied_name="ied1",
        bind_address="127.0.0.1",
        port=1102,
        dry_run=True,
    )

    assert json.loads(fixture_path.read_text(encoding="utf-8"))["schema"] == "unitlab.iec61850.ied-simulator-fixture.v1"
    assert spec.command == (
        str(binary_path),
        "--fixture",
        str(fixture_path),
        "--ied",
        "IED1",
        "--bind",
        "127.0.0.1",
        "--port",
        "1102",
        "--dry-run",
    )
    assert spec.endpoint.mode == Iec61850RuntimeMode.MMS
    assert spec.endpoint.id == "mms-simulator:IED1/AP1@127.0.0.1:1102"


def test_backend_runtime_rejects_invalid_external_ied_simulator_process_config(tmp_path) -> None:
    fixture = build_ied_simulator_fixture_from_subscription_plan(_subscription_plan(_candidate()))
    fixture_path = tmp_path / "ied1.fixture.json"

    with pytest.raises(Iec61850ReportRuntimeError) as missing_binary:
        build_ied_simulator_process_spec(
            fixture=fixture,
            binary_path=tmp_path / "missing-simulator",
            fixture_path=fixture_path,
            ied_name="IED1",
        )
    assert missing_binary.value.code == "SIMULATOR_BINARY_NOT_FOUND"

    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")
    with pytest.raises(Iec61850ReportRuntimeError) as missing_device:
        build_ied_simulator_process_spec(
            fixture=fixture,
            binary_path=binary_path,
            fixture_path=fixture_path,
            ied_name="MISSING",
        )
    assert missing_device.value.code == "SIMULATOR_DEVICE_NOT_IN_FIXTURE"


def test_backend_runtime_external_ied_simulator_startup_check_uses_safe_process_invocation(tmp_path) -> None:
    fixture = build_ied_simulator_fixture_from_subscription_plan(_subscription_plan(_candidate()))
    fixture_path = tmp_path / "ied1.fixture.json"
    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")
    spec = build_ied_simulator_process_spec(
        fixture=fixture,
        binary_path=binary_path,
        fixture_path=fixture_path,
        ied_name="IED1",
    )

    def runner(command, **kwargs):
        assert isinstance(command, tuple)
        assert command[-1] == "--dry-run"
        assert kwargs == {
            "capture_output": True,
            "text": True,
            "timeout": 2.5,
            "check": False,
        }
        return subprocess.CompletedProcess(args=command, returncode=0, stdout="fixture accepted\n", stderr="")

    result = run_ied_simulator_startup_check(spec, timeout_seconds=2.5, runner=runner)

    assert result.return_code == 0
    assert result.stdout == "fixture accepted\n"
    assert result.command[-1] == "--dry-run"


def test_backend_runtime_external_ied_simulator_startup_check_fails_closed(tmp_path) -> None:
    fixture = build_ied_simulator_fixture_from_subscription_plan(_subscription_plan(_candidate()))
    fixture_path = tmp_path / "ied1.fixture.json"
    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")
    spec = build_ied_simulator_process_spec(
        fixture=fixture,
        binary_path=binary_path,
        fixture_path=fixture_path,
        ied_name="IED1",
    )

    def runner(command, **kwargs):
        return subprocess.CompletedProcess(
            args=command,
            returncode=69,
            stdout="",
            stderr="LIBIEC61850_NOT_LINKED\n",
        )

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        run_ied_simulator_startup_check(spec, runner=runner)

    assert error.value.code == "SIMULATOR_PROCESS_CHECK_FAILED"
    assert "LIBIEC61850_NOT_LINKED" in str(error.value)


def test_backend_runtime_external_ied_simulator_metadata_probe_uses_safe_process_invocation(tmp_path) -> None:
    spec = _external_simulator_process_spec(tmp_path)

    def runner(command, **kwargs):
        assert isinstance(command, tuple)
        assert command[-1] == "--metadata-probe"
        assert "--dry-run" not in command
        assert kwargs == {
            "capture_output": True,
            "text": True,
            "timeout": 3.0,
            "check": False,
        }
        return subprocess.CompletedProcess(args=command, returncode=0, stdout=_simulator_probe_stdout(command, probe="metadata"), stderr="")

    result = run_ied_simulator_metadata_probe(spec, timeout_seconds=3.0, runner=runner)

    assert result.return_code == 0
    assert result.stdout == _simulator_probe_stdout(spec.metadata_probe_command, probe="metadata")
    assert result.command[-1] == "--metadata-probe"


def test_backend_runtime_external_ied_simulator_metadata_probe_fails_closed(tmp_path) -> None:
    spec = _external_simulator_process_spec(tmp_path)

    def runner(command, **kwargs):
        return subprocess.CompletedProcess(
            args=command,
            returncode=69,
            stdout="",
            stderr="IEC61850_METADATA_PROBE_RCB_READ_FAILED\n",
        )

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        run_ied_simulator_metadata_probe(spec, runner=runner)

    assert error.value.code == "SIMULATOR_METADATA_PROBE_FAILED"
    assert "IEC61850_METADATA_PROBE_RCB_READ_FAILED" in str(error.value)


def test_backend_runtime_external_ied_simulator_metadata_probe_rejects_malformed_success_output(tmp_path) -> None:
    spec = _external_simulator_process_spec(tmp_path)

    def runner(command, **kwargs):
        return subprocess.CompletedProcess(
            args=command,
            returncode=0,
            stdout=(
                "unitlab-iec61850-ied-sim: metadata probe accepted\n"
                "ied=OTHER\n"
                f"endpoint={spec.bind_address}:{spec.port}\n"
                "dataSets=1\n"
                "reports=1\n"
                "libiec61850=linked\n"
            ),
            stderr="",
        )

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        run_ied_simulator_metadata_probe(spec, runner=runner)

    assert error.value.code == "SIMULATOR_METADATA_PROBE_OUTPUT_INVALID"
    assert 'expected ied="IED1"' in str(error.value)


def test_backend_runtime_external_ied_simulator_gi_probe_uses_safe_process_invocation(tmp_path) -> None:
    spec = _external_simulator_process_spec(tmp_path)

    def runner(command, **kwargs):
        assert isinstance(command, tuple)
        assert command[-1] == "--gi-probe"
        assert "--dry-run" not in command
        assert kwargs == {
            "capture_output": True,
            "text": True,
            "timeout": 4.0,
            "check": False,
        }
        return subprocess.CompletedProcess(args=command, returncode=0, stdout=_simulator_probe_stdout(command, probe="gi"), stderr="")

    result = run_ied_simulator_gi_probe(spec, timeout_seconds=4.0, runner=runner)

    assert result.return_code == 0
    assert result.stdout == _simulator_probe_stdout(spec.gi_probe_command, probe="gi")
    assert result.command[-1] == "--gi-probe"


def test_backend_runtime_external_ied_simulator_gi_probe_fails_closed(tmp_path) -> None:
    spec = _external_simulator_process_spec(tmp_path)

    def runner(command, **kwargs):
        return subprocess.CompletedProcess(
            args=command,
            returncode=69,
            stdout="",
            stderr="IEC61850_GI_PROBE_REPORT_TIMEOUT\n",
        )

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        run_ied_simulator_gi_probe(spec, runner=runner)

    assert error.value.code == "SIMULATOR_GI_PROBE_FAILED"
    assert "IEC61850_GI_PROBE_REPORT_TIMEOUT" in str(error.value)


def test_backend_runtime_external_ied_simulator_gi_probe_rejects_malformed_success_output(tmp_path) -> None:
    spec = _external_simulator_process_spec(tmp_path)

    def runner(command, **kwargs):
        return subprocess.CompletedProcess(
            args=command,
            returncode=0,
            stdout=(
                "unitlab-iec61850-ied-sim: GI probe accepted\n"
                f"ied={spec.ied_name}\n"
                f"endpoint={spec.bind_address}:{spec.port}\n"
                "reports=1\n"
                "libiec61850=unlinked\n"
            ),
            stderr="",
        )

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        run_ied_simulator_gi_probe(spec, runner=runner)

    assert error.value.code == "SIMULATOR_GI_PROBE_OUTPUT_INVALID"
    assert 'expected libiec61850="linked"' in str(error.value)


def test_backend_runtime_starts_and_stops_external_ied_simulator_process(tmp_path) -> None:
    spec = _external_simulator_process_spec(tmp_path)
    process = _FakeSimulatorProcess(pid=61850)
    commands: list[tuple[str, ...]] = []

    def runner(command, **kwargs):
        commands.append(command)
        if command[-1] == "--dry-run":
            return subprocess.CompletedProcess(args=command, returncode=0, stdout="fixture accepted\n", stderr="")
        if command[-1] == "--metadata-probe":
            return subprocess.CompletedProcess(args=command, returncode=0, stdout=_simulator_probe_stdout(command, probe="metadata"), stderr="")
        raise AssertionError(f"unexpected simulator helper command: {command}")

    def process_factory(command, **kwargs):
        assert command == spec.command
        assert command[-1] != "--dry-run"
        assert kwargs == {
            "stdout": subprocess.PIPE,
            "stderr": subprocess.PIPE,
            "text": True,
        }
        return process

    handle = start_ied_simulator_process(
        spec,
        startup_grace_seconds=0,
        runner=runner,
        process_factory=process_factory,
        readiness_connector=_ready_socket_connector,
    )
    stop = stop_ied_simulator_process(handle)

    assert handle.pid == 61850
    assert handle.endpoint.id == "mms-simulator:IED1/AP1@127.0.0.1:1102"
    assert [command[-1] for command in commands] == ["--dry-run", "--metadata-probe"]
    assert process.terminated is True
    assert stop.return_code == 0
    assert stop.killed is False


def test_backend_runtime_external_ied_simulator_process_readiness_uses_tcp_probe(tmp_path) -> None:
    spec = _external_simulator_process_spec(tmp_path)
    process = _FakeSimulatorProcess(pid=61850)
    attempts: list[tuple[tuple[str, int], float]] = []

    def connector(address, timeout):
        attempts.append((address, timeout))
        return _FakeSocket()

    wait_ied_simulator_process_ready(
        spec,
        process,
        timeout_seconds=1.0,
        connector=connector,
        sleep=lambda _: None,
    )

    assert attempts[0][0] == ("127.0.0.1", 1102)
    assert 0 < attempts[0][1] <= 1.0


def test_backend_runtime_external_ied_simulator_process_readiness_fails_closed_and_stops_process(tmp_path) -> None:
    spec = _external_simulator_process_spec(tmp_path)
    process = _FakeSimulatorProcess(pid=61850)

    def runner(command, **kwargs):
        return subprocess.CompletedProcess(args=command, returncode=0, stdout="fixture accepted\n", stderr="")

    def process_factory(command, **kwargs):
        return process

    def connector(address, timeout):
        raise OSError("connection refused")

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        start_ied_simulator_process(
            spec,
            startup_grace_seconds=0,
            readiness_timeout_seconds=0,
            runner=runner,
            process_factory=process_factory,
            readiness_connector=connector,
        )

    assert error.value.code == "SIMULATOR_ENDPOINT_READY_TIMEOUT"
    assert process.terminated is True


def test_backend_runtime_external_ied_simulator_metadata_probe_failure_stops_process(tmp_path) -> None:
    spec = _external_simulator_process_spec(tmp_path)
    process = _FakeSimulatorProcess(pid=61850)

    def runner(command, **kwargs):
        if command[-1] == "--dry-run":
            return subprocess.CompletedProcess(args=command, returncode=0, stdout="fixture accepted\n", stderr="")
        if command[-1] == "--metadata-probe":
            return subprocess.CompletedProcess(args=command, returncode=69, stdout="", stderr="RCB read failed\n")
        raise AssertionError(f"unexpected simulator helper command: {command}")

    def process_factory(command, **kwargs):
        return process

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        start_ied_simulator_process(
            spec,
            startup_grace_seconds=0,
            runner=runner,
            process_factory=process_factory,
            readiness_connector=_ready_socket_connector,
        )

    assert error.value.code == "SIMULATOR_METADATA_PROBE_FAILED"
    assert process.terminated is True


def test_backend_runtime_rejects_dry_run_spec_for_external_ied_simulator_process_start(tmp_path) -> None:
    spec = _external_simulator_process_spec(tmp_path, dry_run=True)

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        start_ied_simulator_process(spec)

    assert error.value.code == "SIMULATOR_PROCESS_START_DRY_RUN_SPEC"


def test_backend_runtime_fails_when_external_ied_simulator_process_exits_during_startup(tmp_path) -> None:
    spec = _external_simulator_process_spec(tmp_path)
    process = _FakeSimulatorProcess(pid=61850, return_code=69, stderr="LIBIEC61850_NOT_LINKED\n")

    def runner(command, **kwargs):
        return subprocess.CompletedProcess(args=command, returncode=0, stdout="fixture accepted\n", stderr="")

    def process_factory(command, **kwargs):
        return process

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        start_ied_simulator_process(
            spec,
            startup_grace_seconds=0,
            runner=runner,
            process_factory=process_factory,
        )

    assert error.value.code == "SIMULATOR_PROCESS_EXITED"
    assert "LIBIEC61850_NOT_LINKED" in str(error.value)


def test_backend_runtime_kills_external_ied_simulator_process_after_stop_timeout(tmp_path) -> None:
    spec = _external_simulator_process_spec(tmp_path)
    process = _FakeSimulatorProcess(pid=61850, wait_timeout=True)

    def runner(command, **kwargs):
        if command[-1] == "--metadata-probe":
            return subprocess.CompletedProcess(args=command, returncode=0, stdout=_simulator_probe_stdout(command, probe="metadata"), stderr="")
        return subprocess.CompletedProcess(args=command, returncode=0, stdout="fixture accepted\n", stderr="")

    def process_factory(command, **kwargs):
        return process

    handle = start_ied_simulator_process(
        spec,
        startup_grace_seconds=0,
        runner=runner,
        process_factory=process_factory,
        readiness_connector=_ready_socket_connector,
    )
    stop = stop_ied_simulator_process(handle, terminate_timeout_seconds=0.1)

    assert process.terminated is True
    assert process.killed is True
    assert stop.return_code == -9
    assert stop.killed is True


def test_backend_runtime_prepares_external_ied_simulator_process_plan_for_required_devices(tmp_path) -> None:
    plan = _multi_device_subscription_plan()
    fixture = build_ied_simulator_fixture_from_subscription_plan(plan)
    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")

    process_plan = prepare_ied_simulator_process_plan(
        fixture=fixture,
        binary_path=binary_path,
        fixture_path=tmp_path / "multi-device.fixture.json",
        base_port=12000,
    )

    assert json.loads(tmp_path.joinpath("multi-device.fixture.json").read_text(encoding="utf-8"))["devices"][1]["iedName"] == "IED2"
    assert [spec.ied_name for spec in process_plan.specs] == ["IED1", "IED2"]
    assert [spec.port for spec in process_plan.specs] == [12000, 12001]
    assert [endpoint.id for endpoint in process_plan.endpoints] == [
        "mms-simulator:IED1/AP1@127.0.0.1:12000",
        "mms-simulator:IED2/AP1@127.0.0.1:12001",
    ]
    assert process_plan.endpoint_for_plan_device(plan.devices[1]).id == "mms-simulator:IED2/AP1@127.0.0.1:12001"

    def runner(command, **kwargs):
        return subprocess.CompletedProcess(args=command, returncode=0, stdout=f"{command[4]} accepted\n", stderr="")

    checks = run_ied_simulator_process_plan_startup_checks(process_plan, runner=runner)

    assert [check.return_code for check in checks] == [0, 0]
    assert [check.command[-1] for check in checks] == ["--dry-run", "--dry-run"]


def test_backend_runtime_external_ied_simulator_process_plan_runs_gi_probes(tmp_path) -> None:
    plan = _multi_device_subscription_plan()
    fixture = build_ied_simulator_fixture_from_subscription_plan(plan)
    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")
    process_plan = prepare_ied_simulator_process_plan(
        fixture=fixture,
        binary_path=binary_path,
        fixture_path=tmp_path / "multi-device.fixture.json",
        base_port=12000,
    )
    commands: list[tuple[str, ...]] = []

    def runner(command, **kwargs):
        commands.append(command)
        return subprocess.CompletedProcess(args=command, returncode=0, stdout=_simulator_probe_stdout(command, probe="gi"), stderr="")

    probes = run_ied_simulator_process_plan_gi_probes(process_plan, runner=runner)

    assert [probe.return_code for probe in probes] == [0, 0]
    assert [command[-1] for command in commands] == ["--gi-probe", "--gi-probe"]


def test_backend_runtime_prepares_external_ied_simulator_process_plan_from_subscription_plan(tmp_path) -> None:
    plan = _multi_device_subscription_plan()
    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")

    process_plan = prepare_ied_simulator_process_plan_from_subscription_plan(
        subscription_plan=plan,
        binary_path=binary_path,
        fixture_path=tmp_path / "multi-device.fixture.json",
        base_port=13000,
    )

    assert [spec.ied_name for spec in process_plan.specs] == ["IED1", "IED2"]
    assert [endpoint.port for endpoint in process_plan.endpoints] == [13000, 13001]
    assert json.loads(tmp_path.joinpath("multi-device.fixture.json").read_text(encoding="utf-8"))["devices"][0]["reports"][0]["key"] == "IED1/AP1/LD0/LLN0/brcbEvents/buffered"


def test_backend_runtime_external_ied_simulator_process_plan_endpoint_resolver_fails_closed(tmp_path) -> None:
    plan = _multi_device_subscription_plan()
    fixture = build_ied_simulator_fixture_from_subscription_plan(plan)
    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")
    process_plan = prepare_ied_simulator_process_plan(
        fixture=fixture,
        binary_path=binary_path,
        fixture_path=tmp_path / "multi-device.fixture.json",
    )
    missing_device = Iec61850ReportSubscriptionPlanDevice(
        ied_name="MISSING",
        access_point_name="AP1",
        reports=(),
    )

    with pytest.raises(Iec61850ReportRuntimeError) as missing_error:
        process_plan.endpoint_for_plan_device(missing_device)
    assert missing_error.value.code == "SIMULATOR_PROCESS_ENDPOINT_NOT_CONFIGURED"

    duplicate_plan = process_plan.__class__(
        fixture_path=process_plan.fixture_path,
        specs=(process_plan.specs[0], process_plan.specs[0]),
    )
    with pytest.raises(Iec61850ReportRuntimeError) as duplicate_error:
        duplicate_plan.endpoint_for_plan_device(plan.devices[0])
    assert duplicate_error.value.code == "SIMULATOR_PROCESS_ENDPOINT_DUPLICATE"


def test_backend_runtime_external_ied_simulator_endpoints_feed_mms_adapter_boundary(tmp_path) -> None:
    plan = _subscription_plan(_candidate())
    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")
    process_plan = prepare_ied_simulator_process_plan_from_subscription_plan(
        subscription_plan=plan,
        binary_path=binary_path,
        fixture_path=tmp_path / "ied1.fixture.json",
    )

    run = run_report_subscription_plan(
        plan=plan,
        adapter=create_unavailable_mms_adapter(),
        client_id="unitlab",
        endpoint_for_device=process_plan.endpoint_for_plan_device,
        now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC),
    )

    assert run.reports[0].runtime_status == Iec61850RuntimeStatus.FAILED
    assert run.reports[0].error_code == "MMS_ADAPTER_NOT_IMPLEMENTED"


def test_backend_runtime_external_ied_simulator_process_plan_cleans_up_on_partial_start_failure(tmp_path) -> None:
    plan = _multi_device_subscription_plan()
    fixture = build_ied_simulator_fixture_from_subscription_plan(plan)
    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")
    process_plan = prepare_ied_simulator_process_plan(
        fixture=fixture,
        binary_path=binary_path,
        fixture_path=tmp_path / "multi-device.fixture.json",
    )
    started_process = _FakeSimulatorProcess(pid=1)
    failed_process = _FakeSimulatorProcess(pid=2, return_code=69, stderr="LIBIEC61850_NOT_LINKED\n")
    processes = [started_process, failed_process]

    def runner(command, **kwargs):
        if command[-1] == "--metadata-probe":
            return subprocess.CompletedProcess(args=command, returncode=0, stdout=_simulator_probe_stdout(command, probe="metadata"), stderr="")
        return subprocess.CompletedProcess(args=command, returncode=0, stdout="fixture accepted\n", stderr="")

    def process_factory(command, **kwargs):
        return processes.pop(0)

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        start_ied_simulator_process_plan(
            process_plan,
            startup_grace_seconds=0,
            runner=runner,
            process_factory=process_factory,
            readiness_connector=_ready_socket_connector,
        )

    assert error.value.code == "SIMULATOR_PROCESS_EXITED"
    assert started_process.terminated is True
    assert failed_process.terminated is False


def test_backend_runtime_external_ied_simulator_process_plan_gi_validation_cleans_up(tmp_path) -> None:
    plan = _multi_device_subscription_plan()
    fixture = build_ied_simulator_fixture_from_subscription_plan(plan)
    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")
    process_plan = prepare_ied_simulator_process_plan(
        fixture=fixture,
        binary_path=binary_path,
        fixture_path=tmp_path / "multi-device.fixture.json",
    )
    processes = [_FakeSimulatorProcess(pid=1), _FakeSimulatorProcess(pid=2)]
    commands: list[str] = []

    def runner(command, **kwargs):
        commands.append(command[-1])
        if command[-1] == "--dry-run":
            return subprocess.CompletedProcess(args=command, returncode=0, stdout="fixture accepted\n", stderr="")
        if command[-1] == "--metadata-probe":
            return subprocess.CompletedProcess(args=command, returncode=0, stdout=_simulator_probe_stdout(command, probe="metadata"), stderr="")
        if command[-1] == "--gi-probe":
            return subprocess.CompletedProcess(args=command, returncode=0, stdout=_simulator_probe_stdout(command, probe="gi"), stderr="")
        raise AssertionError(f"unexpected simulator helper command: {command}")

    def process_factory(command, **kwargs):
        return processes.pop(0)

    result = run_ied_simulator_process_plan_gi_validation(
        process_plan,
        startup_grace_seconds=0,
        runner=runner,
        process_factory=process_factory,
        readiness_connector=_ready_socket_connector,
    )

    assert commands == ["--dry-run", "--metadata-probe", "--dry-run", "--metadata-probe", "--gi-probe", "--gi-probe"]
    assert [probe.return_code for probe in result.gi_probe_results] == [0, 0]
    assert [stop.pid for stop in result.stop_results] == [2, 1]
    assert result.stop_results[0].killed is False


def test_backend_runtime_external_ied_simulator_process_plan_gi_validation_stops_after_probe_failure(tmp_path) -> None:
    plan = _subscription_plan(_candidate())
    fixture = build_ied_simulator_fixture_from_subscription_plan(plan)
    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")
    process_plan = prepare_ied_simulator_process_plan(
        fixture=fixture,
        binary_path=binary_path,
        fixture_path=tmp_path / "ied1.fixture.json",
    )
    process = _FakeSimulatorProcess(pid=61850)

    def runner(command, **kwargs):
        if command[-1] == "--dry-run":
            return subprocess.CompletedProcess(args=command, returncode=0, stdout="fixture accepted\n", stderr="")
        if command[-1] == "--metadata-probe":
            return subprocess.CompletedProcess(args=command, returncode=0, stdout=_simulator_probe_stdout(command, probe="metadata"), stderr="")
        if command[-1] == "--gi-probe":
            return subprocess.CompletedProcess(args=command, returncode=69, stdout="", stderr="GI timeout\n")
        raise AssertionError(f"unexpected simulator helper command: {command}")

    def process_factory(command, **kwargs):
        return process

    with pytest.raises(Iec61850ReportRuntimeError) as error:
        run_ied_simulator_process_plan_gi_validation(
            process_plan,
            startup_grace_seconds=0,
            runner=runner,
            process_factory=process_factory,
            readiness_connector=_ready_socket_connector,
        )

    assert error.value.code == "SIMULATOR_GI_PROBE_FAILED"
    assert process.terminated is True


def test_backend_runtime_external_ied_simulator_subscription_plan_gi_validation(tmp_path) -> None:
    plan = _multi_device_subscription_plan()
    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")
    processes = [_FakeSimulatorProcess(pid=1), _FakeSimulatorProcess(pid=2)]
    commands: list[str] = []

    def runner(command, **kwargs):
        commands.append(command[-1])
        if command[-1] == "--dry-run":
            return subprocess.CompletedProcess(args=command, returncode=0, stdout="fixture accepted\n", stderr="")
        if command[-1] == "--metadata-probe":
            return subprocess.CompletedProcess(args=command, returncode=0, stdout=_simulator_probe_stdout(command, probe="metadata"), stderr="")
        if command[-1] == "--gi-probe":
            return subprocess.CompletedProcess(args=command, returncode=0, stdout=_simulator_probe_stdout(command, probe="gi"), stderr="")
        raise AssertionError(f"unexpected simulator helper command: {command}")

    def process_factory(command, **kwargs):
        return processes.pop(0)

    result = validate_report_subscription_plan_with_external_ied_simulators(
        subscription_plan=plan,
        binary_path=binary_path,
        fixture_path=tmp_path / "multi-device.fixture.json",
        base_port=12102,
        startup_grace_seconds=0,
        runner=runner,
        process_factory=process_factory,
        readiness_connector=_ready_socket_connector,
    )

    fixture_payload = json.loads(tmp_path.joinpath("multi-device.fixture.json").read_text(encoding="utf-8"))
    assert fixture_payload["devices"][1]["iedName"] == "IED2"
    assert [endpoint.port for endpoint in result.process_plan.endpoints] == [12102, 12103]
    assert commands == ["--dry-run", "--metadata-probe", "--dry-run", "--metadata-probe", "--gi-probe", "--gi-probe"]
    assert [probe.return_code for probe in result.gi_probe_results] == [0, 0]
    assert [stop.pid for stop in result.stop_results] == [2, 1]


def test_backend_runtime_external_ied_simulator_wrapper_runs_through_mms_boundary_and_cleans_up(tmp_path) -> None:
    plan = _subscription_plan(_candidate())
    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")
    process = _FakeSimulatorProcess(pid=61850)

    def runner(command, **kwargs):
        if command[-1] == "--metadata-probe":
            return subprocess.CompletedProcess(args=command, returncode=0, stdout=_simulator_probe_stdout(command, probe="metadata"), stderr="")
        return subprocess.CompletedProcess(args=command, returncode=0, stdout="fixture accepted\n", stderr="")

    def process_factory(command, **kwargs):
        return process

    result = run_report_subscription_plan_with_external_ied_simulators(
        subscription_plan=plan,
        adapter=create_unavailable_mms_adapter(),
        client_id="unitlab",
        binary_path=binary_path,
        fixture_path=tmp_path / "ied1.fixture.json",
        startup_grace_seconds=0,
        runner=runner,
        process_factory=process_factory,
        readiness_connector=_ready_socket_connector,
    )

    assert result.subscription_run.reports[0].error_code == "MMS_ADAPTER_NOT_IMPLEMENTED"
    assert result.process_plan.endpoints[0].id == "mms-simulator:IED1/AP1@127.0.0.1:1102"
    assert result.stop_results[0].pid == 61850
    assert process.terminated is True


def test_backend_runtime_external_ied_simulator_wrapper_cleans_up_after_runtime_exception(tmp_path) -> None:
    plan = _subscription_plan(_candidate())
    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")
    process = _FakeSimulatorProcess(pid=61850)

    def runner(command, **kwargs):
        if command[-1] == "--metadata-probe":
            return subprocess.CompletedProcess(args=command, returncode=0, stdout=_simulator_probe_stdout(command, probe="metadata"), stderr="")
        return subprocess.CompletedProcess(args=command, returncode=0, stdout="fixture accepted\n", stderr="")

    def process_factory(command, **kwargs):
        return process

    with pytest.raises(RuntimeError):
        run_report_subscription_plan_with_external_ied_simulators(
            subscription_plan=plan,
            adapter=_ExplodingAdapter(),
            client_id="unitlab",
            binary_path=binary_path,
            fixture_path=tmp_path / "ied1.fixture.json",
            startup_grace_seconds=0,
            runner=runner,
            process_factory=process_factory,
            readiness_connector=_ready_socket_connector,
        )

    assert process.terminated is True


def _endpoint() -> Iec61850DeviceEndpoint:
    return Iec61850DeviceEndpoint(
        id="sim:IED1/AP1",
        mode=Iec61850RuntimeMode.SIMULATOR,
        ied_name="IED1",
        access_point_name="AP1",
        host=None,
        port=102,
    )


def _candidate(
    id: str = "report-1",
    conf_rev: str = "7",
    data_set_ref: str | None = "IED1/AP1/LD0/LLN0.dsEvents",
    ied_name: str = "IED1",
    access_point_name: str = "AP1",
) -> Iec61850ReportControlCandidate:
    return Iec61850ReportControlCandidate(
        id=id,
        ied_name=ied_name,
        access_point_name=access_point_name,
        logical_device_inst="LD0",
        logical_node_name="LLN0",
        report_control_name="brcbEvents",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id=f"{ied_name}LD0/LLN0.BR.Events",
        data_set_ref=data_set_ref,
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


def _matched_signals() -> tuple[Iec61850ReportSubscriptionPlanSignal, ...]:
    return (
        Iec61850ReportSubscriptionPlanSignal(
            selected_signal=Iec61850SelectedSignal(id="sig-1", address="IED1LD0/XCBR1/Pos/stVal[ST]"),
            model_reference="LD0/XCBR1.Pos.stVal[ST]",
            ied_name="IED1",
            match_kind="exact",
        ),
        Iec61850ReportSubscriptionPlanSignal(
            selected_signal=Iec61850SelectedSignal(id="sig-2", address="IED1LD0/PGGIO1/Ind1[ST]"),
            model_reference="LD0/PGGIO1.Ind1[ST]",
            ied_name="IED1",
            match_kind="exact",
        ),
    )


def _data_change_event(candidate: Iec61850ReportControlCandidate) -> Iec61850ReportEvent:
    return Iec61850ReportEvent(
        id="event-1",
        endpoint_id="sim:IED1/AP1",
        received_at="2026-05-29T12:00:03Z",
        report_control=to_report_control_ref(candidate),
        rpt_id=candidate.rpt_id,
        data_set_ref=candidate.data_set_ref,
        conf_rev=candidate.conf_rev,
        sequence_number=3,
        time_of_entry="2026-05-29T12:00:03Z",
        entry_id="entry-3",
        buffer_overflow=False,
        reason=Iec61850ReportReason.DATA_CHANGE,
        values=(
            Iec61850ReportEventValue(
                data_set_index=0,
                reference="LD0/XCBR1.Pos.stVal[ST]",
                data_reference="IED1LD0/XCBR1$ST$Pos$stVal",
                value=True,
                reason_code=Iec61850ReportReason.DATA_CHANGE,
                timestamp="2026-05-29T12:00:03Z",
            ),
        ),
    )


def _subscription_plan(candidate: Iec61850ReportControlCandidate) -> Iec61850ReportSubscriptionPlan:
    return Iec61850ReportSubscriptionPlan(
        selected_signal_count=2,
        matched_signal_count=2,
        unmatched_signal_count=0,
        ambiguous_signal_count=0,
        required_report_count=1,
        devices=(
            Iec61850ReportSubscriptionPlanDevice(
                ied_name=candidate.ied_name,
                access_point_name=candidate.access_point_name,
                reports=(
                    Iec61850ReportSubscriptionPlanReport(
                        status="required",
                        candidate=candidate,
                        matched_signals=_matched_signals(),
                    ),
                ),
            ),
        ),
    )


def _multi_device_subscription_plan() -> Iec61850ReportSubscriptionPlan:
    first_candidate = _candidate(id="report-1")
    second_candidate = _candidate(
        id="report-2",
        data_set_ref="IED2/AP1/LD0/LLN0.dsEvents",
        ied_name="IED2",
    )
    return Iec61850ReportSubscriptionPlan(
        selected_signal_count=4,
        matched_signal_count=4,
        unmatched_signal_count=0,
        ambiguous_signal_count=0,
        required_report_count=2,
        devices=(
            Iec61850ReportSubscriptionPlanDevice(
                ied_name=first_candidate.ied_name,
                access_point_name=first_candidate.access_point_name,
                reports=(
                    Iec61850ReportSubscriptionPlanReport(
                        status="required",
                        candidate=first_candidate,
                        matched_signals=_matched_signals(),
                    ),
                ),
            ),
            Iec61850ReportSubscriptionPlanDevice(
                ied_name=second_candidate.ied_name,
                access_point_name=second_candidate.access_point_name,
                reports=(
                    Iec61850ReportSubscriptionPlanReport(
                        status="required",
                        candidate=second_candidate,
                        matched_signals=_matched_signals(),
                    ),
                ),
            ),
        ),
    )


def _external_simulator_process_spec(tmp_path, *, dry_run: bool = False):
    fixture = build_ied_simulator_fixture_from_subscription_plan(_subscription_plan(_candidate()))
    fixture_path = tmp_path / "ied1.fixture.json"
    binary_path = tmp_path / "unitlab-iec61850-ied-sim"
    binary_path.write_text("", encoding="utf-8")
    return build_ied_simulator_process_spec(
        fixture=fixture,
        binary_path=binary_path,
        fixture_path=fixture_path,
        ied_name="IED1",
        dry_run=dry_run,
    )


def _simulator_probe_stdout(command: tuple[str, ...], *, probe: str) -> str:
    ied_name = command[command.index("--ied") + 1]
    bind_address = command[command.index("--bind") + 1]
    port = command[command.index("--port") + 1]
    if probe == "metadata":
        return (
            "unitlab-iec61850-ied-sim: metadata probe accepted\n"
            f"ied={ied_name}\n"
            f"endpoint={bind_address}:{port}\n"
            "dataSets=1\n"
            "reports=1\n"
            "libiec61850=linked\n"
        )
    if probe == "gi":
        return (
            "unitlab-iec61850-ied-sim: GI probe accepted\n"
            f"ied={ied_name}\n"
            f"endpoint={bind_address}:{port}\n"
            "reports=1\n"
            "libiec61850=linked\n"
        )
    raise AssertionError(f"unknown probe type: {probe}")


class _FakeSimulatorProcess:
    def __init__(
        self,
        *,
        pid: int,
        return_code: int | None = None,
        stdout: str = "",
        stderr: str = "",
        wait_timeout: bool = False,
    ) -> None:
        self.pid = pid
        self._return_code = return_code
        self._stdout = stdout
        self._stderr = stderr
        self._wait_timeout = wait_timeout
        self.terminated = False
        self.killed = False

    def poll(self) -> int | None:
        return self._return_code

    def terminate(self) -> None:
        self.terminated = True
        if not self._wait_timeout:
            self._return_code = 0

    def wait(self, timeout: float | None = None) -> int | None:
        if self._wait_timeout and not self.killed:
            raise subprocess.TimeoutExpired(cmd="unitlab-iec61850-ied-sim", timeout=timeout)
        return self._return_code

    def kill(self) -> None:
        self.killed = True
        self._return_code = -9

    def communicate(self) -> tuple[str, str]:
        return self._stdout, self._stderr


class _FakeSocket:
    def __init__(self) -> None:
        self.closed = False

    def close(self) -> None:
        self.closed = True


def _ready_socket_connector(address, timeout):
    return _FakeSocket()


class _ExplodingAdapter:
    def connect(self, **kwargs):
        raise RuntimeError("runtime exploded")


class _EnableFailureAdapter:
    def __init__(self, candidate: Iec61850ReportControlCandidate) -> None:
        self._candidate = candidate
        self.release_called = False

    def connect(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidates: list[Iec61850ReportControlCandidate],
    ) -> "_EnableFailureSession":
        assert candidates == [self._candidate]
        return _EnableFailureSession(candidate=self._candidate, adapter=self)


class _EnableFailureSession:
    def __init__(self, *, candidate: Iec61850ReportControlCandidate, adapter: _EnableFailureAdapter) -> None:
        self._candidate = candidate
        self._adapter = adapter
        self._state = Iec61850ReportControlState(
            reference=to_report_control_ref(candidate),
            runtime_status=Iec61850RuntimeStatus.DISCONNECTED,
            rpt_id=candidate.rpt_id,
            data_set_ref=candidate.data_set_ref,
            conf_rev=candidate.conf_rev,
            indexed=candidate.indexed,
            buffer_time_ms=candidate.buffer_time_ms,
            integrity_period_ms=candidate.integrity_period_ms,
            trigger_options=candidate.trigger_options,
            optional_fields=candidate.optional_fields,
            signal_count=candidate.signal_count,
        )

    def read_report_control(self, reference: Iec61850ReportControlRef) -> Iec61850ReportControlState:
        self._state.runtime_status = Iec61850RuntimeStatus.READ
        return self._state

    def reserve_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        self._state.reserved_by = client_id
        self._state.owner = client_id
        self._state.runtime_status = Iec61850RuntimeStatus.RESERVED
        return self._state

    def release_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        self._adapter.release_called = True
        self._state.reserved_by = None
        self._state.owner = None
        self._state.runtime_status = Iec61850RuntimeStatus.RELEASED
        return self._state

    def enable_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        self._state.runtime_status = Iec61850RuntimeStatus.FAILED
        raise Iec61850ReportRuntimeError("ENABLE_FAILED", "enable failed")

    def disable_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        raise AssertionError("disable should not be called after failed enable")

    def send_general_interrogation(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportEvent:
        raise AssertionError("GI should not be called after failed enable")

    def disconnect(self) -> None:
        return None


class _ReadMismatchAdapter:
    def __init__(self, candidate: Iec61850ReportControlCandidate) -> None:
        self._candidate = candidate
        self.reserve_called = False

    def connect(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidates: list[Iec61850ReportControlCandidate],
    ) -> "_ReadMismatchSession":
        assert candidates == [self._candidate]
        return _ReadMismatchSession(candidate=self._candidate, adapter=self)


class _ReadMismatchSession:
    def __init__(self, *, candidate: Iec61850ReportControlCandidate, adapter: _ReadMismatchAdapter) -> None:
        self._candidate = candidate
        self._adapter = adapter
        self._state = Iec61850ReportControlState(
            reference=to_report_control_ref(candidate),
            runtime_status=Iec61850RuntimeStatus.DISCONNECTED,
            rpt_id=candidate.rpt_id,
            data_set_ref="IED1/AP1/LD0/LLN0.otherDs",
            conf_rev=candidate.conf_rev,
            indexed=candidate.indexed,
            buffer_time_ms=candidate.buffer_time_ms,
            integrity_period_ms=candidate.integrity_period_ms,
            trigger_options=candidate.trigger_options,
            optional_fields=candidate.optional_fields,
            signal_count=candidate.signal_count,
        )

    def read_report_control(self, reference: Iec61850ReportControlRef) -> Iec61850ReportControlState:
        self._state.runtime_status = Iec61850RuntimeStatus.READ
        return self._state

    def reserve_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        self._adapter.reserve_called = True
        raise AssertionError("reserve should not be called when read precheck fails")

    def release_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        raise AssertionError("release should not be called without reservation")

    def enable_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        raise AssertionError("enable should not be called when read precheck fails")

    def disable_report_control(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportControlState:
        raise AssertionError("disable should not be called when read precheck fails")

    def send_general_interrogation(self, reference: Iec61850ReportControlRef, client_id: str) -> Iec61850ReportEvent:
        raise AssertionError("GI should not be called when read precheck fails")

    def disconnect(self) -> None:
        return None
