from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Sequence

from .scl_parser import (
    Iec61850SclDataSetMember,
    Iec61850SclLogicalDevice,
    Iec61850SclLogicalNode,
    Iec61850SclModel,
    Iec61850SclOptionalFields,
    Iec61850SclReportControl,
    Iec61850SclTriggerOptions,
)

from .report_runtime import (
    Iec61850DataSetMember,
    Iec61850OptionalFields,
    Iec61850ReportControlCandidate,
    Iec61850ReportRuntimeError,
    Iec61850ReportSubscriptionPlan,
    Iec61850RuntimeTriggerOptions,
    report_control_key,
    to_report_control_ref,
)

IED_SIMULATOR_FIXTURE_SCHEMA = "unitlab.iec61850.ied-simulator-fixture.v1"


@dataclass(frozen=True, slots=True)
class Iec61850IedSimulatorFixtureSignal:
    data_set_index: int
    reference: str
    kind: str
    fc: str | None
    initial_value: bool | int | float | str | None


@dataclass(frozen=True, slots=True)
class Iec61850IedSimulatorFixtureDataSet:
    reference: str
    members: tuple[Iec61850IedSimulatorFixtureSignal, ...]


@dataclass(frozen=True, slots=True)
class Iec61850IedSimulatorFixtureReport:
    key: str
    logical_device_inst: str
    logical_node_name: str
    report_control_name: str
    report_kind: str
    rpt_id: str | None
    data_set_ref: str
    conf_rev: str | None
    indexed: bool | None
    buffer_time_ms: int | None
    integrity_period_ms: int | None
    trigger_options: Iec61850RuntimeTriggerOptions
    optional_fields: Iec61850OptionalFields


@dataclass(frozen=True, slots=True)
class Iec61850IedSimulatorFixtureDevice:
    ied_name: str
    access_point_name: str
    data_sets: tuple[Iec61850IedSimulatorFixtureDataSet, ...]
    reports: tuple[Iec61850IedSimulatorFixtureReport, ...]


@dataclass(frozen=True, slots=True)
class Iec61850IedSimulatorFixture:
    schema: str
    devices: tuple[Iec61850IedSimulatorFixtureDevice, ...]



def build_ied_simulator_fixture_from_scl_model(
    model: Iec61850SclModel,
    *,
    selected_ied_name: str | None = None,
) -> Iec61850IedSimulatorFixture:
    devices: list[Iec61850IedSimulatorFixtureDevice] = []

    for ied in model.ieds:
        if selected_ied_name is not None and ied.name != selected_ied_name:
            continue
        for access_point in ied.access_points:
            data_sets: list[Iec61850IedSimulatorFixtureDataSet] = []
            reports: list[Iec61850IedSimulatorFixtureReport] = []
            data_sets_by_ref: dict[str, Iec61850IedSimulatorFixtureDataSet] = {}

            for logical_device in access_point.logical_devices:
                for logical_node in logical_device.logical_nodes:
                    for data_set in logical_node.data_sets:
                        fixture_data_set = _scl_data_set_to_fixture(
                            ied_name=ied.name,
                            access_point_name=access_point.name,
                            logical_device=logical_device,
                            logical_node=logical_node,
                            members=data_set.members,
                            data_set_name=data_set.name,
                        )
                        data_sets.append(fixture_data_set)
                        data_sets_by_ref[fixture_data_set.reference] = fixture_data_set

            for logical_device in access_point.logical_devices:
                for logical_node in logical_device.logical_nodes:
                    for report in logical_node.report_controls:
                        if report.data_set is None:
                            continue
                        data_set_ref = _fixture_data_set_ref(
                            ied_name=ied.name,
                            access_point_name=access_point.name,
                            logical_device_inst=logical_device.inst,
                            logical_node_name=logical_node.name,
                            data_set_name=report.data_set,
                        )
                        if data_set_ref not in data_sets_by_ref:
                            continue
                        reports.append(_scl_report_to_fixture(
                            ied_name=ied.name,
                            access_point_name=access_point.name,
                            logical_device=logical_device,
                            logical_node=logical_node,
                            report=report,
                            data_set_ref=data_set_ref,
                        ))

            if data_sets or reports:
                devices.append(Iec61850IedSimulatorFixtureDevice(
                    ied_name=ied.name,
                    access_point_name=access_point.name,
                    data_sets=tuple(data_sets),
                    reports=tuple(reports),
                ))

    return Iec61850IedSimulatorFixture(
        schema=IED_SIMULATOR_FIXTURE_SCHEMA,
        devices=tuple(devices),
    )

