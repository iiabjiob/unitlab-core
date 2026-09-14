from __future__ import annotations

from dataclasses import replace
from datetime import UTC, datetime
from threading import Event, Lock
from types import SimpleNamespace
import time

import pytest

from app.schemas.verification_schema import VerificationExecutionContextSchema
from app.services.iec61850.report_runtime import (
    Iec61850DeviceEndpoint,
    Iec61850ReportControlReadResult,
    Iec61850ReportControlState,
    Iec61850ReportEvent,
    Iec61850ReportEventValue,
    Iec61850ReportReason,
    Iec61850ReportRuntimeError,
    Iec61850RuntimeDiagnostic,
    Iec61850OptionalFields,
    Iec61850RuntimeMode,
    Iec61850RuntimeStatus,
    Iec61850RuntimeTriggerOptions,
)
from app.services.verification_planner import VerificationTargetSource, build_verification_subscription_plan
from app.services.verification_runtime_orchestrator import VerificationRuntimeOrchestrator, _observation_diagnostics_to_evidence_diagnostics


def _build_multi_ied_plan():
    return build_verification_subscription_plan(
        [
            VerificationTargetSource(
                signal_id=101,
                signal_reference="Breaker Close A",
                signal_path="breaker_close_a",
                signal_metadata={
                    "protocol": "iec61850",
                    "protocol_metadata": {
                        "ied_name": "IED-A",
                        "access_point_name": "P1",
                        "report_control_reference_hint": "IED-A/P1/LLN0.brA/buffered",
                        "report_control_name": "brA",
                        "report_kind": "buffered",
                        "rpt_id": "IED-A/LLN0.brA",
                        "data_set_reference": "IED-A/LLN0.dsA",
                        "expected_feedback_path": "LD0/XCBR1.Pos.stVal[ST]",
                    },
                },
                allocation_id=1,
                allocation_status="assigned",
                allocation_health={
                    "conflict": False,
                    "invalid_type": False,
                    "missing_device": False,
                    "missing_channel": False,
                    "offline_device": False,
                    "stale_device": False,
                },
                channel_id=11,
                channel_label="DO-11",
                unit_id="IED-A/P1",
                unit_online=True,
                source_row_id="signal-101",
            ),
            VerificationTargetSource(
                signal_id=202,
                signal_reference="Breaker Close B",
                signal_path="breaker_close_b",
                signal_metadata={
                    "protocol": "iec61850",
                    "protocol_metadata": {
                        "ied_name": "IED-B",
                        "access_point_name": "P1",
                        "report_control_reference_hint": "IED-B/P1/LLN0.brB/buffered",
                        "report_control_name": "brB",
                        "report_kind": "buffered",
                        "rpt_id": "IED-B/LLN0.brB",
                        "data_set_reference": "IED-B/LLN0.dsB",
                        "expected_feedback_path": "LD0/XCBR2.Pos.stVal[ST]",
                    },
                },
                allocation_id=2,
                allocation_status="assigned",
                allocation_health={
                    "conflict": False,
                    "invalid_type": False,
                    "missing_device": False,
                    "missing_channel": False,
                    "offline_device": False,
                    "stale_device": False,
                },
                channel_id=21,
                channel_label="DO-21",
                unit_id="IED-B/P1",
                unit_online=True,
                source_row_id="signal-202",
            ),
        ]
    )


def _build_same_endpoint_multi_report_plan():
    first_candidate = VerificationTargetSource(
        signal_id=101,
        signal_reference="Breaker Close A",
        signal_path="breaker_close_a",
        signal_metadata={
            "protocol": "iec61850",
            "protocol_metadata": {
                "ied_name": "IED-A",
                "access_point_name": "P1",
                "report_control_reference_hint": "IED-A/P1/LLN0.brA/buffered",
                "report_control_name": "brA",
                "report_kind": "buffered",
                "rpt_id": "IED-A/LLN0.brA",
                "data_set_reference": "IED-A/LLN0.dsA",
                "expected_feedback_path": "LD0/XCBR1.Pos.stVal[ST]",
            },
        },
        allocation_id=1,
        allocation_status="assigned",
        allocation_health={
            "conflict": False,
            "invalid_type": False,
            "missing_device": False,
            "missing_channel": False,
            "offline_device": False,
            "stale_device": False,
        },
        channel_id=11,
        channel_label="DO-11",
        unit_id="IED-A/P1",
        unit_online=True,
        source_row_id="signal-101",
    )
    second_candidate = replace(
        first_candidate,
        signal_id=102,
        signal_reference="Breaker Close B",
        signal_path="breaker_close_b",
        signal_metadata={
            "protocol": "iec61850",
            "protocol_metadata": {
                "ied_name": "IED-A",
                "access_point_name": "P1",
                "report_control_reference_hint": "IED-A/P1/LLN0.brB/buffered",
                "report_control_name": "brB",
                "report_kind": "buffered",
                "rpt_id": "IED-A/LLN0.brB",
                "data_set_reference": "IED-A/LLN0.dsB",
                "expected_feedback_path": "LD0/XCBR2.Pos.stVal[ST]",
            },
        },
        allocation_id=2,
        channel_id=12,
        channel_label="DO-12",
        source_row_id="signal-102",
    )
    return build_verification_subscription_plan([first_candidate, second_candidate])


