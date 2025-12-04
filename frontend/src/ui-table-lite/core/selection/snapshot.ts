import type {
  GridSelectionPoint,
  GridSelectionRange,
} from "./selectionState"

export interface GridSelectionSnapshotRange<TRowKey = unknown> {
  startRow: number
  endRow: number
  startCol: number
  endCol: number
  startRowId: TRowKey | null
  endRowId: TRowKey | null
  anchor: GridSelectionPoint<TRowKey>
  focus: GridSelectionPoint<TRowKey>
}

export interface GridSelectionSnapshot<TRowKey = unknown> {
  ranges: GridSelectionSnapshotRange<TRowKey>[]
  activeRangeIndex: number
  activeCell: GridSelectionPoint<TRowKey> | null
}

export interface CreateSelectionSnapshotOptions<TRowKey = unknown> {
  ranges: readonly GridSelectionRange<TRowKey>[]
  activeRangeIndex: number
  selectedPoint: GridSelectionPoint<TRowKey> | null
  getRowIdByIndex?: (rowIndex: number) => TRowKey | null
}

export function createSelectionSnapshot<TRowKey = unknown>(
  options: CreateSelectionSnapshotOptions<TRowKey>,
): GridSelectionSnapshot<TRowKey> {
  const { ranges, activeRangeIndex, selectedPoint, getRowIdByIndex } = options

  const clonePoint = (point: GridSelectionPoint<TRowKey>): GridSelectionPoint<TRowKey> => {
    const resolvedRowId =
      point.rowId != null
        ? point.rowId
        : getRowIdByIndex
          ? getRowIdByIndex(point.rowIndex) ?? null
          : null
    return {
      rowIndex: point.rowIndex,
      colIndex: point.colIndex,
      rowId: resolvedRowId,
    }
  }

  const resolveRowId = (existing: TRowKey | null | undefined, rowIndex: number): TRowKey | null => {
    if (existing != null) {
      return existing
    }
    if (getRowIdByIndex) {
      const resolved = getRowIdByIndex(rowIndex)
      return resolved != null ? resolved : null
    }
    return null
  }

  const snapshotRanges = ranges.map(range => ({
    startRow: range.startRow,
    endRow: range.endRow,
    startCol: range.startCol,
    endCol: range.endCol,
    startRowId: resolveRowId(range.startRowId, range.startRow),
    endRowId: resolveRowId(range.endRowId, range.endRow),
    anchor: clonePoint(range.anchor),
    focus: clonePoint(range.focus),
  }))

  const activeCell = selectedPoint ? clonePoint(selectedPoint) : null

  return {
    ranges: snapshotRanges,
    activeRangeIndex,
    activeCell,
  }
}

export function selectionSnapshotSignature<TRowKey = unknown>(
  snapshot: GridSelectionSnapshot<TRowKey>,
): string {
  return JSON.stringify(snapshot)
}
