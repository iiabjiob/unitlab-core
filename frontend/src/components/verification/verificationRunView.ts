import type { VerificationRunDetailResponse, VerificationVerdictExplanationSignal } from "@/types/verification"

export type VerificationRunViewSignal = {
  signalId: number
  title: string
  output: string
  expected: string
  observed: string
  unit: string
  endpoint: string
  rcb: string
  dataset: string
  latency: string
  reason: string
  verificationConfidence: string
  confidenceReason: string
  verdictState: string
  evidenceStatus: string
}

export type VerificationRunViewModel = {
  headline: string
  summary: string
  testRunId: string
  verdictState: string
  verificationConfidence: string
  confidenceReason: string
  evidenceCount: number
  signals: VerificationRunViewSignal[]
  diagnostics: string[]
}

function resolveObservedPath(signal: VerificationVerdictExplanationSignal): string {
  return String(signal.observed_path ?? "—").trim() || "—"
}

function resolveFieldValue(value: unknown): string {
  const text = String(value ?? "").trim()
  return text || "—"
}

function resolveLatency(latencyMs: number | null | undefined): string {
  return Number.isFinite(latencyMs as number) ? `${Number(latencyMs)} ms` : "—"
}

function resolveConfidenceLabel(value: string | null | undefined): string {
  const text = String(value ?? "").trim().split("_").join(" ")
  return text ? text.toUpperCase() : "—"
}

export function buildVerificationRunView(result: VerificationRunDetailResponse | null): VerificationRunViewModel | null {
  if (!result) {
    return null
  }

  const targetBySignalId = new Map(
    result.verification_run.verification_targets.map((target) => [target.signal_id, target]),
  )
  const stepBySignalId = new Map(
    result.verification_run.verification_steps.map((step) => [step.signal_id, step]),
  )

  const signals = result.verdict_explanation.signals.map((signal) => ({
    signalId: signal.signal_id,
    title: resolveFieldValue(signal.signal_reference),
    output: resolveFieldValue(signal.signal_path),
    expected: resolveFieldValue(signal.expected_path),
    observed: resolveObservedPath(signal),
    unit: resolveFieldValue(targetBySignalId.get(signal.signal_id)?.unit_id ?? signal.source_ied ?? signal.endpoint_id),
    endpoint: resolveFieldValue(targetBySignalId.get(signal.signal_id)?.endpoint_id ?? signal.endpoint_id),
    rcb: resolveFieldValue(signal.rpt_id),
    dataset: resolveFieldValue(signal.dataset),
    latency: resolveLatency(signal.latency_ms),
    reason: resolveFieldValue(signal.reason),
    verificationConfidence: resolveConfidenceLabel(stepBySignalId.get(signal.signal_id)?.verification_confidence),
    confidenceReason: resolveFieldValue(stepBySignalId.get(signal.signal_id)?.confidence_reason),
    verdictState: signal.verdict_state.toUpperCase(),
    evidenceStatus: signal.evidence_status.toUpperCase(),
  }))

  return {
    headline: result.verdict_explanation.headline,
    summary: result.verdict_explanation.summary,
    testRunId: result.test_run_id,
    verdictState: result.verdict_explanation.verdict_state.toUpperCase(),
    verificationConfidence: resolveConfidenceLabel(result.verdict_explanation.verification_confidence),
    confidenceReason: resolveFieldValue(result.verdict_explanation.confidence_reason),
    evidenceCount: result.verification_run.evidence_set.summary.evidence_count,
    signals,
    diagnostics: result.verdict_explanation.diagnostics.map((diagnostic) => `${diagnostic.code}: ${diagnostic.message}`),
  }
}
