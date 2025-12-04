import {
  createSelectionController,
  type SelectionController,
  type SelectionControllerListener,
} from "../headlessSelectionController"
import type {
  SelectionEnvironment,
  SelectionOverlayEnvironment,
  SelectionOverlaySnapshot,
  SelectionRange,
  SelectionAutoscrollEnvironment,
  SelectionMeasurementEnvironment,
  SelectionDomEnvironment,
  SelectionSchedulerEnvironment,
} from "../selectionEnvironment"
import type { GridSelectionContext } from "../selectionState"
import type { HeadlessSelectionState } from "../update"
import type { UiTableColumn } from "../../types"
import { createFrameRequestScheduler } from "../../runtime/frameRequestScheduler"
import type { PointerCoordinates } from "../autoScroll"

export interface PlainSelectionEnvironmentOptions<RowKey> {
  columns?: readonly UiTableColumn[]
  overlays: SelectionOverlayEnvironment
  measurement: SelectionMeasurementEnvironment<RowKey>
  dom: SelectionDomEnvironment<RowKey>
  autoscroll?: SelectionAutoscrollEnvironment
  scheduler?: SelectionSchedulerEnvironment
  globalObject?: typeof globalThis
}

export interface PlainSelectionEnvironmentBindings<RowKey> {
  environment: SelectionEnvironment<RowKey>
  setColumns(columns: readonly UiTableColumn[]): void
  dispose(): void
}

export function createPlainSelectionEnvironment<RowKey>(
  options: PlainSelectionEnvironmentOptions<RowKey>,
): PlainSelectionEnvironmentBindings<RowKey> {
  const ownsScheduler = !options.scheduler
  const frameScheduler = ownsScheduler
    ? createFrameRequestScheduler({
        globalObject: options.globalObject ?? (typeof window !== "undefined" ? window : globalThis),
      })
    : null

  const scheduler: SelectionSchedulerEnvironment = options.scheduler ?? {
    request: callback => frameScheduler!.request(callback),
    cancel: handle => frameScheduler!.cancel(handle),
  }

  const environment: SelectionEnvironment<RowKey> = {
    columns: Array.isArray(options.columns) ? [...options.columns] : [],
    overlays: options.overlays,
    measurement: options.measurement,
    scheduler,
    dom: options.dom,
  }

  if (options.autoscroll) {
    environment.autoscroll = options.autoscroll
  }

  const setColumns = (columns: readonly UiTableColumn[]) => {
    environment.columns = Array.isArray(columns) ? [...columns] : []
  }

  const dispose = () => {
    if (ownsScheduler && frameScheduler) {
      frameScheduler.dispose()
    }
  }

  return {
    environment,
    setColumns,
    dispose,
  }
}

export interface PlainSelectionAdapterOptions<RowKey> extends PlainSelectionEnvironmentOptions<RowKey> {
  context: GridSelectionContext<RowKey>
  initialState?: HeadlessSelectionState<RowKey>
}

export interface PlainSelectionAdapter<RowKey> {
  controller: SelectionController<RowKey>
  getState(): HeadlessSelectionState<RowKey>
  getOverlaySnapshot(): SelectionOverlaySnapshot | null
  getFillHandleRange(): SelectionRange<RowKey> | null
  getAutoscrollState(): { active: boolean; pointer: PointerCoordinates | null }
  subscribe(listener: SelectionControllerListener<RowKey>): () => void
  setColumns(columns: readonly UiTableColumn[]): void
  dispose(): void
}

export function createPlainSelectionAdapter<RowKey>(
  options: PlainSelectionAdapterOptions<RowKey>,
): PlainSelectionAdapter<RowKey> {
  const bindings = createPlainSelectionEnvironment<RowKey>(options)
  const controller = createSelectionController<RowKey>({
    environment: bindings.environment,
    context: options.context,
    initialState: options.initialState,
  })

  let currentState = controller.getState()
  let overlaySnapshot: SelectionOverlaySnapshot | null = null
  let fillHandleRange: SelectionRange<RowKey> | null = null
  let autoscrollState: { active: boolean; pointer: PointerCoordinates | null } = {
    active: false,
    pointer: null,
  }

  const listeners = new Set<SelectionControllerListener<RowKey>>()
  let disposed = false

  const controllerUnsubscribe = controller.subscribe(event => {
    if (disposed) return

    if (event.type === "selection-change") {
      currentState = event.state
    } else if (event.type === "overlay-update") {
      overlaySnapshot = event.snapshot
    } else if (event.type === "fill-handle-change") {
      fillHandleRange = event.range
    } else if (event.type === "autoscroll") {
      autoscrollState = event.active
        ? { active: true, pointer: event.pointer }
        : { active: false, pointer: null }
    }

    if (listeners.size) {
      for (const listener of listeners) {
        listener(event)
      }
    }
  })

  const subscribe = (listener: SelectionControllerListener<RowKey>) => {
    if (disposed) {
      return () => {}
    }
    listeners.add(listener)
    return () => {
      listeners.delete(listener)
    }
  }

  const setColumns = (columns: readonly UiTableColumn[]) => {
    bindings.setColumns(columns)
  }

  const dispose = () => {
    if (disposed) return
    disposed = true
    controllerUnsubscribe()
    listeners.clear()
    controller.dispose()
    bindings.dispose()
  }

  return {
    controller,
    getState: () => currentState,
    getOverlaySnapshot: () => overlaySnapshot,
    getFillHandleRange: () => fillHandleRange,
    getAutoscrollState: () => autoscrollState,
    subscribe,
    setColumns,
    dispose,
  }
}
