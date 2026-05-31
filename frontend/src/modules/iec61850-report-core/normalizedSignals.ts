import type { Iec61850ReportControlCandidate } from "./types"
import type { NormalizedDataLeaf } from "../scd-sld-core"

type NormalizedDatasetDiagnostic = {
  severity: "warning" | "error" | "info"
  code: string
  message: string
  [key: string]: unknown
}

export function getIec61850ReportCandidateLeaves(
  candidate: Iec61850ReportControlCandidate,
  diagnostics?: NormalizedDatasetDiagnostic[],
): NormalizedDataLeaf[] {
  const leaves = candidate.normalizedDatasetEntries.flatMap(entry => entry.leaves)
  if (leaves.length > 0) {
    return leaves
  }

  if (diagnostics) {
    diagnostics.push({
      severity: "error",
      code: "NORMALIZED_DATASET_REQUIRED",
      message: `Report candidate "${candidate.reportControlName}" has no normalized dataset leaves and cannot be used for runtime report planning.`,
      context: {
        datasetRef: candidate.dataSetRef,
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

  return []
}
