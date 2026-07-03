from __future__ import annotations

import hashlib
import json
import re
from collections import defaultdict
from dataclasses import dataclass
from typing import Any, Mapping, Sequence

from app.models.signal import Signal
from app.schemas.signal_sheet_schema import SignalAllocationRowSchema
from app.schemas.verification_schema import (
    VerificationSubscriptionPlanCoverageSchema,
    VerificationSubscriptionPlanSchema,
    VerificationSubscriptionPlanGroupSchema,
    VerificationSubscriptionPlanUncoveredTargetSchema,
    PlannerConfidenceReportSchema,
    PlannerConfidenceSignalSchema,
    VerificationTargetSchema,
)


@dataclass(frozen=True, slots=True)
class VerificationTargetSource:
    signal_id: int
    signal_reference: str
    signal_path: str
    signal_metadata: dict[str, Any]
    allocation_id: int | None
    allocation_status: str
    allocation_health: dict[str, bool]
    channel_id: int | None
    channel_label: str | None
    unit_id: str | None
    unit_online: bool | None
    source_row_id: str
    source_row_index: int | None = None
    source_kind: str | None = None
    source_reason: str | None = None


def build_verification_target_sources(
    *,
    requested_signal_ids: Sequence[int],
    signals_by_id: Mapping[int, Signal],
    allocation_rows_by_signal_id: Mapping[int, SignalAllocationRowSchema],
) -> list[VerificationTargetSource]:
    sources: list[VerificationTargetSource] = []
    seen: set[int] = set()
    for raw_signal_id in requested_signal_ids:
        signal_id = int(raw_signal_id)
        if signal_id <= 0 or signal_id in seen:
            continue
        seen.add(signal_id)
        signal = signals_by_id.get(signal_id)
        row = allocation_rows_by_signal_id.get(signal_id)
        signal_reference = str(signal.name).strip() if signal is not None and str(signal.name).strip() else f"Signal {signal_id}"
        signal_path = str(signal.key).strip() if signal is not None and str(signal.key).strip() else f"signal-{signal_id}"
        signal_metadata = dict(signal.signal_metadata or {}) if signal is not None else {}
        source_row_index = _resolve_source_row_index(signal_metadata)
        source_kind = _resolve_source_kind(signal_metadata)
        source_reason = _resolve_source_reason(signal_metadata)
        sources.append(
            VerificationTargetSource(
                signal_id=signal_id,
                signal_reference=signal_reference,
                signal_path=signal_path,
                signal_metadata=signal_metadata,
                source_row_index=source_row_index,
                source_kind=source_kind,
                source_reason=source_reason,
                allocation_id=row.allocation_id if row is not None else None,
                allocation_status=str(row.allocation_status or "unassigned") if row is not None else "unassigned",
                allocation_health=dict(row.allocation_health or {}) if row is not None else {},
                channel_id=row.channel_id if row is not None else None,
                channel_label=str(row.channel_label).strip() if row is not None and row.channel_label else None,
                unit_id=str(row.unit_id).strip() if row is not None and row.unit_id else None,
                unit_online=row.unit_online if row is not None else None,
                source_row_id=str(row.row_id).strip() if row is not None else f"signal-{signal_id}",
            )
        )
    return sources


