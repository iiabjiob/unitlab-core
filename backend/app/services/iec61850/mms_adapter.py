from __future__ import annotations

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
        entry = self._entries.get(_endpoint_key(device.ied_name, device.access_point_name))
        if entry is None:
            raise Iec61850ReportRuntimeError(
                "MMS_ENDPOINT_NOT_CONFIGURED",
                f'MMS endpoint for "{device.ied_name}/{device.access_point_name}" is not configured.',
            )
        endpoint_id = entry.endpoint_id or f"mms:{entry.ied_name}/{entry.access_point_name}@{entry.host}:{entry.port}"
        return Iec61850DeviceEndpoint(
            id=endpoint_id,
            mode=Iec61850RuntimeMode.MMS,
            ied_name=entry.ied_name,
            access_point_name=entry.access_point_name,
            host=entry.host,
            port=entry.port,
        )


def build_mms_endpoint_catalog(
    entries: Sequence[Iec61850MmsEndpointCatalogEntry],
) -> Iec61850MmsEndpointCatalog:
    return Iec61850MmsEndpointCatalog(entries)


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
