import type { SignalAllocationRow } from "@/types/signal"

export type VerificationSelectionSummary = {
  signalIds: number[]
  unitId: string | null
  label: string | null
  canRun: boolean
  error: string | null
}

export function resolveVerificationSelection(rows: SignalAllocationRow[]): VerificationSelectionSummary {
  const selectedRows = rows.filter((row) => (
    Number.isFinite(row.channel_id as number)
    && Number.isFinite(row.device_id as number)
    && Boolean(String(row.unit_id ?? "").trim())
  ))
  if (selectedRows.length === 0) {
    return {
      signalIds: [],
      unitId: null,
      label: null,
      canRun: false,
      error: "Select one or more allocated signals.",
    }
  }

  const unitIds = [...new Set(selectedRows.map((row) => String(row.unit_id ?? "").trim()).filter(Boolean))]
  const signalIds = selectedRows.map((row) => row.signal_id)
  const unitId = unitIds.length === 1 ? unitIds[0] : null
  const label = unitIds.length === 1
    ? `${signalIds.length} selected signal${signalIds.length === 1 ? "" : "s"} on ${unitIds[0]}`
    : `${signalIds.length} selected signal${signalIds.length === 1 ? "" : "s"} across ${unitIds.length} IEDs`

  return {
    signalIds,
    unitId,
    label,
    canRun: true,
    error: null,
  }
}
