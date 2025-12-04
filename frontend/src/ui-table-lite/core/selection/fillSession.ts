import {
  clampGridSelectionPoint,
  clampSelectionArea,
  type GridSelectionContext,
  type GridSelectionPoint,
  type GridSelectionPointLike,
  type GridSelectionRange,
  type SelectionArea,
} from "./selectionState"
import { computeFillPreviewRange, type FillPreviewAxis } from "./fillPreview"

export interface FillDragSession<TRowKey = unknown> {
  origin: GridSelectionRange<TRowKey>
  originArea: SelectionArea
  preview: SelectionArea | null
  target: GridSelectionPoint<TRowKey> | null
  axis: FillPreviewAxis | null
}

const originRangeCache: GridSelectionRange<any> = {
  anchor: { rowIndex: 0, colIndex: 0, rowId: null },
  focus: { rowIndex: 0, colIndex: 0, rowId: null },
  startRow: 0,
  endRow: 0,
  startCol: 0,
  endCol: 0,
}

const previewAreaCache: SelectionArea = { startRow: 0, endRow: 0, startCol: 0, endCol: 0 }

const targetPointCache: GridSelectionPoint<any> = { rowIndex: 0, colIndex: 0, rowId: null }

export function startFillDragSession<TRowKey = unknown>(
  input: {
    origin: GridSelectionRange<TRowKey>
    context: GridSelectionContext<TRowKey>
  },
): FillDragSession<TRowKey> | null {
  const originArea = clampSelectionArea(input.origin, input.context)
  if (!originArea) {
    return null
  }
  const range = reuseRange(input.origin)
  return {
    origin: range,
    originArea,
    preview: null,
    target: null,
    axis: null,
  }
}

export function updateFillDragSession<TRowKey = unknown>(
  session: FillDragSession<TRowKey>,
  input: {
    target: GridSelectionPointLike<TRowKey> | null | undefined
    context: GridSelectionContext<TRowKey>
  },
): FillDragSession<TRowKey> {
  const result = computeFillPreviewRange<TRowKey>({
    origin: session.origin,
    target: input.target,
    context: input.context,
  }, { axis: session.axis ?? null })

  session.axis = result.axis ?? session.axis ?? null

  const preview = result.preview ? reuseArea(result.preview) : null
  const target = result.preview && result.target ? reusePoint(result.target) : null

  if (preview) {
    session.preview = preview
  } else {
    session.preview = null
  }
  session.target = target
  return session
}

export function fillDragSessionWithPreview<TRowKey = unknown>(
  session: FillDragSession<TRowKey>,
  input: {
    preview: SelectionArea | null
    target: GridSelectionPointLike<TRowKey> | null
    context: GridSelectionContext<TRowKey>
  },
): FillDragSession<TRowKey> {
  if (!input.preview) {
    session.preview = null
    session.target = null
    return session
  }

  const clampedPreview = clampSelectionArea(input.preview, input.context)
  if (!clampedPreview) {
    session.preview = null
    session.target = null
    return session
  }

  const clampedTarget = input.target ? clampGridSelectionPoint(input.target, input.context) : null

  session.preview = reuseArea(clampedPreview)
  session.target = clampedTarget ? reusePoint(clampedTarget) : null
  session.axis = resolveAxisFromAreas(session.originArea, clampedPreview)
  return session
}

function resolveAxisFromAreas(origin: SelectionArea, preview: SelectionArea | null): FillPreviewAxis | null {
  if (!preview) {
    return null
  }
  const rowChanged = preview.startRow !== origin.startRow || preview.endRow !== origin.endRow
  const colChanged = preview.startCol !== origin.startCol || preview.endCol !== origin.endCol
  if (rowChanged && !colChanged) {
    return "row"
  }
  if (colChanged && !rowChanged) {
    return "column"
  }
  return null
}

function reuseRange<TRowKey>(range: GridSelectionRange<TRowKey>): GridSelectionRange<TRowKey> {
  const cache = originRangeCache as GridSelectionRange<TRowKey>
  cache.anchor.rowIndex = range.anchor.rowIndex
  cache.anchor.colIndex = range.anchor.colIndex
  cache.anchor.rowId = range.anchor.rowId ?? null
  cache.focus.rowIndex = range.focus.rowIndex
  cache.focus.colIndex = range.focus.colIndex
  cache.focus.rowId = range.focus.rowId ?? null
  cache.startRow = range.startRow
  cache.endRow = range.endRow
  cache.startCol = range.startCol
  cache.endCol = range.endCol
  return cache
}

function reuseArea(area: SelectionArea): SelectionArea {
  const cache = previewAreaCache
  cache.startRow = area.startRow
  cache.endRow = area.endRow
  cache.startCol = area.startCol
  cache.endCol = area.endCol
  return cache
}

function reusePoint<TRowKey>(point: GridSelectionPoint<TRowKey>): GridSelectionPoint<TRowKey> {
  const cache = targetPointCache as GridSelectionPoint<TRowKey>
  cache.rowIndex = point.rowIndex
  cache.colIndex = point.colIndex
  cache.rowId = point.rowId ?? null
  return cache
}
