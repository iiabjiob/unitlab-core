from __future__ import annotations

from dataclasses import dataclass
from typing import Literal

from app.schemas.verification_schema import (
    VerificationEvidenceDiagnosticSchema,
    VerificationExecutionContextSchema,
)
from app.services.iec61850.mms_adapter import (
    Iec61850MmsEndpointCatalog,
    build_mms_endpoint_catalog_from_json,
)

VerificationEndpointRuntimeMode = Literal["simulator", "mms"]
VerificationEndpointTransportSource = Literal["simulator", "explicit_request", "settings_catalog", "unavailable"]
VerificationEndpointModelSource = Literal["simulator", "loaded_scd", "discovery_fallback"]


@dataclass(frozen=True, slots=True)
class VerificationEndpointResolutionPolicyResult:
    runtime_mode: VerificationEndpointRuntimeMode
    transport_source: VerificationEndpointTransportSource
    model_source: VerificationEndpointModelSource
    endpoint_catalog: Iec61850MmsEndpointCatalog | None
    selected_runtime_import_id: str | None = None
    selected_runtime_revision: int | None = None
    notes: tuple[str, ...] = ()


def resolve_verification_endpoint_resolution_policy(
    *,
    execution_context: VerificationExecutionContextSchema,
    explicit_mms_endpoint_catalog: Iec61850MmsEndpointCatalog | None = None,
    settings_mms_endpoint_catalog_json: str | None = None,
    active_runtime_selection_import_id: str | None = None,
    active_runtime_selection_revision: int | None = None,
) -> VerificationEndpointResolutionPolicyResult:
    runtime_mode = _normalize_runtime_mode(execution_context.runtime_version)
    if runtime_mode == "simulator":
        return VerificationEndpointResolutionPolicyResult(
            runtime_mode="simulator",
            transport_source="simulator",
            model_source="simulator",
            endpoint_catalog=None,
            notes=("simulator runtime selected",),
        )

    notes: list[str] = []
    endpoint_catalog = explicit_mms_endpoint_catalog
    transport_source: VerificationEndpointTransportSource = "explicit_request" if endpoint_catalog is not None else "unavailable"

    if endpoint_catalog is None:
        endpoint_catalog = build_mms_endpoint_catalog_from_json(settings_mms_endpoint_catalog_json)
        if endpoint_catalog is not None:
            transport_source = "settings_catalog"
            notes.append("loaded MMS endpoint catalog from settings")

    if endpoint_catalog is None:
        notes.append("no MMS endpoint catalog available")

    model_source: VerificationEndpointModelSource = "loaded_scd" if active_runtime_selection_import_id else "discovery_fallback"
    if active_runtime_selection_import_id:
        notes.append(f"loaded runtime SCD import {active_runtime_selection_import_id}")
    else:
        notes.append("discovery required for model binding")

    return VerificationEndpointResolutionPolicyResult(
        runtime_mode="mms",
        transport_source=transport_source,
        model_source=model_source,
        endpoint_catalog=endpoint_catalog,
        selected_runtime_import_id=active_runtime_selection_import_id,
        selected_runtime_revision=active_runtime_selection_revision,
        notes=tuple(notes),
    )


def build_verification_endpoint_resolution_diagnostic(
    policy: VerificationEndpointResolutionPolicyResult,
) -> VerificationEvidenceDiagnosticSchema:
    severity = "info" if policy.endpoint_catalog is not None or policy.runtime_mode == "simulator" else "warning"
    transport_label = policy.transport_source.replace("_", " ")
    model_label = policy.model_source.replace("_", " ")
    details = {
        "runtime_mode": policy.runtime_mode,
        "transport_source": policy.transport_source,
        "model_source": policy.model_source,
        "selected_runtime_import_id": policy.selected_runtime_import_id,
        "selected_runtime_revision": policy.selected_runtime_revision,
        "notes": list(policy.notes),
    }
    if policy.runtime_mode == "simulator":
        message = "Simulator runtime selected; no MMS endpoint catalog is required."
    elif policy.endpoint_catalog is None:
        message = f"MMS runtime selected without an endpoint catalog; model binding uses {model_label}."
    else:
        message = f"MMS transport resolved from {transport_label}; model binding uses {model_label}."
    return VerificationEvidenceDiagnosticSchema(
        code="endpoint_resolution_policy",
        message=message,
        severity=severity,
        details=details,
    )


def _normalize_runtime_mode(runtime_version: str | None) -> VerificationEndpointRuntimeMode:
    value = (runtime_version or "").strip().lower()
    if value in {"mms", "live", "live-mms", "real-mms"}:
        return "mms"
    return "simulator"
