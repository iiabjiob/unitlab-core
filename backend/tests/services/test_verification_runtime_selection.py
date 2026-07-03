from __future__ import annotations

from datetime import UTC, datetime
from types import SimpleNamespace

import pytest

from app.schemas.verification_schema import VerificationExecutionContextSchema
from app.services.iec61850.mms_adapter import Iec61850MmsEndpointCatalogEntry, build_mms_endpoint_catalog
from app.services.iec61850.report_runtime import (
    Iec61850DataSetMember,
    Iec61850DeviceEndpoint,
    Iec61850OptionalFields,
    Iec61850ReportControlCandidate,
    Iec61850ReportEvent,
    Iec61850ReportEventValue,
    Iec61850ReportKind,
    Iec61850RuntimeMode,
    Iec61850RuntimeStatus,
    Iec61850RuntimeTriggerOptions,
    Iec61850SelectedSignal,
    to_report_control_ref,
    Iec61850ReportReason,
)
from app.services.verification_runtime_selection import resolve_verification_runtime


class _FakeClientControlService:
    def __init__(
        self,
        *,
        session_id: str,
        client_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidate: Iec61850ReportControlCandidate,
        available_candidates: tuple[Iec61850ReportControlCandidate, ...] = (),
    ) -> None:
        self.session_id = session_id
        self.client_id = client_id
        self.endpoint = endpoint
        self.candidate = candidate
        self.available_candidates = available_candidates or (candidate,)
        self.calls: list[str] = []

    def _build_report(self) -> Iec61850ReportEvent:
        return Iec61850ReportEvent(
            id=f"{self.session_id}:report-1",
            endpoint_id=self.endpoint.id,
            received_at="2026-06-24T10:11:13.070000Z",
            report_control=to_report_control_ref(self.candidate),
            rpt_id=self.candidate.rpt_id,
            data_set_ref=self.candidate.data_set_ref,
            conf_rev=self.candidate.conf_rev,
            sequence_number=1,
            time_of_entry="2026-06-24T10:11:13.070000Z",
            entry_id="entry-1",
            buffer_overflow=False,
            reason=Iec61850ReportReason.GENERAL_INTERROGATION,
            values=(
                Iec61850ReportEventValue(
                    data_set_index=0,
                    reference="LD0/XCBR1.Pos.stVal[ST]",
                    data_reference="LD0/XCBR1.Pos.stVal[ST]",
                    value=1,
                    reason_code=Iec61850ReportReason.DATA_CHANGE,
                    timestamp="2026-06-24T10:11:13.070000Z",
                ),
            ),
        )

    def select_report_control(self, selected_rcb_ref: str):
        self.calls.append(f"select:{selected_rcb_ref}")
        selected = next(
            (
                candidate
                for candidate in self.available_candidates
                if candidate.id == selected_rcb_ref
                or "/".join((
                    candidate.ied_name,
                    candidate.access_point_name,
                    candidate.logical_device_inst,
                    candidate.logical_node_name,
                    candidate.report_control_name,
                    candidate.report_kind.value,
                )) == selected_rcb_ref
            ),
            None,
        )
        if selected is not None:
            self.candidate = selected
        return SimpleNamespace()

    def discover_ied(self):
        self.calls.append("discover")
        return SimpleNamespace(last_report=None)

    def enable_reporting(self):
        self.calls.append("enable")
        return SimpleNamespace()

    def send_general_interrogation(self):
        self.calls.append("gi")
        return SimpleNamespace()

    def snapshot(self):
        self.calls.append("snapshot")
        return SimpleNamespace(last_report=self._build_report())

    def close_ied(self):
        self.calls.append("close")
        return SimpleNamespace()


def _candidate() -> Iec61850ReportControlCandidate:
    return Iec61850ReportControlCandidate(
        id="report-1",
        ied_name="IED-A",
        access_point_name="P1",
        logical_device_inst="LD0",
        logical_node_name="LLN0",
        report_control_name="brA",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id="IED-A/LLN0.brA",
        data_set_ref="IED-A/LLN0.dsA",
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
        signals=(Iec61850DataSetMember(reference="LD0/XCBR1.Pos.stVal[ST]", fc="ST"),),
    )


def _candidate_variant(*, candidate_id: str, report_control_name: str, rpt_id: str, data_set_ref: str) -> Iec61850ReportControlCandidate:
    return Iec61850ReportControlCandidate(
        id=candidate_id,
        ied_name="IED-A",
        access_point_name="P1",
        logical_device_inst="LD0",
        logical_node_name="LLN0",
        report_control_name=report_control_name,
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id=rpt_id,
        data_set_ref=data_set_ref,
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
        signals=(Iec61850DataSetMember(reference="LD0/XCBR1.Pos.stVal[ST]", fc="ST"),),
    )


def _discovery_candidate(
    *,
    candidate_id: str,
    report_control_name: str,
    data_set_ref: str,
    signal_reference: str,
) -> Iec61850ReportControlCandidate:
    return Iec61850ReportControlCandidate(
        id=candidate_id,
        ied_name="IED-A",
        access_point_name="P1",
        logical_device_inst="CTRL",
        logical_node_name="LLN0",
        report_control_name=report_control_name,
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id=f"IED-ACTRL/LLN0.{report_control_name}",
        data_set_ref=data_set_ref,
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
        signals=(Iec61850DataSetMember(reference=signal_reference, fc="ST"),),
    )


