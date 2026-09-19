from __future__ import annotations

from app.schemas.verification_schema import (
    VerificationConfidenceLevel,
    VerificationSessionSnapshotSchema,
    VerificationStepSchema,
    VerificationSubscriptionSnapshotSchema,
)
from app.services.verification_confidence import derive_run_confidence


def _step(*, signal_id: int, confidence: VerificationConfidenceLevel, reason: str, step_id: str | None = None) -> VerificationStepSchema:
    return VerificationStepSchema(
        step_id=step_id or f"step-{signal_id}",
        signal_id=signal_id,
        target_index=signal_id,
        session_id="run-1:session",
        subscription_id=f"run-1:subscription-{signal_id}",
        step_state="completed",
        expected_path="expected",
        expected_window_ms=1000,
        evidence_status="observed",
        verdict_state="pass",
        evidence_ids=[f"ev-{signal_id}"],
        verification_confidence=confidence,
        confidence_reason=reason,
        diagnostics=[],
    )


def _healthy_snapshots() -> tuple[list[VerificationSessionSnapshotSchema], list[VerificationSubscriptionSnapshotSchema]]:
    return (
        [
            VerificationSessionSnapshotSchema(
                session_id="run-1:session",
                endpoint_id="sim:IED-A/P1",
                runtime_state="reporting",
                connection_generation=1,
                discovery_status="available",
            )
        ],
        [
            VerificationSubscriptionSnapshotSchema(
                subscription_id="run-1:subscription-1",
                session_id="run-1:session",
                endpoint_id="sim:IED-A/P1",
                subscription_state="reporting",
                report_health="healthy",
            )
        ],
    )


def test_derive_run_confidence_uses_weakest_step_independent_of_input_order() -> None:
    steps = [
        _step(signal_id=8, confidence="exact_report_match", reason="exact_report_control_match", step_id="step-8"),
        _step(signal_id=2, confidence="exact_report_match", reason="exact_dataset_match", step_id="step-2"),
    ]

    level, reason = derive_run_confidence(steps=steps, session_snapshots=[], subscription_snapshots=[])
    reversed_level, reversed_reason = derive_run_confidence(steps=list(reversed(steps)), session_snapshots=[], subscription_snapshots=[])

    assert level == "exact_report_match"
    assert reason == "exact_dataset_match"
    assert reversed_level == "exact_report_match"
    assert reversed_reason == "exact_dataset_match"


def test_derive_run_confidence_caps_at_simulated_fallback_for_mixed_steps() -> None:
    steps = [
        _step(signal_id=1, confidence="exact_iec61850", reason="exact_report_control_match"),
        _step(signal_id=2, confidence="simulated_fallback", reason="fallback_planning_used"),
    ]

    level, reason = derive_run_confidence(steps=steps, session_snapshots=[], subscription_snapshots=[])

    assert level == "simulated_fallback"
    assert reason == "fallback_planning_used"


def test_derive_run_confidence_caps_at_degraded_when_runtime_health_is_unhealthy() -> None:
    steps = [
        _step(signal_id=1, confidence="exact_iec61850", reason="exact_report_control_match"),
    ]
    session_snapshots, subscription_snapshots = _healthy_snapshots()
    subscription_snapshots[0] = subscription_snapshots[0].model_copy(update={"report_health": "degraded"})

    level, reason = derive_run_confidence(steps=steps, session_snapshots=session_snapshots, subscription_snapshots=subscription_snapshots)

    assert level == "degraded"
    assert reason == "degraded_recovery_state"
