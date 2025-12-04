import { clearSelection, setSelectionRanges } from "./operations"
import { createGridSelectionRange, type GridSelectionContext } from "./selectionState"
import type { GridSelectionPointLike, GridSelectionRange } from "./selectionState"
import type { ResolveSelectionUpdateResult } from "./update"

type RowSelectionInput<TRowKey> = {
  startRow: number
  endRow: number
  context: GridSelectionContext<TRowKey>
  activeRow?: number | null
  anchorPoint?: GridSelectionPointLike<TRowKey> | null
}

type ColumnSelectionInput<TRowKey> = {
  columnIndex: number
  context: GridSelectionContext<TRowKey>
  anchorRow?: number | null
  retainCursor?: boolean
}

function clamp(value: number, min: number, max: number) {
  if (!Number.isFinite(value)) return min
  if (value < min) return min
  if (value > max) return max
  return value
}

export function resolveFullRowSelection<TRowKey>(input: RowSelectionInput<TRowKey>): ResolveSelectionUpdateResult<TRowKey> {
  const {
    context,
  } = input
  const rowCount = Math.max(context.grid.rowCount, 0)
  const colCount = Math.max(context.grid.colCount, 0)
  if (rowCount === 0 || colCount === 0) {
    return clearSelection({ context })
  }

  const normalizedStart = clamp(Math.min(input.startRow, input.endRow), 0, rowCount - 1)
  const normalizedEnd = clamp(Math.max(input.startRow, input.endRow), 0, rowCount - 1)

  const startCol = 0
  const endCol = Math.max(colCount - 1, 0)

  const anchorPoint = input.anchorPoint ?? {
    rowIndex: normalizedStart,
    colIndex: startCol,
  }
  const focusPoint: GridSelectionPointLike<TRowKey> = {
    rowIndex: normalizedEnd,
    colIndex: endCol,
  }

  const range: GridSelectionRange<TRowKey> = createGridSelectionRange(anchorPoint, focusPoint, context)

  const activeRow = clamp(
    input.activeRow ?? normalizedStart,
    normalizedStart,
    normalizedEnd,
  )

  const activePoint: GridSelectionPointLike<TRowKey> = {
    rowIndex: activeRow,
    colIndex: startCol,
  }

  return setSelectionRanges({
    ranges: [range],
    context,
    activeRangeIndex: 0,
    selectedPoint: activePoint,
    anchorPoint: activePoint,
    dragAnchorPoint: activePoint,
  })
}

export function resolveFullColumnSelection<TRowKey>(input: ColumnSelectionInput<TRowKey>): ResolveSelectionUpdateResult<TRowKey> {
  const { context } = input
  const rowCount = Math.max(context.grid.rowCount, 0)
  const colCount = Math.max(context.grid.colCount, 0)
  if (rowCount === 0 || colCount === 0) {
    return clearSelection({ context })
  }

  const normalizedCol = clamp(input.columnIndex, 0, colCount - 1)
  const normalizedAnchorRow = clamp(input.anchorRow ?? 0, 0, rowCount - 1)

  const columnAnchorPoint: GridSelectionPointLike<TRowKey> = {
    rowIndex: 0,
    colIndex: normalizedCol,
  }

  const columnFocusPoint: GridSelectionPointLike<TRowKey> = {
    rowIndex: Math.max(rowCount - 1, 0),
    colIndex: normalizedCol,
  }

  const cursorPoint = input.retainCursor
    ? {
        rowIndex: normalizedAnchorRow,
        colIndex: normalizedCol,
        rowId: context.getRowIdByIndex ? context.getRowIdByIndex(normalizedAnchorRow) ?? null : null,
      }
    : null

  const range = createGridSelectionRange(columnAnchorPoint, columnFocusPoint, context)

  return setSelectionRanges({
    ranges: [range],
    context,
    activeRangeIndex: 0,
    selectedPoint: cursorPoint,
    anchorPoint: cursorPoint,
    dragAnchorPoint: cursorPoint,
  })
}
