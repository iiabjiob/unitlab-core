from __future__ import annotations

from dataclasses import dataclass
from typing import Any
from xml.etree import ElementTree

SCL_NORMALIZED_SCHEMA = "unitlab.iec61850.scl.normalized.v1"


@dataclass(frozen=True, slots=True)
class Iec61850SclDiagnostic:
    severity: str
    stage: str
    code: str
    message: str
    source_path: str | None = None
    context: dict[str, str | int | bool | None] | None = None


@dataclass(frozen=True, slots=True)
class Iec61850SclAddressParameter:
    type: str
    value: str


@dataclass(frozen=True, slots=True)
class Iec61850SclConnectedAccessPoint:
    ied_name: str
    access_point_name: str
    subnetwork_name: str | None
    subnetwork_type: str | None
    address: tuple[Iec61850SclAddressParameter, ...]


@dataclass(frozen=True, slots=True)
class Iec61850SclDataSetMember:
    kind: str
    reference: str
    ld_inst: str | None
    prefix: str | None
    ln_class: str | None
    ln_inst: str | None
    do_name: str | None
    da_name: str | None
    fc: str | None


@dataclass(frozen=True, slots=True)
class Iec61850SclDataSet:
    name: str
    reference: str
    members: tuple[Iec61850SclDataSetMember, ...]


@dataclass(frozen=True, slots=True)
class Iec61850SclTriggerOptions:
    data_change: bool
    quality_change: bool
    data_update: bool
    integrity: bool
    general_interrogation: bool


@dataclass(frozen=True, slots=True)
class Iec61850SclOptionalFields:
    sequence_number: bool
    timestamp: bool
    reason_code: bool
    data_set_name: bool
    data_reference: bool
    buffer_overflow: bool
    entry_id: bool
    config_revision: bool


@dataclass(frozen=True, slots=True)
class Iec61850SclReportControl:
    name: str
    reference: str
    report_kind: str
    rpt_id: str | None
    data_set: str | None
    data_set_ref: str | None
    conf_rev: int | None
    indexed: bool | None
    buffer_time_ms: int | None
    integrity_period_ms: int | None
    trigger_options: Iec61850SclTriggerOptions
    optional_fields: Iec61850SclOptionalFields


@dataclass(frozen=True, slots=True)
class Iec61850SclLogicalNode:
    name: str
    ln_class: str
    ln_inst: str | None
    prefix: str | None
    ln_type: str | None
    data_sets: tuple[Iec61850SclDataSet, ...]
    report_controls: tuple[Iec61850SclReportControl, ...]


@dataclass(frozen=True, slots=True)
class Iec61850SclLogicalDevice:
    inst: str
    logical_nodes: tuple[Iec61850SclLogicalNode, ...]


@dataclass(frozen=True, slots=True)
class Iec61850SclAccessPoint:
    name: str
    logical_devices: tuple[Iec61850SclLogicalDevice, ...]
    connected_access_points: tuple[Iec61850SclConnectedAccessPoint, ...]


@dataclass(frozen=True, slots=True)
class Iec61850SclIed:
    name: str
    manufacturer: str | None
    type: str | None
    config_version: str | None
    access_points: tuple[Iec61850SclAccessPoint, ...]


@dataclass(frozen=True, slots=True)
class Iec61850SclModel:
    schema: str
    source_file_name: str
    source_content_hash: str
    scl_version: str | None
    scl_revision: str | None
    ieds: tuple[Iec61850SclIed, ...]
    diagnostics: tuple[Iec61850SclDiagnostic, ...]


