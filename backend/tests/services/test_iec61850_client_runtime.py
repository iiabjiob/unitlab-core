from __future__ import annotations

from datetime import UTC, datetime

from app.services.iec61850.client_runtime import Iec61850MmsClientRuntime
from app.services.iec61850.report_runtime import (
    Iec61850DataSetMember,
    Iec61850DeviceEndpoint,
    Iec61850OptionalFields,
    Iec61850ReportControlCandidate,
    Iec61850ReportKind,
    Iec61850ReportReason,
    Iec61850ReportSubscriptionPlan,
    Iec61850ReportSubscriptionPlanDevice,
    Iec61850ReportSubscriptionPlanReport,
    Iec61850ReportSubscriptionPlanSignal,
    Iec61850RuntimeMode,
    Iec61850RuntimeTriggerOptions,
    Iec61850SelectedSignal,
    build_simulator_endpoint_for_plan_device,
    create_iec61850_simulator_adapter,
    to_report_control_ref,
)


def test_client_runtime_wraps_shared_session_lifecycle() -> None:
    candidate = _candidate()
    endpoint = _endpoint()
    adapter = create_iec61850_simulator_adapter(now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC))
    runtime = Iec61850MmsClientRuntime(adapter)

    runtime.open_session(session_id="client-session", endpoint=endpoint, candidates=[candidate])
    read_result = runtime.read_report_control(session_id="client-session", endpoint=endpoint, candidate=candidate)

    assert read_result.diagnostics == ()
    assert read_result.state.reference == to_report_control_ref(candidate)

    assert runtime.reserve_report_control(session_id="client-session", candidate=candidate, client_id="unitlab").reserved_by == "unitlab"
    assert runtime.enable_report_control(session_id="client-session", candidate=candidate, client_id="unitlab").enabled is True

    report = runtime.send_general_interrogation(session_id="client-session", candidate=candidate, client_id="unitlab")

    assert report.reason == Iec61850ReportReason.GENERAL_INTERROGATION
    assert [value.reference for value in report.values] == ["LD0/XCBR1.Pos.stVal[ST]"]

    assert runtime.disable_report_control(session_id="client-session", candidate=candidate, client_id="unitlab").enabled is False
    assert runtime.release_report_control(session_id="client-session", candidate=candidate, client_id="unitlab").released is True
    runtime.close_session("client-session")

    kinds = [event.kind for event in runtime.transcript()]
    assert kinds == [
        "session-open",
        "report-control-read",
        "report-control-reserve",
        "report-control-enable",
        "report-control-gi",
        "report-control-disable",
        "report-control-release",
        "session-close",
    ]


def test_client_runtime_runs_subscription_plan_against_simulator_adapter() -> None:
    candidate = _candidate()
    plan = Iec61850ReportSubscriptionPlan(
        selected_signal_count=1,
        matched_signal_count=1,
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
                        matched_signals=(
                            Iec61850ReportSubscriptionPlanSignal(
                                selected_signal=Iec61850SelectedSignal(id="sig-1", address="IED1LD0/XCBR1/Pos/stVal[ST]"),
                                model_reference="LD0/XCBR1.Pos.stVal[ST]",
                                ied_name=candidate.ied_name,
                                match_kind="exact",
                            ),
                        ),
                    ),
                ),
            ),
        ),
    )
    adapter = create_iec61850_simulator_adapter(now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC))
    runtime = Iec61850MmsClientRuntime(adapter)

    result = runtime.run_report_subscription_plan(
        plan=plan,
        client_id="unitlab",
        endpoint_for_device=build_simulator_endpoint_for_plan_device,
        now=lambda: datetime(2026, 5, 29, 12, 0, tzinfo=UTC),
    )

    assert result.reports[0].candidate_id == candidate.id
    assert result.reports[0].error_code is None
    assert result.reports[0].event is not None
    assert result.reports[0].event.reason == Iec61850ReportReason.GENERAL_INTERROGATION
    assert len(result.diagnostics) == 0
    assert runtime.transcript()[-1].kind == "subscription-plan-report"


def _endpoint() -> Iec61850DeviceEndpoint:
    return Iec61850DeviceEndpoint(
        id="sim:IED1/AP1",
        mode=Iec61850RuntimeMode.SIMULATOR,
        ied_name="IED1",
        access_point_name="AP1",
        host=None,
        port=102,
    )


def _candidate() -> Iec61850ReportControlCandidate:
    return Iec61850ReportControlCandidate(
        id="report-1",
        ied_name="IED1",
        access_point_name="AP1",
        logical_device_inst="LD0",
        logical_node_name="LLN0",
        report_control_name="brcbEvents",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id="IED1LD0/LLN0.BR.Events",
        data_set_ref="IED1/AP1/LD0/LLN0.dsEvents",
        conf_rev="7",
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
        ),
    )
