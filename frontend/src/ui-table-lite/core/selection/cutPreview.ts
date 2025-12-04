import { clampSelectionArea, type GridSelectionContext, type SelectionArea } from "./selectionState"

export interface SelectionAreaLike {
  startRow: number
  endRow: number
  startCol: number
  endCol: number
}

export interface SelectionSnapshotLike {
  ranges: readonly SelectionAreaLike[]
  activeRangeIndex?: number | null
}

export interface CutPreviewState {
  areas: SelectionArea[]
  activeIndex: number
}

export function resolveCutPreviewFromSnapshot<TRowKey = unknown>(
  snapshot: SelectionSnapshotLike | null | undefined,
  context: GridSelectionContext<TRowKey>,
): CutPreviewState | null {
  if (!snapshot || !snapshot.ranges.length) {
    return null
  }

  const areas: SelectionArea[] = []

  for (const range of snapshot.ranges) {
    const area = clampSelectionArea(range, context)
    if (area) {
      areas.push(area)
    }
  }

  if (!areas.length) {
    return null
  }

  const clampedIndex = clampActiveIndex(snapshot.activeRangeIndex, areas.length)

  return {
    areas,
    activeIndex: clampedIndex,
  }
}

export function clampCutPreviewState<TRowKey = unknown>(
  state: CutPreviewState | null,
  context: GridSelectionContext<TRowKey>,
): CutPreviewState | null {
  if (!state || !state.areas.length) {
    return null
  }

  const areas: SelectionArea[] = []

  for (const area of state.areas) {
    const clamped = clampSelectionArea(area, context)
    if (clamped) {
      areas.push(clamped)
    }
  }

  if (!areas.length) {
    return null
  }

  return {
    areas,
    activeIndex: clampActiveIndex(state.activeIndex, areas.length),
  }
}

function clampActiveIndex(index: number | null | undefined, length: number): number {
  if (!Number.isFinite(index ?? NaN)) {
    return 0
  }
  const value = index ?? 0
  if (value < 0) return 0
  if (value >= length) return length - 1
  return value
}
