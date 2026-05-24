import type { SignalAllocationRow } from "@/types/signal"
import { resolveRuntimeChannelTypeForSignal } from "@/utils/signalRuntimeMapping"
import {
  applyRuntimeTestedAt,
  type TestedAtResolver,
} from "@/pages/signals/utils/runtimeProjection"
import { extractSourceRowFromSignalMetadata } from "@/pages/signals/utils/sourceColumns"
import {
  resolveSignalAllocationHealthLabel,
  resolveSignalAllocationStatus,
} from "@/pages/signals/utils/allocationHealth"

export type SignalGridRow = Record<string, unknown> & {
  signal_id: number
  rowId: string
}

export type SignalGridRuntimeOverlay = {
  workspaceId?: number | null
  getTestedAt?: TestedAtResolver
}

export type SignalGridProjectionPatch = {
  rowId: string
  changes: Partial<SignalGridRow>
  columns?: readonly string[]
}

export function signalGridSourceColumnKey(index: number): string {
  return `source_col_${index}`
}

export function resolveSignalAllocationDisplayLabel(row: SignalAllocationRow): string {
  if (Number.isFinite(row.channel_index as number)) {
    const channelSuffix = `ch${Number(row.channel_index) + 1}`
    const unitId = String(row.unit_id ?? "").trim()
    return unitId ? `${unitId}/${channelSuffix}` : channelSuffix
  }
  if (row.channel_label && row.channel_label.trim().length > 0) {
    return row.channel_label
  }
  return "-"
}

export function resolveSignalGridInternalSignalType(row: SignalAllocationRow): string {
  const resolved = resolveRuntimeChannelTypeForSignal(String(row.signal_direction ?? "").trim())
  return resolved ?? "-"
}

export function applySignalGridRuntimeOverlay(
  row: SignalAllocationRow,
  runtime?: SignalGridRuntimeOverlay,
): SignalAllocationRow {
  if (!runtime?.getTestedAt) {
    return row
  }
  return applyRuntimeTestedAt(row, runtime.workspaceId, runtime.getTestedAt)
}

export function createSignalGridRow(
  row: SignalAllocationRow,
  headers: readonly string[],
  runtime?: SignalGridRuntimeOverlay,
): SignalGridRow {
  const projectedRow = applySignalGridRuntimeOverlay(row, runtime)
  const payload: SignalGridRow = {
    signal_id: projectedRow.signal_id,
    rowId: projectedRow.row_id || `signal-${projectedRow.signal_id}`,
    internal_signal_type: resolveSignalGridInternalSignalType(projectedRow),
    channel_select: resolveSignalAllocationDisplayLabel(projectedRow),
    tested_at: projectedRow.tested_at,
    allocation_status: resolveSignalAllocationStatus(projectedRow),
    allocation_health: resolveSignalAllocationHealthLabel(projectedRow),
  }
  const sourceRow = extractSourceRowFromSignalMetadata(projectedRow.signal_metadata)
  headers.forEach((header, index) => {
    payload[signalGridSourceColumnKey(index)] = sourceRow[header] ?? ""
  })
  return payload
}

export function createSignalGridRows(
  rows: readonly SignalAllocationRow[],
  headers: readonly string[],
  runtime?: SignalGridRuntimeOverlay,
): SignalGridRow[] {
  return rows.map(row => createSignalGridRow(row, headers, runtime))
}

function pickSignalGridPatchChanges(
  gridRow: SignalGridRow,
  columns: readonly string[] | undefined,
): Partial<SignalGridRow> {
  if (!columns?.length) {
    return gridRow
  }

  const changes: Partial<SignalGridRow> = {}
  columns.forEach((column) => {
    const key = String(column ?? "").trim()
    if (!key || !(key in gridRow)) {
      return
    }
    changes[key] = gridRow[key]
  })
  return changes
}

export function createSignalGridRowPatch(
  row: SignalAllocationRow,
  headers: readonly string[],
  columns?: readonly string[],
  runtime?: SignalGridRuntimeOverlay,
): SignalGridProjectionPatch {
  const gridRow = createSignalGridRow(row, headers, runtime)
  return {
    rowId: gridRow.rowId,
    changes: pickSignalGridPatchChanges(gridRow, columns),
    columns,
  }
}