def test_resolve_verification_runtime_selects_simulator_by_default() -> None:
    selection = resolve_verification_runtime(
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        now=lambda: datetime(2026, 6, 24, 10, 11, 12, tzinfo=UTC),
    )

    assert selection.runtime_mode == "simulator"
    assert selection.transport_source == "simulator"
    assert selection.model_source == "simulator"
    assert selection.runtime_source == "simulator"
    assert selection.endpoint_for_device(
        SimpleNamespace(ied_name="IED-A", access_point_name="P1")
    ).id == "sim:IED-A/P1"


def test_resolve_verification_runtime_selects_mms_adapter_from_catalog() -> None:
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED-A",
            access_point_name="P1",
            host="10.10.10.250",
            port=12447,
        ),
    ))

    selection = resolve_verification_runtime(
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_catalog=catalog,
        mms_control_service_factory=_FakeClientControlService,
    )

    assert selection.runtime_mode == "mms"
    assert selection.transport_source == "explicit_request"
    assert selection.model_source == "discovery_fallback"
    assert selection.runtime_source == "explicit_request"
    assert selection.endpoint_for_device(
        SimpleNamespace(ied_name="IED-A", access_point_name="P1")
    ).id == "mms:IED-A/P1@10.10.10.250:12447"


def test_resolve_verification_runtime_preserves_selection_sources() -> None:
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED-A",
            access_point_name="P1",
            host="10.10.10.250",
            port=12447,
        ),
    ))

    selection = resolve_verification_runtime(
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_catalog=catalog,
        transport_source="settings_catalog",
        model_source="loaded_scd",
        mms_control_service_factory=_FakeClientControlService,
    )

    assert selection.transport_source == "settings_catalog"
    assert selection.model_source == "loaded_scd"
    assert selection.runtime_source == "settings_catalog"


def test_resolve_verification_runtime_applies_validation_override_to_mms_endpoint() -> None:
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED-A",
            access_point_name="P1",
            host="10.10.10.250",
            port=12447,
        ),
    ))

    selection = resolve_verification_runtime(
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_catalog=catalog,
        transport_override_host="10.10.10.99",
        transport_override_port=12447,
        mms_control_service_factory=_FakeClientControlService,
    )

    endpoint = selection.endpoint_for_device(SimpleNamespace(ied_name="IED-A", access_point_name="P1"))
    assert endpoint.id == "mms:IED-A/P1@10.10.10.99:12447"
    assert selection.transport_source == "validation_override"
    assert selection.runtime_source == "validation_override"


def test_mms_runtime_adapter_surfaces_report_from_control_service() -> None:
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED-A",
            access_point_name="P1",
            host="10.10.10.250",
            port=12447,
        ),
    ))
    selection = resolve_verification_runtime(
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_catalog=catalog,
        mms_control_service_factory=_FakeClientControlService,
    )
    endpoint = selection.endpoint_for_device(SimpleNamespace(ied_name="IED-A", access_point_name="P1"))
    session = selection.adapter.connect(
        session_id="run-1:mms:IED-A/P1@10.10.10.250:12447",
        endpoint=endpoint,
        candidates=(_candidate(),),
    )

    candidate = _candidate()
    reference = to_report_control_ref(candidate)
    state = session.read_report_control(reference)
    reserved = session.reserve_report_control(reference, "unitlab")
    enabled = session.enable_report_control(reference, "unitlab")
    report = session.send_general_interrogation(reference, "unitlab")

    assert state.state.runtime_status == Iec61850RuntimeStatus.READ
    assert reserved.runtime_status == Iec61850RuntimeStatus.RESERVED
    assert enabled.runtime_status == Iec61850RuntimeStatus.ENABLED
    assert report.endpoint_id == endpoint.id


def test_mms_runtime_adapter_returns_discovery_summary_diagnostic() -> None:
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED-A",
            access_point_name="P1",
            host="10.10.10.250",
            port=12447,
        ),
    ))

    class _DiscoverySummaryClientControlService(_FakeClientControlService):
        def discover_ied(self):
            self.calls.append("discover")
            return SimpleNamespace(
                ui_state={
                    "discovery": {
                        "discovered": True,
                        "logical_devices": 7,
                        "logical_nodes": 63,
                        "data_sets": 106,
                        "data_set_members": 379,
                        "report_controls": 33,
                        "signals": 253,
                    },
                },
            )

    selection = resolve_verification_runtime(
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_catalog=catalog,
        mms_control_service_factory=_DiscoverySummaryClientControlService,
    )
    endpoint = selection.endpoint_for_device(SimpleNamespace(ied_name="IED-A", access_point_name="P1"))
    candidate = _candidate()
    session = selection.adapter.connect(
        session_id="run-summary:mms:IED-A/P1@10.10.10.250:12447",
        endpoint=endpoint,
        candidates=(candidate,),
    )

    result = session.read_report_control(to_report_control_ref(candidate))
    summary = next(diagnostic for diagnostic in result.diagnostics if diagnostic.code == "MMS_DISCOVERY_SUMMARY")

    assert summary.severity == "info"
    assert summary.details["logical_devices"] == 7
    assert summary.details["report_controls"] == 33
    assert summary.details["endpoint_host"] == "10.10.10.250"


