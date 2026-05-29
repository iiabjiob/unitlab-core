from __future__ import annotations

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
    create_iec61850_simulator_adapter,
    map_report_event_to_subscription_plan_observations,
    map_report_event_to_signal_observations,
    normalize_report_data_reference,
    run_report_subscription_plan,
    run_simulator_report_subscription_plan,
    to_report_control_ref,
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