def test_observation_diagnostics_filter_unrelated_selected_signals_for_capture_evidence() -> None:
    diagnostics = (
        SimpleNamespace(
            severity="info",
            code="SIGNAL_NOT_INCLUDED_IN_REPORT_EVENT",
            message='Selected signal "kint_251" was not included in this report event.',
            signal_id="251",
            address="kint_251",
            data_reference="LD0/XCBR1.Pos.stVal[ST]",
            reference=None,
        ),
        SimpleNamespace(
            severity="info",
            code="SIGNAL_NOT_INCLUDED_IN_REPORT_EVENT",
            message='Selected signal "kint_252" was not included in this report event.',
            signal_id="252",
            address="kint_252",
            data_reference="LD0/XCBR2.Pos.stVal[ST]",
            reference=None,
        ),
    )

    filtered = _observation_diagnostics_to_evidence_diagnostics(
        diagnostics,
        signal_id=251,
        include_unrelated=False,
    )

    assert len(filtered) == 1
    assert filtered[0].details is not None
    assert filtered[0].details["signal_id"] == "251"


def _custom_endpoint_for_device(device) -> Iec61850DeviceEndpoint:
    return Iec61850DeviceEndpoint(
        id=f"custom:{device.ied_name}/{device.access_point_name}",
        mode=Iec61850RuntimeMode.SIMULATOR,
        ied_name=device.ied_name,
        access_point_name=device.access_point_name,
        host=None,
        port=102,
    )


def _mms_endpoint_for_device(device) -> Iec61850DeviceEndpoint:
    return Iec61850DeviceEndpoint(
        id=f"mms:172.16.40.128:12447/{device.ied_name}/{device.access_point_name}",
        mode=Iec61850RuntimeMode.MMS,
        ied_name=device.ied_name,
        access_point_name=device.access_point_name,
        host="172.16.40.128",
        port=12447,
    )


def _unreachable_mms_endpoint_for_device(device) -> Iec61850DeviceEndpoint:
    return Iec61850DeviceEndpoint(
        id=f"mms:192.168.248.110:102/{device.ied_name}/{device.access_point_name}",
        mode=Iec61850RuntimeMode.MMS,
        ied_name=device.ied_name,
        access_point_name=device.access_point_name,
        host="192.168.248.110",
        port=102,
    )


def _other_unreachable_mms_endpoint_for_device(device) -> Iec61850DeviceEndpoint:
    return Iec61850DeviceEndpoint(
        id=f"mms:192.168.248.111:102/{device.ied_name}/{device.access_point_name}",
        mode=Iec61850RuntimeMode.MMS,
        ied_name=device.ied_name,
        access_point_name=device.access_point_name,
        host="192.168.248.111",
        port=102,
    )


class _FailingReadSession:
    def __init__(self) -> None:
        self.read_count = 0

    def read_report_control(self, reference):  # noqa: ANN001
        self.read_count += 1
        raise Iec61850ReportRuntimeError(
            "EXTERNAL_MMS_ENDPOINT_UNREACHABLE",
            "IEC 61850 endpoint 192.168.248.110:102 is unreachable: timed out",
        )

    def disconnect(self) -> None:
        return None


class _FailingReadAdapter:
    def __init__(self) -> None:
        self.session = _FailingReadSession()
        self.connect_count = 0

    def connect(self, **kwargs):  # noqa: ANN001
        self.connect_count += 1
        return self.session


class _PartialRptEnaFailureSession:
    def __init__(self) -> None:
        self.read_count = 0
        self.enable_count = 0
        self.gi_count = 0

    def read_report_control(self, reference):  # noqa: ANN001
        self.read_count += 1
        return _state_from_reference(reference, Iec61850RuntimeStatus.READ)

    def reserve_report_control(self, reference, client_id):  # noqa: ANN001
        return _state_from_reference(reference, Iec61850RuntimeStatus.RESERVED, reserved_by=client_id)

    def enable_report_control(self, reference, client_id):  # noqa: ANN001
        self.enable_count += 1
        if reference.report_control_name == "brB":
            raise Iec61850ReportRuntimeError(
                "EXTERNAL_MMS_RPTENA_NOT_CONFIRMED",
                "IEC 61850 external MMS client did not confirm RptEna=true.",
            )
        return _state_from_reference(reference, Iec61850RuntimeStatus.ENABLED, enabled=True, reserved_by=client_id, owner=client_id)

    def send_general_interrogation(self, reference, client_id):  # noqa: ANN001, ARG002
        self.gi_count += 1
        return Iec61850ReportEvent(
            id=f"{reference.report_control_name}:report-1",
            endpoint_id="custom:IED-A/P1",
            received_at="2026-06-23T12:00:00Z",
            report_control=reference,
            rpt_id=f"IED-A/LLN0.{reference.report_control_name}",
            data_set_ref="IED-A/LLN0.dsA",
            conf_rev=None,
            sequence_number=1,
            time_of_entry="2026-06-23T12:00:00Z",
            entry_id="entry-1",
            buffer_overflow=False,
            reason=Iec61850ReportReason.GENERAL_INTERROGATION,
            values=(
                Iec61850ReportEventValue(
                    data_set_index=0,
                    reference="LD0/XCBR1.Pos.stVal[ST]",
                    data_reference="LD0/XCBR1.Pos.stVal[ST]",
                    value=True,
                    reason_code=Iec61850ReportReason.GENERAL_INTERROGATION,
                    timestamp="2026-06-23T12:00:00Z",
                ),
            ),
        )

    def disconnect(self) -> None:
        return None