def test_mms_runtime_adapter_supports_multiple_candidates_on_one_session() -> None:
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED-A",
            access_point_name="P1",
            host="10.10.10.250",
            port=12447,
        ),
    ))
    created_services: list[_FakeClientControlService] = []

    def _factory(**kwargs):
        service = _FakeClientControlService(**kwargs)
        created_services.append(service)
        return service

    selection = resolve_verification_runtime(
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_catalog=catalog,
        mms_control_service_factory=_factory,
    )
    endpoint = selection.endpoint_for_device(SimpleNamespace(ied_name="IED-A", access_point_name="P1"))
    candidate_a = _candidate_variant(
        candidate_id="report-a",
        report_control_name="brA",
        rpt_id="IED-A/LLN0.brA",
        data_set_ref="IED-A/LLN0.dsA",
    )
    candidate_b = _candidate_variant(
        candidate_id="report-b",
        report_control_name="brB",
        rpt_id="IED-A/LLN0.brB",
        data_set_ref="IED-A/LLN0.dsB",
    )
    session = selection.adapter.connect(
        session_id="run-2:mms:IED-A/P1@10.10.10.250:12447",
        endpoint=endpoint,
        candidates=(candidate_a, candidate_b),
    )

    ref_a = to_report_control_ref(candidate_a)
    ref_b = to_report_control_ref(candidate_b)
    session.read_report_control(ref_a)
    session.enable_report_control(ref_a, "unitlab")
    report_a = session.send_general_interrogation(ref_a, "unitlab")
    session.read_report_control(ref_b)
    session.enable_report_control(ref_b, "unitlab")
    report_b = session.send_general_interrogation(ref_b, "unitlab")
    session.disconnect()

    assert report_a.endpoint_id == endpoint.id
    assert report_b.endpoint_id == endpoint.id
    assert len(created_services) == 3
    assert created_services[0].calls == [
        "discover",
        "select:report-b",
        "discover",
        "close",
    ]
    assert created_services[1].calls == [
        "discover",
        "select:report-a",
        "enable",
        "gi",
        "snapshot",
        "close",
    ]
    assert created_services[2].calls == [
        "discover",
        "select:report-b",
        "enable",
        "gi",
        "snapshot",
        "close",
    ]


def test_mms_runtime_adapter_seeds_subscription_service_from_discovery_cache() -> None:
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED-A",
            access_point_name="P1",
            host="10.10.10.250",
            port=12447,
        ),
    ))
    created_services: list[_FakeClientControlService] = []

    class _SeedableClientControlService(_FakeClientControlService):
        def __init__(self, **kwargs):
            super().__init__(**kwargs)
            self._external_discovered_rcbs = []  # noqa: SLF001
            self._external_discovered_rcbs_native = False  # noqa: SLF001
            self._external_live_discovery = None  # noqa: SLF001
            self._last_discovery = None  # noqa: SLF001

        def discover_ied(self):
            self.calls.append("discover")
            self._external_discovered_rcbs = [SimpleNamespace(index=0, domain="IED-ALD0", item="LLN0$BR$brA01")]  # noqa: SLF001
            self._external_live_discovery = {"reportControls": []}  # noqa: SLF001
            self._last_discovery = self._external_live_discovery  # noqa: SLF001
            return SimpleNamespace(last_report=None)

    def _factory(**kwargs):
        service = _SeedableClientControlService(**kwargs)
        created_services.append(service)
        return service

    selection = resolve_verification_runtime(
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_catalog=catalog,
        mms_control_service_factory=_factory,
    )
    endpoint = selection.endpoint_for_device(SimpleNamespace(ied_name="IED-A", access_point_name="P1"))
    candidate = _candidate()
    session = selection.adapter.connect(
        session_id="run-cache:mms:IED-A/P1@10.10.10.250:12447",
        endpoint=endpoint,
        candidates=(candidate,),
    )

    session.read_report_control(to_report_control_ref(candidate))
    session.enable_report_control(to_report_control_ref(candidate), "unitlab")

    assert len(created_services) == 2
    assert created_services[0].calls == ["discover"]
    assert created_services[1].calls == ["select:report-1", "enable"]
    assert created_services[1]._external_discovered_rcbs_native is False  # noqa: SLF001
    assert created_services[1]._external_discovered_rcbs  # noqa: SLF001


