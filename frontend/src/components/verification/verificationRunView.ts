import type { VerificationRunDetailResponse, VerificationVerdictExplanationSignal } from "@/types/verification"

export type VerificationRunViewSignal = {
  title: string
  output: string
  expected: string
  observed: string
  ied: string
  rcb: string
  dataset: string
  latency: string
  reason: string
  verdictState: string
  evidenceStatus: string
}

export type VerificationRunViewModel = {
  headline: string
  summary: string
  testRunId: string
  verdictState: string
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

export function buildVerificationRunView(result: VerificationRunDetailResponse | null): VerificationRunViewModel | null {
  if (!result) {
    return null
  }

  const signals = result.verdict_explanation.signals.map((signal) => ({
    title: resolveFieldValue(signal.signal_reference),
    output: resolveFieldValue(signal.signal_path),
    expected: resolveFieldValue(signal.expected_path),
    observed: resolveObservedPath(signal),
    ied: resolveFieldValue(signal.source_ied ?? signal.endpoint_id),
    rcb: resolveFieldValue(signal.rpt_id),
    dataset: resolveFieldValue(signal.dataset),
    latency: resolveLatency(signal.latency_ms),
    reason: resolveFieldValue(signal.reason),
    verdictState: signal.verdict_state.toUpperCase(),
    evidenceStatus: signal.evidence_status.toUpperCase(),
  }))

  return {
    headline: result.verdict_explanation.headline,
    summary: result.verdict_explanation.summary,
    testRunId: result.test_run_id,
    verdictState: result.verdict_explanation.verdict_state.toUpperCase(),
    evidenceCount: result.verification_run.evidence_set.summary.evidence_count,
    signals,
    diagnostics: result.verdict_explanation.diagnostics.map((diagnostic) => `${diagnostic.code}: ${diagnostic.message}`),
  }
}