def build_verification_subscription_plan(
    sources: Sequence[VerificationTargetSource],
    *,
    timeout_ms: int = 5000,
    window_ms: int = 1000,
) -> VerificationSubscriptionPlanSchema:
    targets: list[VerificationTargetSchema] = []
    group_buckets: dict[tuple[str, ...], list[int]] = defaultdict(list)
    group_contexts: dict[tuple[str, ...], dict[str, Any]] = {}
    uncovered_targets: list[VerificationSubscriptionPlanUncoveredTargetSchema] = []
    planning_diagnostics: list[str] = []
    endpoints: set[str] = set()
    exact_count = 0
    partial_count = 0
    uncovered_count = 0
    fallback_group_count = 0

    for target_index, source in enumerate(sources):
        protocol, protocol_metadata = _extract_protocol_metadata(source.signal_metadata)
        expected_feedback_path, feedback_source = _resolve_expected_feedback_path(
            signal_path=source.signal_path,
            signal_metadata=source.signal_metadata,
            protocol_metadata=protocol_metadata,
        )
        coverage_state, coverage_reason = _resolve_coverage_state(
            source=source,
            protocol_metadata=protocol_metadata,
            expected_feedback_path_source=feedback_source,
        )
        endpoint_id, ied_name, access_point_name = _resolve_endpoint_identity(source, protocol_metadata)
        (
            report_control_reference,
            report_control_name,
            report_kind,
            rpt_id,
            data_set_reference,
            group_reference_source,
        ) = _resolve_group_references(
            source=source,
            protocol_metadata=protocol_metadata,
            expected_feedback_path=expected_feedback_path,
        )
        fallback_discovery_group_reference = (
            _fallback_discovery_group_reference(expected_feedback_path)
            if group_reference_source != "protocol_metadata"
            and not report_control_reference
            and not data_set_reference
            else ""
        )
        source_classification, source_reason, group_reason = _resolve_source_classification(
            source=source,
            protocol_metadata=protocol_metadata,
            coverage_reason=coverage_reason,
            has_explicit_group_references=group_reference_source == "protocol_metadata",
        )
        if (
            source_classification == "fallback"
            and fallback_discovery_group_reference
            and not _is_report_observable_fallback_scope(fallback_discovery_group_reference)
        ):
            coverage_state = "uncovered"
            coverage_reason = "signal-list IEC 61850 address is not a report-observable Online 61850 scope"
            source_classification = "not found"
            source_reason = coverage_reason
            group_reason = "not report-observable"

        target = VerificationTargetSchema(
            signal_id=source.signal_id,
            signal_reference=source.signal_reference,
            signal_path=source.signal_path,
            endpoint_id=_resolve_runtime_endpoint_id(endpoint_id, source.unit_id),
            expected_feedback_path=expected_feedback_path,
            timeout_ms=timeout_ms,
            window_ms=window_ms,
            protocol=protocol,
            protocol_metadata=protocol_metadata,
            coverage_state=coverage_state,
            coverage_reason=coverage_reason,
            source_row_index=source.source_row_index,
            source_kind=source.source_kind,
            source_reason=source.source_reason,
            allocation_id=source.allocation_id,
            channel_id=source.channel_id,
            channel_label=source.channel_label,
            unit_id=source.unit_id,
            source_row_id=source.source_row_id,
        )
        targets.append(target)

        if coverage_state == "uncovered":
            uncovered_targets.append(
                VerificationSubscriptionPlanUncoveredTargetSchema(
                    target_index=target_index,
                    reason=coverage_reason or "not found",
                    detail=source_reason or "no endpoint metadata available to bind this target",
                )
            )
        else:
            if endpoint_id:
                endpoints.add(endpoint_id)
            group_key = (
                endpoint_id or "__uncovered__",
                source_classification,
                report_control_reference or "",
                report_control_name or "",
                report_kind or "",
                rpt_id or "",
                data_set_reference or "",
                fallback_discovery_group_reference,
            )
            group_buckets[group_key].append(target_index)
            group_contexts.setdefault(
                group_key,
                {
                    "endpoint_id": endpoint_id,
                    "ied_name": ied_name,
                    "access_point_name": access_point_name,
                    "report_control_reference": report_control_reference,
                    "report_control_name": report_control_name,
                    "report_kind": report_kind,
                    "rpt_id": rpt_id,
                    "data_set_reference": data_set_reference,
                    "source_classification": source_classification,
                    "source_reason": source_reason,
                    "reason": group_reason,
                },
            )
        if coverage_state == "exact":
            exact_count += 1
        elif coverage_state == "partial":
            partial_count += 1
        else:
            uncovered_count += 1

    groups: list[VerificationSubscriptionPlanGroupSchema] = []
    for group_index, group_key in enumerate(
        sorted(
            group_buckets,
            key=lambda item: (
                str(item[0]),
                str(item[1]),
                str(item[2]),
                str(item[3]),
                str(item[4]),
                str(item[5]),
                str(item[6]),
                str(item[7]),
            ),
        ),
        start=1,
    ):
        context = group_contexts[group_key]
        groups.append(
            VerificationSubscriptionPlanGroupSchema(
                group_id=f"group-{group_index}",
                endpoint_id=context["endpoint_id"],
                ied_name=context["ied_name"],
                access_point_name=context["access_point_name"],
                report_control_reference=context["report_control_reference"],
                report_control_name=context["report_control_name"],
                report_kind=context["report_kind"],
                rpt_id=context["rpt_id"],
                data_set_reference=context["data_set_reference"],
                target_indexes=sorted(group_buckets[group_key]),
                reason=context["reason"],
                source_classification=context["source_classification"],
                source_reason=context["source_reason"],
            )
        )
    fallback_group_count = sum(1 for group in groups if group.source_classification == "fallback")

    planning_quality = "exact"
    if uncovered_count > 0:
        planning_quality = "partial"
    elif partial_count > 0 or fallback_group_count > 0:
        planning_quality = "fallback"

    diagnostics = [
        f"normalized {len(targets)} verification targets",
        f"built {len(groups)} subscription groups across {len(endpoints)} endpoints",
    ]
    if uncovered_targets:
        diagnostics.append(f"{len(uncovered_targets)} targets uncovered")
    if fallback_group_count > 0:
        diagnostics.append(f"{fallback_group_count} groups use fallback planning")

    plan_id = _build_plan_id(
        selected_signal_ids=[source.signal_id for source in sources],
        groups=groups,
        uncovered_targets=uncovered_targets,
        coverage={
            "total_targets": len(targets),
            "covered_targets": exact_count,
            "partially_covered_targets": partial_count,
            "uncovered_targets": uncovered_count,
            "groups_count": len(groups),
            "endpoints_count": len(endpoints),
            "planning_quality": planning_quality,
        },
    )

    coverage = VerificationSubscriptionPlanCoverageSchema(
        total_targets=len(targets),
        covered_targets=exact_count,
        partially_covered_targets=partial_count,
        uncovered_targets=uncovered_count,
        groups_count=len(groups),
        endpoints_count=len(endpoints),
        planning_quality=planning_quality,
    )
    return VerificationSubscriptionPlanSchema(
        plan_id=plan_id,
        selected_signal_ids=[source.signal_id for source in sources],
        targets=targets,
        groups=groups,
        uncovered_targets=uncovered_targets,
        planning_diagnostics=diagnostics,
        coverage=coverage,
    )