class _PartialRptEnaFailureAdapter:
    def __init__(self) -> None:
        self.session = _PartialRptEnaFailureSession()
        self.connect_count = 0

    def connect(self, **kwargs):  # noqa: ANN001
        self.connect_count += 1
        return self.session


class _ResolvedCandidateReadSession(_PartialRptEnaFailureSession):
    def read_report_control(self, reference):  # noqa: ANN001
        self.read_count += 1
        state = _state_from_reference(reference, Iec61850RuntimeStatus.READ)
        return Iec61850ReportControlReadResult(
            endpoint=_custom_endpoint_for_device(type("Device", (), {"ied_name": reference.ied_name, "access_point_name": reference.access_point_name})()),
            candidate_id="IED-ACTRL1:LLN0$BR$brcbST01",
            state=state,
            diagnostics=(),
        )


class _ResolvedCandidateReadAdapter:
    def __init__(self) -> None:
        self.session = _ResolvedCandidateReadSession()
        self.connect_count = 0

    def connect(self, **kwargs):  # noqa: ANN001
        self.connect_count += 1
        return self.session


class _PrecheckDiagnosticSession(_PartialRptEnaFailureSession):
    def read_report_control(self, reference):  # noqa: ANN001
        self.read_count += 1
        state = _state_from_reference(reference, Iec61850RuntimeStatus.READ)
        return Iec61850ReportControlReadResult(
            endpoint=_custom_endpoint_for_device(type("Device", (), {"ied_name": reference.ied_name, "access_point_name": reference.access_point_name})()),
            candidate_id="group-unmatched",
            state=state,
            diagnostics=(
                Iec61850RuntimeDiagnostic(
                    severity="error",
                    code="MMS_REPORT_CONTROL_NOT_MATCHED",
                    message="IEC 61850 discovery did not find a ReportControl dataset containing the requested signal-list addresses.",
                    reference=reference,
                ),
            ),
        )


class _PrecheckDiagnosticAdapter:
    def __init__(self) -> None:
        self.session = _PrecheckDiagnosticSession()
        self.connect_count = 0

    def connect(self, **kwargs):  # noqa: ANN001
        self.connect_count += 1
        return self.session


class _GiNoReportSession(_PartialRptEnaFailureSession):
    def enable_report_control(self, reference, client_id):  # noqa: ANN001
        self.enable_count += 1
        return _state_from_reference(reference, Iec61850RuntimeStatus.ENABLED, enabled=True, reserved_by=client_id, owner=client_id)

    def send_general_interrogation(self, reference, client_id):  # noqa: ANN001, ARG002
        self.gi_count += 1
        raise Iec61850ReportRuntimeError(
            "MMS_REPORT_NOT_OBSERVED",
            "IEC 61850 MMS client did not surface a report event.",
        )

    def wait_for_report(self, reference, client_id, *, after_sequence_number=None, after_event_id=None, timeout_ms=5000):  # noqa: ANN001, ARG002
        raise Iec61850ReportRuntimeError(
            "MMS_REPORT_TIMEOUT",
            "IEC 61850 MMS client did not surface a new report event before timeout.",
        )


class _GiNoReportAdapter:
    def __init__(self) -> None:
        self.session = _GiNoReportSession()
        self.connect_count = 0

    def connect(self, **kwargs):  # noqa: ANN001
        self.connect_count += 1
        return self.session


class _GiDelayedReportSession(_GiNoReportSession):
    def wait_for_report(self, reference, client_id, *, after_sequence_number=None, after_event_id=None, timeout_ms=5000):  # noqa: ANN001, ARG002
        return Iec61850ReportEvent(
            id=f"{reference.report_control_name}:delayed-report-1",
            endpoint_id="mms:IED-A/P1",
            received_at="2026-06-23T12:00:01Z",
            report_control=reference,
            rpt_id=f"IED-A/LLN0.{reference.report_control_name}",
            data_set_ref="IED-A/LLN0.dsA",
            conf_rev=None,
            sequence_number=2,
            time_of_entry="2026-06-23T12:00:01Z",
            entry_id="entry-delayed-1",
            buffer_overflow=False,
            reason=Iec61850ReportReason.GENERAL_INTERROGATION,
            values=(
                Iec61850ReportEventValue(
                    data_set_index=0,
                    reference="LD0/XCBR1.Pos.stVal[ST]",
                    data_reference="LD0/XCBR1.Pos.stVal[ST]",
                    value=True,
                    reason_code=Iec61850ReportReason.GENERAL_INTERROGATION,
                    timestamp="2026-06-23T12:00:01Z",
                ),
            ),
        )


