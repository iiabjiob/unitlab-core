from __future__ import annotations

from typing import cast
from collections.abc import Iterable, Sequence

from app.schemas.verification_schema import (
    SignalVerificationEvidenceSchema,
    VerificationConfidenceLevel,
    VerificationSessionSnapshotSchema,
    VerificationStepSchema,
    VerificationSubscriptionSnapshotSchema,
    VerificationSubscriptionPlanGroupSchema,
    VerificationTargetSchema,
)

_CONFIDENCE_RANK: dict[VerificationConfidenceLevel, int] = {
    "unknown": 0,
    "simulated": 1,
    "simulated_fallback": 2,
    "discovery_match": 3,
    "exact_report_match": 4,
    "exact_iec61850": 5,
    "degraded": -1,
}

_CONFIDENCE_REASON: dict[VerificationConfidenceLevel, str] = {
    "exact_iec61850": "exact_report_control_match",
    "exact_report_match": "exact_dataset_match",
    "discovery_match": "discovery_match",
    "simulated_fallback": "fallback_planning_used",
    "simulated": "simulator_generated_report",
    "degraded": "degraded_recovery_state",
    "unknown": "unknown",
}


def _confidence_sort_key(step: VerificationStepSchema) -> tuple[int, int, int, str]:
    return (
        _CONFIDENCE_RANK.get(step.verification_confidence, 0),
        int(step.signal_id),
        int(step.target_index),
        str(step.step_id),
    )


def derive_step_confidence(
    *,
    target: VerificationTargetSchema,
    evidence: SignalVerificationEvidenceSchema,
    group: VerificationSubscriptionPlanGroupSchema | None = None,
) -> tuple[VerificationConfidenceLevel, str]:
    if evidence.evidence_status != "observed":
        return "degraded", _CONFIDENCE_REASON["degraded"]

    if group is not None and group.source_classification == "from discovery":
        return "discovery_match", _CONFIDENCE_REASON["discovery_match"]

    if target.coverage_state == "partial" or (group is not None and group.source_classification == "fallback"):
        return "simulated_fallback", _CONFIDENCE_REASON["simulated_fallback"]

    endpoint_id = _resolve_endpoint_id(target, evidence)
    if _is_simulated_endpoint(endpoint_id):
        if group is not None and group.source_classification == "from SCD":
            return "exact_report_match", _CONFIDENCE_REASON["exact_report_match"]
        return "simulated", _CONFIDENCE_REASON["simulated"]

    if group is not None and group.source_classification == "from SCD":
        return "exact_iec61850", _CONFIDENCE_REASON["exact_iec61850"]

    if target.coverage_state == "exact":
        return "exact_report_match", _CONFIDENCE_REASON["exact_report_match"]

    return "unknown", _CONFIDENCE_REASON["unknown"]


def derive_run_confidence(
    *,
    steps: Sequence[VerificationStepSchema],
    session_snapshots: Sequence[VerificationSessionSnapshotSchema] = (),
    subscription_snapshots: Sequence[VerificationSubscriptionSnapshotSchema] = (),
) -> tuple[VerificationConfidenceLevel, str]:
    if any(snapshot.runtime_state != "reporting" for snapshot in session_snapshots):
        return "degraded", _CONFIDENCE_REASON["degraded"]

    if any(
        snapshot.subscription_state != "reporting" or snapshot.report_health != "healthy"
        for snapshot in subscription_snapshots
    ):
        return "degraded", _CONFIDENCE_REASON["degraded"]

    if not steps:
        return "unknown", _CONFIDENCE_REASON["unknown"]

    weakest = min(steps, key=_confidence_sort_key)
    if weakest.verification_confidence == "degraded":
        return "degraded", _CONFIDENCE_REASON["degraded"]
    return weakest.verification_confidence, weakest.confidence_reason or _CONFIDENCE_REASON[weakest.verification_confidence]


def confidence_reason_for_level(level: VerificationConfidenceLevel) -> str:
    return _CONFIDENCE_REASON.get(level, "unknown")


def sort_confidence_levels(levels: Iterable[VerificationConfidenceLevel]) -> VerificationConfidenceLevel:
    normalized = [level for level in levels if level in _CONFIDENCE_RANK]
    if not normalized:
        return "unknown"
    if any(level == "degraded" for level in normalized):
        return "degraded"
    return cast(
        VerificationConfidenceLevel,
        min(normalized, key=lambda level: _CONFIDENCE_RANK[cast(VerificationConfidenceLevel, level)]),
    )


def _resolve_endpoint_id(
    target: VerificationTargetSchema,
    evidence: SignalVerificationEvidenceSchema,
) -> str:
    return str(target.endpoint_id or evidence.endpoint_id or "").strip()


def _is_simulated_endpoint(endpoint_id: str) -> bool:
    return endpoint_id.startswith("sim:")
