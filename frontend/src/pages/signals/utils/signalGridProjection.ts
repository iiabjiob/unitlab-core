import type { SignalAllocationRow } from "@/types/signal"
import type { ExternalIedStatus } from "@/stores/externalIedStore"
import { resolveRuntimeChannelTypeForSignal } from "@/utils/signalRuntimeMapping"
import { applyRuntimeTestedAt } from "@/pages/signals/utils/runtimeProjection"
import { extractSourceRowFromSignalMetadata } from "@/pages/signals/utils/sourceColumns"
import { resolveOnline61850TransportHost } from "@/pages/signals/utils/online61850Targets"
import {
  resolveSignalAllocationHealthLabel,
  resolveSignalAllocationStatus,
} from "@/pages/signals/utils/allocationHealth"

type TestedAtResolver = (
  signalId: number | null | undefined,
  workspaceId?: number | null,
) => string | null

type TestStatusResolver = (
  signalId: number | null | undefined,
  workspaceId?: number | null,
) => string | null

export type SignalGridRow = Record<string, unknown> & {
  signal_id: number
  rowId: string
}

type SignalGridRuntimeOverlay = {
  workspaceId?: number | null
  getTestedAt?: TestedAtResolver
  getTestStatus?: TestStatusResolver
  getExternalIedStatus?: (row: SignalAllocationRow) => ExternalIedStatus
}

type SignalGridProjectionPatch = {
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

function resolveSignalGridInternalSignalType(row: SignalAllocationRow): string {
  const resolved = resolveRuntimeChannelTypeForSignal(String(row.signal_direction ?? "").trim())
  return resolved ?? "-"
}

function applySignalGridRuntimeOverlay(
  row: SignalAllocationRow,
  runtime?: SignalGridRuntimeOverlay,
): SignalAllocationRow {
  if (!runtime?.getTestedAt) {
    return row
  }
  return applyRuntimeTestedAt(row, runtime.workspaceId, runtime.getTestedAt)
}

function resolveRuntimeTestStatus(row: SignalAllocationRow, runtime?: SignalGridRuntimeOverlay): string {
  const runtimeStatus = String(runtime?.getTestStatus?.(row.signal_id, runtime.workspaceId) ?? "").trim()
  if (runtimeStatus) {
    return runtimeStatus
  }

  if (!String(row.tested_at ?? "").trim()) {
    return ""
  }

  const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
  const hasIec61850Metadata = Boolean(
    String(sourceRow.iec61850_address ?? sourceRow.iec61850Address ?? "").trim()
    || resolveOnline61850TransportHost(row) !== null,
  )
  return hasIec61850Metadata ? "not_validated" : "tested"
}

function resolveExternalIedIp(row: SignalAllocationRow): string {
  return resolveOnline61850TransportHost(row)?.host ?? ""
}

function resolveExternalIedPort(row: SignalAllocationRow): number {
  return resolveOnline61850TransportHost(row)?.port ?? 102
}

function resolveExternalIedStatus(row: SignalAllocationRow, runtime?: SignalGridRuntimeOverlay): ExternalIedStatus {
  return runtime?.getExternalIedStatus?.(row) ?? "not_applicable"
}

function createSignalGridRow(
  row: SignalAllocationRow,
  headers: readonly string[],
  runtime?: SignalGridRuntimeOverlay,
): SignalGridRow {
  const projectedRow = applySignalGridRuntimeOverlay(row, runtime)
  const sourceRow = extractSourceRowFromSignalMetadata(projectedRow.signal_metadata)
  const payload: SignalGridRow = {
    signal_id: projectedRow.signal_id,
    rowId: projectedRow.row_id || `signal-${projectedRow.signal_id}`,
    internal_signal_type: resolveSignalGridInternalSignalType(projectedRow),
    iec61850_address: String(sourceRow.iec61850_address ?? sourceRow.iec61850Address ?? ""),
    external_ied_ip: resolveExternalIedIp(projectedRow),
    external_ied_port: resolveExternalIedPort(projectedRow),
    external_ied_status: resolveExternalIedStatus(projectedRow, runtime),
    channel_select: resolveSignalAllocationDisplayLabel(projectedRow),
    test_status: resolveRuntimeTestStatus(projectedRow, runtime),
    tested_at: projectedRow.tested_at,
    allocation_status: resolveSignalAllocationStatus(projectedRow),
    allocation_health: resolveSignalAllocationHealthLabel(projectedRow),
  }
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