def parse_scl_source(*, file_name: str, content_hash: str, xml_text: str) -> Iec61850SclModel:
    diagnostics: list[Iec61850SclDiagnostic] = []
    if not xml_text.strip():
        diagnostics.append(Iec61850SclDiagnostic(
            severity="error",
            stage="xml",
            code="SCL_EMPTY_SOURCE",
            message="SCL source is empty.",
        ))
        return _model(file_name, content_hash, None, None, (), diagnostics)

    try:
        root = ElementTree.fromstring(xml_text)
    except ElementTree.ParseError as exc:
        diagnostics.append(Iec61850SclDiagnostic(
            severity="error",
            stage="xml",
            code="SCL_XML_PARSE_FAILED",
            message=str(exc),
        ))
        return _model(file_name, content_hash, None, None, (), diagnostics)

    ieds = _parse_ieds(root, diagnostics)
    connected_access_points = _parse_connected_access_points(root)
    ieds = tuple(_attach_connected_access_points(ied, connected_access_points) for ied in ieds)
    if not ieds:
        diagnostics.append(Iec61850SclDiagnostic(
            severity="error",
            stage="parser",
            code="SCL_NO_IED",
            message="No IED section was found in the SCL file.",
        ))

    return _model(
        file_name,
        content_hash,
        root.attrib.get("version"),
        root.attrib.get("revision"),
        ieds,
        diagnostics,
    )


def scl_model_to_payload(model: Iec61850SclModel) -> dict[str, Any]:
    return {
        "schema": model.schema,
        "source": {
            "fileName": model.source_file_name,
            "contentHash": model.source_content_hash,
        },
        "scl": {
            "version": model.scl_version,
            "revision": model.scl_revision,
        },
        "ieds": [
            {
                "name": ied.name,
                "manufacturer": ied.manufacturer,
                "type": ied.type,
                "configVersion": ied.config_version,
                "accessPoints": [
                    {
                        "name": access_point.name,
                        "connectedAccessPoints": [
                            {
                                "iedName": connected.ied_name,
                                "accessPointName": connected.access_point_name,
                                "subnetworkName": connected.subnetwork_name,
                                "subnetworkType": connected.subnetwork_type,
                                "address": [
                                    {"type": parameter.type, "value": parameter.value}
                                    for parameter in connected.address
                                ],
                            }
                            for connected in access_point.connected_access_points
                        ],
                        "logicalDevices": [
                            {
                                "inst": logical_device.inst,
                                "logicalNodes": [
                                    {
                                        "name": logical_node.name,
                                        "lnClass": logical_node.ln_class,
                                        "lnInst": logical_node.ln_inst,
                                        "prefix": logical_node.prefix,
                                        "lnType": logical_node.ln_type,
                                        "dataSets": [
                                            {
                                                "name": data_set.name,
                                                "reference": data_set.reference,
                                                "members": [
                                                    {
                                                        "kind": member.kind,
                                                        "reference": member.reference,
                                                        "ldInst": member.ld_inst,
                                                        "prefix": member.prefix,
                                                        "lnClass": member.ln_class,
                                                        "lnInst": member.ln_inst,
                                                        "doName": member.do_name,
                                                        "daName": member.da_name,
                                                        "fc": member.fc,
                                                    }
                                                    for member in data_set.members
                                                ],
                                            }
                                            for data_set in logical_node.data_sets
                                        ],
                                        "reportControls": [
                                            {
                                                "name": report.name,
                                                "reference": report.reference,
                                                "reportKind": report.report_kind,
                                                "rptId": report.rpt_id,
                                                "dataSet": report.data_set,
                                                "dataSetRef": report.data_set_ref,
                                                "confRev": report.conf_rev,
                                                "indexed": report.indexed,
                                                "bufferTimeMs": report.buffer_time_ms,
                                                "integrityPeriodMs": report.integrity_period_ms,
                                                "triggerOptions": {
                                                    "dataChange": report.trigger_options.data_change,
                                                    "qualityChange": report.trigger_options.quality_change,
                                                    "dataUpdate": report.trigger_options.data_update,
                                                    "integrity": report.trigger_options.integrity,
                                                    "generalInterrogation": report.trigger_options.general_interrogation,
                                                },
                                                "optionalFields": {
                                                    "sequenceNumber": report.optional_fields.sequence_number,
                                                    "timestamp": report.optional_fields.timestamp,
                                                    "reasonCode": report.optional_fields.reason_code,
                                                    "dataSetName": report.optional_fields.data_set_name,
                                                    "dataReference": report.optional_fields.data_reference,
                                                    "bufferOverflow": report.optional_fields.buffer_overflow,
                                                    "entryId": report.optional_fields.entry_id,
                                                    "configRevision": report.optional_fields.config_revision,
                                                },
                                            }
                                            for report in logical_node.report_controls
                                        ],
                                    }
                                    for logical_node in logical_device.logical_nodes
                                ],
                            }
                            for logical_device in access_point.logical_devices
                        ],
                    }
                    for access_point in ied.access_points
                ],
            }
            for ied in model.ieds
        ],
        "diagnostics": [
            {
                "severity": diagnostic.severity,
                "stage": diagnostic.stage,
                "code": diagnostic.code,
                "message": diagnostic.message,
                "sourcePath": diagnostic.source_path,
                "context": diagnostic.context,
            }
            for diagnostic in model.diagnostics
        ],
    }


