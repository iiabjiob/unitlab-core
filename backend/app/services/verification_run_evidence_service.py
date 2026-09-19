from __future__ import annotations

from collections.abc import Sequence
from dataclasses import dataclass
from typing import Literal, cast

from app.schemas.verification_schema import (
    SignalVerificationEvidenceSchema,
    SignalVerificationEvidenceSetSchema,
    SignalVerificationEvidenceSetSummarySchema,
    VerificationEvidenceDiagnosticSchema,
    VerificationRunEvidenceResponseSchema,
    VerificationRunStepDetailsSchema,
    VerificationStepSchema,
)
from app.services.verification_evidence import (
    VerificationEvidenceRepository,
    build_signal_verification_evidence_set,
)


@dataclass(frozen=True, slots=True)
class VerificationRunEvidenceResult:
    test_run_id: str
    evidence_set: SignalVerificationEvidenceSetSchema
    evidence_rows: tuple[SignalVerificationEvidenceSchema, ...]
    verification_steps: tuple[VerificationStepSchema, ...]
    diagnostics: tuple[VerificationEvidenceDiagnosticSchema, ...]

    def as_response(self) -> VerificationRunEvidenceResponseSchema:
        return VerificationRunEvidenceResponseSchema(
            test_run_id=self.test_run_id,
            evidence_set=self.evidence_set,
            evidence_rows=list(self.evidence_rows),
            verification_steps=list(self.verification_steps),
            diagnostics=list(self.diagnostics),
        )

    def as_step_response(self) -> VerificationRunStepDetailsSchema:
        return VerificationRunStepDetailsSchema(
            test_run_id=self.test_run_id,
            verification_steps=list(self.verification_steps),
            diagnostics=list(self.diagnostics),
        )


async def load_verification_run_evidence(
    *,
    workspace_id: int,
    test_run_id: str,
    repository: VerificationEvidenceRepository,
) -> VerificationRunEvidenceResult:
    raw_evidence_rows = await repository.list_signal_verification_evidence(
        workspace_id=workspace_id,
        test_run_id=test_run_id,
    )
    evidence_rows = tuple(
        SignalVerificationEvidenceSchema.model_validate(row, from_attributes=True)
        for row in raw_evidence_rows
    )
    evidence_set = await repository.get_signal_verification_evidence_set(
        workspace_id=workspace_id,
        test_run_id=test_run_id,
    )
    if evidence_set is None:
        evidence_set_schema = build_signal_verification_evidence_set(
            test_run_id=test_run_id,
            evidence=evidence_rows,
            diagnostics=(),
        )
        diagnostics: list[VerificationEvidenceDiagnosticSchema] = []
    else:
        evidence_set_schema = SignalVerificationEvidenceSetSchema(
            test_run_id=evidence_set.test_run_id,
            evidence=list(evidence_rows),
            summary=SignalVerificationEvidenceSetSummarySchema.model_validate(evidence_set.summary),
            diagnostics=[
                VerificationEvidenceDiagnosticSchema.model_validate(item)
                for item in evidence_set.diagnostics
            ],
        )
        diagnostics = [VerificationEvidenceDiagnosticSchema.model_validate(item) for item in evidence_set.diagnostics]

    verification_steps = _project_verification_steps(test_run_id=test_run_id, evidence_rows=evidence_rows)
    return VerificationRunEvidenceResult(
        test_run_id=test_run_id,
        evidence_set=evidence_set_schema,
        evidence_rows=evidence_rows,
        verification_steps=tuple(verification_steps),
        diagnostics=tuple(diagnostics),
    )


def _project_verification_steps(
    *,
    test_run_id: str,
    evidence_rows: Sequence[SignalVerificationEvidenceSchema],
) -> list[VerificationStepSchema]:
    steps: list[VerificationStepSchema] = []
    for target_index, evidence in enumerate(evidence_rows):
        step_state = _resolve_step_state(evidence_status=evidence.evidence_status)
        verdict_state = _resolve_step_verdict_state(evidence_status=evidence.evidence_status)
        verification_confidence = _projected_step_confidence(evidence)
        confidence_reason = _projected_step_confidence_reason(evidence)
        steps.append(
            VerificationStepSchema(
                step_id=evidence.evidence_id,
                signal_id=evidence.signal_id,
                target_index=target_index,
                session_id=_resolve_session_id(test_run_id=test_run_id, evidence=evidence),
                subscription_id=_resolve_subscription_id(test_run_id=test_run_id, evidence=evidence),
                group_id=None,
                step_state=cast(
                    Literal["draft", "planned", "armed", "running", "awaiting_confirmation", "completing", "completed", "aborted", "failed"],
                    step_state,
                ),
                expected_path=evidence.expected_path,
                expected_window_ms=0,
                freshness=evidence.freshness,
                evidence_status=evidence.evidence_status,
                verdict_state=cast(
                    Literal["pending", "pass", "fail", "inconclusive", "aborted"],
                    verdict_state,
                ),
                evidence_ids=[evidence.evidence_id],
                actual_report_path=evidence.actual_report_path,
                source_session_id=_resolve_session_id(test_run_id=test_run_id, evidence=evidence),
                source_subscription_id=_resolve_subscription_id(test_run_id=test_run_id, evidence=evidence),
                source_generation=evidence.source_generation,
                source_report_rpt_id=evidence.rpt_id,
                source_report_dat_set=evidence.dataset,
                verification_confidence=cast(
                    Literal[
                        "exact_iec61850",
                        "exact_report_match",
                        "discovery_match",
                        "simulated_fallback",
                        "simulated",
                        "degraded",
                        "unknown",
                    ],
                    verification_confidence,
                ),
                confidence_reason=confidence_reason,
                triggered_at=evidence.observed_at,
                observed_at=evidence.observed_at,
                latency_ms=evidence.latency_ms,
                reason=evidence.reason_code,
                diagnostics=list(evidence.diagnostics),
            )
        )
    return steps


def _resolve_session_id(*, test_run_id: str, evidence: SignalVerificationEvidenceSchema) -> str:
    return f"{test_run_id}:{evidence.endpoint_id}" if evidence.endpoint_id else f"{test_run_id}:unknown-session"


def _resolve_subscription_id(*, test_run_id: str, evidence: SignalVerificationEvidenceSchema) -> str:
    report_reference = str(evidence.rpt_id or evidence.dataset or "").strip()
    endpoint_id = str(evidence.endpoint_id or "unknown").strip()
    if report_reference:
        return f"{test_run_id}:{endpoint_id}:{report_reference}"
    return f"{test_run_id}:{endpoint_id}:subscription"


def _resolve_step_state(*, evidence_status: str) -> str:
    if evidence_status == "timeout":
        return "failed"
    if evidence_status in {"invalid", "stale", "late", "out_of_window", "observed"}:
        return "completed"
    return "failed"


def _resolve_step_verdict_state(*, evidence_status: str) -> str:
    if evidence_status == "observed":
        return "pass"
    if evidence_status in {"timeout", "invalid", "stale", "late", "out_of_window"}:
        return "fail"
    return "inconclusive"


def _projected_step_confidence(evidence: SignalVerificationEvidenceSchema) -> str:
    if evidence.evidence_status != "observed":
        return "degraded"
    if str(evidence.endpoint_id or "").startswith("sim:"):
        return "simulated"
    return "exact_iec61850"


def _projected_step_confidence_reason(evidence: SignalVerificationEvidenceSchema) -> str:
    if evidence.evidence_status != "observed":
        return "degraded_recovery_state"
    if str(evidence.endpoint_id or "").startswith("sim:"):
        return "simulator_generated_report"
    return "exact_report_control_match"