def test_mms_runtime_adapter_selects_discovered_rcb_matching_signal_list_address() -> None:
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED-A",
            access_point_name="P1",
            host="10.10.10.250",
            port=12447,
        ),
    ))
    wrong_candidate = _discovery_candidate(
        candidate_id="IED-ACTRL:LLN0$BR$brWrong01",
        report_control_name="brWrong",
        data_set_ref="IED-ACTRL/LLN0.RCB1",
        signal_reference="XCBR1.Pos[ST]",
    )
    matching_candidate = _discovery_candidate(
        candidate_id="IED-ACTRL:LLN0$BR$brMatch01",
        report_control_name="brMatch",
        data_set_ref="IED-ACTRL/LLN0.RCB2",
        signal_reference="PTOC1.Str[ST]",
    )
    created_services: list[_FakeClientControlService] = []

    class _DiscoveryClientControlService(_FakeClientControlService):
        def __init__(self, **kwargs):
            super().__init__(**kwargs)
            self.candidate = wrong_candidate
            self.available_candidates = (wrong_candidate, matching_candidate)

        def discover_ied(self):
            self.calls.append("discover")
            return SimpleNamespace(
                candidate=self.candidate,
                last_discovery={
                    "dataSets": [
                        {
                            "reference": "IED-ACTRL/LLN0.RCB1",
                            "members": [{"reference": "XCBR1.Pos[ST]", "mmsReference": "IED-ACTRL/XCBR1$ST$Pos"}],
                        },
                        {
                            "reference": "IED-ACTRL/LLN0.RCB2",
                            "members": [{"reference": "PTOC1.Str[ST]", "mmsReference": "IED-ACTRL/PTOC1$ST$Str"}],
                        },
                    ],
                    "reportControls": [
                        {"id": wrong_candidate.id, "dataSetRef": "IED-ACTRL/LLN0.RCB1"},
                        {"id": matching_candidate.id, "dataSetRef": "IED-ACTRL/LLN0.RCB2"},
                    ],
                },
                ui_state={
                    "discovery": {
                        "available_report_controls": [
                            {"report_control_id": wrong_candidate.id, "data_set_ref": "IED-ACTRL/LLN0.RCB1"},
                            {"report_control_id": matching_candidate.id, "data_set_ref": "IED-ACTRL/LLN0.RCB2"},
                        ]
                    }
                },
            )

        def select_report_control(self, selected_rcb_ref: str):
            super().select_report_control(selected_rcb_ref)
            return SimpleNamespace(candidate=self.candidate)

    def _factory(**kwargs):
        service = _DiscoveryClientControlService(**kwargs)
        created_services.append(service)
        return service

    selection = resolve_verification_runtime(
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_catalog=catalog,
        mms_control_service_factory=_factory,
    )
    endpoint = selection.endpoint_for_device(SimpleNamespace(ied_name="IED-A", access_point_name="P1"))
    fallback_candidate = Iec61850ReportControlCandidate(
        id="group-1",
        ied_name="IED-A",
        access_point_name="P1",
        logical_device_inst="",
        logical_node_name="",
        report_control_name="",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id=None,
        data_set_ref=None,
        conf_rev=None,
        indexed=None,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(Iec61850DataSetMember(reference="IED-ACTRL/PTOC1.Str.stVal[ST]", fc="ST"),),
    )
    session = selection.adapter.connect(
        session_id="run-3:mms:IED-A/P1@10.10.10.250:12447",
        endpoint=endpoint,
        candidates=(fallback_candidate,),
    )

    result = session.read_report_control(to_report_control_ref(fallback_candidate))
    enabled = session.enable_report_control(to_report_control_ref(fallback_candidate), "unitlab")

    assert result.candidate_id == matching_candidate.id
    assert result.state.reference.report_control_name == "brMatch"
    assert enabled.data_set_ref == "IED-ACTRL/LLN0.RCB2"
    assert created_services[0].calls == [
        "discover",
        f"select:{matching_candidate.id}",
    ]
    assert created_services[1].calls == [
        "discover",
        f"select:{matching_candidate.id}",
        "enable",
    ]