def _model(
    file_name: str,
    content_hash: str,
    scl_version: str | None,
    scl_revision: str | None,
    ieds: tuple[Iec61850SclIed, ...],
    diagnostics: list[Iec61850SclDiagnostic],
) -> Iec61850SclModel:
    return Iec61850SclModel(
        schema=SCL_NORMALIZED_SCHEMA,
        source_file_name=file_name,
        source_content_hash=content_hash,
        scl_version=scl_version,
        scl_revision=scl_revision,
        ieds=ieds,
        diagnostics=tuple(diagnostics),
    )


def _parse_ieds(root: ElementTree.Element, diagnostics: list[Iec61850SclDiagnostic]) -> tuple[Iec61850SclIed, ...]:
    return tuple(_parse_ied(element, diagnostics) for element in _children(root, "IED"))


def _parse_ied(element: ElementTree.Element, diagnostics: list[Iec61850SclDiagnostic]) -> Iec61850SclIed:
    return Iec61850SclIed(
        name=element.attrib.get("name", ""),
        manufacturer=element.attrib.get("manufacturer"),
        type=element.attrib.get("type"),
        config_version=element.attrib.get("configVersion"),
        access_points=tuple(_parse_access_point(child, diagnostics) for child in _children(element, "AccessPoint")),
    )


def _parse_access_point(element: ElementTree.Element, diagnostics: list[Iec61850SclDiagnostic]) -> Iec61850SclAccessPoint:
    server = _first_child(element, "Server")
    return Iec61850SclAccessPoint(
        name=element.attrib.get("name", ""),
        logical_devices=_parse_logical_devices(server, diagnostics) if server is not None else (),
        connected_access_points=(),
    )


def _parse_logical_devices(server: ElementTree.Element, diagnostics: list[Iec61850SclDiagnostic]) -> tuple[Iec61850SclLogicalDevice, ...]:
    return tuple(_parse_logical_device(element, diagnostics) for element in _children(server, "LDevice"))


def _parse_logical_device(element: ElementTree.Element, diagnostics: list[Iec61850SclDiagnostic]) -> Iec61850SclLogicalDevice:
    return Iec61850SclLogicalDevice(
        inst=element.attrib.get("inst", ""),
        logical_nodes=tuple(
            _parse_logical_node(child, element.attrib.get("inst", ""), diagnostics)
            for child in element
            if _local_name(child.tag) in {"LN0", "LN"}
        ),
    )


