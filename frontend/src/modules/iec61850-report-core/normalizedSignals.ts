import type { Iec61850ReportControlCandidate } from "./types"

type TransitionalDiagnostic = {
  severity: "warning" | "error" | "info"
  code: string
  message: string
  [key: string]: unknown
}

export type Iec61850ReportSignalReference = {
  reference: string
}

export function getIec61850ReportCandidateSignals(
  candidate: Iec61850ReportControlCandidate,
  diagnostics?: TransitionalDiagnostic[],
): Iec61850ReportSignalReference[] {
  if (candidate.normalizedSignals.length > 0) {
    return candidate.normalizedSignals
  }

  if (diagnostics) {
    diagnostics.push({
      severity: "warning",
      code: "TRANSITIONAL_RAW_SIGNAL_FALLBACK",
      message: `Report candidate "${candidate.reportControlName}" is using transitional raw dataset-member fallback because normalized DataTypeTemplates resolution is unavailable.`,
      context: {
        datasetRef: candidate.dataSetRef,
        memberRef: undefined,
        iedName: candidate.iedName,
        ldInst: candidate.logicalDeviceInst,
        lnClass: null,
        lnInst: null,
        doName: null,
        daName: null,
        fc: null,
      },
    })
  }

  // TODO: remove this fallback once every report consumer reads normalizedDatasetEntries.leaves.
  return candidate.signals
}