def build_planner_confidence_report(
    plan: VerificationSubscriptionPlanSchema,
) -> PlannerConfidenceReportSchema:
    group_by_target_index: dict[int, VerificationSubscriptionPlanGroupSchema] = {}
    for group in plan.groups:
        for target_index in group.target_indexes:
            group_by_target_index[target_index] = group

    source_classification_counts: dict[str, int] = defaultdict(int)
    signals: list[PlannerConfidenceSignalSchema] = []
    diagnostic_messages: list[str] = []

    for target_index, target in enumerate(plan.targets):
        group = group_by_target_index.get(target_index)
        source_classification = group.source_classification if group is not None else "not found"
        source_classification_counts[source_classification] += 1

        confidence_state = "uncovered"
        signal_diagnostics: list[str] = []
        if target.coverage_state == "uncovered" or group is None:
            confidence_state = "uncovered"
            signal_diagnostics.append(target.coverage_reason or "target is uncovered")
        elif target.coverage_state == "partial":
            confidence_state = "watch"
            signal_diagnostics.append(target.coverage_reason or "partial coverage")
            if source_classification == "fallback":
                signal_diagnostics.append("plan depends on fallback endpoint binding")
        elif source_classification == "from SCD":
            confidence_state = "strong"
        elif source_classification == "from discovery":
            confidence_state = "watch"
            signal_diagnostics.append("plan depends on discovery metadata")
        elif source_classification == "fallback":
            confidence_state = "risk"
            signal_diagnostics.append("plan depends on fallback endpoint binding")
        else:
            confidence_state = "risk"
            signal_diagnostics.append("plan source could not be verified")

        if group is not None:
            if not group.report_control_reference:
                signal_diagnostics.append("missing report control reference")
            if not group.data_set_reference:
                signal_diagnostics.append("missing dataset reference")
            if not group.report_control_name:
                signal_diagnostics.append("missing report control name")

        if signal_diagnostics:
            diagnostic_messages.append(f"{target.signal_reference}: " + "; ".join(signal_diagnostics))

        signals.append(
            PlannerConfidenceSignalSchema(
                signal_index=target_index,
                signal_id=target.signal_id,
                signal_reference=target.signal_reference,
                endpoint_id=group.endpoint_id if group is not None else target.endpoint_id,
                expected_feedback_path=target.expected_feedback_path,
                report_control_reference=group.report_control_reference if group is not None else None,
                report_control_name=group.report_control_name if group is not None else None,
                data_set_reference=group.data_set_reference if group is not None else None,
                coverage_state=target.coverage_state,
                source_classification=source_classification,
                confidence_state=confidence_state,
                diagnostics=signal_diagnostics,
            )
        )

    total_targets = len(plan.targets)
    covered_targets = plan.coverage.covered_targets
    partially_covered_targets = plan.coverage.partially_covered_targets
    uncovered_targets = plan.coverage.uncovered_targets
    coverage_percentage = _planner_coverage_percentage(total_targets, covered_targets, partially_covered_targets)
    confidence_percentage = _planner_confidence_percentage(
        total_targets=total_targets,
        covered_targets=covered_targets,
        partially_covered_targets=partially_covered_targets,
        uncovered_targets=uncovered_targets,
        signals=signals,
    )
    risk_level = _planner_risk_level(
        confidence_percentage=confidence_percentage,
        uncovered_targets=uncovered_targets,
        fallback_targets=source_classification_counts.get("fallback", 0),
    )

    diagnostics = list(plan.planning_diagnostics)
    diagnostics.extend(diagnostic_messages)
    if uncovered_targets > 0:
        diagnostics.append(f"{uncovered_targets} targets require attention before runtime")

    return PlannerConfidenceReportSchema(
        plan_id=plan.plan_id,
        total_targets=total_targets,
        covered_targets=covered_targets,
        partially_covered_targets=partially_covered_targets,
        uncovered_targets=uncovered_targets,
        coverage_percentage=coverage_percentage,
        confidence_percentage=confidence_percentage,
        groups_count=plan.coverage.groups_count,
        endpoints_count=plan.coverage.endpoints_count,
        planning_quality=plan.coverage.planning_quality,
        risk_level=risk_level,
        source_classification_counts=dict(source_classification_counts),
        signals=signals,
        diagnostics=diagnostics,
    )


