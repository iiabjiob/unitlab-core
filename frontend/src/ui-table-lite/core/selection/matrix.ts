import {
  clampGridSelectionPoint,
  clampSelectionArea,
  createGridSelectionRange,
  type GridSelectionContext,
  type GridSelectionPoint,
  type GridSelectionPointLike,
  type GridSelectionRange,
  type SelectionArea,
} from "./selectionState"

export interface MatrixApplyInput<TRowKey = unknown> {
  matrix: readonly (readonly string[])[]
  selection: GridSelectionRange<TRowKey> | null
  basePoint?: GridSelectionPointLike<TRowKey> | null
  context: GridSelectionContext<TRowKey>
  writer: (rowIndex: number, colIndex: number, value: string) => boolean
}

export interface MatrixApplyResult<TRowKey = unknown> {
  changed: boolean
  usedSelectionArea: boolean
  appliedArea: SelectionArea | null
  nextSelection: GridSelectionRange<TRowKey> | null
}

export function applyMatrixToGrid<TRowKey = unknown>(
  input: MatrixApplyInput<TRowKey>,
): MatrixApplyResult<TRowKey> | null {
  const { matrix, selection, basePoint = null, context, writer } = input

  if (!matrix.length || !matrix[0]?.length) {
    return null
  }

  const rowCount = Math.max(0, context.grid.rowCount)
  const colCount = Math.max(0, context.grid.colCount)

  if (rowCount <= 0 || colCount <= 0) {
    return null
  }

  const matrixRowCount = matrix.length
  const matrixColBaseline = matrix[0]?.length ?? 1

  const selectionArea = selection ? clampSelectionArea(selection, context) : null

  if (selectionArea) {
    const selectionHeight = selectionArea.endRow - selectionArea.startRow + 1
    const selectionWidth = selectionArea.endCol - selectionArea.startCol + 1
    const matrixWidth = matrixColBaseline

    if (matrixRowCount <= selectionHeight && matrixWidth <= selectionWidth) {
      let changed = false

      for (let rowIndex = selectionArea.startRow; rowIndex <= selectionArea.endRow; rowIndex += 1) {
        const sourceRow = (rowIndex - selectionArea.startRow) % matrixRowCount
        const rowValues = matrix[sourceRow] ?? []
        const rowWidth = rowValues.length || matrixColBaseline
        if (!rowWidth) {
          continue
        }
        for (let colIndex = selectionArea.startCol; colIndex <= selectionArea.endCol; colIndex += 1) {
          const sourceCol = (colIndex - selectionArea.startCol) % rowWidth
          const rawValue = rowValues[sourceCol] ?? ""
          if (writer(rowIndex, colIndex, rawValue)) {
            changed = true
          }
        }
      }

      return {
        changed,
        usedSelectionArea: true,
        appliedArea: selectionArea,
        nextSelection: null,
      }
    }
  }

  if (!basePoint) {
    return null
  }

  const base = clampGridSelectionPoint(basePoint, context)
  const baseRow = base.rowIndex
  const baseCol = base.colIndex

  let changed = false
  let touched = false
  let maxRow = baseRow
  let maxCol = baseCol

  for (let rowOffset = 0; rowOffset < matrixRowCount; rowOffset += 1) {
    const targetRow = baseRow + rowOffset
    if (targetRow >= rowCount) {
      break
    }

    const rowValues = matrix[rowOffset] ?? []
    const rowWidth = rowValues.length

    if (!rowWidth) {
      continue
    }

    for (let colOffset = 0; colOffset < rowWidth; colOffset += 1) {
      const targetCol = baseCol + colOffset
      if (targetCol >= colCount) {
        break
      }

      const rawValue = rowValues[colOffset] ?? ""
      touched = true
      if (writer(targetRow, targetCol, rawValue)) {
        changed = true
      }

      if (targetRow > maxRow) {
        maxRow = targetRow
      }
      if (targetCol > maxCol) {
        maxCol = targetCol
      }
    }
  }

  if (!touched) {
    const nextSelection = createGridSelectionRange(base, base, context)
    return {
      changed: false,
      usedSelectionArea: false,
      appliedArea: {
        startRow: baseRow,
        endRow: baseRow,
        startCol: baseCol,
        endCol: baseCol,
      },
      nextSelection,
    }
  }

  const focusPoint: GridSelectionPoint<TRowKey> = clampGridSelectionPoint(
    {
      rowIndex: maxRow,
      colIndex: maxCol,
      rowId: null,
    },
    context,
  )

  const nextSelection = createGridSelectionRange(base, focusPoint, context)

  return {
    changed,
    usedSelectionArea: false,
    appliedArea: {
      startRow: Math.min(baseRow, maxRow),
      endRow: Math.max(baseRow, maxRow),
      startCol: Math.min(baseCol, maxCol),
      endCol: Math.max(baseCol, maxCol),
    },
    nextSelection,
  }
}
