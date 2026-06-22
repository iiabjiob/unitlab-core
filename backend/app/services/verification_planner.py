from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
from typing import Any, Mapping, Sequence

from app.models.signal import Signal
from app.schemas.signal_sheet_schema import SignalAllocationRowSchema
from app.schemas.verification_schema import (
    VerificationSubscriptionPlanCoverageSchema,
    VerificationSubscriptionPlanSchema,
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
        sources.append(
            VerificationTargetSource(
                signal_id=signal_id,
                signal_reference=signal_reference,
                signal_path=signal_path,
                signal_metadata=signal_metadata,
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
    groups: dict[str, list[int]] = defaultdict(list)
    endpoints: set[str] = set()
    exact_count = 0
    partial_count = 0
    uncovered_count = 0

    for source in sources:
        protocol, protocol_metadata = _extract_protocol_metadata(source.signal_metadata)
        expected_feedback_path, feedback_source = _resolve_expected_feedback_path(
            signal_path=source.signal_path,
            signal_metadata=source.signal_metadata,
            protocol_metadata=protocol_metadata,
        )
        coverage_state, coverage_reason = _resolve_coverage_state(
            source=source,
            expected_feedback_path_source=feedback_source,
        )

        target = VerificationTargetSchema(
            signal_id=source.signal_id,
            signal_reference=source.signal_reference,
            signal_path=source.signal_path,
            endpoint_id=source.unit_id,
            expected_feedback_path=expected_feedback_path,
            timeout_ms=timeout_ms,
            window_ms=window_ms,
            protocol=protocol,
            protocol_metadata=protocol_metadata,
            coverage_state=coverage_state,
            coverage_reason=coverage_reason,
            allocation_id=source.allocation_id,
            channel_id=source.channel_id,
            channel_label=source.channel_label,
            unit_id=source.unit_id,
            source_row_id=source.source_row_id,
        )
        targets.append(target)

        group_key = source.unit_id or "__uncovered__"
        groups[group_key].append(source.signal_id)
        if source.unit_id:
            endpoints.add(source.unit_id)

        if coverage_state == "exact":
            exact_count += 1
        elif coverage_state == "partial":
            partial_count += 1
        else:
            uncovered_count += 1

    planning_quality = "exact"
    if uncovered_count > 0:
        planning_quality = "partial"
    elif partial_count > 0:
        planning_quality = "fallback"

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
        selected_signal_ids=[source.signal_id for source in sources],
        targets=targets,
        coverage=coverage,
    )


def _extract_protocol_metadata(signal_metadata: dict[str, Any]) -> tuple[str | None, dict[str, Any]]:
    if not isinstance(signal_metadata, dict) or not signal_metadata:
        return None, {}

    if isinstance(signal_metadata.get("protocol_metadata"), dict):
        protocol_metadata = dict(signal_metadata["protocol_metadata"])
        protocol = _resolve_protocol_name(signal_metadata, protocol_metadata)
        return protocol, protocol_metadata

    if isinstance(signal_metadata.get("iec61850"), dict):
        protocol_metadata = dict(signal_metadata["iec61850"])
        protocol = _resolve_protocol_name(signal_metadata, protocol_metadata, default="iec61850")
        return protocol, protocol_metadata

    if isinstance(signal_metadata.get("protocol"), str):
        protocol = str(signal_metadata["protocol"]).strip() or None
        return protocol, {}

    return None, {}


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
    expected_feedback_path_source: str,
) -> tuple[str, str | None]:
    if not source.unit_id:
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
    return "exact", None

