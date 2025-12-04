import type {
  GridSelectionContext,
  SelectionArea,
} from "./selectionState"

export interface SelectionMoveHandlers {
  read: (rowIndex: number, colIndex: number) => unknown
  clear: (rowIndex: number, colIndex: number) => boolean
  write: (rowIndex: number, colIndex: number, value: unknown) => boolean
}

export interface SelectionMoveInput<TRowKey = unknown> {
  source: SelectionArea
  target: SelectionArea
  context: GridSelectionContext<TRowKey>
  handlers: SelectionMoveHandlers
}

export interface SelectionMoveResult {
  changed: boolean
  appliedArea: SelectionArea
}

export function moveSelectionArea<TRowKey = unknown>(
  input: SelectionMoveInput<TRowKey>,
): SelectionMoveResult | null {
  const { source, target, context, handlers } = input
  const rowCount = Math.max(0, context.grid?.rowCount ?? 0)
  const colCount = Math.max(0, context.grid?.colCount ?? 0)
  if (rowCount <= 0 || colCount <= 0) {
    return null
  }

  const sourceHeight = source.endRow - source.startRow + 1
  const sourceWidth = source.endCol - source.startCol + 1
  if (sourceHeight <= 0 || sourceWidth <= 0) {
    return null
  }

  const targetHeight = target.endRow - target.startRow + 1
  const targetWidth = target.endCol - target.startCol + 1
  if (targetHeight !== sourceHeight || targetWidth !== sourceWidth) {
    return null
  }

  const clampRow = (index: number) => Math.min(Math.max(index, 0), rowCount - 1)
  const clampCol = (index: number) => Math.min(Math.max(index, 0), colCount - 1)

  const normalizedTargetStartRow = clampRow(target.startRow)
  const normalizedTargetStartCol = clampCol(target.startCol)
  const normalizedTargetEndRow = clampRow(target.startRow + sourceHeight - 1)
  const normalizedTargetEndCol = clampCol(target.startCol + sourceWidth - 1)

  const matrix: unknown[][] = []
  for (let rowOffset = 0; rowOffset < sourceHeight; rowOffset += 1) {
    const rowIndex = clampRow(source.startRow + rowOffset)
    const rowValues: unknown[] = []
    for (let colOffset = 0; colOffset < sourceWidth; colOffset += 1) {
      const colIndex = clampCol(source.startCol + colOffset)
      rowValues.push(handlers.read(rowIndex, colIndex))
    }
    matrix.push(rowValues)
  }

  let changed = false

  for (let rowOffset = 0; rowOffset < sourceHeight; rowOffset += 1) {
    const rowIndex = clampRow(source.startRow + rowOffset)
    for (let colOffset = 0; colOffset < sourceWidth; colOffset += 1) {
      const colIndex = clampCol(source.startCol + colOffset)
      if (handlers.clear(rowIndex, colIndex)) {
        changed = true
      }
    }
  }

  for (let rowOffset = 0; rowOffset < sourceHeight; rowOffset += 1) {
    const rowIndex = clampRow(target.startRow + rowOffset)
    const rowValues = matrix[rowOffset] ?? []
    for (let colOffset = 0; colOffset < sourceWidth; colOffset += 1) {
      const colIndex = clampCol(target.startCol + colOffset)
      const value = rowValues[colOffset]
      if (handlers.write(rowIndex, colIndex, value)) {
        changed = true
      }
    }
  }

  if (!changed) {
    return {
      changed: false,
      appliedArea: {
        startRow: normalizedTargetStartRow,
        endRow: normalizedTargetEndRow,
        startCol: normalizedTargetStartCol,
        endCol: normalizedTargetEndCol,
      },
    }
  }

  return {
    changed: true,
    appliedArea: {
      startRow: normalizedTargetStartRow,
      endRow: normalizedTargetEndRow,
      startCol: normalizedTargetStartCol,
      endCol: normalizedTargetEndCol,
    },
  }
}