class _GiDelayedReportAdapter:
    def __init__(self) -> None:
        self.session = _GiDelayedReportSession()
        self.connect_count = 0

    def connect(self, **kwargs):  # noqa: ANN001
        self.connect_count += 1
        return self.session


class _UnrelatedThenTargetReportSession(_PartialRptEnaFailureSession):
    def __init__(self) -> None:
        super().__init__()
        self.wait_count = 0

    def enable_report_control(self, reference, client_id):  # noqa: ANN001
        self.enable_count += 1
        return _state_from_reference(reference, Iec61850RuntimeStatus.ENABLED, enabled=True, reserved_by=client_id, owner=client_id)

    def wait_for_report(self, reference, client_id, *, after_sequence_number=None, after_event_id=None, timeout_ms=5000):  # noqa: ANN001, ARG002
        self.wait_count += 1
        sequence = int(after_sequence_number or 1) + 1
        if self.wait_count == 1:
            data_reference = "LD0/XCBR2.Pos.stVal[ST]"
            value = False
            event_id = "unrelated-report"
        else:
            data_reference = "LD0/XCBR1.Pos.stVal[ST]"
            value = True
            event_id = "target-report"
        received_at = f"2026-06-23T12:00:00.{self.wait_count:03d}Z"
        return Iec61850ReportEvent(
            id=f"{reference.report_control_name}:{event_id}",
            endpoint_id="mms:IED-A/P1",
            received_at=received_at,
            report_control=reference,
            rpt_id=f"IED-A/LLN0.{reference.report_control_name}",
            data_set_ref="IED-A/LLN0.dsA",
            conf_rev=None,
            sequence_number=sequence,
            time_of_entry=received_at,
            entry_id=f"entry-{event_id}",
            buffer_overflow=False,
            reason=Iec61850ReportReason.DATA_CHANGE,
            values=(
                Iec61850ReportEventValue(
                    data_set_index=0,
                    reference=data_reference,
                    data_reference=data_reference,
                    value=value,
                    reason_code=Iec61850ReportReason.DATA_CHANGE,
                    timestamp=received_at,
                ),
            ),
        )


class _UnrelatedThenTargetReportAdapter:
    def __init__(self) -> None:
        self.session = _UnrelatedThenTargetReportSession()
        self.connect_count = 0

    def connect(self, **kwargs):  # noqa: ANN001
        self.connect_count += 1
        return self.session


class _BlockingConnectAdapter:
    def __init__(self, release_event: Event) -> None:
        self.release_event = release_event
        self.session = _PartialRptEnaFailureSession()
        self.connect_count = 0

    def connect(self, **kwargs):  # noqa: ANN001
        self.connect_count += 1
        self.release_event.wait(timeout=5)
        return self.session


class _ConcurrentConnectAdapter:
    def __init__(self, release_event: Event, both_started_event: Event) -> None:
        self.release_event = release_event
        self.both_started_event = both_started_event
        self.session = _PartialRptEnaFailureSession()
        self.connect_count = 0
        self.lock = Lock()

    def connect(self, **kwargs):  # noqa: ANN001
        with self.lock:
            self.connect_count += 1
            if self.connect_count >= 2:
                self.both_started_event.set()
        self.release_event.wait(timeout=5)
        return self.session


def _state_from_reference(
    reference,
    runtime_status: Iec61850RuntimeStatus,
    *,
    enabled: bool = False,
    reserved_by: str | None = None,
    owner: str | None = None,
) -> Iec61850ReportControlState:
    return Iec61850ReportControlState(
        reference=reference,
        runtime_status=runtime_status,
        rpt_id=f"{reference.ied_name}/LLN0.{reference.report_control_name}",
        data_set_ref=f"{reference.ied_name}/LLN0.ds{reference.report_control_name[-1].upper()}",
        conf_rev=None,
        indexed=True,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signal_count=1,
        enabled=enabled,
        reserved_by=reserved_by,
        owner=owner,
    )


@pytest.mark.anyio
async def test_runtime_orchestrator_preserves_plan_group_id_after_live_candidate_resolution() -> None:
    plan = _build_same_endpoint_multi_report_plan()
    adapter = _ResolvedCandidateReadAdapter()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-live-candidate-id",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        endpoint_for_device=_custom_endpoint_for_device,
        adapter=adapter,
    )

    assert {snapshot.group_id for snapshot in result.subscription_snapshots} == {"group-1", "group-2"}


@pytest.mark.anyio
async def test_runtime_orchestrator_marks_unmatched_report_control_precheck_as_degraded() -> None:
    plan = _build_same_endpoint_multi_report_plan()
    adapter = _PrecheckDiagnosticAdapter()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-precheck-degraded",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        endpoint_for_device=_custom_endpoint_for_device,
        adapter=adapter,
    )

    assert {snapshot.subscription_state for snapshot in result.subscription_snapshots} == {"degraded"}
    assert {snapshot.diagnostic_code for snapshot in result.subscription_snapshots} == {"MMS_REPORT_CONTROL_NOT_MATCHED"}
    assert result.verification_run.runtime_summary["failed_subscriptions"] == 0


