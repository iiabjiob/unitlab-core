import {
  clampGridSelectionPoint,
  type GridSelectionContext,
  type GridSelectionPoint,
  type GridSelectionPointLike,
  type GridSelectionRange,
  type SelectionArea,
  clampSelectionArea,
} from "./selectionState"

export type FillPreviewAxis = "row" | "column"

export interface FillPreviewInput<TRowKey = unknown> {
  origin: GridSelectionRange<TRowKey>
  target: GridSelectionPointLike<TRowKey> | null | undefined
  context: GridSelectionContext<TRowKey>
}

export interface FillPreviewOutput<TRowKey = unknown> {
  preview: SelectionArea | null
  target: GridSelectionPoint<TRowKey> | null
  axis: FillPreviewAxis | null
}

export interface FillPreviewOptions {
  axis?: FillPreviewAxis | null
}

export function computeFillPreviewRange<TRowKey = unknown>(
  input: FillPreviewInput<TRowKey>,
  options?: FillPreviewOptions,
): FillPreviewOutput<TRowKey> {
  const { origin, target, context } = input
  const originArea = clampSelectionArea(origin, context)
  if (!originArea) {
    return { preview: null, target: null, axis: null }
  }

  if (!target) {
    return { preview: null, target: null, axis: null }
  }

  const clampedTarget = clampGridSelectionPoint(target, context)

  const originStartRow = originArea.startRow
  const originEndRow = originArea.endRow
  const originStartCol = originArea.startCol
  const originEndCol = originArea.endCol

  const rowDelta = clampedTarget.rowIndex < originStartRow
    ? clampedTarget.rowIndex - originStartRow
    : clampedTarget.rowIndex > originEndRow
      ? clampedTarget.rowIndex - originEndRow
      : 0
  const colDelta = clampedTarget.colIndex < originStartCol
    ? clampedTarget.colIndex - originStartCol
    : clampedTarget.colIndex > originEndCol
      ? clampedTarget.colIndex - originEndCol
      : 0

  const hasRowExtension = rowDelta !== 0
  const hasColExtension = colDelta !== 0

  let axis: FillPreviewAxis | null = options?.axis ?? null

  if (!axis) {
    if (hasRowExtension && !hasColExtension) {
      axis = "row"
    } else if (!hasRowExtension && hasColExtension) {
      axis = "column"
    } else if (hasRowExtension && hasColExtension) {
      axis = Math.abs(rowDelta) >= Math.abs(colDelta) ? "row" : "column"
    }
  }

  if ((axis === "row" && !hasRowExtension) || (axis === "column" && !hasColExtension)) {
    return { preview: null, target: null, axis }
  }

  let startRow = originStartRow
  let endRow = originEndRow
  let startCol = originStartCol
  let endCol = originEndCol

  if (axis === "row") {
    startCol = originStartCol
    endCol = originEndCol
    if (rowDelta < 0) {
      startRow = clampedTarget.rowIndex
    } else if (rowDelta > 0) {
      endRow = clampedTarget.rowIndex
    }
  } else if (axis === "column") {
    startRow = originStartRow
    endRow = originEndRow
    if (colDelta < 0) {
      startCol = clampedTarget.colIndex
    } else if (colDelta > 0) {
      endCol = clampedTarget.colIndex
    }
  }

  const candidate: SelectionArea = {
    startRow,
    endRow,
    startCol,
    endCol,
  }

  const clampedPreview = clampSelectionArea(candidate, context)
  if (!clampedPreview) {
    return { preview: null, target: null, axis }
  }

  if (
    clampedPreview.startRow === originStartRow &&
    clampedPreview.endRow === originEndRow &&
    clampedPreview.startCol === originStartCol &&
    clampedPreview.endCol === originEndCol
  ) {
    return { preview: null, target: null, axis }
  }
  return {
    preview: clampedPreview,
    target: clampedTarget,
    axis,
  }
}