def build_ied_simulator_fixture_from_subscription_plan(
    plan: Iec61850ReportSubscriptionPlan,
) -> Iec61850IedSimulatorFixture:
    devices: list[Iec61850IedSimulatorFixtureDevice] = []
    for device in plan.devices:
        reports: list[Iec61850IedSimulatorFixtureReport] = []
        data_sets_by_ref: dict[str, Iec61850IedSimulatorFixtureDataSet] = {}

        for plan_report in device.reports:
            if plan_report.status != "required":
                continue
            candidate = plan_report.candidate
            data_set_ref = _require_data_set_ref(candidate)
            reports.append(_to_fixture_report(candidate, data_set_ref))
            data_sets_by_ref.setdefault(
                data_set_ref,
                Iec61850IedSimulatorFixtureDataSet(
                    reference=data_set_ref,
                    members=_to_fixture_signals(candidate.signals),
                ),
            )

        if reports:
            devices.append(Iec61850IedSimulatorFixtureDevice(
                ied_name=device.ied_name,
                access_point_name=device.access_point_name,
                data_sets=tuple(data_sets_by_ref.values()),
                reports=tuple(reports),
            ))

    return Iec61850IedSimulatorFixture(
        schema=IED_SIMULATOR_FIXTURE_SCHEMA,
        devices=tuple(devices),
    )


def ied_simulator_fixture_to_payload(fixture: Iec61850IedSimulatorFixture) -> dict[str, Any]:
    return {
        "schema": fixture.schema,
        "devices": [
            {
                "iedName": device.ied_name,
                "accessPointName": device.access_point_name,
                "dataSets": [
                    {
                        "reference": data_set.reference,
                        "members": [
                            {
                                "dataSetIndex": signal.data_set_index,
                                "reference": signal.reference,
                                "kind": signal.kind,
                                "fc": signal.fc,
                                "initialValue": signal.initial_value,
                            }
                            for signal in data_set.members
                        ],
                    }
                    for data_set in device.data_sets
                ],
                "reports": [
                    {
                        "key": report.key,
                        "logicalDeviceInst": report.logical_device_inst,
                        "logicalNodeName": report.logical_node_name,
                        "reportControlName": report.report_control_name,
                        "reportKind": report.report_kind,
                        "rptId": report.rpt_id,
                        "dataSetRef": report.data_set_ref,
                        "confRev": report.conf_rev,
                        "indexed": report.indexed,
                        "bufferTimeMs": report.buffer_time_ms,
                        "integrityPeriodMs": report.integrity_period_ms,
                        "triggerOptions": _trigger_options_to_payload(report.trigger_options),
                        "optionalFields": _optional_fields_to_payload(report.optional_fields),
                    }
                    for report in device.reports
                ],
            }
            for device in fixture.devices
        ],
    }


def _require_data_set_ref(candidate: Iec61850ReportControlCandidate) -> str:
    if candidate.data_set_ref is None or not candidate.data_set_ref.strip():
        raise Iec61850ReportRuntimeError(
            "SIMULATOR_FIXTURE_DATASET_MISSING",
            f'ReportControl "{candidate.id}" cannot be exported to an IED simulator fixture without DatSet.',
        )
    return candidate.data_set_ref


def _to_fixture_report(
    candidate: Iec61850ReportControlCandidate,
    data_set_ref: str,
) -> Iec61850IedSimulatorFixtureReport:
    return Iec61850IedSimulatorFixtureReport(
        key=report_control_key(to_report_control_ref(candidate)),
        logical_device_inst=candidate.logical_device_inst,
        logical_node_name=candidate.logical_node_name,
        report_control_name=candidate.report_control_name,
        report_kind=candidate.report_kind.value,
        rpt_id=candidate.rpt_id,
        data_set_ref=data_set_ref,
        conf_rev=candidate.conf_rev,
        indexed=candidate.indexed,
        buffer_time_ms=candidate.buffer_time_ms,
        integrity_period_ms=candidate.integrity_period_ms,
        trigger_options=candidate.trigger_options,
        optional_fields=candidate.optional_fields,
    )


def _to_fixture_signals(
    signals: Sequence[Iec61850DataSetMember],
) -> tuple[Iec61850IedSimulatorFixtureSignal, ...]:
    return tuple(
        Iec61850IedSimulatorFixtureSignal(
            data_set_index=index,
            reference=signal.reference,
            kind=signal.kind,
            fc=signal.fc,
            initial_value=index,
        )
        for index, signal in enumerate(signals)
    )


def _trigger_options_to_payload(options: Iec61850RuntimeTriggerOptions) -> dict[str, bool | None]:
    return {
        "dataChange": options.data_change,
        "qualityChange": options.quality_change,
        "dataUpdate": options.data_update,
        "periodic": options.periodic,
        "generalInterrogation": options.general_interrogation,
    }