def _extract_protocol_metadata(signal_metadata: dict[str, Any]) -> tuple[str | None, dict[str, Any]]:
    if not isinstance(signal_metadata, dict) or not signal_metadata:
        return None, {}

    verification_metadata = signal_metadata.get("verification") if isinstance(signal_metadata.get("verification"), dict) else {}
    row_metadata = signal_metadata.get("row") if isinstance(signal_metadata.get("row"), dict) else {}

    if isinstance(signal_metadata.get("protocol_metadata"), dict):
        protocol_metadata = dict(signal_metadata["protocol_metadata"])
        _merge_signal_list_verification_metadata(
            protocol_metadata=protocol_metadata,
            verification_metadata=verification_metadata,
            row_metadata=row_metadata,
            signal_metadata=signal_metadata,
        )
        protocol = _resolve_protocol_name(signal_metadata, protocol_metadata)
        return protocol, protocol_metadata

    if isinstance(signal_metadata.get("iec61850"), dict):
        protocol_metadata = dict(signal_metadata["iec61850"])
        _merge_signal_list_verification_metadata(
            protocol_metadata=protocol_metadata,
            verification_metadata=verification_metadata,
            row_metadata=row_metadata,
            signal_metadata=signal_metadata,
        )
        protocol = _resolve_protocol_name(signal_metadata, protocol_metadata, default="iec61850")
        return protocol, protocol_metadata

    protocol_metadata: dict[str, Any] = {}
    _merge_signal_list_verification_metadata(
        protocol_metadata=protocol_metadata,
        verification_metadata=verification_metadata,
        row_metadata=row_metadata,
        signal_metadata=signal_metadata,
    )
    if protocol_metadata:
        protocol = _resolve_protocol_name(signal_metadata, protocol_metadata, default="iec61850")
        return protocol, protocol_metadata

    if isinstance(signal_metadata.get("protocol"), str):
        protocol = str(signal_metadata["protocol"]).strip() or None
        return protocol, {}

    return None, {}


