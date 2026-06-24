from __future__ import annotations

from dataclasses import dataclass
from typing import Sequence

from app.schemas.verification_schema import (
    SignalVerificationEvidenceSchema,
    VerificationEvidenceDiagnosticSchema,
    VerificationRunSchema,
    VerificationStepSchema,
    VerificationVerdictExplanationSchema,
    VerificationVerdictExplanationSignalSchema,
)


@dataclass(frozen=True, slots=True)
class VerificationVerdictExplanationResult:
    test_run_id: str
    verdict_explanation: VerificationVerdictExplanationSchema

    def as_response(self) -> VerificationVerdictExplanationSchema:
        return self.verdict_explanation


def build_verification_verdict_explanation(
    *,
    verification_run: VerificationRunSchema,
    verification_steps: Sequence[VerificationStepSchema],
    evidence_rows: Sequence[SignalVerificationEvidenceSchema],
) -> VerificationVerdictExplanationSchema:
    evidence_by_signal_id = {evidence.signal_id: evidence for evidence in evidence_rows}
    step_verdicts = [step.verdict_state for step in verification_steps]
    verdict_state = _resolve_verdict_state(step_verdicts)
    signals: list[VerificationVerdictExplanationSignalSchema] = []

    for step in verification_steps:
        evidence = evidence_by_signal_id.get(step.signal_id)
        if evidence is None:
            continue
        signals.append(
            VerificationVerdictExplanationSignalSchema(
                signal_id=step.signal_id,
                signal_reference=_resolve_signal_reference(verification_run, step.signal_id, evidence),
                signal_path=evidence.signal_path,
                expected_path=step.expected_path,
                observed_path=evidence.actual_report_path,
                source_ied=evidence.source_ied,
                endpoint_id=evidence.endpoint_id,
                rpt_id=evidence.rpt_id,
                dataset=evidence.dataset,
                evidence_status=evidence.evidence_status,
                verdict_state=step.verdict_state,
                latency_ms=evidence.latency_ms,
                reason=_resolve_signal_reason(step, evidence),
                diagnostics=list(evidence.diagnostics),
            )
        )

    headline = _resolve_headline(verdict_state)
    summary = _resolve_summary(verdict_state, signals)
    diagnostics = _collect_diagnostics(verification_run, signals)
    return VerificationVerdictExplanationSchema(
        test_run_id=verification_run.test_run_id,
        verdict_state=verdict_state,
        headline=headline,
        summary=summary,
        signals=signals,
        diagnostics=diagnostics,
    )


def _resolve_signal_reference(
    verification_run: VerificationRunSchema,
    signal_id: int,
    evidence: SignalVerificationEvidenceSchema,
) -> str:
    for target in verification_run.verification_targets:
        if int(target.signal_id) == int(signal_id):
            return target.signal_reference
    return evidence.signal_path


def _resolve_signal_reason(
    step: VerificationStepSchema,
    evidence: SignalVerificationEvidenceSchema,
) -> str:
    if evidence.evidence_status == "observed":
        return "expected feedback observed within the verification window"
    if evidence.evidence_status == "late":
        return "feedback observed after the allowed window"
    if evidence.evidence_status == "out_of_window":
        return "feedback observed outside the allowed timeout window"
    if evidence.evidence_status == "stale":
        return "feedback arrived from a stale generation"
    if evidence.evidence_status == "invalid":
        return "report confirmation was invalid"
    if evidence.evidence_status == "timeout":
        return "no report confirmation arrived before timeout"
    return step.reason or evidence.reason_code


def _resolve_summary(
    verdict_state: str,
    signals: Sequence[VerificationVerdictExplanationSignalSchema],
) -> str:
    if not signals:
        return "No evidence was collected for this verification run."

    first = signals[0]
    if verdict_state == "pass":
        latency = f" in {first.latency_ms} ms" if first.latency_ms is not None else ""
        observed = first.observed_path or first.expected_path
        location = first.source_ied or first.endpoint_id or "the selected IED"
        return f"PASS: observed {observed} on {location}{latency}."
    if verdict_state == "fail" and first.evidence_status == "timeout":
        return "FAIL: no confirmation arrived before the timeout expired."
    if verdict_state == "fail" and first.evidence_status in {"late", "out_of_window"}:
        return "FAIL: confirmation arrived outside the allowed window."
    if verdict_state == "fail" and first.evidence_status == "stale":
        return "FAIL: confirmation was stale and could not be trusted."
    if verdict_state == "fail" and first.evidence_status == "invalid":
        return "FAIL: confirmation was invalid or incomplete."
    return f"{verdict_state.upper()}: verification completed with derived evidence."


def _resolve_headline(verdict_state: str) -> str:
    if verdict_state == "pass":
        return "PASS"
    if verdict_state == "fail":
        return "FAIL"
    return verdict_state.upper()


def _collect_diagnostics(
    verification_run: VerificationRunSchema,
    signals: Sequence[VerificationVerdictExplanationSignalSchema],
) -> list[VerificationEvidenceDiagnosticSchema]:
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = []
    seen: set[tuple[str, str]] = set()

    def add(diagnostic: VerificationEvidenceDiagnosticSchema) -> None:
        key = (diagnostic.code, diagnostic.message)
        if key in seen:
            return
        seen.add(key)
        diagnostics.append(diagnostic)

    for diagnostic in verification_run.diagnostics:
        add(diagnostic)
    for signal in signals:
        for diagnostic in signal.diagnostics:
            add(diagnostic)
    if not diagnostics:
        add(
            VerificationEvidenceDiagnosticSchema(
                code="verdict_explained",
                message="Derived from persisted verification evidence.",
            )
        )
    return diagnostics


def _resolve_verdict_state(step_verdicts: Sequence[str]) -> str:
    if not step_verdicts:
        return "pending"
    if any(verdict == "fail" for verdict in step_verdicts):
        return "fail"
    if all(verdict == "pass" for verdict in step_verdicts):
        return "pass"
    if any(verdict == "aborted" for verdict in step_verdicts):
        return "aborted"
    return "inconclusive"