@pytest.mark.anyio
async def test_runtime_orchestrator_keeps_mms_subscription_reporting_when_startup_gi_has_no_report() -> None:
    plan = _build_same_endpoint_multi_report_plan()
    adapter = _GiNoReportAdapter()
    orchestrator = VerificationRuntimeOrchestrator(
        now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        mms_reachability_probe=lambda _endpoint: (True, None),
    )

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-mms-gi-no-report",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_for_device=_mms_endpoint_for_device,
        adapter=adapter,
    )

    assert adapter.session.gi_count == 2
    assert result.session_snapshots[0].runtime_state == "reporting"
    assert {snapshot.subscription_state for snapshot in result.subscription_snapshots} == {"reporting"}
    assert {snapshot.report_health for snapshot in result.subscription_snapshots} == {"healthy"}
    assert {snapshot.gi_requested for snapshot in result.subscription_snapshots} == {True}
    assert {snapshot.last_report_value_count for snapshot in result.subscription_snapshots} == {0}
    assert result.verification_run.runtime_summary["failed_subscriptions"] == 0
    assert {diagnostic.severity for snapshot in result.subscription_snapshots for diagnostic in snapshot.diagnostics} == {"warning"}


@pytest.mark.anyio
async def test_runtime_orchestrator_does_not_wait_for_delayed_mms_startup_gi_report_values() -> None:
    plan = _build_same_endpoint_multi_report_plan()
    adapter = _GiDelayedReportAdapter()
    orchestrator = VerificationRuntimeOrchestrator(
        now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        mms_reachability_probe=lambda _endpoint: (True, None),
    )

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-mms-delayed-gi-report",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_for_device=_mms_endpoint_for_device,
        adapter=adapter,
    )

    assert {snapshot.gi_requested for snapshot in result.subscription_snapshots} == {True}
    assert {snapshot.last_report_value_count for snapshot in result.subscription_snapshots} == {0}
    assert all(not snapshot.last_report_values for snapshot in result.subscription_snapshots)
    assert {diagnostic.severity for snapshot in result.subscription_snapshots for diagnostic in snapshot.diagnostics} == {"warning"}


@pytest.mark.anyio
async def test_runtime_orchestrator_reuses_one_session_for_multiple_reports_on_same_endpoint() -> None:
    plan = _build_same_endpoint_multi_report_plan()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-same-endpoint",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
    )

    assert len(result.session_snapshots) == 1
    assert len(result.subscription_snapshots) == 2
    assert result.session_snapshots[0].endpoint_id == "sim:IED-A/P1"
    assert {snapshot.session_id for snapshot in result.subscription_snapshots} == {
        result.session_snapshots[0].session_id,
    }
    assert {snapshot.endpoint_id for snapshot in result.subscription_snapshots} == {"sim:IED-A/P1"}


@pytest.mark.anyio
async def test_runtime_orchestrator_deferred_start_returns_before_mms_connect_finishes() -> None:
    plan = _build_same_endpoint_multi_report_plan()
    release_event = Event()
    adapter = _BlockingConnectAdapter(release_event)
    orchestrator = VerificationRuntimeOrchestrator(
        now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        mms_reachability_probe=lambda _endpoint: (True, None),
    )

    result = orchestrator.start_deferred(
        workspace_id=7,
        test_run_id="run-deferred-start",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_for_device=_mms_endpoint_for_device,
        adapter=adapter,
    )

    assert len(result.session_snapshots) == 1
    assert result.session_snapshots[0].runtime_state in {"connecting", "discovering"}
    assert {snapshot.subscription_state for snapshot in result.subscription_snapshots} == {"pending"}

    release_event.set()
    for _ in range(20):
        snapshot = orchestrator.snapshot(result.orchestration_id)
        if all(item.subscription_state != "pending" for item in snapshot.subscription_snapshots):
            break
        time.sleep(0.05)
    orchestrator.stop(result.orchestration_id)


@pytest.mark.anyio
async def test_runtime_orchestrator_deferred_start_connects_endpoint_groups_in_parallel() -> None:
    plan = _build_multi_ied_plan()
    release_event = Event()
    both_started_event = Event()
    adapter = _ConcurrentConnectAdapter(release_event, both_started_event)
    orchestrator = VerificationRuntimeOrchestrator(
        now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        mms_reachability_probe=lambda _endpoint: (True, None),
    )

    result = orchestrator.start_deferred(
        workspace_id=7,
        test_run_id="run-deferred-parallel-start",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_for_device=_mms_endpoint_for_device,
        adapter=adapter,
    )

    assert both_started_event.wait(timeout=1.0)
    assert adapter.connect_count == 2
    release_event.set()
    orchestrator.stop(result.orchestration_id)


