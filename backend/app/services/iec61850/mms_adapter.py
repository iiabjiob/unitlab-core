from __future__ import annotations

import json
from dataclasses import dataclass
from xml.etree import ElementTree as ET
from collections.abc import Sequence
from typing import cast

from .report_runtime import (
    Iec61850DeviceEndpoint,
    Iec61850ReportControlCandidate,
    Iec61850ReportRuntimeError,
    Iec61850ReportSession,
    Iec61850ReportSubscriptionPlanDevice,
    Iec61850RuntimeMode,
)


@dataclass(frozen=True, slots=True)
class Iec61850MmsEndpointCatalogEntry:
    ied_name: str
    access_point_name: str
    host: str
    port: int = 102
    endpoint_id: str | None = None


class Iec61850MmsEndpointCatalog:
    def __init__(self, entries: Sequence[Iec61850MmsEndpointCatalogEntry]) -> None:
        self._entries: dict[tuple[str, str], Iec61850MmsEndpointCatalogEntry] = {}
        self._entries_by_endpoint_id: dict[str, Iec61850MmsEndpointCatalogEntry] = {}
        for entry in entries:
            if not entry.host.strip():
                raise Iec61850ReportRuntimeError(
                    "INVALID_MMS_ENDPOINT",
                    f'MMS endpoint for "{entry.ied_name}/{entry.access_point_name}" has an empty host.',
                )
            if entry.port <= 0 or entry.port > 65535:
                raise Iec61850ReportRuntimeError(
                    "INVALID_MMS_ENDPOINT",
                    f'MMS endpoint for "{entry.ied_name}/{entry.access_point_name}" has invalid port {entry.port}.',
                )
            endpoint_id = entry.endpoint_id.strip() if entry.endpoint_id is not None else ""
            if endpoint_id:
                if endpoint_id in self._entries_by_endpoint_id:
                    raise Iec61850ReportRuntimeError(
                        "DUPLICATE_MMS_ENDPOINT",
                        f'MMS endpoint "{endpoint_id}" is configured more than once.',
                    )
                self._entries_by_endpoint_id[endpoint_id] = entry
            key = _endpoint_key(entry.ied_name, entry.access_point_name)
            if key in self._entries:
                if endpoint_id and not entry.ied_name.strip():
                    continue
                raise Iec61850ReportRuntimeError(
                    "DUPLICATE_MMS_ENDPOINT",
                    f'MMS endpoint for "{entry.ied_name}/{entry.access_point_name}" is configured more than once.',
                )
            self._entries[key] = entry

    def endpoint_for_plan_device(self, device: Iec61850ReportSubscriptionPlanDevice) -> Iec61850DeviceEndpoint:
        device_endpoint_id = getattr(device, "endpoint_id", None)
        endpoint_id = device_endpoint_id.strip() if isinstance(device_endpoint_id, str) else ""
        if endpoint_id:
            entry = self._entries_by_endpoint_id.get(endpoint_id)
            if entry is not None:
                return _endpoint_from_catalog_entry(entry, requested_host=None, requested_port=None)
        endpoint, _ = self.resolve_transport_endpoint(
            ied_name=device.ied_name,
            access_point_name=device.access_point_name,
            requested_host=None,
            requested_port=None,
        )
        return endpoint

    def resolve_transport_endpoint(
        self,
        *,
        ied_name: str,
        access_point_name: str,
        requested_host: str | None,
        requested_port: int | None,
    ) -> tuple[Iec61850DeviceEndpoint, tuple[str, ...]]:
        entry = self._entries.get(_endpoint_key(ied_name, access_point_name))
        requested_host = requested_host.strip() if requested_host is not None else None
        notes: list[str] = []
        if entry is None:
            if not requested_host:
                raise Iec61850ReportRuntimeError(
                    "MMS_ENDPOINT_NOT_CONFIGURED",
                    f'MMS endpoint for "{ied_name}/{access_point_name}" is not configured.',
                )
            if requested_port is None or requested_port <= 0 or requested_port > 65535:
                raise Iec61850ReportRuntimeError(
                    "MMS_ENDPOINT_PORT_REQUIRED",
                    f'MMS endpoint for "{ied_name}/{access_point_name}" requires a valid port.',
                )
            endpoint = Iec61850DeviceEndpoint(
                id=f"mms:{ied_name}@{requested_host}:{requested_port}" if ied_name else f"mms:{requested_host}:{requested_port}",
                mode=Iec61850RuntimeMode.MMS,
                ied_name=ied_name,
                access_point_name=access_point_name,
                host=requested_host,
                port=requested_port,
            )
            return endpoint, ("explicit transport request",)

        resolved_host = requested_host or entry.host
        resolved_port = requested_port if requested_port is not None and requested_port > 0 else entry.port
        if requested_host and requested_host != entry.host:
            notes.append("requested host overrides catalog host")
        if requested_port is not None and requested_port != entry.port:
            notes.append("requested port overrides catalog port")
        if not requested_host:
            notes.append("transport host resolved from endpoint catalog")
        endpoint = _endpoint_from_catalog_entry(entry, requested_host=resolved_host, requested_port=resolved_port)
        return endpoint, tuple(notes)