def test_mms_runtime_adapter_keeps_fallback_groups_distinct_during_live_matching() -> None:
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED-A",
            access_point_name="P1",
            host="10.10.10.250",
            port=12447,
        ),
    ))
    candidate_a = _discovery_candidate(
        candidate_id="IED-ACTRL:LLN0$BR$brA01",
        report_control_name="brA",
        data_set_ref="IED-ACTRL/LLN0.RCB1",
        signal_reference="XCBR1.Pos[ST]",
    )
    candidate_b = _discovery_candidate(
        candidate_id="IED-ACTRL:LLN0$BR$brB01",
        report_control_name="brB",
        data_set_ref="IED-ACTRL/LLN0.RCB2",
        signal_reference="PTOC1.Str[ST]",
    )

    class _DiscoveryClientControlService(_FakeClientControlService):
        def __init__(self, **kwargs):
            super().__init__(**kwargs)
            self.candidate = candidate_a
            self.available_candidates = (candidate_a, candidate_b)

        def discover_ied(self):
            self.calls.append("discover")
            return SimpleNamespace(
                candidate=self.candidate,
                last_discovery={
                    "dataSets": [
                        {
                            "reference": "IED-ACTRL/LLN0.RCB1",
                            "members": [{"reference": "XCBR1.Pos[ST]", "mmsReference": "IED-ACTRL/XCBR1$ST$Pos"}],
                        },
                        {
                            "reference": "IED-ACTRL/LLN0.RCB2",
                            "members": [{"reference": "PTOC1.Str[ST]", "mmsReference": "IED-ACTRL/PTOC1$ST$Str"}],
                        },
                    ],
                    "reportControls": [
                        {"id": candidate_a.id, "dataSetRef": "IED-ACTRL/LLN0.RCB1"},
                        {"id": candidate_b.id, "dataSetRef": "IED-ACTRL/LLN0.RCB2"},
                    ],
                },
                ui_state={
                    "discovery": {
                        "available_report_controls": [
                            {"report_control_id": candidate_a.id, "data_set_ref": "IED-ACTRL/LLN0.RCB1"},
                            {"report_control_id": candidate_b.id, "data_set_ref": "IED-ACTRL/LLN0.RCB2"},
                        ]
                    }
                },
            )

        def select_report_control(self, selected_rcb_ref: str):
            super().select_report_control(selected_rcb_ref)
            return SimpleNamespace(candidate=self.candidate)

    selection = resolve_verification_runtime(
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_catalog=catalog,
        mms_control_service_factory=_DiscoveryClientControlService,
    )
    endpoint = selection.endpoint_for_device(SimpleNamespace(ied_name="IED-A", access_point_name="P1"))
    fallback_a = Iec61850ReportControlCandidate(
        id="group-a",
        ied_name="IED-A",
        access_point_name="P1",
        logical_device_inst="",
        logical_node_name="",
        report_control_name="group-a",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id=None,
        data_set_ref=None,
        conf_rev=None,
        indexed=None,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(Iec61850DataSetMember(reference="IED-ACTRL/XCBR1.Pos.stVal[ST]", fc="ST"),),
    )
    fallback_b = Iec61850ReportControlCandidate(
        id="group-b",
        ied_name="IED-A",
        access_point_name="P1",
        logical_device_inst="",
        logical_node_name="",
        report_control_name="group-b",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id=None,
        data_set_ref=None,
        conf_rev=None,
        indexed=None,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(Iec61850DataSetMember(reference="IED-ACTRL/PTOC1.Str.stVal[ST]", fc="ST"),),
    )
    session = selection.adapter.connect(
        session_id="run-distinct:mms:IED-A/P1@10.10.10.250:12447",
        endpoint=endpoint,
        candidates=(fallback_a, fallback_b),
    )

    result_a = session.read_report_control(to_report_control_ref(fallback_a))
    result_b = session.read_report_control(to_report_control_ref(fallback_b))

    assert result_a.candidate_id == candidate_a.id
    assert result_b.candidate_id == candidate_b.id


def test_mms_runtime_adapter_matches_live_report_control_by_requested_domain_when_members_are_sparse() -> None:
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED-A",
            access_point_name="P1",
            host="10.10.10.250",
            port=12447,
        ),
    ))
    ctrl1_candidate = _discovery_candidate(
        candidate_id="IED-ACTRL1:LLN0$BR$brcbST01",
        report_control_name="brcbST",
        data_set_ref="IED-ACTRL1/LLN0.LLN0BRptStDs",
        signal_reference="XCBR1.Pos[ST]",
    )
    ctrl2_candidate = _discovery_candidate(
        candidate_id="IED-ACTRL2:LLN0$BR$brcbST01",
        report_control_name="brcbST",
        data_set_ref="IED-ACTRL2/LLN0.LLN0BRptStDs",
        signal_reference="SlotHGGIO12.Ind15[ST]",
    )

    class _SparseDiscoveryClientControlService(_FakeClientControlService):
        def __init__(self, **kwargs):
            super().__init__(**kwargs)
            self.candidate = ctrl1_candidate
            self.available_candidates = (ctrl1_candidate, ctrl2_candidate)

        def discover_ied(self):
            self.calls.append("discover")
            return SimpleNamespace(
                candidate=self.candidate,
                last_discovery={
                    "dataSets": [
                        {"reference": "IED-ACTRL1/LLN0.LLN0BRptStDs", "members": []},
                        {"reference": "IED-ACTRL2/LLN0.LLN0BRptStDs", "members": []},
                    ],
                    "reportControls": [
                        {"id": ctrl1_candidate.id, "dataSetRef": "IED-ACTRL1/LLN0.LLN0BRptStDs"},
                        {"id": ctrl2_candidate.id, "dataSetRef": "IED-ACTRL2/LLN0.LLN0BRptStDs"},
                    ],
                },
                ui_state={
                    "discovery": {
                        "available_report_controls": [
                            {"report_control_id": ctrl1_candidate.id, "data_set_ref": "IED-ACTRL1/LLN0.LLN0BRptStDs"},
                            {"report_control_id": ctrl2_candidate.id, "data_set_ref": "IED-ACTRL2/LLN0.LLN0BRptStDs"},
                        ]
                    }
                },
            )

        def select_report_control(self, selected_rcb_ref: str):
            super().select_report_control(selected_rcb_ref)
            return SimpleNamespace(candidate=self.candidate)

    selection = resolve_verification_runtime(
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_catalog=catalog,
        mms_control_service_factory=_SparseDiscoveryClientControlService,
    )
    endpoint = selection.endpoint_for_device(SimpleNamespace(ied_name="IED-A", access_point_name="P1"))
    fallback_candidate = Iec61850ReportControlCandidate(
        id="group-ctrl2",
        ied_name="IED-A",
        access_point_name="P1",
        logical_device_inst="",
        logical_node_name="",
        report_control_name="group-ctrl2",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id=None,
        data_set_ref=None,
        conf_rev=None,
        indexed=None,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(Iec61850DataSetMember(reference="IED-ACTRL2/SlotHGGIO12/Ind15/stVal[ST]", fc="ST"),),
    )
    session = selection.adapter.connect(
        session_id="run-domain:mms:IED-A/P1@10.10.10.250:12447",
        endpoint=endpoint,
        candidates=(fallback_candidate,),
    )

    result = session.read_report_control(to_report_control_ref(fallback_candidate))

    assert result.candidate_id == ctrl2_candidate.id
    assert result.state.data_set_ref == "IED-ACTRL2/LLN0.LLN0BRptStDs"
    assert not any(diagnostic.code == "MMS_REPORT_CONTROL_NOT_MATCHED" for diagnostic in result.diagnostics)