@pytest.mark.anyio
async def test_runtime_orchestrator_keeps_session_alive_when_one_report_rptena_fails() -> None:
    plan = _build_same_endpoint_multi_report_plan()
    adapter = _PartialRptEnaFailureAdapter()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-partial-rptena-failure",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_for_device=_custom_endpoint_for_device,
        adapter=adapter,
    )

    states_by_report = {
        snapshot.report_control_name: snapshot for snapshot in result.subscription_snapshots
    }

    assert adapter.connect_count == 1
    assert adapter.session.read_count == 2
    assert adapter.session.enable_count == 2
    assert adapter.session.gi_count == 1
    assert states_by_report["brA"].gi_requested is True
    assert states_by_report["brA"].last_report_value_count == 1
    assert states_by_report["brA"].last_report_values[0]["value"] is True
    assert result.verification_run.runtime_state == "degraded"
    assert result.session_snapshots[0].runtime_state == "degraded"
    assert result.session_snapshots[0].diagnostic_code == "EXTERNAL_MMS_RPTENA_NOT_CONFIRMED"
    assert states_by_report["brA"].subscription_state == "reporting"
    assert states_by_report["brA"].report_health == "healthy"
    assert states_by_report["brB"].subscription_state == "failed"
    assert states_by_report["brB"].report_health == "degraded"
    assert states_by_report["brB"].diagnostic_code == "EXTERNAL_MMS_RPTENA_NOT_CONFIRMED"
    assert result.verification_run.runtime_summary["failed_sessions"] == 0
    assert result.verification_run.runtime_summary["failed_subscriptions"] == 1


@pytest.mark.anyio
async def test_runtime_orchestrator_requests_startup_gi_for_mms_endpoints() -> None:
    plan = _build_same_endpoint_multi_report_plan()
    adapter = _PartialRptEnaFailureAdapter()
    orchestrator = VerificationRuntimeOrchestrator(
        now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        mms_reachability_probe=lambda _endpoint: (True, None),
    )

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-mms-startup-gi",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_for_device=_mms_endpoint_for_device,
        adapter=adapter,
    )

    assert adapter.connect_count == 1
    assert adapter.session.read_count == 2
    assert adapter.session.enable_count == 2
    assert adapter.session.gi_count == 1
    assert result.verification_run.runtime_state == "degraded"
    assert result.session_snapshots[0].runtime_state == "degraded"
    assert {snapshot.subscription_state for snapshot in result.subscription_snapshots} == {"reporting", "failed"}


@pytest.mark.anyio
async def test_runtime_orchestrator_reconnects_one_session_with_multiple_reports_on_same_endpoint() -> None:
    plan = _build_same_endpoint_multi_report_plan()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-same-endpoint-reconnect",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
    )

    session_id = result.session_snapshots[0].session_id
    reconnected = orchestrator.reconnect(result.orchestration_id, session_id)
    session_snapshots = {snapshot.session_id: snapshot for snapshot in reconnected.session_snapshots}
    subscription_snapshots = [snapshot for snapshot in reconnected.subscription_snapshots if snapshot.session_id == session_id]

    assert reconnected.verification_run.recovery_state is None
    assert session_snapshots[session_id].connection_generation == 2
    assert session_snapshots[session_id].runtime_state == "reporting"
    assert len(subscription_snapshots) == 2
    assert {snapshot.subscription_state for snapshot in subscription_snapshots} == {"reporting"}
    assert {snapshot.report_health for snapshot in subscription_snapshots} == {"healthy"}
    assert {snapshot.session_id for snapshot in subscription_snapshots} == {session_id}


@pytest.mark.anyio
async def test_runtime_orchestrator_opens_sessions_and_keeps_live_state() -> None:
    plan = _build_multi_ied_plan()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-1",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
    )

    assert result.verification_run.workflow_state == "running"
    assert result.verification_run.verdict_state == "pending"
    assert result.verification_run.runtime_state == "reporting"
    assert result.verification_run.evidence_set.summary.evidence_count == 0
    assert len(result.session_snapshots) == 2
    assert {snapshot.endpoint_id for snapshot in result.session_snapshots} == {
        "sim:IED-A/P1",
        "sim:IED-B/P1",
    }
    assert all(snapshot.runtime_state == "reporting" for snapshot in result.session_snapshots)
    assert len(result.subscription_snapshots) == 2
    assert {snapshot.endpoint_id for snapshot in result.subscription_snapshots} == {
        "sim:IED-A/P1",
        "sim:IED-B/P1",
    }
    assert all(snapshot.subscription_state == "reporting" for snapshot in result.subscription_snapshots)
    assert all(snapshot.report_health == "healthy" for snapshot in result.subscription_snapshots)
    assert all(snapshot.current_rptena_owner == "unitlab-backend-simulator" for snapshot in result.subscription_snapshots)
    assert result.verification_run.runtime_summary["active_sessions"] == 2
    assert result.verification_run.runtime_summary["reporting_sessions"] == 2
    assert result.verification_run.runtime_summary["active_subscriptions"] == 2
    assert result.verification_run.runtime_summary["reporting_subscriptions"] == 2

    snapshot = orchestrator.snapshot(result.orchestration_id)
    assert snapshot.verification_run.runtime_state == "reporting"
    assert snapshot.verification_run.verdict_state == "pending"
    assert len(snapshot.active_session_ids) == 2

    stopped = orchestrator.stop(result.orchestration_id)
    assert stopped.verification_run.runtime_state == "closed"
    assert stopped.verification_run.runtime_summary["closed_sessions"] == 2

    with pytest.raises(RuntimeError):
        orchestrator.snapshot(result.orchestration_id)