def _merge_signal_list_verification_metadata(
    *,
    protocol_metadata: dict[str, Any],
    verification_metadata: Any,
    row_metadata: Any,
    signal_metadata: dict[str, Any],
) -> None:
    verification = verification_metadata if isinstance(verification_metadata, dict) else {}
    row = row_metadata if isinstance(row_metadata, dict) else {}

    transport_host = _first_non_empty_string(
        protocol_metadata.get("transport_host"),
        protocol_metadata.get("mms_host"),
        protocol_metadata.get("endpoint_host"),
        verification.get("transport_host"),
        verification.get("transport_reference"),
        row.get("transport_host"),
        row.get("transport_reference"),
        signal_metadata.get("transport_host"),
    )
    if transport_host is not None:
        protocol_metadata.setdefault("transport_host", transport_host)

    address = _first_non_empty_string(
        protocol_metadata.get("iec61850_address"),
        protocol_metadata.get("expected_feedback_path"),
        protocol_metadata.get("feedback_path"),
        protocol_metadata.get("data_reference"),
        verification.get("iec61850_address"),
        verification.get("iec61850"),
        row.get("iec61850_address"),
        row.get("iec61850"),
        signal_metadata.get("iec61850_address"),
    )
    if address is not None:
        protocol_metadata.setdefault("iec61850_address", address)
        protocol_metadata.setdefault("expected_feedback_path", address)
        protocol_metadata.setdefault("data_reference", address)

    if transport_host is not None:
        protocol_metadata.setdefault("access_point_name", "AP1")


def _resolve_protocol_name(
    signal_metadata: dict[str, Any],
    protocol_metadata: dict[str, Any],
    *,
    default: str | None = None,
) -> str | None:
    protocol = signal_metadata.get("protocol")
    if isinstance(protocol, str) and protocol.strip():
        return protocol.strip()
    protocol = protocol_metadata.get("protocol")
    if isinstance(protocol, str) and protocol.strip():
        return protocol.strip()
    return default


def _resolve_source_row_index(signal_metadata: dict[str, Any]) -> int | None:
    for candidate in (
        signal_metadata.get("source_row_index"),
        signal_metadata.get("row_index"),
        signal_metadata.get("row", {}).get("row_index") if isinstance(signal_metadata.get("row"), dict) else None,
    ):
        if isinstance(candidate, int) and candidate >= 0:
            return candidate
        if isinstance(candidate, str):
            value = candidate.strip()
            if value.isdigit():
                return int(value)
    return None


def _resolve_source_kind(signal_metadata: dict[str, Any]) -> str | None:
    for candidate in (
        signal_metadata.get("source_kind"),
        signal_metadata.get("protocol_metadata", {}).get("source_kind")
        if isinstance(signal_metadata.get("protocol_metadata"), dict)
        else None,
    ):
        if isinstance(candidate, str):
            value = candidate.strip()
            if value:
                return value
    return None


def _resolve_source_reason(signal_metadata: dict[str, Any]) -> str | None:
    for candidate in (
        signal_metadata.get("source_reason"),
        signal_metadata.get("protocol_metadata", {}).get("source_reason")
        if isinstance(signal_metadata.get("protocol_metadata"), dict)
        else None,
    ):
        if isinstance(candidate, str):
            value = candidate.strip()
            if value:
                return value
    return None


