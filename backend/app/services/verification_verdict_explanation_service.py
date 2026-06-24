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
    summary = _resolve_summary(verification_run, verdict_state, signals, verification_run.subscription_plan.coverage.planning_quality)
    diagnostics = _collect_diagnostics(verification_run, signals)
    return VerificationVerdictExplanationSchema(
        test_run_id=verification_run.test_run_id,
        verdict_state=verdict_state,
        verification_confidence=verification_run.verification_confidence,
        confidence_reason=verification_run.confidence_reason,
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
    verification_run: VerificationRunSchema,
    verdict_state: str,
    signals: Sequence[VerificationVerdictExplanationSignalSchema],
    planning_quality: str | None = None,
) -> str:
    if not signals:
        return "No evidence was collected for this verification run."

    first = signals[0]
    if verdict_state == "pass":
        latency = f" in {first.latency_ms} ms" if first.latency_ms is not None else ""
        observed = first.observed_path or first.expected_path
        location = first.source_ied or first.endpoint_id or "the selected IED"
        source_clause = _resolve_source_clause(verification_run)
        if planning_quality and planning_quality != "exact":
            summary = f"PASS with fallback planning: observed {observed} on {location}{latency}."
            if source_clause is not None:
                return f"{summary} {source_clause}"
            return f"{summary} Verified by simulator fallback source, not exact IEC 61850 report-control match."
        if source_clause is not None:
            return f"PASS: observed {observed} on {location}{latency}. {source_clause}"
        return f"PASS: observed {observed} on {location}{latency}."
    if verdict_state == "fail" and first.evidence_status == "timeout":
        return _append_source_clause("FAIL: no confirmation arrived before the timeout expired.", verification_run)
    if verdict_state == "fail" and first.evidence_status in {"late", "out_of_window"}:
        return _append_source_clause("FAIL: confirmation arrived outside the allowed window.", verification_run)
    if verdict_state == "fail" and first.evidence_status == "stale":
        return _append_source_clause("FAIL: confirmation was stale and could not be trusted.", verification_run)
    if verdict_state == "fail" and first.evidence_status == "invalid":
        return _append_source_clause("FAIL: confirmation was invalid or incomplete.", verification_run)
    return f"{verdict_state.upper()}: verification completed with derived evidence."


def _append_source_clause(summary: str, verification_run: VerificationRunSchema) -> str:
    source_clause = _resolve_source_clause(verification_run)
    return f"{summary} {source_clause}" if source_clause is not None else summary


def _resolve_source_clause(verification_run: VerificationRunSchema) -> str | None:
    for diagnostic in verification_run.diagnostics:
        if diagnostic.code != "endpoint_resolution_policy" or diagnostic.details is None:
            continue
        transport_source = str(diagnostic.details.get("transport_source") or "").strip()
        model_source = str(diagnostic.details.get("model_source") or "").strip()
        source_parts: list[str] = []
        if transport_source == "explicit_request":
            source_parts.append("transport from explicit request")
        elif transport_source == "settings_catalog":
            source_parts.append("transport from settings catalog")
        elif transport_source == "loaded_scd":
            source_parts.append("transport from loaded SCD")
        elif transport_source == "validation_override":
            source_parts.append("transport from validation override")
        elif transport_source == "signal_list_fallback":
            source_parts.append("transport from signal list fallback")
        elif transport_source == "simulator":
            source_parts.append("simulator transport")
        if model_source == "loaded_scd":
            source_parts.append("model binding from loaded SCD")
        elif model_source == "discovery_fallback":
            source_parts.append("model binding from discovery fallback")
        if source_parts:
            return f"Source: {'; '.join(source_parts)}."
    return None


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
    if verification_run.subscription_plan.coverage.planning_quality != "exact":
        add(
            VerificationEvidenceDiagnosticSchema(
                code="fallback_planning",
                message="Verification run used fallback planning and did not achieve an exact IEC 61850 report-control match.",
                severity="warning",
            )
        )
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