@pytest.mark.anyio
async def test_runtime_orchestrator_uses_injected_endpoint_for_device() -> None:
    plan = _build_multi_ied_plan()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-custom-endpoint",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        endpoint_for_device=_custom_endpoint_for_device,
    )

    assert {snapshot.endpoint_id for snapshot in result.session_snapshots} == {
        "custom:IED-A/P1",
        "custom:IED-B/P1",
    }
    assert {snapshot.endpoint_id for snapshot in result.subscription_snapshots} == {
        "custom:IED-A/P1",
        "custom:IED-B/P1",
    }


@pytest.mark.anyio
async def test_runtime_orchestrator_surfaces_unreachable_mms_endpoint_without_raising() -> None:
    plan = _build_same_endpoint_multi_report_plan()
    orchestrator = VerificationRuntimeOrchestrator(
        now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        mms_reachability_probe=lambda _endpoint: (True, None),
    )
    adapter = _FailingReadAdapter()

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-unreachable-mms",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_for_device=_unreachable_mms_endpoint_for_device,
        adapter=adapter,
    )

    assert adapter.connect_count == 1
    assert adapter.session.read_count == 1
    assert result.verification_run.runtime_state == "degraded"
    assert result.verification_run.recovery_state is not None
    assert result.verification_run.recovery_state.recovery_reason == "runtime_failure"
    assert len(result.session_snapshots) == 1
    assert result.session_snapshots[0].runtime_state == "failed"
    assert result.session_snapshots[0].discovery_status == "unavailable"
    assert result.session_snapshots[0].diagnostic_code == "EXTERNAL_MMS_ENDPOINT_UNREACHABLE"
    assert len(result.subscription_snapshots) == 2
    assert {snapshot.subscription_state for snapshot in result.subscription_snapshots} == {"failed"}
    assert {snapshot.report_health for snapshot in result.subscription_snapshots} == {"degraded"}
    assert result.diagnostics[0].code == "EXTERNAL_MMS_ENDPOINT_UNREACHABLE"
    assert result.diagnostics[0].details["endpoint_host"] == "192.168.248.110"
    assert result.verification_run.runtime_summary["failed_sessions"] == 1
    assert result.verification_run.runtime_summary["failed_subscriptions"] == 2


@pytest.mark.anyio
async def test_runtime_orchestrator_skips_unreachable_mms_endpoint_before_opening_session() -> None:
    plan = _build_same_endpoint_multi_report_plan()
    adapter = _FailingReadAdapter()
    orchestrator = VerificationRuntimeOrchestrator(
        now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        mms_reachability_probe=lambda _endpoint: (False, "timed out"),
    )

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-unreachable-mms-preflight",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_for_device=_other_unreachable_mms_endpoint_for_device,
        adapter=adapter,
    )

    assert adapter.connect_count == 0
    assert adapter.session.read_count == 0
    assert result.verification_run.runtime_state == "degraded"
    assert len(result.session_snapshots) == 1
    assert result.session_snapshots[0].runtime_state == "failed"
    assert result.session_snapshots[0].discovery_status == "unavailable"
    assert result.session_snapshots[0].diagnostic_code == "EXTERNAL_MMS_ENDPOINT_UNREACHABLE"
    assert len(result.subscription_snapshots) == 2
    assert {snapshot.subscription_state for snapshot in result.subscription_snapshots} == {"failed"}
    assert result.diagnostics[0].message == "IEC 61850 endpoint 192.168.248.111:102 is unreachable: timed out"
    assert result.diagnostics[0].details["endpoint_host"] == "192.168.248.111"


@pytest.mark.anyio
async def test_runtime_orchestrator_prefights_mixed_mms_fleet_before_opening_sessions() -> None:
    plan = _build_multi_ied_plan()
    adapter = _FailingReadAdapter()

    def endpoint_for_device(device) -> Iec61850DeviceEndpoint:  # noqa: ANN001
        host = "172.16.40.128" if device.ied_name == "IED-A" else "192.168.248.111"
        return Iec61850DeviceEndpoint(
            id=f"mms:{host}:102/{device.ied_name}/{device.access_point_name}",
            mode=Iec61850RuntimeMode.MMS,
            ied_name=device.ied_name,
            access_point_name=device.access_point_name,
            host=host,
            port=102,
        )

    def reachability_probe(endpoint: Iec61850DeviceEndpoint) -> tuple[bool, str | None]:
        if endpoint.host == "172.16.40.128":
            return True, None
        return False, "timed out"

    orchestrator = VerificationRuntimeOrchestrator(
        now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        mms_reachability_probe=reachability_probe,
    )

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-mixed-mms-preflight",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_for_device=endpoint_for_device,
        adapter=adapter,
    )

    assert adapter.connect_count == 1
    assert adapter.session.read_count == 1
    assert len(result.session_snapshots) == 2
    session_by_host = {snapshot.endpoint_id.split(":")[1]: snapshot for snapshot in result.session_snapshots}
    assert session_by_host["172.16.40.128"].diagnostic_code == "EXTERNAL_MMS_ENDPOINT_UNREACHABLE"
    assert session_by_host["192.168.248.111"].diagnostic_code == "EXTERNAL_MMS_ENDPOINT_UNREACHABLE"
    assert "192.168.248.111" in {
        str(diagnostic.details.get("endpoint_host"))
        for diagnostic in result.diagnostics
        if diagnostic.details is not None
    }


