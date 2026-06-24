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
      error: "Select one or more allocated signals on the same IED.",
    }
  }

  const unitIds = [...new Set(selectedRows.map((row) => String(row.unit_id ?? "").trim()).filter(Boolean))]
  if (unitIds.length !== 1) {
    return {
      signalIds: selectedRows.map((row) => row.signal_id),
      unitId: null,
      label: null,
      canRun: false,
      error: "Select signals from one IED only.",
    }
  }

  const unitId = unitIds[0]
  const signalIds = selectedRows.map((row) => row.signal_id)
  const label = `${signalIds.length} selected signal${signalIds.length === 1 ? "" : "s"} on ${unitId}`

  return {
    signalIds,
    unitId,
    label,
    canRun: true,
    error: null,
  }
}
