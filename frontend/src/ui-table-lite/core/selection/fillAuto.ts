import {
  clampGridSelectionPoint,
  clampSelectionArea,
  type GridSelectionContext,
  type GridSelectionPoint,
  type GridSelectionPointLike,
  type GridSelectionRange,
  type SelectionArea,
} from "./selectionState"

export interface AutoFillDownInput<TRowKey = unknown> {
  origin: GridSelectionRange<TRowKey>
  context: GridSelectionContext<TRowKey>
}

export interface AutoFillDownResult<TRowKey = unknown> {
  preview: SelectionArea
  target: GridSelectionPoint<TRowKey>
}

export function resolveAutoFillDown<TRowKey = unknown>(
  input: AutoFillDownInput<TRowKey>,
): AutoFillDownResult<TRowKey> | null {
  const { origin, context } = input

  const originArea = clampSelectionArea(origin, context)
  if (!originArea) {
    return null
  }

  const rowCount = Math.max(0, context.grid.rowCount)
  if (rowCount <= 0) {
    return null
  }

  const targetRowIndex = rowCount - 1
  if (originArea.endRow >= targetRowIndex) {
    return null
  }

  const previewArea: SelectionArea = {
    startRow: originArea.startRow,
    endRow: targetRowIndex,
    startCol: originArea.startCol,
    endCol: originArea.endCol,
  }

  if (
    previewArea.startRow === originArea.startRow &&
    previewArea.endRow === originArea.endRow &&
    previewArea.startCol === originArea.startCol &&
    previewArea.endCol === originArea.endCol
  ) {
    return null
  }

  const targetPointCandidate: GridSelectionPointLike<TRowKey> = {
    rowIndex: targetRowIndex,
    colIndex: origin.focus.colIndex,
    rowId: origin.focus.rowId ?? null,
  }

  const targetPoint = clampGridSelectionPoint(targetPointCandidate, context)

  return {
    preview: previewArea,
    target: targetPoint,
  }
}