def test_mms_runtime_adapter_prefers_first_available_indexed_rcb_for_duplicate_dataset() -> None:
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED-A",
            access_point_name="P1",
            host="10.10.10.250",
            port=12447,
        ),
    ))
    first_candidate = _discovery_candidate(
        candidate_id="IED-ACTRL1:LLN0$BR$brcbST01",
        report_control_name="brcbST",
        data_set_ref="IED-ACTRL1/LLN0.LLN0BRptStDs",
        signal_reference="XCBR1.Pos[ST]",
    )
    last_candidate = _discovery_candidate(
        candidate_id="IED-ACTRL1:LLN0$BR$brcbST06",
        report_control_name="brcbST",
        data_set_ref="IED-ACTRL1/LLN0.LLN0BRptStDs",
        signal_reference="XCBR1.Pos[ST]",
    )

    class _DuplicateDatasetDiscoveryClientControlService(_FakeClientControlService):
        def __init__(self, **kwargs):
            super().__init__(**kwargs)
            self.candidate = first_candidate
            self.available_candidates = (first_candidate, last_candidate)

        def discover_ied(self):
            self.calls.append("discover")
            return SimpleNamespace(
                candidate=self.candidate,
                last_discovery={
                    "dataSets": [
                        {
                            "reference": "IED-ACTRL1/LLN0.LLN0BRptStDs",
                            "members": [{"reference": "XCBR1.Pos[ST]", "mmsReference": "IED-ACTRL1/XCBR1$ST$Pos"}],
                        },
                    ],
                    "reportControls": [
                        {"id": first_candidate.id, "dataSetRef": "IED-ACTRL1/LLN0.LLN0BRptStDs"},
                    ],
                },
                ui_state={
                    "discovery": {
                        "available_report_controls": [
                            {"report_control_id": first_candidate.id, "data_set_ref": "IED-ACTRL1/LLN0.LLN0BRptStDs"},
                            {"report_control_id": last_candidate.id, "data_set_ref": "IED-ACTRL1/LLN0.LLN0BRptStDs"},
                        ]
                    }
                },
            )

        def select_report_control(self, selected_rcb_ref: str):
            super().select_report_control(selected_rcb_ref)
            return SimpleNamespace(candidate=self.candidate)

    selection = resolve_verification_runtime(
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_catalog=catalog,
        mms_control_service_factory=_DuplicateDatasetDiscoveryClientControlService,
    )
    endpoint = selection.endpoint_for_device(SimpleNamespace(ied_name="IED-A", access_point_name="P1"))
    fallback_candidate = Iec61850ReportControlCandidate(
        id="group-ctrl1",
        ied_name="IED-A",
        access_point_name="P1",
        logical_device_inst="",
        logical_node_name="",
        report_control_name="group-ctrl1",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id=None,
        data_set_ref=None,
        conf_rev=None,
        indexed=None,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(Iec61850DataSetMember(reference="IED-ACTRL1/XCBR1.Pos.stVal[ST]", fc="ST"),),
    )
    session = selection.adapter.connect(
        session_id="run-index:mms:IED-A/P1@10.10.10.250:12447",
        endpoint=endpoint,
        candidates=(fallback_candidate,),
    )

    result = session.read_report_control(to_report_control_ref(fallback_candidate))

    assert result.candidate_id == first_candidate.id