def _optional_fields_to_payload(fields: Iec61850OptionalFields) -> dict[str, bool | None]:
    return {
        "sequenceNumber": fields.sequence_number,
        "timestamp": fields.timestamp,
        "reasonCode": fields.reason_code,
        "dataSetName": fields.data_set_name,
        "dataReference": fields.data_reference,
        "entryId": fields.entry_id,
        "configRevision": fields.config_revision,
        "bufferOverflow": fields.buffer_overflow,
    }


def _scl_data_set_to_fixture(
    *,
    ied_name: str,
    access_point_name: str,
    logical_device: Iec61850SclLogicalDevice,
    logical_node: Iec61850SclLogicalNode,
    members: Sequence[Iec61850SclDataSetMember],
    data_set_name: str,
) -> Iec61850IedSimulatorFixtureDataSet:
    return Iec61850IedSimulatorFixtureDataSet(
        reference=_fixture_data_set_ref(
            ied_name=ied_name,
            access_point_name=access_point_name,
            logical_device_inst=logical_device.inst,
            logical_node_name=logical_node.name,
            data_set_name=data_set_name,
        ),
        members=tuple(
            Iec61850IedSimulatorFixtureSignal(
                data_set_index=index,
                reference=_fixture_signal_reference(member, logical_device.inst),
                kind=member.kind,
                fc=member.fc,
                initial_value=index,
            )
            for index, member in enumerate(members)
        ),
    )


def _scl_report_to_fixture(
    *,
    ied_name: str,
    access_point_name: str,
    logical_device: Iec61850SclLogicalDevice,
    logical_node: Iec61850SclLogicalNode,
    report: Iec61850SclReportControl,
    data_set_ref: str,
) -> Iec61850IedSimulatorFixtureReport:
    report_kind = report.report_kind if report.report_kind in {"buffered", "unbuffered"} else "buffered"
    return Iec61850IedSimulatorFixtureReport(
        key=f"{ied_name}/{access_point_name}/{logical_device.inst}/{logical_node.name}/{report.name}/{report_kind}",
        logical_device_inst=logical_device.inst,
        logical_node_name=logical_node.name,
        report_control_name=report.name,
        report_kind=report_kind,
        rpt_id=report.rpt_id,
        data_set_ref=data_set_ref,
        conf_rev=str(report.conf_rev) if report.conf_rev is not None else None,
        indexed=report.indexed,
        buffer_time_ms=report.buffer_time_ms,
        integrity_period_ms=report.integrity_period_ms,
        trigger_options=_scl_trigger_options_to_runtime(report.trigger_options),
        optional_fields=_scl_optional_fields_to_runtime(report.optional_fields),
    )


def _fixture_data_set_ref(
    *,
    ied_name: str,
    access_point_name: str,
    logical_device_inst: str,
    logical_node_name: str,
    data_set_name: str,
) -> str:
    return f"{ied_name}/{access_point_name}/{logical_device_inst}/{logical_node_name}.{data_set_name}"


def _fixture_signal_reference(member: Iec61850SclDataSetMember, fallback_logical_device_inst: str) -> str:
    logical_device_inst = member.ld_inst or fallback_logical_device_inst
    logical_node_name = f"{member.prefix or ''}{member.ln_class or ''}{member.ln_inst or ''}"
    object_parts = [part for part in (member.do_name, member.da_name) if part]
    object_path = ".".join(object_parts)
    reference = f"{logical_device_inst}/{logical_node_name}"
    if object_path:
        reference = f"{reference}.{object_path}"
    if member.fc:
        reference = f"{reference}[{member.fc}]"
    return reference


def _scl_trigger_options_to_runtime(options: Iec61850SclTriggerOptions) -> Iec61850RuntimeTriggerOptions:
    return Iec61850RuntimeTriggerOptions(
        data_change=options.data_change,
        quality_change=options.quality_change,
        data_update=options.data_update,
        periodic=options.integrity,
        general_interrogation=options.general_interrogation,
    )


def _scl_optional_fields_to_runtime(fields: Iec61850SclOptionalFields) -> Iec61850OptionalFields:
    return Iec61850OptionalFields(
        sequence_number=fields.sequence_number,
        timestamp=fields.timestamp,
        reason_code=fields.reason_code,
        data_set_name=fields.data_set_name,
        data_reference=fields.data_reference,
        entry_id=fields.entry_id,
        config_revision=fields.config_revision,
        buffer_overflow=fields.buffer_overflow,
    )