def _resolve_endpoint_identity(
    source: VerificationTargetSource,
    protocol_metadata: dict[str, Any],
) -> tuple[str | None, str | None, str | None]:
    transport_host = _first_non_empty_string(
        protocol_metadata.get("transport_host"),
        protocol_metadata.get("mms_host"),
        protocol_metadata.get("endpoint_host"),
    )
    ied_name = _first_non_empty_string(
        protocol_metadata.get("ied_name"),
        protocol_metadata.get("iedName"),
        protocol_metadata.get("endpoint_ied_name"),
    )
    access_point_name = _first_non_empty_string(
        protocol_metadata.get("access_point_name"),
        protocol_metadata.get("accessPointName"),
    )
    endpoint_id = _normalize_transport_endpoint_id(transport_host) if transport_host is not None else None
    if endpoint_id is None:
        endpoint_id = source.unit_id.strip() if source.unit_id else None
    if endpoint_id and "/" in endpoint_id:
        endpoint_ied_name, endpoint_access_point_name = endpoint_id.split("/", 1)
        ied_name = ied_name or endpoint_ied_name.strip() or None
        access_point_name = access_point_name or endpoint_access_point_name.strip() or None
    if endpoint_id and not ied_name and not _looks_like_transport_endpoint(endpoint_id):
        ied_name = endpoint_id
    return endpoint_id, ied_name, access_point_name or "unknown"


def _looks_like_transport_endpoint(value: str) -> bool:
    text = value.strip()
    return ":" in text or bool(re.search(r"\b\d{1,3}(?:\.\d{1,3}){3}\b", text))


def _normalize_transport_endpoint_id(transport_host: str) -> str:
    text = transport_host.strip()
    first_token = text.split(None, 1)[0].strip()
    return first_token or text


def _resolve_runtime_endpoint_id(endpoint_id: str | None, unit_id: str | None) -> str | None:
    base_endpoint_id = _first_non_empty_string(endpoint_id, unit_id)
    if base_endpoint_id is None:
        return None
    return f"sim:{base_endpoint_id}/unknown"


def _fallback_discovery_group_reference(expected_feedback_path: str) -> str:
    text = str(expected_feedback_path or "").strip()
    if not text:
        return ""
    reference = text.split("!", 1)[-1] if "!" in text else text
    functional_constraint = _fallback_functional_constraint(reference)
    reference = reference.split("[", 1)[0]
    domain, separator, item = reference.partition("/")
    if not separator:
        domain, _separator, item = reference.partition(".")
    domain = domain.strip()
    if not domain:
        return ""
    return f"{domain}/{functional_constraint}" if functional_constraint else domain


def _fallback_functional_constraint(reference: str) -> str:
    bracket_match = re.search(r"\[([A-Za-z0-9]+)\]\s*$", reference)
    if bracket_match is not None:
        return bracket_match.group(1).upper()
    item = reference.split("!", 1)[-1]
    if "/" in item:
        item = item.split("/", 1)[1]
    parts = [part.strip() for part in item.split("$") if part.strip()]
    if len(parts) >= 2 and re.fullmatch(r"[A-Za-z]{2}", parts[1]):
        return parts[1].upper()
    return ""


def _is_report_observable_fallback_scope(scope: str) -> bool:
    domain, _separator, functional_constraint = scope.partition("/")
    if domain.strip().upper().endswith("SYSTEM"):
        return False
    return functional_constraint.strip().upper() in {"ST", "MX"}


def _resolve_group_references(
    *,
    source: VerificationTargetSource,
    protocol_metadata: dict[str, Any],
    expected_feedback_path: str,
) -> tuple[str | None, str | None, str | None, str | None, str | None, str]:
    explicit_reference = False
    report_control_reference = _first_non_empty_string(
        protocol_metadata.get("report_control_reference"),
        protocol_metadata.get("report_control_reference_hint"),
        protocol_metadata.get("reportControlReference"),
        protocol_metadata.get("report_reference"),
        protocol_metadata.get("report_path"),
    )
    rpt_id = _first_non_empty_string(
        protocol_metadata.get("rpt_id"),
        protocol_metadata.get("rptId"),
    )
    report_control_name = _first_non_empty_string(
        protocol_metadata.get("report_control_name"),
        protocol_metadata.get("reportControlName"),
    )
    report_kind = _first_non_empty_string(
        protocol_metadata.get("report_kind"),
        protocol_metadata.get("reportKind"),
    )
    data_set_reference = _first_non_empty_string(
        protocol_metadata.get("data_set_reference"),
        protocol_metadata.get("dataSetReference"),
        protocol_metadata.get("dataset_reference"),
        protocol_metadata.get("datasetReference"),
    )
    if report_control_reference is not None or rpt_id is not None or data_set_reference is not None:
        explicit_reference = True

    if report_control_reference is None and rpt_id is not None:
        report_control_reference = rpt_id
    if rpt_id is None and report_control_reference is not None:
        rpt_id = report_control_reference
    if report_control_name is None:
        report_control_name = _derive_report_control_name(report_control_reference, rpt_id, source.signal_path) if explicit_reference else None
    if report_kind is None:
        report_kind = _first_non_empty_string(protocol_metadata.get("report_kind_hint"), "unknown")
    reference_source = "protocol_metadata" if explicit_reference else ("fallback_expected_feedback_path" if expected_feedback_path else "fallback_signal_path")
    return report_control_reference, report_control_name, report_kind, rpt_id, data_set_reference, reference_source