def test_mms_runtime_adapter_uses_functional_constraint_when_domain_match_is_sparse() -> None:
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED-A",
            access_point_name="P1",
            host="10.10.10.250",
            port=12447,
        ),
    ))
    mx_candidate = _discovery_candidate(
        candidate_id="IED-ACTRL1:RSYN1$RP$urcbMx01",
        report_control_name="urcbMx",
        data_set_ref="IED-ACTRL1/RSYN1.RSYN1URptMxDs",
        signal_reference="RSYN1.Hz[MX]",
    )
    st_candidate = _discovery_candidate(
        candidate_id="IED-ACTRL1:LLN0$BR$brcbST01",
        report_control_name="brcbST",
        data_set_ref="IED-ACTRL1/LLN0.LLN0BRptStDs",
        signal_reference="XCBR1.Pos[ST]",
    )

    class _SparseFcDiscoveryClientControlService(_FakeClientControlService):
        def __init__(self, **kwargs):
            super().__init__(**kwargs)
            self.candidate = mx_candidate
            self.available_candidates = (mx_candidate, st_candidate)

        def discover_ied(self):
            self.calls.append("discover")
            return SimpleNamespace(
                candidate=self.candidate,
                last_discovery={
                    "dataSets": [
                        {"reference": "IED-ACTRL1/RSYN1.RSYN1URptMxDs", "members": []},
                        {"reference": "IED-ACTRL1/LLN0.LLN0BRptStDs", "members": []},
                    ],
                    "reportControls": [
                        {"id": mx_candidate.id, "domain": "IED-ACTRL1", "item": "RSYN1$RP$urcbMx01", "dataSetRef": "IED-ACTRL1/RSYN1.RSYN1URptMxDs"},
                        {"id": st_candidate.id, "domain": "IED-ACTRL1", "item": "LLN0$BR$brcbST01", "dataSetRef": "IED-ACTRL1/LLN0.LLN0BRptStDs"},
                    ],
                },
                ui_state={
                    "discovery": {
                        "available_report_controls": [
                            {"report_control_id": mx_candidate.id, "data_set_ref": "IED-ACTRL1/RSYN1.RSYN1URptMxDs"},
                            {"report_control_id": st_candidate.id, "data_set_ref": "IED-ACTRL1/LLN0.LLN0BRptStDs"},
                        ]
                    }
                },
            )

        def select_report_control(self, selected_rcb_ref: str):
            super().select_report_control(selected_rcb_ref)
            return SimpleNamespace(candidate=self.candidate)

    selection = resolve_verification_runtime(
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_catalog=catalog,
        mms_control_service_factory=_SparseFcDiscoveryClientControlService,
    )
    endpoint = selection.endpoint_for_device(SimpleNamespace(ied_name="IED-A", access_point_name="P1"))
    fallback_candidate = Iec61850ReportControlCandidate(
        id="group-ctrl1-st",
        ied_name="IED-A",
        access_point_name="P1",
        logical_device_inst="",
        logical_node_name="",
        report_control_name="group-ctrl1-st",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id=None,
        data_set_ref=None,
        conf_rev=None,
        indexed=None,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(Iec61850DataSetMember(reference="IED-ACTRL1/CBCSWI1/Pos/stVal[ST]", fc="ST"),),
    )
    session = selection.adapter.connect(
        session_id="run-fc:mms:IED-A/P1@10.10.10.250:12447",
        endpoint=endpoint,
        candidates=(fallback_candidate,),
    )

    result = session.read_report_control(to_report_control_ref(fallback_candidate))

    assert result.candidate_id == st_candidate.id


def test_mms_runtime_adapter_deduplicates_duplicate_live_dataset_enable() -> None:
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED-A",
            access_point_name="P1",
            host="10.10.10.250",
            port=12447,
        ),
    ))
    matching_candidate = _discovery_candidate(
        candidate_id="IED-ACTRL:LLN0$BR$brA01",
        report_control_name="brA",
        data_set_ref="IED-ACTRL/LLN0.RCB1",
        signal_reference="XCBR1.Pos[ST]",
    )
    created_services: list[_FakeClientControlService] = []

    class _DuplicateDiscoveryClientControlService(_FakeClientControlService):
        def __init__(self, **kwargs):
            super().__init__(**kwargs)
            self.candidate = matching_candidate
            self.available_candidates = (matching_candidate,)

        def discover_ied(self):
            self.calls.append("discover")
            return SimpleNamespace(
                candidate=self.candidate,
                last_discovery={
                    "dataSets": [
                        {
                            "reference": "IED-ACTRL/LLN0.RCB1",
                            "members": [{"reference": "XCBR1.Pos[ST]", "mmsReference": "IED-ACTRL/XCBR1$ST$Pos"}],
                        },
                    ],
                    "reportControls": [{"id": matching_candidate.id, "dataSetRef": "IED-ACTRL/LLN0.RCB1"}],
                },
                ui_state={
                    "discovery": {
                        "available_report_controls": [
                            {"report_control_id": matching_candidate.id, "data_set_ref": "IED-ACTRL/LLN0.RCB1"},
                        ]
                    }
                },
            )

        def select_report_control(self, selected_rcb_ref: str):
            super().select_report_control(selected_rcb_ref)
            return SimpleNamespace(candidate=self.candidate)

    def _factory(**kwargs):
        service = _DuplicateDiscoveryClientControlService(**kwargs)
        created_services.append(service)
        return service

    selection = resolve_verification_runtime(
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_catalog=catalog,
        mms_control_service_factory=_factory,
    )
    endpoint = selection.endpoint_for_device(SimpleNamespace(ied_name="IED-A", access_point_name="P1"))
    fallback_a = Iec61850ReportControlCandidate(
        id="group-a",
        ied_name="IED-A",
        access_point_name="P1",
        logical_device_inst="",
        logical_node_name="",
        report_control_name="group-a",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id=None,
        data_set_ref=None,
        conf_rev=None,
        indexed=None,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(Iec61850DataSetMember(reference="IED-ACTRL/XCBR1.Pos.stVal[ST]", fc="ST"),),
    )
    fallback_b = Iec61850ReportControlCandidate(
        id="group-b",
        ied_name="IED-A",
        access_point_name="P1",
        logical_device_inst="",
        logical_node_name="",
        report_control_name="group-b",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id=None,
        data_set_ref=None,
        conf_rev=None,
        indexed=None,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(Iec61850DataSetMember(reference="IED-ACTRL/XCBR1.Pos.q[ST]", fc="ST"),),
    )
    session = selection.adapter.connect(
        session_id="run-dedupe:mms:IED-A/P1@10.10.10.250:12447",
        endpoint=endpoint,
        candidates=(fallback_a, fallback_b),
    )

    ref_a = to_report_control_ref(fallback_a)
    ref_b = to_report_control_ref(fallback_b)
    session.read_report_control(ref_a)
    session.enable_report_control(ref_a, "unitlab")
    session.read_report_control(ref_b)
    session.enable_report_control(ref_b, "unitlab")

    assert len(created_services) == 2
    assert created_services[0].calls == [
        "discover",
        f"select:{matching_candidate.id}",
        "select:group-b",
        "discover",
        f"select:{matching_candidate.id}",
    ]
    assert created_services[1].calls == [
        "discover",
        f"select:{matching_candidate.id}",
        "enable",
    ]


