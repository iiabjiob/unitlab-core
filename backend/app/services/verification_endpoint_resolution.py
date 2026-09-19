from __future__ import annotations

from dataclasses import dataclass
from typing import Literal, cast

from app.schemas.verification_schema import (
    VerificationEvidenceDiagnosticSchema,
    VerificationExecutionContextSchema,
)
from app.services.iec61850.mms_adapter import (
    Iec61850MmsEndpointCatalog,
    build_mms_endpoint_catalog_from_json,
)

VerificationEndpointRuntimeMode = Literal["simulator", "mms"]
VerificationEndpointTransportSource = Literal["simulator", "explicit_request", "settings_catalog", "loaded_scd", "validation_override", "signal_list_fallback", "unavailable"]
VerificationEndpointModelSource = Literal["simulator", "loaded_scd", "discovery_fallback"]


@dataclass(frozen=True, slots=True)
class VerificationEndpointResolutionPolicyResult:
    runtime_mode: VerificationEndpointRuntimeMode
    transport_source: VerificationEndpointTransportSource
    model_source: VerificationEndpointModelSource
    endpoint_catalog: Iec61850MmsEndpointCatalog | None
    selected_runtime_import_id: str | None = None
    selected_runtime_selected_ied: str | None = None
    selected_runtime_revision: int | None = None
    transport_override_host: str | None = None
    transport_override_port: int | None = None
    notes: tuple[str, ...] = ()


def resolve_verification_endpoint_resolution_policy(
    *,
    execution_context: VerificationExecutionContextSchema,
    explicit_mms_endpoint_catalog: Iec61850MmsEndpointCatalog | None = None,
    settings_mms_endpoint_catalog_json: str | None = None,
    loaded_runtime_scd_endpoint_catalog: Iec61850MmsEndpointCatalog | None = None,
    loaded_runtime_scd_available: bool = False,
    active_runtime_selection_import_id: str | None = None,
    active_runtime_selection_selected_ied: str | None = None,
    active_runtime_selection_revision: int | None = None,
    transport_override_host: str | None = None,
    transport_override_port: int | None = None,
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
    override_requested = _has_transport_override(transport_override_host, transport_override_port)

    if override_requested:
        transport_source = "validation_override"
        notes.append("validation-only transport override requested")

    if endpoint_catalog is None:
        endpoint_catalog = build_mms_endpoint_catalog_from_json(settings_mms_endpoint_catalog_json)
        if endpoint_catalog is not None and not override_requested:
            transport_source = "settings_catalog"
            notes.append("loaded MMS endpoint catalog from settings")
        elif endpoint_catalog is not None:
            notes.append("loaded MMS endpoint catalog from settings")

    if endpoint_catalog is None and loaded_runtime_scd_endpoint_catalog is not None:
        endpoint_catalog = loaded_runtime_scd_endpoint_catalog
        if not override_requested:
            transport_source = "loaded_scd"
        notes.append("loaded MMS endpoint catalog from runtime SCD")

    if endpoint_catalog is None:
        if loaded_runtime_scd_available:
            notes.append("loaded runtime SCD did not yield MMS transport endpoints")
        else:
            notes.append("no MMS endpoint catalog available")

    model_source: VerificationEndpointModelSource = "loaded_scd" if active_runtime_selection_import_id else "discovery_fallback"
    if active_runtime_selection_import_id:
        notes.append(f"loaded runtime SCD import {active_runtime_selection_import_id}")
        if active_runtime_selection_selected_ied:
            notes.append(f"loaded runtime SCD selected IED {active_runtime_selection_selected_ied}")
    else:
        notes.append("discovery required for model binding")

    return VerificationEndpointResolutionPolicyResult(
        runtime_mode="mms",
        transport_source=transport_source,
        model_source=model_source,
        endpoint_catalog=endpoint_catalog,
        selected_runtime_import_id=active_runtime_selection_import_id,
        selected_runtime_selected_ied=active_runtime_selection_selected_ied,
        selected_runtime_revision=active_runtime_selection_revision,
        transport_override_host=transport_override_host.strip() if transport_override_host is not None and transport_override_host.strip() else None,
        transport_override_port=transport_override_port if transport_override_port is not None and transport_override_port > 0 else None,
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
        "selected_runtime_selected_ied": policy.selected_runtime_selected_ied,
        "selected_runtime_revision": policy.selected_runtime_revision,
        "transport_override_host": policy.transport_override_host,
        "transport_override_port": policy.transport_override_port,
        "notes": list(policy.notes),
    }
    if policy.runtime_mode == "simulator":
        message = "Simulator runtime selected; no MMS endpoint catalog is required."
    elif policy.endpoint_catalog is None:
        message = f"MMS runtime selected without an endpoint catalog; model binding uses {model_label}."
    elif policy.transport_source == "validation_override":
        message = f"MMS transport remapped by validation override; model binding uses {model_label}."
    elif policy.transport_source == "signal_list_fallback":
        message = f"MMS transport derived from signal list metadata fallback; model binding uses {model_label}."
    else:
        message = f"MMS transport resolved from {transport_label}; model binding uses {model_label}."
    return VerificationEvidenceDiagnosticSchema(
        code="endpoint_resolution_policy",
        message=message,
        severity=severity,
        details=cast(dict[str, object], details),
    )


def _has_transport_override(host: str | None, port: int | None) -> bool:
    return bool((host is not None and host.strip()) or (port is not None and port > 0))


def _normalize_runtime_mode(runtime_version: str | None) -> VerificationEndpointRuntimeMode:
    value = (runtime_version or "").strip().lower()
    if value in {"mms", "live", "live-mms", "real-mms"}:
        return "mms"
    return "simulator"
