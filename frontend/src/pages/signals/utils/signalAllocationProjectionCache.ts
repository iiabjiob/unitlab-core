import type { SignalAllocationRow } from "@/types/signal"

type SignalAllocationProjectionPatchResult = {
  changed: number
  missingSignalIds: number[]
}

function normalizeSignalId(value: unknown): number | null {
  const signalId = Number(value)
  return Number.isFinite(signalId) && signalId > 0 ? signalId : null
}

function normalizeChannelId(value: unknown): number | null {
  const channelId = Number(value)
  return Number.isInteger(channelId) && channelId > 0 ? channelId : null
}

function resolveProjectionRowId(row: SignalAllocationRow): string {
  const rowId = String(row.row_id ?? "").trim()
  return rowId || `signal-${row.signal_id}`
}

function cloneSignalMetadata(signalMetadata: SignalAllocationRow["signal_metadata"]): Record<string, unknown> {
  if (!signalMetadata || typeof signalMetadata !== "object" || Array.isArray(signalMetadata)) {
    return {}
  }
  const cloned: Record<string, unknown> = { ...signalMetadata }
  const sourceRow = cloned.row
  if (sourceRow && typeof sourceRow === "object" && !Array.isArray(sourceRow)) {
    cloned.row = { ...(sourceRow as Record<string, unknown>) }
  }
  return cloned
}

function cloneProjectionRow(row: SignalAllocationRow): SignalAllocationRow {
  return {
    ...row,
    row_id: resolveProjectionRowId(row),
    signal_metadata: cloneSignalMetadata(row.signal_metadata),
    allocation_health: row.allocation_health && typeof row.allocation_health === "object"
      ? { ...row.allocation_health }
      : row.allocation_health,
  }
}

export function createSignalAllocationProjectionCache() {
  const rows: SignalAllocationRow[] = []
  const rowsBySignalId = new Map<number, SignalAllocationRow>()
  const rowsByRowId = new Map<string, SignalAllocationRow>()
  const rowIndexBySignalId = new Map<number, number>()
  const ownerSignalIdByChannelId = new Map<number, number>()
  const rowOrder: string[] = []
  const signalIds: number[] = []

  let version = 0
  let allocatedCount = 0

  function clear() {
    rows.length = 0
    rowsBySignalId.clear()
    rowsByRowId.clear()
    rowIndexBySignalId.clear()
    ownerSignalIdByChannelId.clear()
    rowOrder.length = 0
    signalIds.length = 0
    allocatedCount = 0
  }

  function indexRow(row: SignalAllocationRow, index: number) {
    const signalId = normalizeSignalId(row.signal_id)
    if (signalId === null) {
      return false
    }
    const rowId = resolveProjectionRowId(row)
    rows[index] = row
    rowsBySignalId.set(signalId, row)
    rowsByRowId.set(rowId, row)
    rowIndexBySignalId.set(signalId, index)
    rowOrder[index] = rowId
    signalIds[index] = signalId

    const channelId = normalizeChannelId(row.channel_id)
    if (channelId !== null) {
      ownerSignalIdByChannelId.set(channelId, signalId)
      allocatedCount += 1
    }
    return true
  }

  function unindexRow(row: SignalAllocationRow) {
    const signalId = normalizeSignalId(row.signal_id)
    if (signalId === null) {
      return
    }
    rowsBySignalId.delete(signalId)
    rowsByRowId.delete(resolveProjectionRowId(row))
    rowIndexBySignalId.delete(signalId)
    const channelId = normalizeChannelId(row.channel_id)
    if (channelId !== null && ownerSignalIdByChannelId.get(channelId) === signalId) {
      ownerSignalIdByChannelId.delete(channelId)
      allocatedCount = Math.max(0, allocatedCount - 1)
    }
  }

  function replaceRows(nextRows: readonly SignalAllocationRow[]) {
    clear()
    nextRows.forEach((row) => {
      const signalId = normalizeSignalId(row.signal_id)
      if (signalId === null || rowsBySignalId.has(signalId)) {
        return
      }
      indexRow(cloneProjectionRow(row), rows.length)
    })
    version += 1
  }

  function patchRows(nextRows: readonly SignalAllocationRow[]): SignalAllocationProjectionPatchResult {
    let changed = 0
    const missingSignalIds: number[] = []

    nextRows.forEach((row) => {
      const signalId = normalizeSignalId(row.signal_id)
      if (signalId === null) {
        return
      }
      const rowIndex = rowIndexBySignalId.get(signalId)
      if (rowIndex === undefined) {
        missingSignalIds.push(signalId)
        return
      }
      const currentRow = rows[rowIndex]
      if (!currentRow) {
        missingSignalIds.push(signalId)
        return
      }
      unindexRow(currentRow)
      const nextRow = cloneProjectionRow({
        ...currentRow,
        ...row,
        signal_metadata: row.signal_metadata ?? currentRow.signal_metadata,
        allocation_health: row.allocation_health ?? currentRow.allocation_health,
      })
      indexRow(nextRow, rowIndex)
      changed += 1
    })

    if (changed > 0) {
      version += 1
    }

    return { changed, missingSignalIds }
  }

  return {
    get version() {
      return version
    },
    get rowCount() {
      return rows.length
    },
    get allocatedCount() {
      return allocatedCount
    },
    replaceRows,
    patchRows,
    getRows: () => rows as readonly SignalAllocationRow[],
    getRowBySignalId: (signalId: number) => rowsBySignalId.get(signalId) ?? null,
    getRowByRowId: (rowId: string) => rowsByRowId.get(rowId) ?? null,
    hasSignalId: (signalId: number) => rowsBySignalId.has(signalId),
    getOwnerSignalIdByChannelId: (channelId: number) => ownerSignalIdByChannelId.get(channelId) ?? null,
    getRowOrder: () => rowOrder as readonly string[],
    getSignalIds: () => signalIds as readonly number[],
  }
}
