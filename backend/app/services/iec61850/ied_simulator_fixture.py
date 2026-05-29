from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Sequence

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
