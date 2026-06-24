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
    def __init__(self, *, session_id: str, client_id: str, endpoint: Iec61850DeviceEndpoint, candidate: Iec61850ReportControlCandidate) -> None:
        self.session_id = session_id
        self.client_id = client_id
        self.endpoint = endpoint
        self.candidate = candidate
        self.calls: list[str] = []
        self._report = Iec61850ReportEvent(
            id=f"{session_id}:report-1",
            endpoint_id=endpoint.id,
            received_at="2026-06-24T10:11:13.070000Z",
            report_control=to_report_control_ref(candidate),
            rpt_id=candidate.rpt_id,
            data_set_ref=candidate.data_set_ref,
            conf_rev=candidate.conf_rev,
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
        return SimpleNamespace(last_report=self._report)

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


def test_mms_runtime_adapter_supports_multiple_candidates_on_one_session() -> None:
    catalog = build_mms_endpoint_catalog((
        Iec61850MmsEndpointCatalogEntry(
            ied_name="IED-A",
            access_point_name="P1",
            host="10.10.10.250",
            port=12447,
        ),
    ))
    created_services: dict[str, _FakeClientControlService] = {}

    def _factory(**kwargs):
        service = _FakeClientControlService(**kwargs)
        created_services[kwargs["candidate"].report_control_name] = service
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
    assert set(created_services) == {"brA", "brB"}
    assert created_services["brA"].calls == ["discover", "enable", "gi", "snapshot", "close"]
    assert created_services["brB"].calls == ["discover", "enable", "gi", "snapshot", "close"]