def test_mms_runtime_adapter_does_not_select_arbitrary_discovered_rcb_when_signal_list_address_is_unmatched() -> None:
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED-A",
            access_point_name="P1",
            host="10.10.10.250",
            port=12447,
        ),
    ))
    wrong_candidate = _discovery_candidate(
        candidate_id="IED-ACTRL:LLN0$BR$brWrong01",
        report_control_name="brWrong",
        data_set_ref="IED-ACTRL/LLN0.RCB1",
        signal_reference="XCBR1.Pos[ST]",
    )
    created_services: list[_FakeClientControlService] = []

    class _UnmatchedDiscoveryClientControlService(_FakeClientControlService):
        def __init__(self, **kwargs):
            super().__init__(**kwargs)
            self.candidate = wrong_candidate
            self.available_candidates = (wrong_candidate,)

        def discover_ied(self):
            self.calls.append("discover")
            return SimpleNamespace(
                candidate=self.candidate,
                last_discovery={
                    "dataSets": [
                        {
                            "reference": "IED-ACTRL/LLN0.RCB1",
                            "members": [{"reference": "XCBR1.Pos[ST]", "mmsReference": "IED-ACTRL/XCBR1$ST$Pos"}],
                        },
                    ],
                    "reportControls": [
                        {"id": wrong_candidate.id, "dataSetRef": "IED-ACTRL/LLN0.RCB1"},
                    ],
                },
                ui_state={
                    "discovery": {
                        "available_report_controls": [
                            {"report_control_id": wrong_candidate.id, "data_set_ref": "IED-ACTRL/LLN0.RCB1"},
                        ]
                    }
                },
            )

        def select_report_control(self, selected_rcb_ref: str):
            super().select_report_control(selected_rcb_ref)
            return SimpleNamespace(candidate=self.candidate)

    def _factory(**kwargs):
        service = _UnmatchedDiscoveryClientControlService(**kwargs)
        created_services.append(service)
        return service

    selection = resolve_verification_runtime(
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="mms",
            policy_version="v1",
        ),
        endpoint_catalog=catalog,
        mms_control_service_factory=_factory,
    )
    endpoint = selection.endpoint_for_device(SimpleNamespace(ied_name="IED-A", access_point_name="P1"))
    fallback_candidate = Iec61850ReportControlCandidate(
        id="group-unmatched",
        ied_name="IED-A",
        access_point_name="P1",
        logical_device_inst="",
        logical_node_name="",
        report_control_name="",
        report_kind=Iec61850ReportKind.BUFFERED,
        rpt_id=None,
        data_set_ref=None,
        conf_rev=None,
        indexed=None,
        buffer_time_ms=None,
        integrity_period_ms=None,
        trigger_options=Iec61850RuntimeTriggerOptions(),
        optional_fields=Iec61850OptionalFields(),
        signals=(Iec61850DataSetMember(reference="IED-BCTRL/PTOC1.Str.stVal[ST]", fc="ST"),),
    )
    session = selection.adapter.connect(
        session_id="run-unmatched:mms:IED-A/P1@10.10.10.250:12447",
        endpoint=endpoint,
        candidates=(fallback_candidate,),
    )

    result = session.read_report_control(to_report_control_ref(fallback_candidate))

    assert result.candidate_id == fallback_candidate.id
    assert result.state.reference.report_control_name == ""
    assert any(diagnostic.code == "MMS_REPORT_CONTROL_NOT_MATCHED" for diagnostic in result.diagnostics)
    assert created_services[0].calls == ["discover"]
