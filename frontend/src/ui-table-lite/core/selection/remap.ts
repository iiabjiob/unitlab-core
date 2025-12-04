import { startFillDragSession, updateFillDragSession, type FillDragSession } from "./fillSession"
import {
  clampGridSelectionPoint,
  createGridSelectionRange,
  type GridSelectionContext,
  type GridSelectionPoint,
  type GridSelectionRange,
} from "./selectionState"

export interface SelectionRemapContext<TRowKey = unknown> {
  context: GridSelectionContext<TRowKey>
  findRowIndexById: (rowId: TRowKey) => number | null
}

export interface SelectionRemapState<TRowKey = unknown> {
  ranges: readonly GridSelectionRange<TRowKey>[]
  selected: GridSelectionPoint<TRowKey> | null
  anchor: GridSelectionPoint<TRowKey> | null
  dragAnchor: GridSelectionPoint<TRowKey> | null
  fillSession: FillDragSession<TRowKey> | null
}

export interface SelectionRemapResult<TRowKey = unknown> {
  ranges: GridSelectionRange<TRowKey>[]
  selected: GridSelectionPoint<TRowKey> | null
  anchor: GridSelectionPoint<TRowKey> | null
  dragAnchor: GridSelectionPoint<TRowKey> | null
  fillSession: FillDragSession<TRowKey> | null
}

export function remapSelectionState<TRowKey = unknown>(
  state: SelectionRemapState<TRowKey>,
  options: SelectionRemapContext<TRowKey>,
): SelectionRemapResult<TRowKey> {
  const ranges = state.ranges
    .map(range => remapGridRange(range, options))
    .filter((range): range is GridSelectionRange<TRowKey> => range != null)

  const selected = remapGridPoint(state.selected, options)
  const anchor = remapGridPoint(state.anchor, options)
  const dragAnchor = remapGridPoint(state.dragAnchor, options)
  const fillSession = remapFillSession(state.fillSession, options)

  return {
    ranges,
    selected,
    anchor,
    dragAnchor,
    fillSession,
  }
}

export function remapGridPoint<TRowKey = unknown>(
  point: GridSelectionPoint<TRowKey> | null | undefined,
  options: SelectionRemapContext<TRowKey>,
): GridSelectionPoint<TRowKey> | null {
  if (!point) {
    return null
  }

  const nextRowIndex = remapRowIndex(point.rowIndex, point.rowId, options)
  if (nextRowIndex == null) {
    return null
  }

  return clampGridSelectionPoint(
    {
      ...point,
      rowIndex: nextRowIndex,
    },
    options.context,
  )
}

export function remapGridRange<TRowKey = unknown>(
  range: GridSelectionRange<TRowKey> | null | undefined,
  options: SelectionRemapContext<TRowKey>,
): GridSelectionRange<TRowKey> | null {
  if (!range) {
    return null
  }

  const anchor = remapGridPoint(range.anchor, options)
  const focus = remapGridPoint(range.focus, options)

  if (!anchor || !focus) {
    return null
  }

  return createGridSelectionRange(anchor, focus, options.context)
}

export function remapFillSession<TRowKey = unknown>(
  session: FillDragSession<TRowKey> | null | undefined,
  options: SelectionRemapContext<TRowKey>,
): FillDragSession<TRowKey> | null {
  if (!session) {
    return null
  }

  const origin = remapGridRange(session.origin, options)
  if (!origin) {
    return null
  }

  const nextSession = startFillDragSession<TRowKey>({
    origin,
    context: options.context,
  })

  if (!nextSession) {
    return null
  }

  const target = remapGridPoint(session.target, options)
  if (!target) {
    return nextSession
  }

  return updateFillDragSession(nextSession, {
    target,
    context: options.context,
  })
}

function remapRowIndex<TRowKey = unknown>(
  rowIndex: number,
  rowId: TRowKey | null | undefined,
  options: SelectionRemapContext<TRowKey>,
): number | null {
  if (rowId == null) {
    return rowIndex
  }
  const mapped = options.findRowIndexById(rowId)
  if (mapped == null) {
    return null
  }
  return mapped
}