def build_mms_endpoint_catalog(
    entries: Sequence[Iec61850MmsEndpointCatalogEntry],
) -> Iec61850MmsEndpointCatalog:
    return Iec61850MmsEndpointCatalog(entries)


def _endpoint_from_catalog_entry(
    entry: Iec61850MmsEndpointCatalogEntry,
    *,
    requested_host: str | None,
    requested_port: int | None,
) -> Iec61850DeviceEndpoint:
    resolved_host = requested_host or entry.host
    resolved_port = requested_port if requested_port is not None and requested_port > 0 else entry.port
    endpoint_id = (
        entry.endpoint_id
        if entry.endpoint_id and not entry.ied_name.strip()
        else f"mms:{entry.ied_name}/{entry.access_point_name}@{resolved_host}:{resolved_port}"
    )
    return Iec61850DeviceEndpoint(
        id=endpoint_id,
        mode=Iec61850RuntimeMode.MMS,
        ied_name=entry.ied_name,
        access_point_name=entry.access_point_name,
        host=resolved_host,
        port=resolved_port,
    )


def build_mms_endpoint_catalog_from_scd_source(
    source: bytes,
    *,
    selected_ied: str | None = None,
) -> Iec61850MmsEndpointCatalog | None:
    text = source.decode("utf-8", errors="replace").strip()
    if not text:
        return None

    try:
        root = ET.fromstring(text)
    except ET.ParseError as exc:
        raise Iec61850ReportRuntimeError(
            "INVALID_MMS_ENDPOINT_CATALOG",
            "IEC 61850 SCD source is invalid XML.",
        ) from exc

    selected_ied_normalized = selected_ied.strip().lower() if selected_ied is not None else None
    entries: list[Iec61850MmsEndpointCatalogEntry] = []
    for connected_ap in _iter_elements(root, "ConnectedAP"):
        ied_name = (connected_ap.attrib.get("iedName") or connected_ap.attrib.get("ied_name") or "").strip()
        access_point_name = (connected_ap.attrib.get("apName") or connected_ap.attrib.get("accessPointName") or "AP1").strip() or "AP1"
        if not ied_name:
            continue
        if selected_ied_normalized is not None and ied_name.lower() != selected_ied_normalized:
            continue

        address = _first_child(connected_ap, "Address")
        host = _first_text_child_value_by_attr(address, "P", "type", "IP") if address is not None else None
        if host is None or not host.strip():
            continue
        entries.append(
            Iec61850MmsEndpointCatalogEntry(
                ied_name=ied_name,
                access_point_name=access_point_name,
                host=host.strip(),
                port=102,
            )
        )

    if not entries:
        return None
    return build_mms_endpoint_catalog(entries)


