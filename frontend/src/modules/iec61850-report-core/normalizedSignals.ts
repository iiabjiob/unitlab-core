import type { Iec61850ReportControlCandidate } from "./types"

export type Iec61850ReportSignalReference = {
  reference: string
}

export function getIec61850ReportCandidateSignals(
  candidate: Iec61850ReportControlCandidate,
): Iec61850ReportSignalReference[] {
  if (candidate.normalizedSignals.length > 0) {
    return candidate.normalizedSignals
  }
  return candidate.signals
}
