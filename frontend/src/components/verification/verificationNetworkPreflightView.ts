import type { VerificationNetworkPreflightResponse } from "@/types/verification"

export type VerificationNetworkPreflightView = {
  headline: string
  summary: string
  stateTone: "ready" | "attention_required" | "unknown"
  runtimeSummary: string
  groupHints: string[]
}

function resolveHeadline(state: VerificationNetworkPreflightView["stateTone"]): string {
  if (state === "ready") return "Network ready for real MMS"
  if (state === "attention_required") return "Network attention required"
  return "Network status unknown"
}

export function buildVerificationNetworkPreflightView(
  result: VerificationNetworkPreflightResponse | null,
): VerificationNetworkPreflightView | null {
  if (!result) {
    return null
  }

  const preflight = result.preflight
  const stateTone = preflight.overall_state
  const requested = preflight.requested_runtime_version.toUpperCase()
  const recommended = preflight.recommended_runtime_version.toUpperCase()
  const runtimeSummary = requested === recommended
    ? `${recommended} selected`
    : `${requested} requested, ${recommended} recommended`

  return {
    headline: resolveHeadline(stateTone),
    summary: preflight.overall_hint,
    stateTone,
    runtimeSummary,
    groupHints: preflight.groups.map((group) => group.operator_hint).filter((item) => Boolean(String(item).trim())),
  }
}