def build_mms_endpoint_catalog_from_json(payload: str | None) -> Iec61850MmsEndpointCatalog | None:
    text = payload.strip() if payload is not None else ""
    if not text:
        return None
    try:
        document = cast(object, json.loads(text))
    except json.JSONDecodeError as exc:
        raise Iec61850ReportRuntimeError(
            "INVALID_MMS_ENDPOINT_CATALOG",
            "IEC 61850 MMS endpoint catalog JSON is invalid.",
        ) from exc
    if not isinstance(document, list):
        raise Iec61850ReportRuntimeError(
            "INVALID_MMS_ENDPOINT_CATALOG",
            "IEC 61850 MMS endpoint catalog JSON must be a list of endpoint entries.",
        )

    entries: list[Iec61850MmsEndpointCatalogEntry] = []
    for item in cast(list[object], document):
        if not isinstance(item, dict):
            raise Iec61850ReportRuntimeError(
                "INVALID_MMS_ENDPOINT_CATALOG",
                "IEC 61850 MMS endpoint catalog entries must be JSON objects.",
            )
        typed_item = cast(dict[str, object], item)
        ied_name = str(typed_item.get("ied_name") or typed_item.get("iedName") or "").strip()
        access_point_name = str(typed_item.get("access_point_name") or typed_item.get("accessPointName") or "AP1").strip() or "AP1"
        host = str(typed_item.get("host") or "").strip()
        port_value = typed_item.get("port")
        if port_value is None or not str(port_value).strip():
            port = 102
        else:
            try:
                port = int(cast(str | int | float, port_value))
            except (TypeError, ValueError) as exc:
                raise Iec61850ReportRuntimeError(
                    "INVALID_MMS_ENDPOINT_CATALOG",
                    "IEC 61850 MMS endpoint catalog entries require a numeric port.",
                ) from exc
        endpoint_id = str(typed_item.get("endpoint_id") or typed_item.get("endpointId") or "").strip() or None
        if not ied_name:
            raise Iec61850ReportRuntimeError(
                "INVALID_MMS_ENDPOINT_CATALOG",
                "IEC 61850 MMS endpoint catalog entries require an ied_name.",
            )
        entries.append(
            Iec61850MmsEndpointCatalogEntry(
                ied_name=ied_name,
                access_point_name=access_point_name,
                host=host,
                port=port,
                endpoint_id=endpoint_id,
            )
        )
    return build_mms_endpoint_catalog(entries)


class Iec61850UnavailableMmsAdapter:
    def connect(
        self,
        *,
        session_id: str,
        endpoint: Iec61850DeviceEndpoint,
        candidates: Sequence[Iec61850ReportControlCandidate],
    ) -> Iec61850ReportSession:
        _ = (session_id, candidates)
        if endpoint.mode != Iec61850RuntimeMode.MMS:
            raise Iec61850ReportRuntimeError(
                "UNSUPPORTED_ENDPOINT_MODE",
                "IEC 61850 MMS adapter only accepts MMS endpoints.",
            )
        raise Iec61850ReportRuntimeError(
            "MMS_ADAPTER_NOT_IMPLEMENTED",
            "IEC 61850 MMS adapter is not implemented yet; use the simulator adapter or external test tool boundary.",
        )


def create_unavailable_mms_adapter() -> Iec61850UnavailableMmsAdapter:
    return Iec61850UnavailableMmsAdapter()


def _endpoint_key(ied_name: str, access_point_name: str) -> tuple[str, str]:
    return (ied_name.strip().lower(), access_point_name.strip().lower())

def _iter_elements(element: ET.Element, local_name: str) -> tuple[ET.Element, ...]:
    return tuple(child for child in element.iter() if _local_name(child.tag) == local_name)


def _first_child(element: ET.Element, local_name: str) -> ET.Element | None:
    for child in element:
        if _local_name(child.tag) == local_name:
            return child
    return None


def _first_text_child_value_by_attr(
    element: ET.Element | None,
    local_name: str,
    attr_name: str,
    attr_value: str,
) -> str | None:
    if element is None:
        return None
    for child in element:
        if _local_name(child.tag) != local_name:
            continue
        if str(child.attrib.get(attr_name, "")).strip() != attr_value:
            continue
        if child.text is None:
            return None
        return child.text
    return None


def _local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]
