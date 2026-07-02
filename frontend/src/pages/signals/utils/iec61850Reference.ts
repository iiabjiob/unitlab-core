import { normalizeReportDataReference } from "@/modules/iec61850-report-core/reportEventNormalizer"
import type { Iec61850ReportControlCandidate } from "@/modules/iec61850-report-core/types"

const REFERENCE_NORMALIZATION_CANDIDATE = {
  iedName: "",
  logicalDeviceInst: "",
} as Iec61850ReportControlCandidate

export function normalizeIec61850Reference(reference: string): string {
  return normalizeReportDataReference(String(reference ?? ""), REFERENCE_NORMALIZATION_CANDIDATE)
}
