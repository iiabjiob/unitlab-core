from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Sequence

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
        for entry in entries:
            key = _endpoint_key(entry.ied_name, entry.access_point_name)
            if key in self._entries:
                raise Iec61850ReportRuntimeError(
                    "DUPLICATE_MMS_ENDPOINT",
                    f'MMS endpoint for "{entry.ied_name}/{entry.access_point_name}" is configured more than once.',
                )
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
            self._entries[key] = entry

    def endpoint_for_plan_device(self, device: Iec61850ReportSubscriptionPlanDevice) -> Iec61850DeviceEndpoint:
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
        endpoint_id = entry.endpoint_id or f"mms:{entry.ied_name}/{entry.access_point_name}@{resolved_host}:{resolved_port}"
        endpoint = Iec61850DeviceEndpoint(
            id=endpoint_id,
            mode=Iec61850RuntimeMode.MMS,
            ied_name=entry.ied_name,
            access_point_name=entry.access_point_name,
            host=resolved_host,
            port=resolved_port,
        )
        return endpoint, tuple(notes)


def build_mms_endpoint_catalog(
    entries: Sequence[Iec61850MmsEndpointCatalogEntry],
) -> Iec61850MmsEndpointCatalog:
    return Iec61850MmsEndpointCatalog(entries)


def build_mms_endpoint_catalog_from_json(payload: str | None) -> Iec61850MmsEndpointCatalog | None:
    text = payload.strip() if payload is not None else ""
    if not text:
        return None
    try:
        document = json.loads(text)
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
    for item in document:
        if not isinstance(item, dict):
            raise Iec61850ReportRuntimeError(
                "INVALID_MMS_ENDPOINT_CATALOG",
                "IEC 61850 MMS endpoint catalog entries must be JSON objects.",
            )
        ied_name = str(item.get("ied_name") or item.get("iedName") or "").strip()
        access_point_name = str(item.get("access_point_name") or item.get("accessPointName") or "AP1").strip() or "AP1"
        host = str(item.get("host") or "").strip()
        port_value = item.get("port")
        if port_value is None or not str(port_value).strip():
            port = 102
        else:
            try:
                port = int(port_value)
            except (TypeError, ValueError) as exc:
                raise Iec61850ReportRuntimeError(
                    "INVALID_MMS_ENDPOINT_CATALOG",
                    "IEC 61850 MMS endpoint catalog entries require a numeric port.",
                ) from exc
        endpoint_id = str(item.get("endpoint_id") or item.get("endpointId") or "").strip() or None
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
