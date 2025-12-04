import {
  clearSelection as clearSelectionHeadless,
  extendSelectionToPoint,
  selectSingleCell,
  setSelectionRanges,
  toggleCellSelection,
} from "./operations"
import { type HeadlessSelectionState } from "./update"
import type {
  SelectionEnvironment,
  SelectionOverlaySnapshot,
  SelectionPoint,
  SelectionRange,
} from "./selectionEnvironment"
import type { GridSelectionContext, GridSelectionRange } from "./selectionState"
import { createSelectionStore, type SelectionStore } from "./selectionStore"
import type { PointerCoordinates } from "./autoScroll"

export type SelectionControllerEvent<RowKey> =
  | { type: "selection-change"; state: HeadlessSelectionState<RowKey> }
  | { type: "overlay-update"; snapshot: SelectionOverlaySnapshot }
  | { type: "fill-handle-change"; range: SelectionRange<RowKey> | null }
  | { type: "autoscroll"; active: true; pointer: PointerCoordinates }
  | { type: "autoscroll"; active: false }

export type SelectionControllerListener<RowKey> = (event: SelectionControllerEvent<RowKey>) => void

export interface SelectionControllerOptions<RowKey> {
  environment: SelectionEnvironment<RowKey>
  context: GridSelectionContext<RowKey>
  initialState?: HeadlessSelectionState<RowKey>
}

export interface SelectionController<RowKey> {
  getState(): HeadlessSelectionState<RowKey>
  setState(state: HeadlessSelectionState<RowKey>): void
  updateRanges(ranges: readonly GridSelectionRange<RowKey>[], activeRangeIndex?: number): void
  focusCell(point: SelectionPoint<RowKey>): void
  extendSelection(point: SelectionPoint<RowKey>): void
  toggleCell(point: SelectionPoint<RowKey>): void
  clearSelection(): void
  commitOverlaySnapshot(snapshot: SelectionOverlaySnapshot): void
  updateFillHandleRange(range: SelectionRange<RowKey> | null): void
  subscribe(listener: SelectionControllerListener<RowKey>): () => void
  triggerAutoscroll(pointer: PointerCoordinates): void
  stopAutoscroll(): void
  dispose(): void
}

export type SelectionControllerFactory = <RowKey>(
  options: SelectionControllerOptions<RowKey>,
) => SelectionController<RowKey>

class BasicSelectionController<RowKey> implements SelectionController<RowKey> {
  private readonly environment: SelectionEnvironment<RowKey>
  private readonly context: GridSelectionContext<RowKey>
  private readonly store: SelectionStore<RowKey>
  private readonly listeners = new Set<SelectionControllerListener<RowKey>>()
  private storeSubscription: (() => void) | null
  private lastFillHandleSignature: string | null = null
  private disposed = false

  constructor(options: SelectionControllerOptions<RowKey>) {
    this.environment = options.environment
    this.context = options.context
    this.store = createSelectionStore<RowKey>({ initialState: options.initialState })
    this.storeSubscription = this.store.subscribe(state => {
      this.notify({ type: "selection-change", state })
    })
    this.notify({ type: "selection-change", state: this.store.getState() })
  }

  getState(): HeadlessSelectionState<RowKey> {
    return this.store.getState()
  }

  setState(state: HeadlessSelectionState<RowKey>): void {
    this.assertActive()
    this.store.setState(state)
  }

  updateRanges(ranges: readonly GridSelectionRange<RowKey>[], activeRangeIndex?: number): void {
    this.assertActive()
    const result = setSelectionRanges<RowKey>({
      ranges,
      activeRangeIndex,
      context: this.context,
    })
    this.store.applyResult(result)
  }

  focusCell(point: SelectionPoint<RowKey>): void {
    this.assertActive()
    const result = selectSingleCell<RowKey>({
      point,
      context: this.context,
    })
    this.store.applyResult(result)
  }

  extendSelection(point: SelectionPoint<RowKey>): void {
    this.assertActive()
    const state = this.store.peekState()
    const result = extendSelectionToPoint<RowKey>({
      state,
      activeRangeIndex: state.activeRangeIndex,
      point,
      context: this.context,
    })
    this.store.applyResult(result)
  }

  toggleCell(point: SelectionPoint<RowKey>): void {
    this.assertActive()
    const state = this.store.peekState()
    const result = toggleCellSelection<RowKey>({
      state,
      point,
      context: this.context,
    })
    this.store.applyResult(result)
  }

  clearSelection(): void {
    this.assertActive()
    const result = clearSelectionHeadless<RowKey>({ context: this.context })
    this.store.applyResult(result)
  }

  commitOverlaySnapshot(snapshot: SelectionOverlaySnapshot): void {
    this.assertActive()
    this.environment.overlays.commit(snapshot)
    this.notify({ type: "overlay-update", snapshot })
  }

  updateFillHandleRange(range: SelectionRange<RowKey> | null): void {
    this.assertActive()
    const signature = range ? this.serializeFillHandleRange(range) : null
    if (signature === this.lastFillHandleSignature) {
      return
    }
    this.lastFillHandleSignature = signature
    this.notify({ type: "fill-handle-change", range })
  }

  subscribe(listener: SelectionControllerListener<RowKey>): () => void {
    if (this.disposed) {
      return () => {}
    }
    this.listeners.add(listener)
    return () => this.listeners.delete(listener)
  }

  triggerAutoscroll(pointer: PointerCoordinates): void {
    this.assertActive()
    this.environment.autoscroll?.update(pointer)
    this.notify({ type: "autoscroll", active: true, pointer })
  }

  stopAutoscroll(): void {
    this.assertActive()
    this.environment.autoscroll?.stop()
    this.notify({ type: "autoscroll", active: false })
  }

  dispose(): void {
    if (this.disposed) return
    this.storeSubscription?.()
    this.store.dispose()
    this.listeners.clear()
    this.lastFillHandleSignature = null
    this.disposed = true
  }

  private notify(event: SelectionControllerEvent<RowKey>): void {
    if (this.disposed) return
    for (const listener of this.listeners) {
      listener(event)
    }
  }

  private assertActive() {
    if (this.disposed) {
      throw new Error("SelectionController has been disposed")
    }
  }

  private serializeFillHandleRange(range: SelectionRange<RowKey>): string {
    return [
      range.startRow,
      range.endRow,
      range.startCol,
      range.endCol,
    ].join(":")
  }
}

export const createSelectionController: SelectionControllerFactory = options =>
  new BasicSelectionController(options)
