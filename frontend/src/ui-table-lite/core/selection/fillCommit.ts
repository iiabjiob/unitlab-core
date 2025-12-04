import {
  clampGridSelectionPoint,
  clampSelectionArea,
  type GridSelectionContext,
  type GridSelectionPoint,
  type GridSelectionPointLike,
  type GridSelectionRange,
  type SelectionArea,
} from "./selectionState"
import { areaContainsCell } from "./geometry"

export interface FillCommitReader {
  (rowIndex: number, colIndex: number): unknown
}

export interface FillCommitWriter {
  (rowIndex: number, colIndex: number, value: unknown): boolean
}

export interface FillCommitInput<TRowKey = unknown> {
  origin: GridSelectionRange<TRowKey>
  preview: SelectionArea
  target?: GridSelectionPointLike<TRowKey> | null
  context: GridSelectionContext<TRowKey>
  reader: FillCommitReader
  writer: FillCommitWriter
}

export interface FillCommitResult<TRowKey = unknown> {
  changed: boolean
  appliedArea: SelectionArea
  focus: GridSelectionPoint<TRowKey>
}

export function commitFillPreview<TRowKey = unknown>(
  input: FillCommitInput<TRowKey>,
): FillCommitResult<TRowKey> | null {
  const { origin, preview, target, context, reader, writer } = input

  const originArea = clampSelectionArea(origin, context)
  if (!originArea) {
    return null
  }

  const previewArea = clampSelectionArea(preview, context)
  if (!previewArea) {
    return null
  }

  const patternRows = originArea.endRow - originArea.startRow + 1
  const patternCols = originArea.endCol - originArea.startCol + 1
  if (patternRows <= 0 || patternCols <= 0) {
    return null
  }

  let changed = false
  let touched = false

  for (let row = previewArea.startRow; row <= previewArea.endRow; row += 1) {
    for (let col = previewArea.startCol; col <= previewArea.endCol; col += 1) {
      if (areaContainsCell(originArea, row, col)) {
        continue
      }

      touched = true

      const relativeRow = row - originArea.startRow
      const relativeCol = col - originArea.startCol
      const sourceRowOffset = ((relativeRow % patternRows) + patternRows) % patternRows
      const sourceColOffset = ((relativeCol % patternCols) + patternCols) % patternCols
      const sourceRow = originArea.startRow + sourceRowOffset
      const sourceCol = originArea.startCol + sourceColOffset
      const value = reader(sourceRow, sourceCol)

      if (writer(row, col, value)) {
        changed = true
      }
    }
  }

  const focusPoint = clampGridSelectionPoint(
    target ?? {
      rowIndex: previewArea.endRow,
      colIndex: previewArea.endCol,
    },
    context,
  )

  if (!touched) {
    return {
      changed: false,
      appliedArea: { ...previewArea },
      focus: focusPoint,
    }
  }

  return {
    changed,
    appliedArea: { ...previewArea },
    focus: focusPoint,
  }
}
