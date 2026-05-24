type SignalGridRowSelectionSnapshotLike = {
  focusedRow?: unknown
  selectedRows?: readonly unknown[]
  mode?: string
  excludedRows?: readonly unknown[]
} | null | undefined

type SignalGridSelectableRowLike = {
  row_id?: unknown
  rowId?: unknown
  signal_id?: unknown
}

function normalizeSignalGridRowKey(value: unknown): string | null {
  if (typeof value !== "string" && typeof value !== "number") {
    return null
  }
  const normalized = String(value).trim()
  return normalized ? normalized : null
}

export function resolveSignalGridRowKey(row: SignalGridSelectableRowLike): string | null {
  const explicitRowId = normalizeSignalGridRowKey(row.rowId ?? row.row_id)
  if (explicitRowId) {
    return explicitRowId
  }

  const signalId = Number(row.signal_id)
  return Number.isFinite(signalId) && signalId > 0 ? `signal-${signalId}` : null
}

export function resolveSignalGridSelectedRowKeys(
  snapshot: SignalGridRowSelectionSnapshotLike,
  candidateRowKeys: readonly string[],
): string[] {
  if (!snapshot) {
    return []
  }

  if (snapshot.mode === "all") {
    const excludedRows = new Set(
      (snapshot.excludedRows ?? [])
        .map(normalizeSignalGridRowKey)
        .filter((rowKey): rowKey is string => Boolean(rowKey)),
    )
    return candidateRowKeys.filter(rowKey => !excludedRows.has(rowKey))
  }

  return (snapshot.selectedRows ?? [])
    .map(normalizeSignalGridRowKey)
    .filter((rowKey): rowKey is string => Boolean(rowKey))
}