@pytest.mark.anyio
async def test_runtime_orchestrator_reconnects_one_session_without_affecting_the_other() -> None:
    plan = _build_multi_ied_plan()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-2",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
    )

    session_ids = [snapshot.session_id for snapshot in result.session_snapshots]
    reconnected = orchestrator.reconnect(result.orchestration_id, session_ids[0])

    session_snapshots = {snapshot.session_id: snapshot for snapshot in reconnected.session_snapshots}
    subscription_snapshots = {snapshot.session_id: snapshot for snapshot in reconnected.subscription_snapshots}

    assert reconnected.verification_run.runtime_state == "reporting"
    assert reconnected.verification_run.recovery_state is None
    assert session_snapshots[session_ids[0]].connection_generation == 2
    assert session_snapshots[session_ids[0]].runtime_state == "reporting"
    assert session_snapshots[session_ids[1]].connection_generation == 1
    assert session_snapshots[session_ids[1]].runtime_state == "reporting"
    assert subscription_snapshots[session_ids[0]].subscription_state == "reporting"
    assert subscription_snapshots[session_ids[1]].subscription_state == "reporting"


@pytest.mark.anyio
async def test_runtime_orchestrator_reconnect_failure_surfaces_recovery_state(monkeypatch: pytest.MonkeyPatch) -> None:
    plan = _build_multi_ied_plan()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-3",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
    )

    handle = orchestrator._handles[result.orchestration_id]  # noqa: SLF001
    session_id = result.session_snapshots[0].session_id

    def _raise_open_session(**kwargs):  # noqa: ANN001
        raise RuntimeError("simulated reconnect failure")

    monkeypatch.setattr(handle.runtime_service, "open_session", _raise_open_session)

    recovered = orchestrator.reconnect(result.orchestration_id, session_id)
    session_snapshots = {snapshot.session_id: snapshot for snapshot in recovered.session_snapshots}

    assert recovered.verification_run.runtime_state == "degraded"
    assert recovered.verification_run.recovery_state is not None
    assert recovered.verification_run.recovery_state.recovery_reason == "runtime_failure"
    assert recovered.verification_run.recovery_state.desired_state == "reconnecting"
    assert session_snapshots[session_id].runtime_state == "failed"
    assert session_snapshots[session_id].connection_generation == 1
    assert session_snapshots[result.session_snapshots[1].session_id].runtime_state == "reporting"


@pytest.mark.anyio
async def test_runtime_orchestrator_captures_triggered_signal_report_after_initial_gi() -> None:
    plan = _build_same_endpoint_multi_report_plan()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))

    started = orchestrator.start(
        workspace_id=7,
        test_run_id="run-capture",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
    )

    capture = orchestrator.capture_triggered_signal(
        started.orchestration_id,
        signal_id=101,
        triggered_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        test_run_id="job-1",
        timeout_ms=1000,
    )

    assert capture.step.verdict_state == "pass"
    assert capture.evidence.evidence_status == "observed"
    assert capture.evidence.report_reason == "data-change"
    assert capture.evidence.source_report_sequence_number == 2
    assert capture.step.source_generation == 1


@pytest.mark.anyio
async def test_runtime_orchestrator_honors_cooperative_capture_cancellation() -> None:
    plan = _build_same_endpoint_multi_report_plan()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))
    started = orchestrator.start(
        workspace_id=7,
        test_run_id="run-capture-cancelled",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
    )
    cancel_event = Event()
    cancel_event.set()

    capture = orchestrator.capture_triggered_signal(
        started.orchestration_id,
        signal_id=101,
        triggered_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        timeout_ms=1000,
        cancel_event=cancel_event,
    )

    assert capture.evidence.evidence_status != "observed"


@pytest.mark.anyio
async def test_runtime_orchestrator_waits_past_unrelated_report_for_triggered_signal() -> None:
    plan = _build_same_endpoint_multi_report_plan()
    adapter = _UnrelatedThenTargetReportAdapter()
    orchestrator = VerificationRuntimeOrchestrator(
        now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        mms_reachability_probe=lambda endpoint: (True, None),
    )

    started = orchestrator.start(
        workspace_id=7,
        test_run_id="run-capture-unrelated",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        adapter=adapter,
        endpoint_for_device=_mms_endpoint_for_device,
    )

    capture = orchestrator.capture_triggered_signal(
        started.orchestration_id,
        signal_id=101,
        triggered_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        test_run_id="job-1",
        timeout_ms=1000,
    )

    assert capture.step.verdict_state == "pass"
    assert capture.evidence.evidence_status == "observed"
    assert capture.evidence.actual_report_path == "LD0/XCBR1.Pos.stVal[ST]"
    assert adapter.session.wait_count == 2