def _parse_logical_node(
    element: ElementTree.Element,
    logical_device_inst: str,
    diagnostics: list[Iec61850SclDiagnostic],
) -> Iec61850SclLogicalNode:
    tag_name = _local_name(element.tag)
    ln_class = "LLN0" if tag_name == "LN0" else element.attrib.get("lnClass", "")
    ln_inst = None if tag_name == "LN0" else element.attrib.get("inst")
    prefix = element.attrib.get("prefix")
    name = _format_logical_node_name(prefix, ln_class, ln_inst)
    data_sets = tuple(_parse_data_set(child, logical_device_inst, name) for child in _children(element, "DataSet"))
    return Iec61850SclLogicalNode(
        name=name,
        ln_class=ln_class,
        ln_inst=ln_inst,
        prefix=prefix,
        ln_type=element.attrib.get("lnType"),
        data_sets=data_sets,
        report_controls=tuple(
            _parse_report_control(child, logical_device_inst, name, diagnostics)
            for child in _children(element, "ReportControl")
        ),
    )


def _parse_data_set(element: ElementTree.Element, logical_device_inst: str, logical_node_name: str) -> Iec61850SclDataSet:
    name = element.attrib.get("name", "")
    return Iec61850SclDataSet(
        name=name,
        reference=f"{logical_device_inst}/{logical_node_name}${name}",
        members=tuple(
            _parse_data_set_member(child, logical_device_inst)
            for child in element
            if _local_name(child.tag) in {"FCDA", "FCD"}
        ),
    )


def _parse_data_set_member(element: ElementTree.Element, fallback_logical_device_inst: str) -> Iec61850SclDataSetMember:
    kind = _local_name(element.tag)
    ld_inst = element.attrib.get("ldInst", fallback_logical_device_inst)
    prefix = element.attrib.get("prefix")
    ln_class = element.attrib.get("lnClass")
    ln_inst = element.attrib.get("lnInst")
    do_name = element.attrib.get("doName")
    da_name = element.attrib.get("daName")
    fc = element.attrib.get("fc")
    logical_node_name = _format_logical_node_name(prefix, ln_class or "", ln_inst)
    base = f"{ld_inst}/{logical_node_name}"
    if fc:
        base = f"{base}${fc}"
    if do_name:
        base = f"{base}${do_name}"
    reference = f"{base}${da_name}" if da_name else base
    return Iec61850SclDataSetMember(
        kind=kind,
        reference=reference,
        ld_inst=ld_inst,
        prefix=prefix,
        ln_class=ln_class,
        ln_inst=ln_inst,
        do_name=do_name,
        da_name=da_name,
        fc=fc,
    )


def _parse_report_control(
    element: ElementTree.Element,
    logical_device_inst: str,
    logical_node_name: str,
    diagnostics: list[Iec61850SclDiagnostic],
) -> Iec61850SclReportControl:
    name = element.attrib.get("name", "")
    data_set = element.attrib.get("datSet")
    buffered = _parse_bool(element.attrib.get("buffered")) or False
    return Iec61850SclReportControl(
        name=name,
        reference=f"{logical_device_inst}/{logical_node_name}.BR.{name}",
        report_kind="buffered" if buffered else "unbuffered",
        rpt_id=element.attrib.get("rptID"),
        data_set=data_set,
        data_set_ref=f"{logical_device_inst}/{logical_node_name}${data_set}" if data_set else None,
        conf_rev=_parse_int(element.attrib.get("confRev")),
        indexed=_parse_bool(element.attrib.get("indexed")),
        buffer_time_ms=_parse_int(element.attrib.get("bufTime")),
        integrity_period_ms=_parse_int(element.attrib.get("intgPd")),
        trigger_options=_parse_trigger_options(_first_child(element, "TrgOps")),
        optional_fields=_parse_optional_fields(_first_child(element, "OptFields")),
    )


def _parse_trigger_options(element: ElementTree.Element | None) -> Iec61850SclTriggerOptions:
    return Iec61850SclTriggerOptions(
        data_change=_parse_bool(_attr(element, "dchg")) or False,
        quality_change=_parse_bool(_attr(element, "qchg")) or False,
        data_update=_parse_bool(_attr(element, "dupd")) or False,
        integrity=_parse_bool(_attr(element, "period")) or False,
        general_interrogation=_parse_bool(_attr(element, "gi")) or False,
    )