def _resolve_source_classification(
    *,
    source: VerificationTargetSource,
    protocol_metadata: dict[str, Any],
    coverage_reason: str | None,
    has_explicit_group_references: bool,
) -> tuple[str, str | None, str]:
    raw_kind = _first_non_empty_string(
        source.source_kind,
        protocol_metadata.get("source_kind"),
        protocol_metadata.get("sourceKind"),
    )
    normalized = _normalize_source_classification(raw_kind)
    if normalized is not None:
        reason = _first_non_empty_string(
            source.source_reason,
            protocol_metadata.get("source_reason"),
            protocol_metadata.get("sourceReason"),
            coverage_reason,
        )
        return normalized, reason, _source_classification_reason(normalized, reason)

    if has_explicit_group_references:
        reason = _first_non_empty_string(
            source.source_reason,
            protocol_metadata.get("source_reason"),
            protocol_metadata.get("sourceReason"),
            "resolved from protocol metadata hints",
        )
        return "from SCD", reason, "SCD hint match"

    has_signal_list_endpoint = _first_non_empty_string(
        protocol_metadata.get("transport_host"),
        protocol_metadata.get("mms_host"),
        protocol_metadata.get("endpoint_host"),
    ) is not None
    if source.unit_id or has_signal_list_endpoint:
        reason = _first_non_empty_string(
            source.source_reason,
            coverage_reason,
            "no SCD or discovery snapshot available",
        )
        return "fallback", reason, "fallback endpoint binding"

    reason = _first_non_empty_string(
        source.source_reason,
        coverage_reason,
        "no endpoint metadata",
    )
    return "not found", reason, "no endpoint metadata"


def _normalize_source_classification(raw_kind: str | None) -> str | None:
    if not raw_kind:
        return None
    value = raw_kind.strip().lower()
    if value in {"scd", "from scd"}:
        return "from SCD"
    if value in {"discovery", "from discovery"}:
        return "from discovery"
    if value in {"fallback"}:
        return "fallback"
    if value in {"not found", "not_found", "notfound"}:
        return "not found"
    return None


def _source_classification_reason(source_classification: str, reason: str | None) -> str:
    if source_classification == "from SCD":
        return reason or "SCD hint match"
    if source_classification == "from discovery":
        return reason or "discovery snapshot match"
    if source_classification == "fallback":
        return reason or "fallback endpoint binding"
    return reason or "no endpoint metadata"


def _first_non_empty_string(*candidates: Any) -> str | None:
    for candidate in candidates:
        if isinstance(candidate, str):
            value = candidate.strip()
            if value:
                return value
    return None


def _derive_report_control_name(
    report_control_reference: str | None,
    rpt_id: str | None,
    signal_path: str,
) -> str:
    for candidate in (rpt_id, report_control_reference, signal_path):
        if not candidate:
            continue
        token = candidate.strip().rstrip("/")
        if not token:
            continue
        if "." in token:
            segment = token.rsplit(".", 1)[-1].strip()
            if segment:
                return segment
        parts = [part.strip() for part in token.split("/") if part.strip()]
        if len(parts) >= 2 and parts[-1].lower() in {"buffered", "unbuffered"}:
            return parts[-2]
        if parts:
            return parts[-1]
    return signal_path


