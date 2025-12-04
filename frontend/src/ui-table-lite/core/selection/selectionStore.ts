import { emptySelectionState, type HeadlessSelectionState, type ResolveSelectionUpdateResult } from "./update"

export type SelectionStoreListener<RowKey> = (state: HeadlessSelectionState<RowKey>) => void

export interface SelectionStoreOptions<RowKey> {
  initialState?: HeadlessSelectionState<RowKey>
}

export interface SelectionStore<RowKey> {
  getState(): HeadlessSelectionState<RowKey>
  peekState(): HeadlessSelectionState<RowKey>
  setState(state: HeadlessSelectionState<RowKey>): void
  applyResult(result: ResolveSelectionUpdateResult<RowKey>): void
  subscribe(listener: SelectionStoreListener<RowKey>): () => void
  dispose(): void
}

function cloneState<RowKey>(state: HeadlessSelectionState<RowKey>): HeadlessSelectionState<RowKey> {
  return {
    ranges: state.ranges.slice(),
    areas: state.areas.slice(),
    activeRangeIndex: state.activeRangeIndex,
    selectedPoint: state.selectedPoint,
    anchorPoint: state.anchorPoint,
    dragAnchorPoint: state.dragAnchorPoint,
  }
}

function pointsEqual<RowKey>(a: HeadlessSelectionState<RowKey>["selectedPoint"], b: HeadlessSelectionState<RowKey>["selectedPoint"]): boolean {
  if (a === b) return true
  if (!a || !b) return false
  return a.rowIndex === b.rowIndex && a.colIndex === b.colIndex && (a.rowId ?? null) === (b.rowId ?? null)
}

function areasEqual<RowKey>(left: HeadlessSelectionState<RowKey>["areas"], right: HeadlessSelectionState<RowKey>["areas"]): boolean {
  if (left === right) return true
  if (left.length !== right.length) return false
  for (let index = 0; index < left.length; index += 1) {
    const a = left[index]
    const b = right[index]
    if (
      a.startRow !== b.startRow ||
      a.endRow !== b.endRow ||
      a.startCol !== b.startCol ||
      a.endCol !== b.endCol
    ) {
      return false
    }
  }
  return true
}

function rangesEqual<RowKey>(left: HeadlessSelectionState<RowKey>["ranges"], right: HeadlessSelectionState<RowKey>["ranges"], allowRowIdCompare = true): boolean {
  if (left === right) return true
  if (left.length !== right.length) return false
  for (let index = 0; index < left.length; index += 1) {
    const a = left[index]
    const b = right[index]
    if (
      a.startRow !== b.startRow ||
      a.endRow !== b.endRow ||
      a.startCol !== b.startCol ||
      a.endCol !== b.endCol
    ) {
      return false
    }
    if (
      a.anchor.rowIndex !== b.anchor.rowIndex ||
      a.anchor.colIndex !== b.anchor.colIndex ||
      a.focus.rowIndex !== b.focus.rowIndex ||
      a.focus.colIndex !== b.focus.colIndex
    ) {
      return false
    }
    if (allowRowIdCompare) {
      if ((a.anchor.rowId ?? null) !== (b.anchor.rowId ?? null)) return false
      if ((a.focus.rowId ?? null) !== (b.focus.rowId ?? null)) return false
      if ((a.startRowId ?? null) !== (b.startRowId ?? null)) return false
      if ((a.endRowId ?? null) !== (b.endRowId ?? null)) return false
    }
  }
  return true
}

function statesEqual<RowKey>(a: HeadlessSelectionState<RowKey>, b: HeadlessSelectionState<RowKey>): boolean {
  if (a === b) return true
  if (a.activeRangeIndex !== b.activeRangeIndex) return false
  if (!rangesEqual(a.ranges, b.ranges)) return false
  if (!areasEqual(a.areas, b.areas)) return false
  if (!pointsEqual(a.selectedPoint, b.selectedPoint)) return false
  if (!pointsEqual(a.anchorPoint, b.anchorPoint)) return false
  if (!pointsEqual(a.dragAnchorPoint, b.dragAnchorPoint)) return false
  return true
}

export function createSelectionStore<RowKey>(options: SelectionStoreOptions<RowKey> = {}): SelectionStore<RowKey> {
  let current = options.initialState ? cloneState(options.initialState) : emptySelectionState<RowKey>()
  const listeners = new Set<SelectionStoreListener<RowKey>>()
  let disposed = false

  function ensureActive() {
    if (disposed) {
      throw new Error("SelectionStore has been disposed")
    }
  }

  function notify() {
    if (disposed) return
    const snapshot = cloneState(current)
    for (const listener of listeners) {
      listener(snapshot)
    }
  }

  return {
    getState() {
      return cloneState(current)
    },
    peekState() {
      return current
    },
    setState(state) {
      ensureActive()
      if (statesEqual(current, state)) return
      current = cloneState(state)
      notify()
    },
    applyResult(result) {
      ensureActive()
      if (statesEqual(current, result)) return
      current = {
        ranges: result.ranges.slice(),
        areas: result.areas.slice(),
        activeRangeIndex: result.activeRangeIndex,
        selectedPoint: result.selectedPoint,
        anchorPoint: result.anchorPoint,
        dragAnchorPoint: result.dragAnchorPoint,
      }
      notify()
    },
    subscribe(listener) {
      if (disposed) return () => {}
      listeners.add(listener)
      return () => {
        listeners.delete(listener)
      }
    },
    dispose() {
      if (disposed) return
      disposed = true
      listeners.clear()
    },
  }
}
