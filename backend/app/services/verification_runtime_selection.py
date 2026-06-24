from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime
from typing import Callable, Literal

from app.schemas.verification_schema import VerificationExecutionContextSchema
from app.services.iec61850.mms_adapter import Iec61850MmsEndpointCatalog, create_unavailable_mms_adapter
from app.services.iec61850.report_runtime import (
    Iec61850DeviceEndpoint,
    Iec61850ReportRuntimeAdapter,
    Iec61850ReportSubscriptionPlanDevice,
    build_simulator_endpoint_for_plan_device,
    create_iec61850_simulator_adapter,
)


VerificationRuntimeMode = Literal["simulator", "mms"]


@dataclass(frozen=True, slots=True)
class VerificationRuntimeSelection:
    runtime_mode: VerificationRuntimeMode
    adapter: Iec61850ReportRuntimeAdapter
    endpoint_for_device: Callable[[Iec61850ReportSubscriptionPlanDevice], Iec61850DeviceEndpoint]
    runtime_source: Literal["simulator", "catalog", "unavailable-mms"]


def resolve_verification_runtime(
    *,
    execution_context: VerificationExecutionContextSchema,
    now: Callable[[], datetime] | None = None,
    endpoint_catalog: Iec61850MmsEndpointCatalog | None = None,
    simulator_endpoint_for_device: Callable[[Iec61850ReportSubscriptionPlanDevice], Iec61850DeviceEndpoint] = build_simulator_endpoint_for_plan_device,
) -> VerificationRuntimeSelection:
    runtime_mode = _normalize_runtime_mode(execution_context.runtime_version)
    if runtime_mode == "mms":
        if endpoint_catalog is None:
            raise ValueError("MMS runtime requires an endpoint catalog.")
        return VerificationRuntimeSelection(
            runtime_mode="mms",
            adapter=create_unavailable_mms_adapter(),
            endpoint_for_device=endpoint_catalog.endpoint_for_plan_device,
            runtime_source="catalog",
        )

    return VerificationRuntimeSelection(
        runtime_mode="simulator",
        adapter=create_iec61850_simulator_adapter(now=now),
        endpoint_for_device=simulator_endpoint_for_device,
        runtime_source="simulator",
    )


def _normalize_runtime_mode(runtime_version: str | None) -> VerificationRuntimeMode:
    value = (runtime_version or "").strip().lower()
    if value in {"mms", "live", "live-mms", "real-mms"}:
        return "mms"
    return "simulator"