def _build_plan_id(
    *,
    selected_signal_ids: Sequence[int],
    groups: Sequence[VerificationSubscriptionPlanGroupSchema],
    uncovered_targets: Sequence[VerificationSubscriptionPlanUncoveredTargetSchema],
    coverage: dict[str, Any],
) -> str:
    payload = {
        "selected_signal_ids": list(selected_signal_ids),
        "groups": [
            {
                "endpoint_id": group.endpoint_id,
                "group_id": group.group_id,
                "report_control_reference": group.report_control_reference,
                "report_control_name": group.report_control_name,
                "report_kind": group.report_kind,
                "rpt_id": group.rpt_id,
                "data_set_reference": group.data_set_reference,
                "target_indexes": list(group.target_indexes),
                "source_classification": group.source_classification,
            }
            for group in groups
        ],
        "uncovered_targets": [
            {
                "target_index": item.target_index,
                "reason": item.reason,
                "detail": item.detail,
            }
            for item in uncovered_targets
        ],
        "coverage": coverage,
    }
    digest = hashlib.sha1(json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest()
    return f"plan-{digest[:12]}"


def _planner_coverage_percentage(
    total_targets: int,
    covered_targets: int,
    partially_covered_targets: int,
) -> int:
    if total_targets <= 0:
        return 0
    score = ((covered_targets + (0.5 * partially_covered_targets)) / total_targets) * 100.0
    return max(0, min(100, int(round(score))))


def _planner_confidence_percentage(
    *,
    total_targets: int,
    covered_targets: int,
    partially_covered_targets: int,
    uncovered_targets: int,
    signals: Sequence[PlannerConfidenceSignalSchema],
) -> int:
    if total_targets <= 0:
        return 0
    base_score = ((covered_targets + (0.5 * partially_covered_targets)) / total_targets) * 100.0
    penalty = 0.0
    penalty += uncovered_targets * 12.0
    penalty += sum(4.0 for signal in signals if signal.source_classification == "fallback")
    penalty += sum(2.0 for signal in signals if signal.source_classification == "from discovery")
    penalty += sum(6.0 for signal in signals if signal.confidence_state == "risk")
    score = max(0.0, min(100.0, base_score - penalty))
    return int(round(score))


def _planner_risk_level(
    *,
    confidence_percentage: int,
    uncovered_targets: int,
    fallback_targets: int,
) -> str:
    if uncovered_targets > 0 or confidence_percentage < 70:
        return "high"
    if fallback_targets > 0 or confidence_percentage < 90:
        return "medium"
    return "low"


def _resolve_expected_feedback_path(
    *,
    signal_path: str,
    signal_metadata: dict[str, Any],
    protocol_metadata: dict[str, Any],
) -> tuple[str, str]:
    for candidate in (
        protocol_metadata.get("expected_feedback_path"),
        protocol_metadata.get("feedback_path"),
        protocol_metadata.get("report_path"),
        protocol_metadata.get("data_reference"),
        protocol_metadata.get("signal_path"),
        signal_metadata.get("expected_feedback_path"),
        signal_metadata.get("feedback_path"),
    ):
        value = str(candidate).strip() if candidate is not None else ""
        if value:
            return value, "protocol_metadata"
    return signal_path, "fallback_signal_path"


def _resolve_coverage_state(
    *,
    source: VerificationTargetSource,
    protocol_metadata: dict[str, Any],
    expected_feedback_path_source: str,
) -> tuple[str, str | None]:
    has_signal_list_endpoint = _first_non_empty_string(
        protocol_metadata.get("transport_host"),
        protocol_metadata.get("mms_host"),
        protocol_metadata.get("endpoint_host"),
    ) is not None
    if not source.unit_id and not has_signal_list_endpoint:
        return "uncovered", "no_endpoint"

    health = source.allocation_health
    if source.allocation_status != "assigned":
        reason = source.allocation_status or "unassigned"
        return "partial", reason
    if health.get("conflict"):
        return "partial", "allocation_conflict"
    if health.get("invalid_type"):
        return "partial", "allocation_invalid_type"
    if health.get("missing_device"):
        return "partial", "allocation_missing_device"
    if health.get("missing_channel"):
        return "partial", "allocation_missing_channel"
    if health.get("offline_device"):
        return "partial", "allocation_offline_device"
    if expected_feedback_path_source != "protocol_metadata":
        return "partial", "fallback_expected_feedback_path"
    if not source.unit_id and has_signal_list_endpoint:
        return "partial", "signal_list_endpoint"
    return "exact", None
