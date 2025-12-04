import {
  clampGridSelectionPoint,
  mergeRanges as mergeSelectionAreas,
  normalizeGridSelectionRange,
  rangesFromSelection,
  type GridSelectionContext,
  type GridSelectionPoint,
  type GridSelectionPointLike,
  type GridSelectionRange,
  type SelectionArea,
} from "./selectionState"

export interface HeadlessSelectionState<TRowKey = unknown> {
  ranges: GridSelectionRange<TRowKey>[]
  areas: SelectionArea[]
  activeRangeIndex: number
  selectedPoint: GridSelectionPoint<TRowKey> | null
  anchorPoint: GridSelectionPoint<TRowKey> | null
  dragAnchorPoint: GridSelectionPoint<TRowKey> | null
}

export interface ResolveSelectionUpdateInput<TRowKey = unknown> {
  ranges: readonly GridSelectionRange<TRowKey>[]
  activeRangeIndex: number
  context: GridSelectionContext<TRowKey>
  selectedPoint?: GridSelectionPointLike<TRowKey> | null
  anchorPoint?: GridSelectionPointLike<TRowKey> | null
  dragAnchorPoint?: GridSelectionPointLike<TRowKey> | null
}

export interface ResolveSelectionUpdateResult<TRowKey = unknown> extends HeadlessSelectionState<TRowKey> {
  changed: boolean
}

export function resolveSelectionUpdate<TRowKey = unknown>(
  input: ResolveSelectionUpdateInput<TRowKey>,
): ResolveSelectionUpdateResult<TRowKey> {
  const normalized = input.ranges
    .map(range => normalizeGridSelectionRange(range, input.context))
    .filter((range): range is GridSelectionRange<TRowKey> => range != null)

  if (!normalized.length) {
    return {
      ranges: [],
      areas: [],
      activeRangeIndex: -1,
      selectedPoint: null,
      anchorPoint: null,
      dragAnchorPoint: null,
      changed: true,
    }
  }

  const nextActiveIndex = clampIndex(input.activeRangeIndex, 0, normalized.length - 1)
  const activeRange = normalized[nextActiveIndex]

  const selectedPoint = input.selectedPoint === undefined
    ? clonePoint(activeRange.focus)
    : input.selectedPoint
      ? clampGridSelectionPoint(input.selectedPoint, input.context)
      : null

  const anchorPoint = input.anchorPoint === undefined
    ? clonePoint(activeRange.anchor)
    : input.anchorPoint
      ? clampGridSelectionPoint(input.anchorPoint, input.context)
      : null

  const dragAnchorPoint = input.dragAnchorPoint === undefined
    ? (anchorPoint ? clonePoint(anchorPoint) : null)
    : input.dragAnchorPoint
      ? clampGridSelectionPoint(input.dragAnchorPoint, input.context)
      : null

  const areas = mergeSelectionAreas(rangesFromSelection(normalized))

  const state: HeadlessSelectionState<TRowKey> = {
    ranges: normalized,
    areas,
    activeRangeIndex: nextActiveIndex,
    selectedPoint,
    anchorPoint,
    dragAnchorPoint,
  }

  return {
    ...state,
    changed: true,
  }
}

export function emptySelectionState<TRowKey = unknown>(): HeadlessSelectionState<TRowKey> {
  return {
    ranges: [],
    areas: [],
    activeRangeIndex: -1,
    selectedPoint: null,
    anchorPoint: null,
    dragAnchorPoint: null,
  }
}

function clonePoint<TRowKey>(point: GridSelectionPoint<TRowKey>): GridSelectionPoint<TRowKey> {
  return {
    rowIndex: point.rowIndex,
    colIndex: point.colIndex,
    rowId: point.rowId ?? null,
  }
}

function clampIndex(value: number, min: number, max: number): number {
  if (!Number.isFinite(value)) return min
  if (value < min) return min
  if (value > max) return max
  return value
}