def _parse_optional_fields(element: ElementTree.Element | None) -> Iec61850SclOptionalFields:
    return Iec61850SclOptionalFields(
        sequence_number=_parse_bool(_attr(element, "seqNum")) or False,
        timestamp=_parse_bool(_attr(element, "timeStamp")) or False,
        reason_code=_parse_bool(_attr(element, "reasonCode")) or False,
        data_set_name=_parse_bool(_attr(element, "dataSet")) or False,
        data_reference=_parse_bool(_attr(element, "dataRef")) or False,
        buffer_overflow=_parse_bool(_attr(element, "bufOvfl")) or False,
        entry_id=_parse_bool(_attr(element, "entryID")) or False,
        config_revision=_parse_bool(_attr(element, "configRef")) or False,
    )


def _parse_connected_access_points(root: ElementTree.Element) -> tuple[Iec61850SclConnectedAccessPoint, ...]:
    connected_access_points: list[Iec61850SclConnectedAccessPoint] = []
    for subnetwork in _iter_descendants(root, "SubNetwork"):
        subnetwork_name = subnetwork.attrib.get("name")
        subnetwork_type = subnetwork.attrib.get("type")
        for connected_ap in _children(subnetwork, "ConnectedAP"):
            address = _first_child(connected_ap, "Address")
            connected_access_points.append(Iec61850SclConnectedAccessPoint(
                ied_name=connected_ap.attrib.get("iedName", ""),
                access_point_name=connected_ap.attrib.get("apName", ""),
                subnetwork_name=subnetwork_name,
                subnetwork_type=subnetwork_type,
                address=tuple(
                    Iec61850SclAddressParameter(type=parameter.attrib.get("type", ""), value=(parameter.text or "").strip())
                    for parameter in (_children(address, "P") if address is not None else ())
                ),
            ))
    return tuple(connected_access_points)


def _attach_connected_access_points(
    ied: Iec61850SclIed,
    connected_access_points: tuple[Iec61850SclConnectedAccessPoint, ...],
) -> Iec61850SclIed:
    return Iec61850SclIed(
        name=ied.name,
        manufacturer=ied.manufacturer,
        type=ied.type,
        config_version=ied.config_version,
        access_points=tuple(
            Iec61850SclAccessPoint(
                name=access_point.name,
                logical_devices=access_point.logical_devices,
                connected_access_points=tuple(
                    connected
                    for connected in connected_access_points
                    if connected.ied_name == ied.name and connected.access_point_name == access_point.name
                ),
            )
            for access_point in ied.access_points
        ),
    )


def _children(element: ElementTree.Element | None, local_name: str) -> tuple[ElementTree.Element, ...]:
    if element is None:
        return ()
    return tuple(child for child in element if _local_name(child.tag) == local_name)


def _iter_descendants(element: ElementTree.Element, local_name: str) -> tuple[ElementTree.Element, ...]:
    return tuple(child for child in element.iter() if _local_name(child.tag) == local_name)


def _first_child(element: ElementTree.Element | None, local_name: str) -> ElementTree.Element | None:
    children = _children(element, local_name)
    return children[0] if children else None


def _attr(element: ElementTree.Element | None, name: str) -> str | None:
    return element.attrib.get(name) if element is not None else None


def _local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1] if "}" in tag else tag.rsplit(":", 1)[-1]


def _format_logical_node_name(prefix: str | None, ln_class: str, ln_inst: str | None) -> str:
    return f"{prefix or ''}{ln_class}{ln_inst or ''}"


def _parse_bool(value: str | None) -> bool | None:
    if value is None or value == "":
        return None
    if value in {"true", "1"}:
        return True
    if value in {"false", "0"}:
        return False
    return None


def _parse_int(value: str | None) -> int | None:
    if value is None or value.strip() == "":
        return None
    try:
        return int(value, 10)
    except ValueError:
        return None
