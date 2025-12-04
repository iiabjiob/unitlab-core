import { computed, isRef, nextTick, onBeforeUnmount, ref, shallowRef, watch, watchEffect } from "vue"
import { moveSelectionArea } from "@/ui-table/core/selection/moveSelection"
import type { ComputedRef, Ref } from "vue"
import {
  clampGridSelectionPoint,
  createGridSelectionRange,
  createGridSelectionRangeFromInput,
  resolveSelectionBounds,
  type GridSelectionPoint as CoreSelectionPoint,
  type GridSelectionRange as CoreSelectionRange,
  type GridSelectionContext,
  type SelectionArea as CoreSelectionArea,
} from "../../core/selection/selectionState"
import { canPasteIntoColumn } from "./useTableEditing"
import type { ColumnMetric } from "./useAxisVirtualizer"
import type { RowKey } from "@/composables/useSelectableRows"
import type {
  ImperativeRowClassState,
  ImperativeSelectionCellState,
  ImperativeSelectionSnapshot,
} from "../../core/imperative/types"
import {
  areaContainsCell,
  areaContainsColumn,
  areaContainsRow,
  areaEdges,
  anyAreaContainsColumn,
  anyAreaContainsRow,
  findAreaIndexContaining,
  findRangeIndexContaining as findRangeIndexContainingCell,
  rangeContainsCell,
  selectionEdges as computeSelectionEdges,
} from "../../core/selection/geometry"
import {
  clampCutPreviewState,
  resolveCutPreviewFromSnapshot,
  type CutPreviewState,
} from "../../core/selection/cutPreview"
import { commitFillPreview } from "@/ui-table/core/selection/fillCommit"
import { resolveAutoFillDown } from "../../core/selection/fillAuto"
import { applyMatrixToGrid } from "@/ui-table/core/selection/matrix"
import { remapSelectionState } from "@/ui-table/core/selection/remap"
import type { SelectionMeasurementHandle } from "@/ui-table/core/selection/selectionEnvironment"
import { releaseFillHandleStyle, type FillHandleStylePayload } from "@/ui-table/core/selection/fillHandleStylePool"
import {
  createSelectionSnapshot,
  selectionSnapshotSignature,
  type GridSelectionSnapshot,
} from "@/ui-table/core/selection/snapshot"
import type { HeadlessSelectionState, ResolveSelectionUpdateResult } from "@/ui-table/core/selection/update"
import { setSelectionRanges as setSelectionRangesHeadless } from "@/ui-table/core/selection/operations"
import { resolveFullColumnSelection, resolveFullRowSelection } from "@/ui-table/core/selection/fullSelection"
import {
  type SelectionDragSession,
  startSelectionDragSession,
  updateSelectionDragSession,
  endSelectionDragSession,
} from "@/ui-table/core/selection/dragSession"
import {
  type FillDragSession,
  startFillDragSession as startCoreFillDragSession,
  updateFillDragSession as updateCoreFillDragSession,
  fillDragSessionWithPreview,
} from "@/ui-table/core/selection/fillSession"
import { createSharedStateContainer } from "@/ui-table/core/state/sharedStateContainer"
import { createSelectionDomAdapter } from "@/ui-table/vue/imperative/selectionDomAdapter"
import {
  computeCursorOverlayRect,
  computeFillPreviewOverlayRects,
  computeCutPreviewOverlayRects,
  computeActiveRangeOverlayRects,
  computeStaticRangeOverlayRects,
  prepareSelectionOverlayResources,
  type SelectionOverlayRect,
  type SelectionOverlayColumnSurface,
  type SelectionOverlayComputationContext,
  type SelectionOverlayComputationPrepared,
} from "@/ui-table/core/selection/selectionOverlay"
import { releaseOverlayRect, releaseOverlayRectArray } from "@/ui-table/core/selection/selectionOverlayRectPool"
import type { UiTableOverlayHandle, UiTableOverlayRect } from "../types/overlay"
import { useSelectionOverlayAdapter } from "./useSelectionOverlayAdapter"
import { createTableOverlayScrollEmitter } from "./useTableOverlayScrollState"
import { createVueSelectionEnvironment } from "@/ui-table/vue/adapters/selectionEnvironment"
import { createSelectionControllerAdapter } from "@/ui-table/vue/adapters/selectionControllerAdapter"
import { useSharedStateProperty } from "@/ui-table/vue/adapters/sharedState"
import type { PointerCoordinates } from "@/ui-table/core/selection/autoScroll"
import {
  BASE_ROW_HEIGHT,
  FILL_HANDLE_SIZE,
  clamp,
} from "../../core/utils/constants"
import type {
  CellEditEvent,
  UiTableColumn,
  UiTableSelectedCell,
  UiTableSelectionPoint,
  UiTableSelectionRangeInput,
  UiTableSelectionSnapshot,
  UiTableSelectionSnapshotRange,
  VisibleRow,
} from "../../core/types"
import type { HistoryEntry } from "./useTableHistory"

export type SelectionPoint = CoreSelectionPoint<RowKey>

type SelectionPointLike = CoreSelectionPoint<RowKey> | { rowId?: RowKey | null; rowIndex: number; colIndex: number }

export type SelectionRange = CoreSelectionRange<RowKey>

export type SelectionArea = CoreSelectionArea

export interface SelectedRowPayload {
  displayIndex: number
  originalIndex: number
  rowId?: RowKey | null
  row: any
}

interface PendingCutState {
  snapshot: UiTableSelectionSnapshot
  fullRowRange: { start: number; end: number } | null
}

const isSelectionDebugEnabled = typeof window !== "undefined" && Boolean(
  (window as any).__UNITLAB_SELECTION_DEBUG__ ?? (window as any).__UNITLAB_TABLE_DEBUG__ ?? false,
)

const selectionPerformance: Performance | null = typeof performance !== "undefined" ? performance : null
let overlayTimingCounter = 0

function runWithOverlayTiming<T>(label: string, callback: () => T): T {
  if (!isSelectionDebugEnabled || !selectionPerformance) {
    return callback()
  }

  const { mark, measure, clearMarks } = selectionPerformance
  if (typeof mark !== "function" || typeof measure !== "function") {
    return callback()
  }

  const id = `ui-table-overlay:${label}:${overlayTimingCounter++}`
  const startMark = `${id}:start`
  const endMark = `${id}:end`

  mark.call(selectionPerformance, startMark)
  try {
    return callback()
  } finally {
    mark.call(selectionPerformance, endMark)
    measure.call(selectionPerformance, id, startMark, endMark)
    if (typeof clearMarks === "function") {
      clearMarks.call(selectionPerformance, startMark)
      clearMarks.call(selectionPerformance, endMark)
    }
  }
}

function debugSelectionLog(..._args: unknown[]) {}

interface UseTableSelectionOptions {
  containerRef: Ref<HTMLDivElement | null>
  overlayContainerRef?: Ref<HTMLElement | null>
  localColumns: Ref<UiTableColumn[]>
  processedRows: ComputedRef<VisibleRow[]>
  totalRowCount: Ref<number>
  viewport: {
    effectiveRowHeight: Ref<number>
    viewportHeight: Ref<number>
    viewportWidth: Ref<number>
    scrollTop: Ref<number>
    scrollLeft: Ref<number>
    startIndex: Ref<number>
    endIndex: Ref<number>
    visibleStartCol: Ref<number>
    visibleEndCol: Ref<number>
    columnWidthMap: Ref<Map<string, number>>
    pinnedLeftEntries: Ref<ColumnMetric[]>
    pinnedRightEntries: Ref<ColumnMetric[]>
    virtualizationEnabled: Ref<boolean>
    clampScrollTopValue: (value: number) => number
    scrollToColumn?: (key: string) => void
  }
  isEditingCell: Ref<boolean>
  columnSurfaces?: ComputedRef<Map<string, SelectionOverlayColumnSurface>>
  focusContainer: () => void
  emitSelectionChange: (snapshot: UiTableSelectionSnapshot) => void
  setCellValueDirect: (rowRef: RowKey | number, colIndex: number, value: unknown, options?: {
    collector?: CellEditEvent[]
    historyCollector?: HistoryEntry[]
    suppressHistory?: boolean
    force?: boolean
  }) => boolean
  setCellValueFromPaste: (rowRef: RowKey | number, colIndex: number, rawValue: string, options?: {
    collector?: CellEditEvent[]
    historyCollector?: HistoryEntry[]
    suppressHistory?: boolean
  }) => boolean
  getCellRawValue: (rowRef: RowKey | number, colIndex: number) => unknown
  dispatchEvents: (events: CellEditEvent[]) => void
  recordHistory: (entries: HistoryEntry[]) => void
  refreshViewport?: (force?: boolean) => void
  stopAutoScroll: () => void
  updateAutoScroll: (pointer: PointerCoordinates) => void
  lastPointer: Ref<PointerCoordinates | null>
  deleteRows?: (rows: SelectedRowPayload[]) => void
  rowIndexColumnKey?: string
  rowSelectionMode?: Ref<"cell" | "row"> | "cell" | "row"
  getRowIdByIndex: (rowIndex: number) => RowKey | null
  findRowIndexById: (rowId: RowKey) => number | null
  resolveCellElement: (rowIndex: number, columnKey: string) => HTMLElement | null
  resolveHeaderCellElement: (columnKey: string) => HTMLElement | null
  overlayComponentRef?: Ref<UiTableOverlayHandle | null>
  pinnedRightWidth?: Ref<number>
  pinnedLeftOffset?: Ref<number>
  pinnedRightOffset?: Ref<number>
  overlayScrollControlledExternally?: boolean
}

export const useTableSelection = (options: UseTableSelectionOptions) => {
  const {
    containerRef,
    localColumns,
    processedRows,
    totalRowCount,
    viewport,
    isEditingCell,
    focusContainer,
    emitSelectionChange,
    setCellValueDirect,
    setCellValueFromPaste,
    getCellRawValue,
    dispatchEvents,
    recordHistory,
  refreshViewport,
    stopAutoScroll,
    updateAutoScroll,
  lastPointer,
    deleteRows,
    rowIndexColumnKey,
    rowSelectionMode: rowSelectionModeOption,
    getRowIdByIndex,
    findRowIndexById,
    resolveCellElement,
    resolveHeaderCellElement,
    overlayContainerRef,
    overlayComponentRef: overlayComponentRefOption,
    pinnedRightWidth: pinnedRightWidthRef,
    pinnedLeftOffset: pinnedLeftOffsetRef,
    pinnedRightOffset: pinnedRightOffsetRef,
    columnSurfaces: columnSurfacesRef,
    overlayScrollControlledExternally,
  } = options

  const overlayComponentRef = overlayComponentRefOption ?? shallowRef<UiTableOverlayHandle | null>(null)
  const pinnedRightWidth = pinnedRightWidthRef ?? ref(0)
  const pinnedLeftOffset = pinnedLeftOffsetRef ?? ref(0)
  const pinnedRightOffset = pinnedRightOffsetRef ?? pinnedRightWidth
  const overlayScrollExternal = overlayScrollControlledExternally === true

  const {
    effectiveRowHeight: effectiveRowHeightRef,
    viewportHeight: viewportHeightRef,
    viewportWidth: viewportWidthRef,
  } = viewport

  const domAdapter = createSelectionDomAdapter<RowKey>({
    containerRef,
    overlayContainerRef,
    localColumns,
    totalRowCount,
    viewport,
    rowIndexColumnKey,
    getRowIdByIndex,
    resolveCellElement,
    resolveHeaderCellElement,
  })

  const selectionContext: GridSelectionContext<RowKey> = {
    grid: getGridDimensions(),
    getRowIdByIndex,
  }

  watch(
    () => [totalRowCount.value, localColumns.value.length] as const,
    () => {
      selectionContext.grid = getGridDimensions()
    },
    { immediate: true },
  )

  const rowSelectionMode = isRef(rowSelectionModeOption)
    ? rowSelectionModeOption
    : ref(rowSelectionModeOption ?? "cell")
  const isFullRowMode = computed(() => rowSelectionMode.value === "row")

  const selectionSharedState = createSharedStateContainer({
    selectionRanges: [] as SelectionRange[],
    selectionAreas: [] as SelectionArea[],
    selectedCell: null as SelectionPoint | null,
    anchorCell: null as SelectionPoint | null,
    activeRangeIndex: -1,
    dragAnchorCell: null as SelectionPoint | null,
    selectionDragSession: null as SelectionDragSession<RowKey> | null,
    isDraggingSelection: false,
    isFillDragging: false,
    fullRowSelection: null as { start: number; end: number } | null,
    rowSelectionAnchor: null as number | null,
    isRowSelectionDragging: false,
    columnSelectionState: null as { column: number; anchorRow: number | null } | null,
    fillSession: null as FillDragSession<RowKey> | null,
  })

  const selectionRanges = useSharedStateProperty(selectionSharedState, "selectionRanges")
  const selectionAreas = useSharedStateProperty(selectionSharedState, "selectionAreas")
  const selectedCell = useSharedStateProperty(selectionSharedState, "selectedCell")
  const anchorCell = useSharedStateProperty(selectionSharedState, "anchorCell")
  const activeRangeIndex = useSharedStateProperty(selectionSharedState, "activeRangeIndex")
  const dragAnchorCell = useSharedStateProperty(selectionSharedState, "dragAnchorCell")
  const selectionDragSession = useSharedStateProperty(selectionSharedState, "selectionDragSession")
  const isDraggingSelection = useSharedStateProperty(selectionSharedState, "isDraggingSelection")
  const isFillDragging = useSharedStateProperty(selectionSharedState, "isFillDragging")
  const fullRowSelection = useSharedStateProperty(selectionSharedState, "fullRowSelection")
  const rowSelectionAnchor = useSharedStateProperty(selectionSharedState, "rowSelectionAnchor")
  const isRowSelectionDragging = useSharedStateProperty(selectionSharedState, "isRowSelectionDragging")
  const columnSelectionState = useSharedStateProperty(selectionSharedState, "columnSelectionState")
  const fillSession = useSharedStateProperty(selectionSharedState, "fillSession")
  const selectionStateView = selectionSharedState.state

  const initialHeadlessSelectionState: HeadlessSelectionState<RowKey> = {
    ranges: selectionStateView.selectionRanges,
    areas: selectionStateView.selectionAreas,
    activeRangeIndex: selectionStateView.activeRangeIndex,
    selectedPoint: selectionStateView.selectedCell,
    anchorPoint: selectionStateView.anchorCell,
    dragAnchorPoint: selectionStateView.dragAnchorCell,
  }

  const fullColumnSelection = computed<number | null>(() =>
    detectFullColumnSelectionIndex(selectionRanges.value, getGridDimensions())
  )
  const selectionOverlayRects = ref<UiTableOverlayRect[]>([])
  const activeSelectionOverlayRects = ref<UiTableOverlayRect[]>([])
  const selectionCursorOverlay = shallowRef<UiTableOverlayRect | null>(null)
  const fillPreviewOverlayRects = ref<UiTableOverlayRect[]>([])
  const cutPreviewOverlayRects = ref<UiTableOverlayRect[]>([])
  const cutPreviewState = ref<CutPreviewState | null>(null)
  const pendingCutState = ref<PendingCutState | null>(null)
  const cutPreviewRanges = computed(() => cutPreviewState.value?.areas ?? [])
  const cutPreviewActiveIndex = computed(() => cutPreviewState.value?.activeIndex ?? -1)

  const measureFillHandle = (range: SelectionRange) =>
    domAdapter.resolveFillHandleStyle({
      range,
      fillHandleSize: FILL_HANDLE_SIZE,
    })

  const measureCellRect = (point: SelectionPoint): SelectionMeasurementHandle<DOMRect | null> => {
    const column = localColumns.value[point.colIndex]
    const key = column?.key
    const element = key && typeof resolveCellElement === "function"
      ? resolveCellElement(point.rowIndex, key)
      : null
    const rect = element?.getBoundingClientRect() ?? null
    return {
      promise: Promise.resolve(rect),
      cancel() {},
    }
  }

  const {
    environment: selectionEnvironment,
    dispose: disposeSelectionEnvironment,
  } = createVueSelectionEnvironment<RowKey>({
    localColumns,
    selectionOverlayRects,
    activeSelectionOverlayRects,
    selectionCursorOverlay,
    fillPreviewOverlayRects,
    cutPreviewOverlayRects,
    focusContainer,
    resolveCellElement,
    resolveHeaderCellElement,
    getRowIdByIndex,
    findRowIndexById,
    measureFillHandle,
    measureCellRect,
    scrollSelectionIntoView: input => {
      const {
        range,
        cursor = selectedCell.value,
        attempt,
        maxAttempts = 2,
      } = input
      domAdapter.scrollSelectionIntoView({
        range,
        cursor,
        attempt,
        maxAttempts,
      })
    },
    resolveCellFromPoint: (clientX, clientY) => domAdapter.resolveCellFromPoint(clientX, clientY),
    resolveRowIndexFromPoint: (clientX, clientY) => domAdapter.resolveRowIndexFromPoint(clientX, clientY),
    invalidateMetrics: () => domAdapter.invalidateMetrics(),
    stopAutoScroll,
    updateAutoScroll,
  })

  const selectionControllerAdapter = createSelectionControllerAdapter<RowKey>({
    environment: selectionEnvironment,
    context: getSelectionContext(),
    initialState: initialHeadlessSelectionState,
  })

  debugSelectionLog("controllerAdapter initialized", true)

  type OverlaySignature = {
    ranges: string
    active: string
    fill: string
    cut: string
  }

  type OverlayComputationInputs = {
    context: SelectionOverlayComputationContext<RowKey>
    prepared: SelectionOverlayComputationPrepared | null
    preparedKey: string
  }

  let lastOverlaySignature: OverlaySignature = { ranges: "", active: "", fill: "", cut: "" }
  let pendingOverlaySignature: OverlaySignature | null = null

  type ControllerFrameHandle = number | ReturnType<typeof setTimeout>
  const controllerFrameGlobal: typeof globalThis = typeof window !== "undefined" ? window : globalThis

  let pendingControllerState: HeadlessSelectionState<RowKey> | null = null
  let applyStateFrameHandle: ControllerFrameHandle | null = null
  let applyStateFrameCancel: ((handle: ControllerFrameHandle) => void) | null = null
  let pendingNextTickFlush = false

  const flushPendingControllerState = () => {
    if (!pendingControllerState) {
      return
    }
    const stateToApply = pendingControllerState
    pendingControllerState = null
    doApplyControllerState(stateToApply)
  }

  const scheduleNextTickFlush = () => {
    if (pendingNextTickFlush) {
      return
    }
    pendingNextTickFlush = true
    nextTick(() => {
      pendingNextTickFlush = false
      flushPendingControllerState()
    })
  }

  const scheduleControllerStateFlush = () => {
    if (applyStateFrameHandle !== null) {
      scheduleNextTickFlush()
      return
    }

    if (typeof (controllerFrameGlobal as any).requestAnimationFrame === "function") {
      const handle = (controllerFrameGlobal as any).requestAnimationFrame(() => {
        applyStateFrameHandle = null
        applyStateFrameCancel = null
        flushPendingControllerState()
      })
      applyStateFrameHandle = handle
      applyStateFrameCancel = cancelHandle => {
        if (typeof (controllerFrameGlobal as any).cancelAnimationFrame === "function") {
          (controllerFrameGlobal as any).cancelAnimationFrame(cancelHandle as number)
        }
      }
      scheduleNextTickFlush()
      return
    }

    const timeoutHandle = controllerFrameGlobal.setTimeout(() => {
      applyStateFrameHandle = null
      applyStateFrameCancel = null
      flushPendingControllerState()
    }, 16)
    applyStateFrameHandle = timeoutHandle
    applyStateFrameCancel = cancelHandle => {
      controllerFrameGlobal.clearTimeout(cancelHandle as ReturnType<typeof setTimeout>)
    }
    scheduleNextTickFlush()
  }

  const cancelPendingControllerFrame = () => {
    if (applyStateFrameHandle === null || !applyStateFrameCancel) {
      return
    }
    applyStateFrameCancel(applyStateFrameHandle)
    applyStateFrameHandle = null
    applyStateFrameCancel = null
  }

  const doApplyControllerState = (state: HeadlessSelectionState<RowKey>) => {
    const nextOverlaySignature = buildOverlaySignature()
    const baseline = pendingOverlaySignature ?? lastOverlaySignature
    const rangesChanged = nextOverlaySignature.ranges !== baseline.ranges
    const activeChanged = nextOverlaySignature.active !== baseline.active
    const fillChanged = nextOverlaySignature.fill !== baseline.fill
    const cutChanged = nextOverlaySignature.cut !== baseline.cut
    const overlayChanged = rangesChanged || activeChanged || fillChanged || cutChanged
    emitSelectionChangeSnapshot()
    if (!overlayChanged) {
      updateCursorOverlayFromState(state)
      return
    }

    if (rangesChanged) {
      pendingOverlaySignature = nextOverlaySignature
      const resources = resolveOverlayComputationInputs(state)

      if (!resources) {
        let overlayUpdated = false
        overlayUpdated = updateStaticRangeOverlayFromState(state, null, { commit: false, signature: nextOverlaySignature }) || overlayUpdated
        if (fillChanged) {
          overlayUpdated = updateFillPreviewOverlayFromState(state, null, { commit: false }) || overlayUpdated
        }
        if (cutChanged) {
          overlayUpdated = updateCutPreviewOverlayFromState(state, null, { commit: false }) || overlayUpdated
        }
        if (activeChanged) {
          overlayUpdated = updateActiveRangeOverlayFromState(state, null, { commit: false }) || overlayUpdated
        }
        updateCursorOverlayFromState(state, null)
        if (overlayUpdated) {
          commitOverlaySnapshotFromRefs()
        }
        scheduleOverlayUpdate()
        return
      }

      let overlayUpdated = false
      overlayUpdated = updateStaticRangeOverlayFromState(state, resources, { commit: false, signature: nextOverlaySignature }) || overlayUpdated
      if (fillChanged) {
        overlayUpdated = updateFillPreviewOverlayFromState(state, resources, { commit: false }) || overlayUpdated
      }
      if (cutChanged) {
        overlayUpdated = updateCutPreviewOverlayFromState(state, resources, { commit: false }) || overlayUpdated
      }
      if (activeChanged) {
        overlayUpdated = updateActiveRangeOverlayFromState(state, resources, { commit: false }) || overlayUpdated
      }

      updateCursorOverlayFromState(state, resources)

      if (overlayUpdated) {
        commitOverlaySnapshotFromRefs()
      }

      lastOverlaySignature = nextOverlaySignature
      pendingOverlaySignature = null
      return
    }

    const resources = resolveOverlayComputationInputs(state)
    if (!resources || !resources.prepared) {
      pendingOverlaySignature = nextOverlaySignature
      scheduleOverlayUpdate()
      return
    }

    updateCursorOverlayFromState(state, resources)
    if (fillChanged) {
      updateFillPreviewOverlayFromState(state, resources, { commit: true })
    }
    if (cutChanged) {
      updateCutPreviewOverlayFromState(state, resources, { commit: true })
    }
    if (activeChanged) {
      updateActiveRangeOverlayFromState(state, resources, { commit: true })
    }

    lastOverlaySignature = nextOverlaySignature
    pendingOverlaySignature = null
  }

  const applyControllerState = (state: HeadlessSelectionState<RowKey>) => {
    pendingControllerState = state
    syncSelectionFromController(state)
    handleSelectionStateChange(state)
    scheduleControllerStateFlush()
  }

  const runAutoScrollUpdate = (pointer: PointerCoordinates) => {
    selectionControllerAdapter.controller.triggerAutoscroll(pointer)
  }

  const haltAutoScroll = (fromControllerEvent = false) => {
    if (!fromControllerEvent) {
      selectionControllerAdapter.controller.stopAutoscroll()
    }
    lastPointer.value = null
  }

  const lastSelectionSignature = ref("")
  const OVERLAY_BACKGROUND_TIMEOUT = 160
  const OVERLAY_MICROTASK_HANDLE = -1
  const scheduleOverlayMicrotask =
    typeof globalThis.queueMicrotask === "function"
      ? (callback: () => void) => {
          globalThis.queueMicrotask(callback)
        }
      : (callback: () => void) => {
          void Promise.resolve().then(callback)
        }

  let overlayScheduleHandle: number | null = null
  let overlayScheduleMode: "frame" | "idle" | null = null
  let overlayMicrotaskScheduled = false
  const imperativeSnapshotCache: ImperativeSelectionSnapshot = {
    hasCutPreview: false,
    rowSelection: new Set<number>(),
    columnSelection: new Set<number>(),
    anchorKey: null,
    cursorKey: null,
    cells: new Map<string, ImperativeSelectionCellState>(),
    rowClasses: new Map<number, ImperativeRowClassState>(),
    generation: 0,
  }
  let imperativeSnapshotGeneration = 0

  const uiRangeCache: UiTableSelectionSnapshotRange[] = []
  const uiRangePool: UiTableSelectionSnapshotRange[] = []
  let uiActiveCellCache: UiTableSelectionPoint | null = { rowId: null, rowIndex: 0, colIndex: 0 }
  const uiSnapshotCache: UiTableSelectionSnapshot = {
    ranges: uiRangeCache,
    activeRangeIndex: 0,
    activeCell: null,
    clone: () => cloneUiSelectionSnapshot(uiSnapshotCache),
  }

  const controllerAutoscrollUnsubscribe = selectionControllerAdapter.subscribe(event => {
    if (event.type !== "autoscroll") return
    if (event.active) {
      lastPointer.value = event.pointer
    } else {
      haltAutoScroll(true)
    }
  })

  const invalidateSelectionMetrics = () => {
    if (selectionEnvironment.dom.invalidateMetrics) {
      selectionEnvironment.dom.invalidateMetrics()
      return
    }
    domAdapter.invalidateMetrics()
  }

  function cloneSelectionPoint(point: SelectionPoint): SelectionPoint {
    return {
      rowIndex: point.rowIndex,
      colIndex: point.colIndex,
      rowId: point.rowId ?? null,
    }
  }

  function cloneSelectionArea(area: SelectionArea): SelectionArea {
    return {
      startRow: area.startRow,
      endRow: area.endRow,
      startCol: area.startCol,
      endCol: area.endCol,
    }
  }

  function cloneSelectionRange(range: SelectionRange): SelectionRange {
    return {
      startRow: range.startRow,
      endRow: range.endRow,
      startCol: range.startCol,
      endCol: range.endCol,
      anchor: cloneSelectionPoint(range.anchor),
      focus: cloneSelectionPoint(range.focus),
      startRowId: range.startRowId ?? null,
      endRowId: range.endRowId ?? null,
    }
  }

  const toPointerCoordinates = (event: MouseEvent): PointerCoordinates => ({
    clientX: event.clientX,
    clientY: event.clientY,
  })

  function cloneFillSession(session: FillDragSession<RowKey>): FillDragSession<RowKey> {
    return {
      origin: {
        anchor: cloneSelectionPoint(session.origin.anchor),
        focus: cloneSelectionPoint(session.origin.focus),
        startRow: session.origin.startRow,
        endRow: session.origin.endRow,
        startCol: session.origin.startCol,
        endCol: session.origin.endCol,
      },
      originArea: cloneSelectionArea(session.originArea),
      preview: session.preview ? cloneSelectionArea(session.preview) : null,
      target: session.target ? cloneSelectionPoint(session.target) : null,
      axis: session.axis ?? null,
    }
  }

  function setFillSession(session: FillDragSession<RowKey> | null) {
    fillSession.value = session ? cloneFillSession(session) : null
  }

  function refreshFillSession(session: FillDragSession<RowKey>) {
    setFillSession(session)
  }

  const fillPreviewRange = computed(() => fillSession.value?.preview ?? null)
  const fillHandleStyle = ref<FillHandleStylePayload | null>(null)

  let fillHandleMeasurement: SelectionMeasurementHandle<FillHandleStylePayload | null> | null = null

  function cancelFillHandleMeasurement() {
    if (fillHandleMeasurement) {
      fillHandleMeasurement.cancel()
      fillHandleMeasurement = null
    }
  }

  const overlayDimensions = ref({ width: 0, height: 0 })

  const resolveOverlayElement = () => overlayContainerRef?.value ?? containerRef.value

  watch(
    () => [resolveOverlayElement(), containerRef.value] as const,
    ([overlayEl, containerEl], _previous, onCleanup) => {
      if (!overlayEl) {
        overlayDimensions.value = { width: 0, height: 0 }
        return
      }

      let lastWidth = overlayDimensions.value.width
      let lastHeight = overlayDimensions.value.height

      const updateMetrics = () => {
        const nextWidth = overlayEl.clientWidth ?? 0
        const nextHeight = overlayEl.clientHeight ?? 0
        if (nextWidth === lastWidth && nextHeight === lastHeight) {
          return
        }
        overlayDimensions.value = { width: nextWidth, height: nextHeight }
        lastWidth = nextWidth
        lastHeight = nextHeight
        scheduleOverlayUpdate()
      }

      updateMetrics()

      let resizeObserver: ResizeObserver | null = null
      let intervalId: number | null = null

      if (typeof ResizeObserver !== "undefined") {
        resizeObserver = new ResizeObserver(() => updateMetrics())
        resizeObserver.observe(overlayEl)
        if (containerEl && containerEl !== overlayEl) {
          resizeObserver.observe(containerEl)
        }
      } else if (typeof window !== "undefined") {
        intervalId = window.setInterval(updateMetrics, 250)
      }

      onCleanup(() => {
        resizeObserver?.disconnect()
        if (intervalId !== null) {
          window.clearInterval(intervalId)
        }
      })
    },
    { immediate: true },
  )

  const overlayViewport = computed(() => {
    const baseWidth = viewportWidthRef.value || overlayDimensions.value.width
    return {
      width: baseWidth,
      height: viewportHeightRef.value || overlayDimensions.value.height,
      scrollLeft: viewport.scrollLeft.value,
      scrollTop: viewport.scrollTop.value,
      startRow: viewport.startIndex.value,
      endRow: viewport.endIndex.value,
      visibleStartCol: viewport.visibleStartCol.value,
      visibleEndCol: viewport.visibleEndCol.value,
      virtualizationEnabled: viewport.virtualizationEnabled.value !== false,
    }
  })

  const overlayScrollState = createTableOverlayScrollEmitter()

  const normalizeFinite = (value: number | null | undefined) => {
    if (!Number.isFinite(value ?? NaN)) {
      return 0
    }
    return value as number
  }

  if (!overlayScrollExternal) {
    watchEffect(
      () => {
        const viewportSnapshot = overlayViewport.value
        const pinnedLeft = pinnedLeftOffset.value
        const pinnedRight = pinnedRightOffset.value
        const nextWidth = Math.max(0, normalizeFinite(viewportSnapshot?.width))
        const nextHeight = Math.max(0, normalizeFinite(viewportSnapshot?.height))
        const nextScrollLeft = normalizeFinite(viewportSnapshot?.scrollLeft)
        const nextScrollTop = normalizeFinite(viewportSnapshot?.scrollTop)
        const nextPinnedLeft = Math.max(0, normalizeFinite(pinnedLeft))
        const nextPinnedRight = Math.max(0, normalizeFinite(pinnedRight))
        overlayScrollState.emit({
          viewportWidth: nextWidth,
          viewportHeight: nextHeight,
          scrollLeft: nextScrollLeft,
          scrollTop: nextScrollTop,
          pinnedOffsetLeft: nextPinnedLeft,
          pinnedOffsetRight: nextPinnedRight,
        })
      },
      { flush: "sync" },
    )
  }

  const selectionOverlayAdapterHandle = useSelectionOverlayAdapter({
    overlayComponentRef,
    selectionRects: selectionOverlayRects,
    activeSelectionRects: activeSelectionOverlayRects,
    fillPreviewRects: fillPreviewOverlayRects,
    cutPreviewRects: cutPreviewOverlayRects,
    cursorRect: selectionCursorOverlay,
    fillHandleStyle,
    overlayViewport,
    pinnedLeftOffset,
    pinnedRightOffset,
    overlayScrollState,
  })

  const fillHandleSignature = computed(() => {
    const range = selectionControllerAdapter.fillHandleRange.value
    if (!range) return ""
    const selected = selectedCell.value
    return [
      range.startRow,
      range.endRow,
      range.startCol,
      range.endCol,
      selected?.rowIndex ?? "",
      selected?.colIndex ?? "",
    ].join(":")
  })

  const resolveControllerFillHandleRange = (): SelectionRange | null => {
    const state = selectionControllerAdapter.state.value
    if (isFillDragging.value) return null
    if (isEditingCell.value) return null
    if (fullRowSelection.value) return null
    if (fullColumnSelection.value !== null) return null
    if (!state.selectedPoint) return null
    const index = state.activeRangeIndex
    if (index < 0 || index >= state.ranges.length) {
      return null
    }
    const range = state.ranges[index]
    if (!range) {
      return null
    }
    return cloneSelectionRange(range)
  }

  const updateControllerFillHandleRange = () => {
    selectionControllerAdapter.controller.updateFillHandleRange(resolveControllerFillHandleRange())
  }

  watch(
    selectionControllerAdapter.state,
    state => {
      debugSelectionLog("controller state update", state)
      applyControllerState(state)
      updateControllerFillHandleRange()
    },
    { immediate: true },
  )

  watch(
    () => [
      isFillDragging.value,
      isEditingCell.value,
      fullRowSelection.value ? `${fullRowSelection.value.start}:${fullRowSelection.value.end}` : null,
      fullColumnSelection.value,
    ],
    () => {
      updateControllerFillHandleRange()
    },
    { immediate: true },
  )

  let lastResolvedFillHandleSignature: string | null = null

  watch(
    () => ({
      range: selectionControllerAdapter.fillHandleRange.value,
      signature: fillHandleSignature.value,
    }),
    ({ range, signature }) => {
      if (!range || !signature) {
        lastResolvedFillHandleSignature = null
        cancelFillHandleMeasurement()
        fillHandleStyle.value = null
        return
      }

      if (signature === lastResolvedFillHandleSignature) return

      cancelFillHandleMeasurement()
      const measurement = selectionEnvironment.measurement.measureFillHandle(range)

      fillHandleMeasurement = measurement
      lastResolvedFillHandleSignature = signature

      measurement.promise
        .then(style => {
          if (fillHandleMeasurement !== measurement) {
            if (style) {
              releaseFillHandleStyle(style)
            }
            return
          }
          fillHandleStyle.value = style
          fillHandleMeasurement = null
        })
        .catch(() => {
          if (fillHandleMeasurement !== measurement) return
          fillHandleStyle.value = null
          fillHandleMeasurement = null
        })
    },
    { immediate: true },
  )
  const lastCommittedFillArea = ref<SelectionArea | null>(null)

  function cloneUiSelectionPoint(point: UiTableSelectionPoint): UiTableSelectionPoint {
    return {
      rowId: point.rowId,
      rowIndex: point.rowIndex,
      colIndex: point.colIndex,
    }
  }

  function cloneUiSelectionRange(range: UiTableSelectionSnapshotRange): UiTableSelectionSnapshotRange {
    return {
      startRow: range.startRow,
      endRow: range.endRow,
      startCol: range.startCol,
      endCol: range.endCol,
      startRowId: range.startRowId,
      endRowId: range.endRowId,
      anchor: cloneUiSelectionPoint(range.anchor),
      focus: cloneUiSelectionPoint(range.focus),
    }
  }

  function cloneUiSelectionSnapshot(source: UiTableSelectionSnapshot): UiTableSelectionSnapshot {
    const ranges = source.ranges.map(cloneUiSelectionRange)
    const activeCell = source.activeCell ? cloneUiSelectionPoint(source.activeCell) : null
    const snapshot: UiTableSelectionSnapshot = {
      ranges,
      activeRangeIndex: source.activeRangeIndex,
      activeCell,
      clone: () => cloneUiSelectionSnapshot(snapshot),
    }
    return snapshot
  }

  function getGridDimensions() {
    return {
      rowCount: totalRowCount.value,
      colCount: localColumns.value.length,
    }
  }

  function getSelectionContext() {
    return selectionContext
  }

  function clampPointToGrid(point: SelectionPointLike): SelectionPoint {
    return clampGridSelectionPoint<RowKey>(point, getSelectionContext())
  }

  function createSelectionPointByIndex(rowIndex: number, colIndex: number): SelectionPoint {
    return clampPointToGrid({
      rowId: getRowIdByIndex(rowIndex),
      rowIndex,
      colIndex,
    })
  }

  function createRange(anchor: SelectionPointLike, focus: SelectionPointLike): SelectionRange {
    return createGridSelectionRange<RowKey>(anchor, focus, getSelectionContext())
  }

  function scheduleOverlayUpdate(options?: { priority?: "user-blocking" | "background"; timeout?: number }) {
    const priority = options?.priority ?? "user-blocking"
    const mode: "frame" | "idle" = priority === "background" ? "idle" : "frame"
    const scheduleTimeout = options?.timeout ?? OVERLAY_BACKGROUND_TIMEOUT

    if (overlayScheduleHandle !== null) {
      if (overlayScheduleMode === mode) {
        return
      }
      if (overlayScheduleHandle === OVERLAY_MICROTASK_HANDLE) {
        overlayMicrotaskScheduled = false
      } else {
        selectionEnvironment.scheduler.cancel(overlayScheduleHandle)
      }
      overlayScheduleHandle = null
      overlayScheduleMode = null
    }

    if (mode === "frame") {
      overlayScheduleMode = mode
      overlayScheduleHandle = OVERLAY_MICROTASK_HANDLE
      overlayMicrotaskScheduled = true

      const runOverlayFlush = () => {
        if (!overlayMicrotaskScheduled) {
          return
        }
        overlayMicrotaskScheduled = false
        overlayScheduleHandle = null
        overlayScheduleMode = null
        updateSelectionOverlay()
      }

      scheduleOverlayMicrotask(runOverlayFlush)
      return
    }

    overlayScheduleMode = mode

    const schedulerOptions = mode === "idle"
      ? { mode: "idle" as const, timeout: scheduleTimeout }
      : undefined

    overlayScheduleHandle = selectionEnvironment.scheduler.request(() => {
      overlayScheduleHandle = null
      overlayScheduleMode = null
      updateSelectionOverlay()
    }, schedulerOptions)
  }

  function applyOverlayOffsetToRect(rect: SelectionOverlayRect): UiTableOverlayRect {
    const clone: UiTableOverlayRect = {
      id: rect.id,
      left: rect.left,
      top: rect.top,
      width: rect.width,
      height: rect.height,
    }
    if (typeof rect.active === "boolean") {
      clone.active = rect.active
    }
    if (rect.pin) {
      clone.pin = rect.pin
    }
    return clone
  }

  function offsetOverlayRect(rect: SelectionOverlayRect | null): UiTableOverlayRect | null {
    if (!rect) return null
    const clone = applyOverlayOffsetToRect(rect)
    releaseOverlayRect(rect)
    return clone
  }

  function offsetOverlayRectArray(
    rects: SelectionOverlayRect[],
    options?: { release?: boolean },
  ): UiTableOverlayRect[] {
    const shouldRelease = options?.release !== false
    if (!rects.length) {
      if (shouldRelease) {
        releaseOverlayRectArray(rects)
      }
      return []
    }
    const result: UiTableOverlayRect[] = new Array(rects.length)
    for (let index = 0; index < rects.length; index += 1) {
      result[index] = applyOverlayOffsetToRect(rects[index])
    }
    if (shouldRelease) {
      releaseOverlayRectArray(rects)
    }
    return result
  }

  function rectsEqual(left: UiTableOverlayRect | null, right: UiTableOverlayRect | null): boolean {
    if (left === right) return true
    if (!left || !right) return false
    return (
      left.left === right.left &&
      left.top === right.top &&
      left.width === right.width &&
      left.height === right.height &&
      (left.pin ?? null) === (right.pin ?? null) &&
      (left.active ?? false) === (right.active ?? false) &&
      left.id === right.id
    )
  }

  function rectArraysEqual(left: UiTableOverlayRect[], right: UiTableOverlayRect[]): boolean {
    if (left === right) return true
    if (left.length !== right.length) return false
    for (let index = 0; index < left.length; index += 1) {
      if (!rectsEqual(left[index], right[index])) {
        return false
      }
    }
    return true
  }

  type PreparedResourceCacheEntry = {
    key: string
    prepared: SelectionOverlayComputationPrepared | null
  }

  let cachedPreparedResources: PreparedResourceCacheEntry | null = null

  type StaticOverlayCacheEntry = {
    signature: string
    preparedKey: string
    rects: SelectionOverlayRect[]
  }

  let cachedStaticOverlayRects: StaticOverlayCacheEntry | null = null

  function disposeStaticOverlayCache() {
    if (!cachedStaticOverlayRects) {
      return
    }
    releaseOverlayRectArray(cachedStaticOverlayRects.rects)
    cachedStaticOverlayRects = null
  }

  function buildPreparedResourceKey(context: SelectionOverlayComputationContext<RowKey>): string {
    const columnKeys = context.columns.map(column => context.getColumnKey(column)).join("|")
    const pinnedLeftKeys = context.pinnedLeft.map(entry => context.getColumnKey(entry.column)).join("|")
    const pinnedRightKeys = context.pinnedRight.map(entry => context.getColumnKey(entry.column)).join("|")
    const widths = Array.from(context.columnWidthMap.entries())
      .sort((a, b) => (a[0] > b[0] ? 1 : a[0] < b[0] ? -1 : 0))
      .map(([key, value]) => `${key}:${value}`)
      .join("|")
    const surfaces = context.columnSurfaces
      ? Array.from(context.columnSurfaces.entries())
          .sort((a, b) => (a[0] > b[0] ? 1 : a[0] < b[0] ? -1 : 0))
          .map(([key, surface]) => {
            const left = Math.round(surface.left * 1000) / 1000
            const width = Math.round(surface.width * 1000) / 1000
            return `${key}:${left}:${width}:${surface.pin}`
          })
          .join("|")
      : ""
    return [
      context.rowHeight,
      context.viewport.width,
      context.viewport.height,
      context.viewport.scrollTop,
      context.viewport.scrollLeft,
      columnKeys,
      pinnedLeftKeys,
      pinnedRightKeys,
      widths,
      surfaces,
      context.rowIndexColumnKey ?? "",
    ].join(";")
  }

  function resolvePreparedResourcesEntry(
    context: SelectionOverlayComputationContext<RowKey>,
  ): PreparedResourceCacheEntry {
    const cacheKey = buildPreparedResourceKey(context)
    if (cachedPreparedResources && cachedPreparedResources.key === cacheKey) {
      return cachedPreparedResources
    }
    const prepared = runWithOverlayTiming("prepareResources", () => prepareSelectionOverlayResources(context))
    const entry: PreparedResourceCacheEntry = { key: cacheKey, prepared }
    cachedPreparedResources = entry
    return entry
  }

  function commitOverlaySnapshotFromRefs() {
    selectionControllerAdapter.controller.commitOverlaySnapshot({
      ranges: selectionOverlayRects.value,
      activeRange: activeSelectionOverlayRects.value,
      fillPreview: fillPreviewOverlayRects.value,
      cutPreview: cutPreviewOverlayRects.value,
      cursor: selectionCursorOverlay.value,
    })
  }

  function resolveOverlayComputationInputs(
    state: HeadlessSelectionState<RowKey> = selectionControllerAdapter.state.value,
  ): OverlayComputationInputs | null {
    const rowHeight = resolveOverlayRowHeight()
    if (!(Number.isFinite(rowHeight) && rowHeight > 0)) {
      return null
    }

    const context: SelectionOverlayComputationContext<RowKey> = {
      ranges: state.ranges,
      activeRangeIndex: state.activeRangeIndex,
      activeCell: state.selectedPoint ?? state.anchorPoint ?? null,
      fillPreview: fillPreviewRange.value,
      cutPreview: cutPreviewRanges.value,
      cutPreviewActiveIndex: cutPreviewActiveIndex.value,
      columns: localColumns.value,
      columnWidthMap: viewport.columnWidthMap.value,
      pinnedLeft: viewport.pinnedLeftEntries.value,
      pinnedRight: viewport.pinnedRightEntries.value,
      viewport: overlayViewport.value,
      rowHeight,
      getColumnKey: column => column.key,
      isSystemColumn: column => column.isSystem === true,
      rowIndexColumnKey,
      columnSurfaces: columnSurfacesRef?.value,
    }

    const preparedEntry = resolvePreparedResourcesEntry(context)
    return { context, prepared: preparedEntry.prepared, preparedKey: preparedEntry.key }
  }

  function resolveStaticOverlayRects(
    inputs: OverlayComputationInputs,
    signature: string,
  ): SelectionOverlayRect[] {
    if (
      cachedStaticOverlayRects &&
      cachedStaticOverlayRects.signature === signature &&
      cachedStaticOverlayRects.preparedKey === inputs.preparedKey
    ) {
      return cachedStaticOverlayRects.rects
    }

    if (!inputs.prepared) {
      disposeStaticOverlayCache()
      cachedStaticOverlayRects = {
        signature,
        preparedKey: inputs.preparedKey,
        rects: [],
      }
      return cachedStaticOverlayRects.rects
    }

    const rects = runWithOverlayTiming("computeStaticRangeRects", () =>
      computeStaticRangeOverlayRects(inputs.context, inputs.prepared!),
    )
    disposeStaticOverlayCache()
    cachedStaticOverlayRects = {
      signature,
      preparedKey: inputs.preparedKey,
      rects,
    }
    return rects
  }

  function serializeSelectionRange(range: SelectionRange): string {
    return [
      range.startRow,
      range.endRow,
      range.startCol,
      range.endCol,
      range.anchor.rowIndex,
      range.anchor.colIndex,
      range.focus.rowIndex,
      range.focus.colIndex,
    ].join(":")
  }

  function serializeSelectionRanges(ranges: readonly SelectionRange[]): string {
    if (!ranges.length) return ""
    return ranges.map(serializeSelectionRange).join("|")
  }

  function serializeSelectionArea(area: SelectionArea | null): string {
    if (!area) return ""
    return [area.startRow, area.endRow, area.startCol, area.endCol].join(":")
  }

  function serializeSelectionAreas(areas: readonly SelectionArea[]): string {
    if (!areas.length) return ""
    return areas.map(serializeSelectionArea).join("|")
  }

  function buildOverlaySignature(): OverlaySignature {
    const ranges = selectionRanges.value
    const activeIndexRaw = activeRangeIndex.value
    const hasActive = ranges.length > 0 && activeIndexRaw >= 0 && activeIndexRaw < ranges.length
    const activeIndexNormalized = hasActive ? clamp(activeIndexRaw, 0, ranges.length - 1) : -1
    const activeRange = hasActive ? ranges[activeIndexNormalized] ?? null : null
    const staticRanges = hasActive ? ranges.filter((_, index) => index !== activeIndexNormalized) : ranges

    return {
      ranges: serializeSelectionRanges(staticRanges),
      active: activeRange ? serializeSelectionRange(activeRange) : "",
      fill: serializeSelectionArea(fillPreviewRange.value),
      cut: serializeSelectionAreas(cutPreviewRanges.value),
    }
  }

  function resolveOverlayRowHeight(): number {
    const measured = viewport.effectiveRowHeight.value
    if (Number.isFinite(measured) && measured > 0) {
      return measured
    }

    const sampleColumn = localColumns.value.find(column => {
      if (!column) return false
      if ((column as any).isSystem === true) return false
      if (rowIndexColumnKey && column.key === rowIndexColumnKey) return false
      return true
    })

    if (sampleColumn) {
      const normalizedRow = clamp(viewport.startIndex.value ?? 0, 0, Math.max(totalRowCount.value - 1, 0))
      const cellElement = typeof resolveCellElement === "function"
        ? resolveCellElement(normalizedRow, sampleColumn.key)
        : null
      const fallbackCell = cellElement ?? containerRef.value?.querySelector<HTMLElement>(
        `.ui-table-cell[data-row-index="${normalizedRow}"][data-col-key="${sampleColumn.key}"]`,
      )
      const rect = fallbackCell?.getBoundingClientRect()
      if (rect && rect.height > 0) {
        return rect.height
      }
    }

    return BASE_ROW_HEIGHT
  }

  function updateStaticRangeOverlayFromState(
    state: HeadlessSelectionState<RowKey> = selectionControllerAdapter.state.value,
    inputs?: OverlayComputationInputs | null,
    options?: { commit?: boolean; updateSignature?: boolean; signature?: OverlaySignature },
  ): boolean {
    const shouldCommit = options?.commit ?? false
    const shouldUpdateSignature = options?.updateSignature ?? false
    const signature = options?.signature ?? buildOverlaySignature()
    const staticSignature = signature.ranges

    let overlayChanged = false

    const resources = inputs === undefined ? resolveOverlayComputationInputs(state) : inputs
    if (!resources) {
      if (selectionOverlayRects.value.length) {
        selectionOverlayRects.value = []
        overlayChanged = true
        if (shouldCommit) {
          commitOverlaySnapshotFromRefs()
        }
      }
      if (shouldUpdateSignature) {
        lastOverlaySignature = signature
        pendingOverlaySignature = null
      }
      disposeStaticOverlayCache()
      return overlayChanged
    }

    const rects = resolveStaticOverlayRects(resources, staticSignature)
    const offsetRects = offsetOverlayRectArray(rects, { release: false })

    if (rectArraysEqual(selectionOverlayRects.value, offsetRects)) {
      if (shouldUpdateSignature) {
        lastOverlaySignature = signature
        pendingOverlaySignature = null
      }
      return overlayChanged
    }

    selectionOverlayRects.value = offsetRects
    overlayChanged = true
    if (shouldCommit) {
      commitOverlaySnapshotFromRefs()
    }

    if (shouldUpdateSignature) {
      lastOverlaySignature = signature
      pendingOverlaySignature = null
    }

    return overlayChanged
  }

  function updateCursorOverlayFromState(
    state: HeadlessSelectionState<RowKey> = selectionControllerAdapter.state.value,
    inputs?: OverlayComputationInputs | null,
  ) {
    const activeRange = state.ranges.length
      ? state.ranges[clamp(state.activeRangeIndex, 0, state.ranges.length - 1)] ?? null
      : null
    const isSingleCellActive = Boolean(
      activeRange &&
      activeRange.startRow === activeRange.endRow &&
      activeRange.startCol === activeRange.endCol,
    )

    if (!isSingleCellActive) {
      if (selectionCursorOverlay.value) {
        selectionCursorOverlay.value = null
      }
      return
    }

    const point = state.selectedPoint ?? state.anchorPoint
    if (!point) {
      if (selectionCursorOverlay.value) {
        selectionCursorOverlay.value = null
      }
      return
    }

    const resources = inputs === undefined ? resolveOverlayComputationInputs(state) : inputs
    if (!resources || !resources.prepared) {
      if (selectionCursorOverlay.value) {
        selectionCursorOverlay.value = null
      }
      return
    }

    // ensure active cell populated for computation context reuse
    resources.context.activeCell = point

    const cursorRect = runWithOverlayTiming("computeCursorRect", () =>
      computeCursorOverlayRect(resources.context, resources.prepared!),
    )
    const offsetCursor = offsetOverlayRect(cursorRect)
    if (!rectsEqual(selectionCursorOverlay.value, offsetCursor)) {
      // HOT PATH: apply cursor update without touching range overlays
      selectionCursorOverlay.value = offsetCursor
    }
  }

  function updateFillPreviewOverlayFromState(
    state: HeadlessSelectionState<RowKey> = selectionControllerAdapter.state.value,
    inputs?: OverlayComputationInputs | null,
    options?: { commit?: boolean; updateSignature?: boolean; signature?: OverlaySignature },
  ): boolean {
    const shouldCommit = options?.commit ?? false
    const shouldUpdateSignature = options?.updateSignature ?? false
    const signatureOverride = options?.signature

    let overlayChanged = false

    const preview = fillPreviewRange.value
    if (!preview) {
      if (fillPreviewOverlayRects.value.length) {
        fillPreviewOverlayRects.value = []
        overlayChanged = true
      }

      if (overlayChanged && shouldCommit) {
        commitOverlaySnapshotFromRefs()
      }

      if (shouldUpdateSignature) {
        lastOverlaySignature = signatureOverride ?? buildOverlaySignature()
        pendingOverlaySignature = null
      }

      return overlayChanged
    }

    const resources = inputs === undefined ? resolveOverlayComputationInputs(state) : inputs
    if (!resources || !resources.prepared) {
      if (fillPreviewOverlayRects.value.length) {
        fillPreviewOverlayRects.value = []
        overlayChanged = true
        if (shouldCommit) {
          commitOverlaySnapshotFromRefs()
        }
      }

      if (shouldUpdateSignature) {
        lastOverlaySignature = signatureOverride ?? buildOverlaySignature()
        pendingOverlaySignature = null
      }

      return overlayChanged
    }

    const fillRects = runWithOverlayTiming("computeFillPreviewRects", () =>
      computeFillPreviewOverlayRects(resources.context, resources.prepared!),
    )
    const offsetFillRects = offsetOverlayRectArray(fillRects)

    if (!rectArraysEqual(fillPreviewOverlayRects.value, offsetFillRects)) {
      fillPreviewOverlayRects.value = offsetFillRects
      overlayChanged = true
      if (shouldCommit) {
        commitOverlaySnapshotFromRefs()
      }
    }

    if (shouldUpdateSignature) {
      lastOverlaySignature = signatureOverride ?? buildOverlaySignature()
      pendingOverlaySignature = null
    }

    return overlayChanged
  }

  function updateCutPreviewOverlayFromState(
    state: HeadlessSelectionState<RowKey> = selectionControllerAdapter.state.value,
    inputs?: OverlayComputationInputs | null,
    options?: { commit?: boolean; updateSignature?: boolean; signature?: OverlaySignature },
  ): boolean {
    const shouldCommit = options?.commit ?? false
    const shouldUpdateSignature = options?.updateSignature ?? false
    const signatureOverride = options?.signature

    let overlayChanged = false

    const cutAreas = cutPreviewRanges.value
    if (!cutAreas.length) {
      if (cutPreviewOverlayRects.value.length) {
        cutPreviewOverlayRects.value = []
        overlayChanged = true
      }

      if (overlayChanged && shouldCommit) {
        commitOverlaySnapshotFromRefs()
      }

      if (shouldUpdateSignature) {
        lastOverlaySignature = signatureOverride ?? buildOverlaySignature()
        pendingOverlaySignature = null
      }

      return overlayChanged
    }

    const resources = inputs === undefined ? resolveOverlayComputationInputs(state) : inputs
    if (!resources || !resources.prepared) {
      if (cutPreviewOverlayRects.value.length) {
        cutPreviewOverlayRects.value = []
        overlayChanged = true
        if (shouldCommit) {
          commitOverlaySnapshotFromRefs()
        }
      }

      if (shouldUpdateSignature) {
        lastOverlaySignature = signatureOverride ?? buildOverlaySignature()
        pendingOverlaySignature = null
      }

      return overlayChanged
    }

    const cutRects = runWithOverlayTiming("computeCutPreviewRects", () =>
      computeCutPreviewOverlayRects(resources.context, resources.prepared!),
    )
    const offsetCutRects = offsetOverlayRectArray(cutRects)

    if (!rectArraysEqual(cutPreviewOverlayRects.value, offsetCutRects)) {
      cutPreviewOverlayRects.value = offsetCutRects
      overlayChanged = true
      if (shouldCommit) {
        commitOverlaySnapshotFromRefs()
      }
    }

    if (shouldUpdateSignature) {
      lastOverlaySignature = signatureOverride ?? buildOverlaySignature()
      pendingOverlaySignature = null
    }

    return overlayChanged
  }

  function updateActiveRangeOverlayFromState(
    state: HeadlessSelectionState<RowKey> = selectionControllerAdapter.state.value,
    inputs?: OverlayComputationInputs | null,
    options?: { commit?: boolean; updateSignature?: boolean; signature?: OverlaySignature },
  ): boolean {
    const shouldCommit = options?.commit ?? false
    const shouldUpdateSignature = options?.updateSignature ?? false
    const signatureOverride = options?.signature

    let overlayChanged = false

    const hasRanges = state.ranges.length > 0
    const activeIndexRaw = state.activeRangeIndex
    const hasActive = hasRanges && activeIndexRaw >= 0 && activeIndexRaw < state.ranges.length

    if (!hasActive) {
      if (activeSelectionOverlayRects.value.length) {
        activeSelectionOverlayRects.value = []
        overlayChanged = true
      }

      if (overlayChanged && shouldCommit) {
        commitOverlaySnapshotFromRefs()
      }

      if (shouldUpdateSignature) {
        lastOverlaySignature = signatureOverride ?? buildOverlaySignature()
        pendingOverlaySignature = null
      }

      return overlayChanged
    }

    const resources = inputs ?? resolveOverlayComputationInputs(state)
    if (!resources || !resources.prepared) {
      if (activeSelectionOverlayRects.value.length) {
        activeSelectionOverlayRects.value = []
        overlayChanged = true
        if (shouldCommit) {
          commitOverlaySnapshotFromRefs()
        }
      }

      if (shouldUpdateSignature) {
        lastOverlaySignature = signatureOverride ?? buildOverlaySignature()
        pendingOverlaySignature = null
      }

      return overlayChanged
    }

    const activeRects = runWithOverlayTiming("computeActiveRangeRects", () =>
      computeActiveRangeOverlayRects(resources.context, resources.prepared!),
    )
    const offsetActiveRects = offsetOverlayRectArray(activeRects)

    if (!rectArraysEqual(activeSelectionOverlayRects.value, offsetActiveRects)) {
      activeSelectionOverlayRects.value = offsetActiveRects
      overlayChanged = true
      if (shouldCommit) {
        commitOverlaySnapshotFromRefs()
      }
    }

    if (shouldUpdateSignature) {
      lastOverlaySignature = signatureOverride ?? buildOverlaySignature()
      pendingOverlaySignature = null
    }

    return overlayChanged
  }

  function updateSelectionOverlay() {
    runWithOverlayTiming("updateSelectionOverlay", () => {
      const state = selectionControllerAdapter.state.value
      const signature = pendingOverlaySignature ?? buildOverlaySignature()
      const resources = resolveOverlayComputationInputs(state)

      if (!resources || !resources.prepared) {
        let overlayUpdated = false
        overlayUpdated = updateStaticRangeOverlayFromState(state, resources ?? null, { commit: false, signature }) || overlayUpdated
        overlayUpdated = updateActiveRangeOverlayFromState(state, resources ?? null, { commit: false, signature }) || overlayUpdated
        overlayUpdated = updateFillPreviewOverlayFromState(state, resources ?? null, { commit: false, signature }) || overlayUpdated
        overlayUpdated = updateCutPreviewOverlayFromState(state, resources ?? null, { commit: false, signature }) || overlayUpdated
        updateCursorOverlayFromState(state, resources ?? null)
        if (overlayUpdated) {
          commitOverlaySnapshotFromRefs()
        }
        pendingOverlaySignature = signature
        return
      }

      let overlayUpdated = false
      overlayUpdated = updateStaticRangeOverlayFromState(state, resources, { commit: false, signature }) || overlayUpdated
      overlayUpdated = updateActiveRangeOverlayFromState(state, resources, { commit: false, signature }) || overlayUpdated
      overlayUpdated = updateFillPreviewOverlayFromState(state, resources, { commit: false, signature }) || overlayUpdated
      overlayUpdated = updateCutPreviewOverlayFromState(state, resources, { commit: false, signature }) || overlayUpdated

      updateCursorOverlayFromState(state, resources)

      if (overlayUpdated) {
        commitOverlaySnapshotFromRefs()
      }

      lastOverlaySignature = signature
      pendingOverlaySignature = null
    })
  }

  function getActiveRange(): SelectionRange | null {
    if (!selectionRanges.value.length) return null
    const index = clamp(activeRangeIndex.value, 0, selectionRanges.value.length - 1)
    return selectionRanges.value[index] ?? null
  }

  function getActiveSelectionArea(): SelectionArea | null {
    const active = getActiveRange()
    if (!active) return null
    return {
      startRow: active.startRow,
      endRow: active.endRow,
      startCol: active.startCol,
      endCol: active.endCol,
    }
  }

  function syncSelectionFromController(state: HeadlessSelectionState<RowKey>) {
    selectionSharedState.patch({
      selectionRanges: state.ranges,
      selectionAreas: state.areas,
      activeRangeIndex: state.activeRangeIndex,
      selectedCell: state.selectedPoint,
      anchorCell: state.anchorPoint,
      dragAnchorCell: state.dragAnchorPoint,
    })
  }

  function handleSelectionStateChange(state: HeadlessSelectionState<RowKey>) {
    const grid = getGridDimensions()
    const detectedColumn = detectFullColumnSelectionIndex(state.ranges, grid)
    if (detectedColumn !== null) {
      const anchorRow = columnSelectionState.value?.anchorRow ?? state.selectedPoint?.rowIndex ?? null
      setColumnSelectionState(detectedColumn, anchorRow)
    } else if (!columnSelectionState.value) {
      clearColumnSelectionState()
    }

    if (!state.ranges.length) {
      selectionDragSession.value = null
      setFillSession(null)
      isFillDragging.value = false
    } else {
      setFillSession(null)
      isFillDragging.value = false
    }
  }

  function commitHeadlessSelectionState(state: HeadlessSelectionState<RowKey>) {
    selectionControllerAdapter.controller.setState(state)
    applyControllerState(selectionControllerAdapter.controller.getState())
  }

  function detectFullColumnSelectionIndex(ranges: readonly SelectionRange[], grid: { rowCount: number; colCount: number }): number | null {
    const rowCount = Math.max(grid.rowCount, 0)
    const colCount = Math.max(grid.colCount, 0)
    if (rowCount === 0 || colCount === 0) return null
    if (ranges.length !== 1) return null
    const [range] = ranges
    if (range.startCol !== range.endCol) return null
    if (range.startRow !== 0) return null
    const expectedEndRow = Math.max(rowCount - 1, 0)
    if (range.endRow !== expectedEndRow) return null
    return clamp(range.startCol, 0, colCount - 1)
  }

  function commitSelectionResult(result: ResolveSelectionUpdateResult<RowKey>) {
    commitHeadlessSelectionState(result)
    handleSelectionStateChange(result)
    emitSelectionChangeSnapshot(true)
    scheduleOverlayUpdate()
  }

  function reapplySelectionState(overrides?: {
    selectedPoint?: SelectionPoint | null
    anchorPoint?: SelectionPoint | null
    dragAnchorPoint?: SelectionPoint | null
  }) {
    const baseState = selectionControllerAdapter.state.value
    const nextState: HeadlessSelectionState<RowKey> = {
      ranges: baseState.ranges,
      areas: baseState.areas,
      activeRangeIndex: baseState.activeRangeIndex,
      selectedPoint:
        overrides?.selectedPoint === undefined ? baseState.selectedPoint : overrides.selectedPoint ?? null,
      anchorPoint: overrides?.anchorPoint === undefined ? baseState.anchorPoint : overrides.anchorPoint ?? null,
      dragAnchorPoint:
        overrides?.dragAnchorPoint === undefined ? baseState.dragAnchorPoint : overrides.dragAnchorPoint ?? null,
    }
    selectionControllerAdapter.controller.setState(nextState)
    applyControllerState(selectionControllerAdapter.controller.getState())
  }

  function applySelectionUpdate(
    ranges: SelectionRange[],
    activeIndex: number,
    options?: { selectedPoint?: SelectionPoint | null; anchorPoint?: SelectionPoint | null; dragAnchorPoint?: SelectionPoint | null }
  ) {
    if (!options || (options.selectedPoint === undefined && options.anchorPoint === undefined && options.dragAnchorPoint === undefined)) {
      selectionControllerAdapter.controller.updateRanges(ranges, activeIndex)
      applyControllerState(selectionControllerAdapter.controller.getState())
      return
    }

    const result = setSelectionRangesHeadless<RowKey>({
      ranges,
      context: getSelectionContext(),
      activeRangeIndex: activeIndex,
      selectedPoint: options?.selectedPoint,
      anchorPoint: options?.anchorPoint,
      dragAnchorPoint: options?.dragAnchorPoint,
    })

    commitSelectionResult(result)
  }

  function setSingleCellSelection(point: SelectionPointLike) {
    clearColumnSelectionState()
    const target = clampPointToGrid(point)
    debugSelectionLog("setSingleCellSelection via controller", target)
    selectionControllerAdapter.controller.focusCell(target)
    applyControllerState(selectionControllerAdapter.controller.getState())
  }

  function extendActiveRangeTo(point: SelectionPointLike) {
    debugSelectionLog("extendActiveRangeTo invoked")
    clearColumnSelectionState()
    const target = clampPointToGrid(point)
    debugSelectionLog("extendSelection", { target, before: selectionControllerAdapter.state.value })
    selectionControllerAdapter.controller.extendSelection(target)
    debugSelectionLog("extendSelection", { after: selectionControllerAdapter.state.value })
    applyControllerState(selectionControllerAdapter.controller.getState())
  }

  function findRangeIndexContainingPoint(point: SelectionPointLike) {
    const normalized = clampPointToGrid(point)
    return findRangeIndexContainingCell(selectionRanges.value, normalized.rowIndex, normalized.colIndex)
  }

  function toggleCellSelection(point: SelectionPointLike) {
    clearColumnSelectionState()
    const target = clampPointToGrid(point)
    selectionControllerAdapter.controller.toggleCell(target)
    applyControllerState(selectionControllerAdapter.controller.getState())
  }

  function clearCellSelection() {
    clearColumnSelectionState()
    selectionControllerAdapter.controller.clearSelection()
    applyControllerState(selectionControllerAdapter.controller.getState())
  }

  function emitSelectionChangeSnapshot(force = false) {
    const coreSnapshot = buildCoreSelectionSnapshot()
    const signature = selectionSnapshotSignature(coreSnapshot)
    if (!force && signature === lastSelectionSignature.value) return
    lastSelectionSignature.value = signature
    emitSelectionChange(convertToUiSnapshot(coreSnapshot))
  }

  function getSelectionSnapshot(): UiTableSelectionSnapshot {
    const coreSnapshot = buildCoreSelectionSnapshot()
    return convertToUiSnapshot(coreSnapshot).clone()
  }

  function buildCoreSelectionSnapshot(): GridSelectionSnapshot<RowKey> {
    return createSelectionSnapshot<RowKey>({
      ranges: selectionRanges.value,
      activeRangeIndex: activeRangeIndex.value,
      selectedPoint: selectedCell.value,
      getRowIdByIndex,
    })
  }

  function convertToUiSnapshot(coreSnapshot: GridSelectionSnapshot<RowKey>): UiTableSelectionSnapshot {
    const ranges = coreSnapshot.ranges
    const cacheRanges = uiRangeCache

    let index = 0
    for (; index < ranges.length; index += 1) {
      const source = ranges[index]
      let target = cacheRanges[index]
      if (!target) {
        target = uiRangePool.pop() ?? {
          startRow: 0,
          endRow: 0,
          startCol: 0,
          endCol: 0,
          startRowId: null,
          endRowId: null,
          anchor: { rowId: null, rowIndex: 0, colIndex: 0 },
          focus: { rowId: null, rowIndex: 0, colIndex: 0 },
        }
        cacheRanges[index] = target
      }

      target.startRow = source.startRow
      target.endRow = source.endRow
      target.startCol = source.startCol
      target.endCol = source.endCol
      target.startRowId = source.startRowId ?? null
      target.endRowId = source.endRowId ?? null

      const anchor = target.anchor
      anchor.rowId = source.anchor.rowId ?? null
      anchor.rowIndex = source.anchor.rowIndex
      anchor.colIndex = source.anchor.colIndex

      const focus = target.focus
      focus.rowId = source.focus.rowId ?? null
      focus.rowIndex = source.focus.rowIndex
      focus.colIndex = source.focus.colIndex
    }

    if (cacheRanges.length > ranges.length) {
      for (let release = ranges.length; release < cacheRanges.length; release += 1) {
        uiRangePool.push(cacheRanges[release])
      }
      cacheRanges.length = ranges.length
    }

    uiSnapshotCache.activeRangeIndex = coreSnapshot.activeRangeIndex
    if (coreSnapshot.activeCell) {
      if (!uiActiveCellCache) {
        uiActiveCellCache = { rowId: null, rowIndex: 0, colIndex: 0 }
      }
      uiActiveCellCache.rowId = coreSnapshot.activeCell.rowId ?? null
      uiActiveCellCache.rowIndex = coreSnapshot.activeCell.rowIndex
      uiActiveCellCache.colIndex = coreSnapshot.activeCell.colIndex
      uiSnapshotCache.activeCell = uiActiveCellCache
    } else {
      uiSnapshotCache.activeCell = null
    }

    return uiSnapshotCache
  }

  function focusCell(rowIndex: number, column: number | string, options?: { extend?: boolean }) {
    if (!hasGridData()) return false
    const { rowCount, colCount } = getGridDimensions()
    const targetRow = clamp(rowIndex, 0, Math.max(rowCount - 1, 0))

    if (isFullRowMode.value) {
      clearFullColumnSelection()
      const anchorRow = options?.extend
        ? clamp(resolveRowAnchor(targetRow), 0, Math.max(rowCount - 1, 0))
        : targetRow
      setFullRowSelectionRange(anchorRow, targetRow, {
        focus: true,
        preserveAnchor: options?.extend,
        anchorRow,
      })
      nextTick(() => scrollSelectionIntoView())
      return true
    }

    const colIndex = typeof column === "number" ? column : localColumns.value.findIndex(col => col.key === column)
    if (colIndex < 0) return false
    const target = createSelectionPointByIndex(targetRow, clamp(colIndex, 0, Math.max(colCount - 1, 0)))
    clearFullRowSelection()
    clearFullColumnSelection()
    if (options?.extend) {
      extendActiveRangeTo(target)
    } else {
      setSingleCellSelection(target)
    }
    focusContainer()
    nextTick(() => scrollSelectionIntoView())
    return true
  }

  function getActiveCell() {
    if (anchorCell.value) {
      return { ...anchorCell.value }
    }
    return selectedCell.value ? { ...selectedCell.value } : null
  }

  function getSelectionCursor(option?: { preferAnchor?: boolean }) {
    if (option?.preferAnchor) {
      return anchorCell.value ?? selectedCell.value
    }
    return selectedCell.value ?? anchorCell.value
  }

  function goToRowEdge(edge: "start" | "end", options?: { extend?: boolean }) {
    const active = getSelectionCursor()
    if (!active) return false
    const targetCol = edge === "start" ? 0 : Math.max(0, localColumns.value.length - 1)
    return focusCell(active.rowIndex, targetCol, options)
  }

  function goToColumnEdge(edge: "start" | "end", options?: { extend?: boolean }) {
    const active = getSelectionCursor()
    if (!active) return false
    const targetRow = edge === "start" ? 0 : Math.max(0, totalRowCount.value - 1)
    return focusCell(targetRow, active.colIndex, options)
  }

  function goToGridEdge(edge: "start" | "end", options?: { extend?: boolean }) {
    const targetRow = edge === "start" ? 0 : Math.max(0, totalRowCount.value - 1)
    const targetCol = edge === "start" ? 0 : Math.max(0, localColumns.value.length - 1)
    return focusCell(targetRow, targetCol, options)
  }

  function moveByPage(direction: number, options?: { extend?: boolean }) {
    const active = getSelectionCursor()
    if (!active) return false
    const rowHeight = effectiveRowHeightRef.value || 1
    const viewportHeight = viewportHeightRef.value || rowHeight
    const pageSize = Math.max(1, Math.round(viewportHeight / rowHeight))
    const targetRow = active.rowIndex + direction * pageSize
    return focusCell(targetRow, active.colIndex, options)
  }

  function hasGridData() {
    return totalRowCount.value > 0 && localColumns.value.length > 0
  }

  function clearFullRowSelection(force = false) {
    if (!force && isFullRowMode.value) return
    fullRowSelection.value = null
    rowSelectionAnchor.value = null
  }

  function setColumnSelectionState(column: number, anchorRow: number | null) {
    columnSelectionState.value = {
      column,
      anchorRow,
    }
  }

  function clearColumnSelectionState() {
    columnSelectionState.value = null
  }

  function clearFullColumnSelection() {
    if (fullColumnSelection.value === null) {
      return
    }
    clearColumnSelectionState()
    clearCellSelection()
  }

  function setFullRowSelectionRange(startRow: number, endRow: number, options?: { focus?: boolean; preserveAnchor?: boolean; anchorRow?: number | null }) {
    if (!hasGridData()) {
      debugSelectionLog("moveSelection aborted: no grid data", {
        rowCount: totalRowCount.value,
        colCount: localColumns.value.length,
      })
      return
    }
    const { rowCount, colCount } = getGridDimensions()
    if (rowCount <= 0 || colCount <= 0) return
    const normalizedStart = clamp(Math.min(startRow, endRow), 0, rowCount - 1)
    const normalizedEnd = clamp(Math.max(startRow, endRow), 0, rowCount - 1)
    if (normalizedStart > normalizedEnd) return

    const nextAnchor = options?.preserveAnchor
      ? rowSelectionAnchor.value
      : options?.anchorRow ?? normalizedStart

    rowSelectionAnchor.value = nextAnchor != null
      ? clamp(nextAnchor, normalizedStart, normalizedEnd)
      : normalizedStart

    fullRowSelection.value = {
      start: normalizedStart,
      end: normalizedEnd,
    }

    clearFullColumnSelection()
    const activeRow = rowSelectionAnchor.value ?? normalizedStart
    const result = resolveFullRowSelection<RowKey>({
      startRow: normalizedStart,
      endRow: normalizedEnd,
      activeRow,
      context: getSelectionContext(),
    })

    commitSelectionResult(result)

    if (options?.focus ?? true) {
      focusContainer()
    }
  }

  function setFullColumnSelection(columnIndex: number, options?: { focus?: boolean; anchorRow?: number | null; retainCursor?: boolean }) {
    if (!hasGridData()) return
    const { rowCount, colCount } = getGridDimensions()
    if (rowCount <= 0 || colCount <= 0) return

    const normalizedColumn = clamp(columnIndex, 0, colCount - 1)
    const anchorCandidate =
      options?.anchorRow ??
      anchorCell.value?.rowIndex ??
      selectedCell.value?.rowIndex ??
      0
    const normalizedAnchorRow = clamp(anchorCandidate, 0, Math.max(rowCount - 1, 0))

    setColumnSelectionState(normalizedColumn, normalizedAnchorRow)
    clearFullRowSelection()

    const result = resolveFullColumnSelection<RowKey>({
      columnIndex: normalizedColumn,
      anchorRow: normalizedAnchorRow,
      retainCursor: options?.retainCursor ?? false,
      context: getSelectionContext(),
    })

    commitSelectionResult(result)

    if (options?.focus ?? true) {
      focusContainer()
    }
  }

  function selectCell(
    rowIndex: number,
    columnKey: string,
    focus = true,
    options?: { fullRow?: boolean; fullColumn?: boolean; colIndex?: number }
  ) {
    if (!hasGridData()) return
    const colIndex = options?.colIndex ?? localColumns.value.findIndex(col => col.key === columnKey)

    if (isFullRowMode.value && !options?.fullColumn) {
      clearFullColumnSelection()
      setFullRowSelectionRange(rowIndex, rowIndex, { focus, anchorRow: rowIndex })
      return
    }

    if (options?.fullRow) {
      setFullRowSelectionRange(rowIndex, rowIndex, { focus, anchorRow: rowIndex })
      return
    }

    if (options?.fullColumn) {
      if (isFullRowMode.value) {
        if (focus) focusContainer()
        return
      }
      if (colIndex === -1) {
        clearFullColumnSelection()
        clearCellSelection()
        if (focus) focusContainer()
        return
      }
      setFullColumnSelection(colIndex, { focus, anchorRow: rowIndex })
      return
    }

    clearFullRowSelection()
    clearFullColumnSelection()

    if (colIndex === -1) {
      if (focus) focusContainer()
      return
    }

    const clampedRow = clamp(rowIndex, 0, totalRowCount.value - 1)
    const clampedCol = clamp(colIndex, 0, localColumns.value.length - 1)
    setSingleCellSelection({ rowIndex: clampedRow, colIndex: clampedCol })
    if (focus) focusContainer()
  }

  function scrollSelectionIntoView(attempt = 0) {
    const activeRange = getActiveRange()
    const cursor = getSelectionCursor()
    if (!activeRange && !cursor) return

    selectionEnvironment.dom.scrollSelectionIntoView({
      range: activeRange,
      cursor: cursor ?? null,
      attempt,
      maxAttempts: 4,
    })
  }

  function setSelection(
    rangesInput: UiTableSelectionRangeInput | UiTableSelectionRangeInput[],
    options?: { activeRangeIndex?: number; focus?: boolean }
  ) {
    const rangesArray = Array.isArray(rangesInput) ? rangesInput : [rangesInput]
    if (!rangesArray.length) {
      clearSelection()
      return
    }
    clearFullColumnSelection()
    const normalized = rangesArray.map(range => normalizeExternalRange(range))
    if (isFullRowMode.value) {
      const target = normalized[0]
      if (target) {
        setFullRowSelectionRange(target.startRow, target.endRow, {
          focus: options?.focus,
          anchorRow: target.startRow,
        })
      } else {
        clearFullRowSelection(true)
        clearCellSelection()
      }
      return
    }
    clearFullRowSelection()
    const activeIndex = options?.activeRangeIndex ?? 0
    applySelectionUpdate(normalized, activeIndex)
    if (options?.focus ?? true) {
      focusContainer()
      nextTick(() => scrollSelectionIntoView())
    }
  }

  function normalizeExternalRange(input: UiTableSelectionRangeInput): SelectionRange {
    return createGridSelectionRangeFromInput<RowKey>(
      {
        startRow: input.startRow,
        endRow: input.endRow,
        startCol: input.startCol,
        endCol: input.endCol,
        anchor: input.anchor
          ? {
              rowId: input.anchor.rowId ?? null,
              rowIndex: input.anchor.rowIndex,
              colIndex: input.anchor.colIndex,
            }
          : undefined,
        focus: input.focus
          ? {
              rowId: input.focus.rowId ?? null,
              rowIndex: input.focus.rowIndex,
              colIndex: input.focus.colIndex,
            }
          : undefined,
      },
      getSelectionContext(),
    )
  }

  function clearSelection() {
    const hadColumnSelection = fullColumnSelection.value !== null
    clearFullRowSelection(true)
    clearFullColumnSelection()
    if (!hadColumnSelection) {
      clearCellSelection()
    }
  }

  function iterSelectionCells(callback: (rowIndex: number, colIndex: number, column: UiTableColumn | undefined) => void) {
    const { rowCount, colCount } = getGridDimensions()
    const ranges = selectionRanges.value.length ? selectionRanges.value : []
    let hadRanges = false
    for (const range of ranges) {
      hadRanges = true
      const startRow = clamp(range.startRow, 0, rowCount - 1)
      const endRow = clamp(range.endRow, 0, rowCount - 1)
      const startCol = clamp(range.startCol, 0, colCount - 1)
      const endCol = clamp(range.endCol, 0, colCount - 1)
      for (let row = startRow; row <= endRow; row += 1) {
        for (let col = startCol; col <= endCol; col += 1) {
          callback(row, col, localColumns.value[col])
        }
      }
    }
    if (!hadRanges) {
      const rowRange = fullRowSelection.value
      if (rowRange) {
        const startRow = clamp(rowRange.start, 0, rowCount - 1)
        const endRow = clamp(rowRange.end, 0, rowCount - 1)
        for (let row = startRow; row <= endRow; row += 1) {
          for (let col = 0; col < colCount; col += 1) {
            callback(row, col, localColumns.value[col])
          }
        }
        return
      }
    }
    const activePoint = anchorCell.value ?? selectedCell.value
    if (!hadRanges && activePoint) {
      const row = clamp(activePoint.rowIndex, 0, Math.max(rowCount - 1, 0))
      const col = clamp(activePoint.colIndex, 0, Math.max(colCount - 1, 0))
      callback(row, col, localColumns.value[col])
    }
  }

  function getSelectedCells(): UiTableSelectedCell[] {
    const cells: UiTableSelectedCell[] = []
    iterSelectionCells((rowIndex, colIndex, column) => {
      if (!column) return
      const entry = processedRows.value[rowIndex]
      const rowId = entry?.rowId ?? getRowIdByIndex(rowIndex)
      if (rowId == null) {
        return
      }
      cells.push({
        rowId,
        rowIndex,
        colIndex,
        columnKey: column.key,
        value: entry?.row?.[column.key],
        row: entry?.row,
      })
    })
    return cells
  }

  function isCellSelected(rowIndex: number, colIndex: number) {
    const rowRange = fullRowSelection.value
    if ((rowRange && rowIndex >= rowRange.start && rowIndex <= rowRange.end) || fullColumnSelection.value === colIndex) {
      return false
    }
    if (selectionAreas.value.length) {
      return selectionAreas.value.some(area => areaContainsCell(area, rowIndex, colIndex))
    }
    return pointsMatch(anchorCell.value ?? selectedCell.value, rowIndex, colIndex)
  }

  function pointsMatch(point: SelectionPoint | null, rowIndex: number, colIndex: number): boolean {
    if (!point) return false
    if (point.rowId != null) {
      const candidateRowId = getRowIdByIndex(rowIndex)
      if (candidateRowId != null && candidateRowId === point.rowId) {
        return point.colIndex === colIndex
      }
    }
    return point.rowIndex === rowIndex && point.colIndex === colIndex
  }

  function isSelectionAnchorCell(rowIndex: number, colIndex: number) {
    return pointsMatch(anchorCell.value, rowIndex, colIndex)
  }

  function isSelectionCursorCell(rowIndex: number, colIndex: number) {
    return pointsMatch(selectedCell.value ?? anchorCell.value, rowIndex, colIndex)
  }

  function isRowFullySelected(rowIndex: number) {
    const range = fullRowSelection.value
    if (!range) return false
    return rowIndex >= range.start && rowIndex <= range.end
  }

  function isColumnFullySelected(colIndex: number) {
    return fullColumnSelection.value === colIndex
  }

  function isRowInSelectionRect(rowIndex: number) {
    const rowRange = fullRowSelection.value
    if (rowRange && rowIndex >= rowRange.start && rowIndex <= rowRange.end) {
      return true
    }
    if (anyAreaContainsRow(selectionAreas.value, rowIndex)) {
      return true
    }
    const preview = fillPreviewRange.value
    if (preview && areaContainsRow(preview, rowIndex)) {
      return true
    }
    return false
  }

  function isColumnInSelectionRect(colIndex: number) {
    if (anyAreaContainsColumn(selectionAreas.value, colIndex)) {
      return true
    }
    const preview = fillPreviewRange.value
    if (preview && areaContainsColumn(preview, colIndex)) {
      return true
    }
    return false
  }

  function isCellInFillPreview(rowIndex: number, colIndex: number) {
    const preview = fillPreviewRange.value
    if (!preview || !areaContainsCell(preview, rowIndex, colIndex)) return false
    const origin = fillSession.value?.origin
    if (origin && rangeContainsCell(origin, rowIndex, colIndex)) {
      return false
    }
    return true
  }

  function getSelectionEdges(rowIndex: number, colIndex: number) {
    return computeSelectionEdges(selectionRanges.value, activeRangeIndex.value, rowIndex, colIndex)
  }

  function getFillPreviewEdges(rowIndex: number, colIndex: number) {
    const preview = fillPreviewRange.value
    if (!preview) return null
    const origin = fillSession.value?.origin
    if (origin && rangeContainsCell(origin, rowIndex, colIndex)) {
      return null
    }
    const edges = areaEdges(preview, rowIndex, colIndex)
    if (!edges) return null
    return {
      ...edges,
      active: true,
    }
  }

  function beginCutPreview(snapshot: UiTableSelectionSnapshot | null) {
    const result = resolveCutPreviewFromSnapshot(snapshot, getSelectionContext())
    if (!result) {
      pendingCutState.value = null
      cutPreviewState.value = null
      const controllerState = selectionControllerAdapter.state.value
      const resources = resolveOverlayComputationInputs(controllerState)
      updateCutPreviewOverlayFromState(controllerState, resources, {
        commit: true,
        updateSignature: true,
      })
      return
    }

    pendingCutState.value = snapshot
      ? {
          snapshot: snapshot.clone(),
          fullRowRange: fullRowSelection.value
            ? { start: fullRowSelection.value.start, end: fullRowSelection.value.end }
            : null,
        }
      : null

    cutPreviewState.value = result
    const controllerState = selectionControllerAdapter.state.value
    const resources = resolveOverlayComputationInputs(controllerState)
    updateCutPreviewOverlayFromState(controllerState, resources, {
      commit: true,
      updateSignature: true,
    })
  }

  function clearCutPreview() {
    cutPreviewState.value = null
    pendingCutState.value = null
    const controllerState = selectionControllerAdapter.state.value
    const resources = resolveOverlayComputationInputs(controllerState)
    updateCutPreviewOverlayFromState(controllerState, resources, {
      commit: true,
      updateSignature: true,
    })
  }

  function commitPendingCut(): boolean {
    const pending = pendingCutState.value
    if (!pending) return false
    if (!hasGridData()) {
      pendingCutState.value = null
      return false
    }

    const { snapshot, fullRowRange } = pending
    const { rowCount, colCount } = getGridDimensions()
    let changed = false

    if (fullRowRange && deleteRows) {
      const startRow = clamp(fullRowRange.start, 0, Math.max(rowCount - 1, 0))
      const endRow = clamp(fullRowRange.end, 0, Math.max(rowCount - 1, 0))
      if (startRow <= endRow) {
        const rows: number[] = []
        for (let row = startRow; row <= endRow; row += 1) {
          rows.push(row)
        }
        if (rows.length) {
          const payload = rows.map(displayIndex => {
            const entry = processedRows.value[displayIndex]
            return {
              displayIndex,
              originalIndex: entry?.originalIndex ?? displayIndex,
              row: entry?.row,
            }
          })
          deleteRows(payload)
          changed = true
        }
      }
    } else {
      const preview = resolveCutPreviewFromSnapshot(snapshot, getSelectionContext())
      const areas = preview?.areas ?? []
      if (areas.length) {
        const batchEvents: CellEditEvent[] = []
        const historyEntries: HistoryEntry[] = []
        for (const area of areas) {
          const startRow = clamp(area.startRow, 0, Math.max(rowCount - 1, 0))
          const endRow = clamp(area.endRow, 0, Math.max(rowCount - 1, 0))
          const startCol = clamp(area.startCol, 0, Math.max(colCount - 1, 0))
          const endCol = clamp(area.endCol, 0, Math.max(colCount - 1, 0))
          for (let row = startRow; row <= endRow; row += 1) {
            for (let col = startCol; col <= endCol; col += 1) {
              const column = localColumns.value[col]
              if (!column || !canPasteIntoColumn(column)) continue
              if (setCellValueDirect(row, col, null, {
                collector: batchEvents,
                historyCollector: historyEntries,
              })) {
                changed = true
              }
            }
          }
        }
        dispatchEvents(batchEvents)
        if (historyEntries.length) {
          recordHistory(historyEntries)
        }
      }
    }

    pendingCutState.value = null
    if (changed) {
      scheduleOverlayUpdate()
    }
    return changed
  }

  function hasCutPreview() {
    return Boolean(cutPreviewState.value?.areas.length)
  }

  function isCellInCutPreview(rowIndex: number, colIndex: number) {
    const areas = cutPreviewState.value?.areas
    if (!areas) return false
    return areas.some(range => areaContainsCell(range, rowIndex, colIndex))
  }

  function getCutPreviewEdges(rowIndex: number, colIndex: number) {
    const state = cutPreviewState.value
    if (!state) return null
    const index = findAreaIndexContaining(state.areas, rowIndex, colIndex)
    if (index === -1) return null
    const edges = areaEdges(state.areas[index], rowIndex, colIndex)
    if (!edges) return null
    return {
      ...edges,
      active: index === state.activeIndex,
    }
  }

  function createCellKey(rowIndex: number, columnIndex: number): string {
    return `${rowIndex}:${columnIndex}`
  }

  const ROW_HEADER_CLASS_NONE = ""
  const ROW_HEADER_CLASS_FULL = "ui-table__row-index--full"
  const ROW_HEADER_CLASS_HIGHLIGHT = "ui-table__row-index--highlight"
  const ROW_HEADER_CLASS_RANGE = "ui-table__row-index--range"

  function resolveRowHeaderClassValue(isFull: boolean, isHighlighted: boolean, inSelection: boolean): string {
    if (isFull) return ROW_HEADER_CLASS_FULL
    if (isHighlighted) return ROW_HEADER_CLASS_HIGHLIGHT
    if (inSelection) return ROW_HEADER_CLASS_RANGE
    return ROW_HEADER_CLASS_NONE
  }

  function buildSelectionSnapshot(options: {
    rowIndices: readonly number[]
    columnIndices: readonly number[]
  }): ImperativeSelectionSnapshot {
    imperativeSnapshotGeneration += 1
    const generation = imperativeSnapshotGeneration
    const snapshot = imperativeSnapshotCache
    const snapshotHasCutPreview = hasCutPreview()

    const rowSelection = snapshot.rowSelection
    const columnSelection = snapshot.columnSelection
    rowSelection.clear()
    columnSelection.clear()

    const cells = snapshot.cells
    for (const [, cell] of cells) {
      cell.generation = 0
    }

    const rowClasses = snapshot.rowClasses
    for (const [, entry] of rowClasses) {
      entry.generation = 0
    }

    const activeRowIndex = (anchorCell.value ?? selectedCell.value)?.rowIndex ?? null

    for (let index = 0; index < options.rowIndices.length; index += 1) {
      const rowIndex = options.rowIndices[index]
      if (!Number.isFinite(rowIndex) || rowIndex < 0) continue
      if (isRowFullySelected(rowIndex)) {
        rowSelection.add(rowIndex)
      }
    }

    for (let index = 0; index < options.columnIndices.length; index += 1) {
      const columnIndex = options.columnIndices[index]
      if (!Number.isFinite(columnIndex) || columnIndex < 0) continue
      if (isColumnFullySelected(columnIndex)) {
        columnSelection.add(columnIndex)
      }
    }

    for (let rIndex = 0; rIndex < options.rowIndices.length; rIndex += 1) {
      const rowIndex = options.rowIndices[rIndex]
      if (!Number.isFinite(rowIndex) || rowIndex < 0) continue

      const isFullRow = rowSelection.has(rowIndex)
      const inSelectionRect = isRowInSelectionRect(rowIndex)
      const isHighlighted = isFullRow || activeRowIndex === rowIndex || inSelectionRect
      const rowClassValue = resolveRowHeaderClassValue(isFullRow, isHighlighted, inSelectionRect)
      const rowEntry = rowClasses.get(rowIndex)
      if (rowEntry) {
        rowEntry.value = rowClassValue
        rowEntry.generation = generation
      } else {
        rowClasses.set(rowIndex, { value: rowClassValue, generation })
      }

      for (let cIndex = 0; cIndex < options.columnIndices.length; cIndex += 1) {
        const columnIndex = options.columnIndices[cIndex]
        if (!Number.isFinite(columnIndex) || columnIndex < 0) continue
        const key = createCellKey(rowIndex, columnIndex)
        let cellState = cells.get(key)
        if (!cellState) {
          cellState = {
            isSelected: false,
            isRangeSelected: false,
            isFillPreview: false,
            fillPreviewEdges: null,
            isCutPreview: false,
            cutPreviewEdges: null,
            rangeEdges: null,
            generation,
          }
          cells.set(key, cellState)
        } else {
          cellState.generation = generation
        }

        cellState.isSelected = isCellSelected(rowIndex, columnIndex)
        cellState.isRangeSelected = findRangeIndexContainingPoint({ rowIndex, colIndex: columnIndex }) !== -1
        cellState.isFillPreview = isCellInFillPreview(rowIndex, columnIndex)
        cellState.fillPreviewEdges = cellState.isFillPreview ? getFillPreviewEdges(rowIndex, columnIndex) : null
        cellState.isCutPreview = snapshotHasCutPreview && isCellInCutPreview(rowIndex, columnIndex)
        cellState.cutPreviewEdges = cellState.isCutPreview ? getCutPreviewEdges(rowIndex, columnIndex) : null
        cellState.rangeEdges = cellState.isRangeSelected ? getSelectionEdges(rowIndex, columnIndex) : null
      }
    }

    for (const [key, cell] of cells) {
      if (cell.generation !== generation) {
        cells.delete(key)
      }
    }

    for (const [rowIndex, entry] of rowClasses) {
      if (entry.generation !== generation) {
        rowClasses.delete(rowIndex)
      }
    }

    const anchor = anchorCell.value
    const cursor = selectedCell.value ?? anchorCell.value ?? null
    snapshot.hasCutPreview = snapshotHasCutPreview
    snapshot.anchorKey = anchor ? createCellKey(anchor.rowIndex, anchor.colIndex) : null
    snapshot.cursorKey = cursor ? createCellKey(cursor.rowIndex, cursor.colIndex) : null
    snapshot.generation = generation

    return snapshot
  }

  function areCutPreviewStatesEqual(a: CutPreviewState | null, b: CutPreviewState | null): boolean {
    if (a === b) return true
    if (!a || !b) return false
    if (a.activeIndex !== b.activeIndex) return false
    if (a.areas.length !== b.areas.length) return false
    for (let index = 0; index < a.areas.length; index += 1) {
      const left = a.areas[index]
      const right = b.areas[index]
      if (
        left.startRow !== right.startRow ||
        left.endRow !== right.endRow ||
        left.startCol !== right.startCol ||
        left.endCol !== right.endCol
      ) {
        return false
      }
    }
    return true
  }

  function rowHeaderClass(rowIndex: number) {
    const cachedRow = imperativeSnapshotCache.rowClasses.get(rowIndex)
    if (cachedRow && cachedRow.generation === imperativeSnapshotCache.generation) {
      return cachedRow.value
    }
    const isFull = isRowFullySelected(rowIndex)
    const activeRow = (anchorCell.value ?? selectedCell.value)?.rowIndex ?? null
    const inSelection = isRowInSelectionRect(rowIndex)
    const isHighlighted = isFull || activeRow === rowIndex || inSelection
    return resolveRowHeaderClassValue(isFull, isHighlighted, inSelection)
  }

  function onCellSelect(payload: { rowIndex: number; key: string; colIndex?: number; focus?: boolean; event?: MouseEvent }) {
    const shouldFocus = payload.focus ?? true
    const colIndex = payload.colIndex ?? localColumns.value.findIndex(col => col.key === payload.key)
    if (colIndex === -1) {
      selectCell(payload.rowIndex, payload.key, shouldFocus, { colIndex })
      return
    }

    const clampedRow = clamp(payload.rowIndex, 0, Math.max(totalRowCount.value - 1, 0))
    const clampedCol = clamp(colIndex, 0, Math.max(localColumns.value.length - 1, 0))
    const point = createSelectionPointByIndex(clampedRow, clampedCol)

    const event = payload.event

    if (isFullRowMode.value) {
      const targetRow = point.rowIndex
      if (event?.shiftKey) {
        const anchorRow = clamp(resolveRowAnchor(targetRow), 0, Math.max(totalRowCount.value - 1, 0))
        setFullRowSelectionRange(anchorRow, targetRow, { focus: shouldFocus, anchorRow })
      } else {
        setFullRowSelectionRange(targetRow, targetRow, { focus: shouldFocus, anchorRow: targetRow })
      }
      if (shouldFocus) focusContainer()
      return
    }

    if (event?.shiftKey) {
      extendActiveRangeTo(point)
      if (shouldFocus) focusContainer()
      return
    }

    if (event && (event.ctrlKey || event.metaKey)) {
      toggleCellSelection(point)
      if (shouldFocus) focusContainer()
      return
    }

    selectCell(point.rowIndex, payload.key, shouldFocus, { colIndex: point.colIndex })
  }

  function resolveRowAnchor(fallback: number) {
    if (rowSelectionAnchor.value != null) return rowSelectionAnchor.value
    const active = anchorCell.value ?? selectedCell.value
    if (active) return active.rowIndex
    return fallback
  }

  function onRowIndexClick(rowIndex: number, event?: MouseEvent) {
    if (!hasGridData()) return
    reapplySelectionState({ dragAnchorPoint: null })
    const clampedRow = clamp(rowIndex, 0, Math.max(totalRowCount.value - 1, 0))
    const isShift = Boolean(event?.shiftKey)
    const isToggle = Boolean(event?.metaKey || event?.ctrlKey)

    if (isShift) {
      const anchorRow = clamp(resolveRowAnchor(clampedRow), 0, Math.max(totalRowCount.value - 1, 0))
      setFullRowSelectionRange(anchorRow, clampedRow, { focus: true, anchorRow })
      event?.preventDefault()
      return
    }

    if (isToggle) {
      const range = fullRowSelection.value
      if (range && clampedRow >= range.start && clampedRow <= range.end) {
        clearFullRowSelection(true)
        clearCellSelection()
        focusContainer()
        return
      }
    }

    setFullRowSelectionRange(clampedRow, clampedRow, { focus: true, anchorRow: clampedRow })
    rowSelectionAnchor.value = clampedRow

    if (event?.button !== 0) return

    haltAutoScroll()
    isRowSelectionDragging.value = true
    const pointer = toPointerCoordinates(event)
    lastPointer.value = pointer
    runAutoScrollUpdate(pointer)
    window.addEventListener("mousemove", onRowSelectionMouseMove, { passive: false })
    window.addEventListener("mouseup", handleRowSelectionMouseUp)
    event.preventDefault()
  }

  function onColumnHeaderClick(colIndex: number) {
    if (!hasGridData()) return
    if (isFullRowMode.value) {
      clearFullColumnSelection()
      focusContainer()
      return
    }
    reapplySelectionState({ dragAnchorPoint: null })

    const clampedIndex = clamp(colIndex, 0, Math.max(localColumns.value.length - 1, 0))
    if (fullColumnSelection.value === clampedIndex) {
      clearFullColumnSelection()
      clearCellSelection()
      focusContainer()
      return
    }

    setFullColumnSelection(clampedIndex, { focus: true })
  }

  function onCellDragStart(payload: { rowIndex: number; colIndex: number; event: MouseEvent }) {
    if (isEditingCell.value) return
    if (!hasGridData()) return
    if (payload.event.button !== 0) return
    if (isFullRowMode.value) return
    clearFullRowSelection()
    clearFullColumnSelection()
    isDraggingSelection.value = true
    const anchor = createSelectionPointByIndex(
      clamp(payload.rowIndex, 0, totalRowCount.value - 1),
      clamp(payload.colIndex, 0, localColumns.value.length - 1)
    )
    selectionDragSession.value = startSelectionDragSession<RowKey>({
      anchorPoint: anchor,
      context: getSelectionContext(),
    })
    reapplySelectionState({ dragAnchorPoint: anchor })
    const pointer = toPointerCoordinates(payload.event)
    lastPointer.value = pointer
    if (!selectionRanges.value.length) {
      setSingleCellSelection(anchor)
    }
    haltAutoScroll()
    runAutoScrollUpdate(pointer)
    window.addEventListener("mousemove", onSelectionMouseMove, { passive: false })
    window.addEventListener("mouseup", handleSelectionMouseUp)
    focusContainer()
    payload.event.preventDefault()
  }

  function onCellDragEnter(payload: { rowIndex: number; colIndex: number; event: MouseEvent }) {
    if (!isDraggingSelection.value) return
    if (!(payload.event.buttons & 1)) {
      handleSelectionMouseUp()
      return
    }
    extendSelectionDragToPoint(createSelectionPointByIndex(payload.rowIndex, payload.colIndex))
  }

  function extendSelectionDragToPoint(point: SelectionPoint | null) {
    if (!isDraggingSelection.value || !point) return
    extendActiveRangeTo(point)
  }

  function updateSelectionDragFromPoint(clientX: number, clientY: number) {
    if (!isDraggingSelection.value) return
    const session = selectionDragSession.value
    if (session) {
      const point = updateSelectionDragSession(session, {
        clientX,
        clientY,
        resolvePoint: getCellFromPoint,
        context: getSelectionContext(),
      })
      extendSelectionDragToPoint(point)
      return
    }
    extendSelectionDragToPoint(getCellFromPoint(clientX, clientY))
  }

  function onSelectionMouseMove(event: MouseEvent) {
    if (!isDraggingSelection.value) return
    event.preventDefault()
    const pointer = toPointerCoordinates(event)
    lastPointer.value = pointer
    updateSelectionDragFromPoint(pointer.clientX, pointer.clientY)
    runAutoScrollUpdate(pointer)
  }

  function handleSelectionMouseUp() {
    if (!isDraggingSelection.value) return
    isDraggingSelection.value = false
    window.removeEventListener("mouseup", handleSelectionMouseUp)
    window.removeEventListener("mousemove", onSelectionMouseMove)
    if (selectionDragSession.value) {
      endSelectionDragSession(selectionDragSession.value)
      selectionDragSession.value = null
    }
    haltAutoScroll()
    lastPointer.value = null
  }

  function getCellFromPoint(clientX: number, clientY: number): SelectionPoint | null {
    const resolvePoint = selectionEnvironment.dom.resolveCellFromPoint
    const resolved = resolvePoint ? resolvePoint(clientX, clientY) : domAdapter.resolveCellFromPoint(clientX, clientY)
    if (!resolved) return null
    return clampPointToGrid(resolved)
  }

  function getRowIndexFromPoint(clientX: number, clientY: number): number | null {
    const resolveRow = selectionEnvironment.dom.resolveRowIndexFromPoint
    if (resolveRow) {
      return resolveRow(clientX, clientY)
    }
    return domAdapter.resolveRowIndexFromPoint(clientX, clientY)
  }

  function updateRowSelectionFromPoint(clientX: number, clientY: number) {
    const targetRow = getRowIndexFromPoint(clientX, clientY)
    if (targetRow === null) return
    const anchorRow = rowSelectionAnchor.value ?? targetRow
    setFullRowSelectionRange(anchorRow, targetRow, { focus: false, anchorRow })
  }

  function onRowSelectionMouseMove(event: MouseEvent) {
    if (!isRowSelectionDragging.value) return
    event.preventDefault()
    const pointer = toPointerCoordinates(event)
    updateRowSelectionFromPoint(pointer.clientX, pointer.clientY)
    lastPointer.value = pointer
    runAutoScrollUpdate(pointer)
  }

  function handleRowSelectionMouseUp() {
    if (!isRowSelectionDragging.value) return
    isRowSelectionDragging.value = false
    window.removeEventListener("mousemove", onRowSelectionMouseMove)
    window.removeEventListener("mouseup", handleRowSelectionMouseUp)
    haltAutoScroll()
    lastPointer.value = null
  }

  function startFillDrag(event: MouseEvent) {
    if (isEditingCell.value) return
    if (isFullRowMode.value) return
    const origin = getActiveRange()
    if (!origin) return
    event.preventDefault()
    event.stopPropagation()
    const session = startCoreFillDragSession<RowKey>({
      origin,
      context: getSelectionContext(),
    })
    if (!session) return
    setFillSession(session)
    isFillDragging.value = true
    const pointer = toPointerCoordinates(event)
    lastPointer.value = pointer
    updateFillPreviewFromPoint(pointer.clientX, pointer.clientY)
    runAutoScrollUpdate(pointer)
    window.addEventListener("mousemove", onFillMouseMove, { passive: false })
    window.addEventListener("mouseup", onFillMouseUp, { passive: false })
  }

  function onFillMouseMove(event: MouseEvent) {
    if (!isFillDragging.value) return
    event.preventDefault()
    const pointer = toPointerCoordinates(event)
    updateFillPreviewFromPoint(pointer.clientX, pointer.clientY)
    runAutoScrollUpdate(pointer)
  }

  function onFillMouseUp(event: MouseEvent) {
    if (!isFillDragging.value) return
    event.preventDefault()
    window.removeEventListener("mousemove", onFillMouseMove)
    window.removeEventListener("mouseup", onFillMouseUp)
    updateFillPreviewFromPoint(event.clientX, event.clientY)
    haltAutoScroll()
    const shouldCommit = Boolean(fillPreviewRange.value)
    if (shouldCommit) {
      applyFillOperation()
    }
    resetFillState()
    scheduleOverlayUpdate()
  }

  function resetFillState() {
    haltAutoScroll()
    isFillDragging.value = false
    setFillSession(null)
    const controllerState = selectionControllerAdapter.state.value
    updateFillPreviewOverlayFromState(controllerState, undefined, {
      commit: true,
      updateSignature: true,
    })
  }

  function applyFillOperation() {
    const session = fillSession.value
    if (!session || !session.preview) return

    const origin = session.origin
    const preview = session.preview

    const batchEvents: CellEditEvent[] = []
    const historyEntries: HistoryEntry[] = []

    const result = commitFillPreview<RowKey>({
      origin,
      preview,
      target: session.target ?? undefined,
      context: getSelectionContext(),
      reader: (rowIndex: number, colIndex: number) => getCellRawValue(rowIndex, colIndex),
      writer: (rowIndex: number, colIndex: number, value: unknown) => setCellValueDirect(rowIndex, colIndex, value, {
        collector: batchEvents,
        historyCollector: historyEntries,
      }),
    })

    dispatchEvents(batchEvents)
    if (historyEntries.length) {
      recordHistory(historyEntries)
    }

    const anchor = { ...origin.anchor }

    if (!result) {
      applySelectionUpdate([createRange(anchor, origin.focus)], 0)
      lastCommittedFillArea.value = null
      focusContainer()
      return
    }

    const focus = result.changed ? { ...result.focus } : { ...origin.focus }

    applySelectionUpdate([createRange(anchor, focus)], 0)
    lastCommittedFillArea.value = result.changed ? { ...result.appliedArea } : null
    focusContainer()
  }

  function autoFillDownFromActiveRange() {
    const origin = getActiveRange()
    if (!origin) return

    const context = getSelectionContext()

    const result = resolveAutoFillDown<RowKey>({
      origin,
      context,
    })

    if (!result) return

    const session = startCoreFillDragSession<RowKey>({
      origin,
      context,
    })

    if (!session) return

    const hydratedSession = fillDragSessionWithPreview(session, {
      preview: result.preview,
      target: result.target,
      context,
    })

    setFillSession(hydratedSession)

    applyFillOperation()
    resetFillState()
    scheduleOverlayUpdate()
  }

  function handleAutoScrollFrame() {
    const pointer = lastPointer.value
    if (isFillDragging.value && pointer) {
      updateFillPreviewFromPoint(pointer.clientX, pointer.clientY)
    } else if (isDraggingSelection.value && pointer) {
      updateSelectionDragFromPoint(pointer.clientX, pointer.clientY)
    } else if (isRowSelectionDragging.value && pointer) {
      updateRowSelectionFromPoint(pointer.clientX, pointer.clientY)
    }
    scheduleOverlayUpdate()
  }

  function updateFillPreviewFromPoint(clientX: number, clientY: number) {
    if (!fillSession.value) return
    const target = getCellFromPoint(clientX, clientY)
    const effectiveTarget = target ?? fillSession.value.target
    updateFillPreviewForTarget(effectiveTarget ?? null)
  }

  function updateFillPreviewForTarget(target: SelectionPointLike | null) {
    const session = fillSession.value
    if (!session) return
    updateCoreFillDragSession(session, {
      target,
      context: getSelectionContext(),
    })
    refreshFillSession(session)
    const controllerState = selectionControllerAdapter.state.value
    const resources = resolveOverlayComputationInputs(controllerState)
    updateFillPreviewOverlayFromState(controllerState, resources, {
      commit: true,
      updateSignature: true,
    })
    updateCursorOverlayFromState(controllerState, resources)
  }

  function getBoundsForRange(range: SelectionRange | null, fallbackToAll: boolean): SelectionArea | null {
    return resolveSelectionBounds(range, getSelectionContext(), fallbackToAll)
  }

  function buildSelectionMatrix(
    range: SelectionRange | null,
    options?: { includeHeaders?: boolean; fallbackToAll?: boolean },
  ) {
    const bounds = getBoundsForRange(range, options?.fallbackToAll ?? false)
    if (!bounds) return []

    const { rowCount, colCount } = getGridDimensions()
    if (rowCount <= 0 || colCount <= 0) return []

    const clampRow = (index: number) => clamp(index, 0, rowCount - 1)
    const clampCol = (index: number) => clamp(index, 0, colCount - 1)

    const columnIndices: number[] = []
    for (let colIndex = bounds.startCol; colIndex <= bounds.endCol; colIndex += 1) {
      columnIndices.push(clampCol(colIndex))
    }

    if (!columnIndices.length) {
      return []
    }

    const matrix: string[][] = []

    if (options?.includeHeaders) {
      const headerRow: string[] = []
      for (const colIndex of columnIndices) {
        const column = localColumns.value[colIndex]
        const label = typeof column?.label === "string" && column.label.trim().length
          ? column.label
          : column?.key ?? ""
        headerRow.push(String(label ?? ""))
      }
      matrix.push(headerRow)
    }

    for (let rowIndex = bounds.startRow; rowIndex <= bounds.endRow; rowIndex += 1) {
      const safeRowIndex = clampRow(rowIndex)
      const values: string[] = []
      for (const colIndex of columnIndices) {
        const raw = getCellRawValue(safeRowIndex, colIndex)
        if (raw === null || raw === undefined) {
          values.push("")
          continue
        }
        if (typeof raw === "string") {
          values.push(raw)
          continue
        }
        if (typeof raw === "number" || typeof raw === "boolean") {
          values.push(String(raw))
          continue
        }
        values.push(raw?.toString?.() ?? "")
      }
      matrix.push(values)
    }

    return matrix
  }

  function applyMatrixToSelection(matrix: string[][], baseOverride?: SelectionPointLike | null) {
    if (!matrix.length || !matrix[0]?.length) return
    const { rowCount, colCount } = getGridDimensions()
    if (rowCount <= 0 || colCount <= 0) return
    if (isFullRowMode.value) return

    const matrixRowCount = matrix.length
    const matrixColBaseline = matrix[0]?.length ?? 0
    const activeRange = getActiveRange()
    const selectionHeight = activeRange ? activeRange.endRow - activeRange.startRow + 1 : 0
    const selectionWidth = activeRange ? activeRange.endCol - activeRange.startCol + 1 : 0
    const selectionIsMultiCell = selectionHeight > 1 || selectionWidth > 1
    const matrixFitsSelection =
      selectionIsMultiCell &&
      matrixRowCount <= selectionHeight &&
      matrixColBaseline <= selectionWidth

    const selectionForPaste = matrixFitsSelection ? activeRange : null
    const basePoint = baseOverride
      ? baseOverride
      : selectionForPaste
        ? selectionForPaste.anchor
        : anchorCell.value ?? selectedCell.value ?? activeRange?.anchor ?? null

    const batchEvents: CellEditEvent[] = []
    const historyEntries: HistoryEntry[] = []

    const result = applyMatrixToGrid<RowKey>({
      matrix,
      selection: selectionForPaste,
      basePoint,
      context: getSelectionContext(),
      writer: (rowIndex, colIndex, value) => setCellValueFromPaste(rowIndex, colIndex, value, {
        collector: batchEvents,
        historyCollector: historyEntries,
      }),
    })

    if (!result) {
      return
    }

    if (result.nextSelection) {
      applySelectionUpdate([result.nextSelection], 0)
    }

    dispatchEvents(batchEvents)
    if (historyEntries.length) {
      recordHistory(historyEntries)
    }

    if (result.changed) {
      refreshViewport?.(true)
      if (!result.nextSelection) {
        scheduleOverlayUpdate()
      }
    }

    focusContainer()
  }

  function clearSelectionValues() {
    if (!hasGridData()) return false
    if (isFullRowMode.value) return false
    const rowRange = fullRowSelection.value
    if (rowRange) {
      const { rowCount } = getGridDimensions()
      const startRow = clamp(rowRange.start, 0, rowCount - 1)
      const endRow = clamp(rowRange.end, 0, rowCount - 1)
      const rows: number[] = []
      for (let row = startRow; row <= endRow; row += 1) {
        rows.push(row)
      }
      if (rows.length && deleteRows) {
        const payload = rows.map(displayIndex => {
          const entry = processedRows.value[displayIndex]
          return {
            displayIndex,
            originalIndex: entry?.originalIndex ?? displayIndex,
            row: entry?.row,
          }
        })
        deleteRows(payload)
        clearSelection()
        focusContainer()
        scheduleOverlayUpdate()
        clearCutPreview()
        return true
      }
    }
    const batchEvents: CellEditEvent[] = []
    const historyEntries: HistoryEntry[] = []
    let cleared = false
    iterSelectionCells((rowIndex, colIndex, column) => {
      if (!column) return
      if (!canPasteIntoColumn(column)) return
      if (setCellValueDirect(rowIndex, colIndex, null, {
        collector: batchEvents,
        historyCollector: historyEntries,
      })) {
        cleared = true
      }
    })
    dispatchEvents(batchEvents)
    if (historyEntries.length) {
      recordHistory(historyEntries)
    }
    if (cleared) {
      focusContainer()
      scheduleOverlayUpdate()
    }
    return cleared
  }

  function handleDocumentMouseDown(event: MouseEvent) {
    const container = containerRef.value
    if (!container) return
    const target = event.target as HTMLElement | null
    if (!target) return
    if (container.contains(target)) return
    if (target.closest(".ignore-selection")) return
    clearFullRowSelection(true)
    clearFullColumnSelection()
    clearCellSelection()
  }

  function ensureSelectionBounds() {
    if (!hasGridData()) {
      clearCellSelection()
      isEditingCell.value = false
      fullRowSelection.value = null
      clearColumnSelectionState()
      isDraggingSelection.value = false
      selectionDragSession.value = null
      return
    }

    const { rowCount, colCount } = getGridDimensions()

    const clampedCutPreview = clampCutPreviewState(cutPreviewState.value, getSelectionContext())
    if (!clampedCutPreview) {
      if (cutPreviewState.value) {
        cutPreviewState.value = null
        pendingCutState.value = null
        scheduleOverlayUpdate()
      }
    } else if (!areCutPreviewStatesEqual(clampedCutPreview, cutPreviewState.value)) {
      cutPreviewState.value = clampedCutPreview
      scheduleOverlayUpdate()
    }

    if (fullRowSelection.value) {
      const nextStart = clamp(fullRowSelection.value.start, 0, Math.max(rowCount - 1, 0))
      const nextEnd = clamp(fullRowSelection.value.end, 0, Math.max(rowCount - 1, 0))
      if (nextStart > nextEnd) {
        clearFullRowSelection(true)
        clearCellSelection()
      } else if (nextStart !== fullRowSelection.value.start || nextEnd !== fullRowSelection.value.end) {
        setFullRowSelectionRange(nextStart, nextEnd, { focus: false, preserveAnchor: true })
      } else if (rowSelectionAnchor.value != null) {
        rowSelectionAnchor.value = clamp(rowSelectionAnchor.value, nextStart, nextEnd)
      }
    }

    const detectedColumnSelection = detectFullColumnSelectionIndex(selectionRanges.value, { rowCount, colCount })
    if (detectedColumnSelection !== null) {
      if (!columnSelectionState.value || columnSelectionState.value.column !== detectedColumnSelection) {
        const anchorRow = selectionRanges.value[0]?.anchor.rowIndex ?? null
        setColumnSelectionState(detectedColumnSelection, anchorRow)
      }
    } else if (!columnSelectionState.value) {
      clearColumnSelectionState()
    }

    const columnState = columnSelectionState.value
    if (columnState) {
      const normalizedColumn = clamp(columnState.column, 0, Math.max(colCount - 1, 0))
      const normalizedAnchorRow = columnState.anchorRow != null
        ? clamp(columnState.anchorRow, 0, Math.max(rowCount - 1, 0))
        : columnState.anchorRow

      if (normalizedColumn !== columnState.column || normalizedAnchorRow !== columnState.anchorRow) {
        columnSelectionState.value = {
          column: normalizedColumn,
          anchorRow: normalizedAnchorRow,
        }
      }

      if (rowCount > 0 && colCount > 0 && detectedColumnSelection !== normalizedColumn) {
        setFullColumnSelection(normalizedColumn, {
          focus: false,
          anchorRow: normalizedAnchorRow ?? 0,
          retainCursor: Boolean(selectedCell.value),
        })
      }
    }

    if (selectionRanges.value.length) {
      applySelectionUpdate(selectionRanges.value, activeRangeIndex.value)
    } else {
      const current = anchorCell.value ?? selectedCell.value
      if (current) {
        setSingleCellSelection(clampPointToGrid(current))
      }
    }

    reapplySelectionState()
    if (selectionDragSession.value) {
      selectionDragSession.value.anchor = clampPointToGrid(selectionDragSession.value.anchor)
    }
  }

  function moveSelection(rowDelta: number, colDelta: number, options: { extend?: boolean } = {}) {
    debugSelectionLog("moveSelection called", {
      rowDelta,
      colDelta,
      options,
      mode: rowSelectionMode.value,
      hasCursor: Boolean(getSelectionCursor()),
    })
    if (isFullRowMode.value) {
      if (!hasGridData()) return
      const { rowCount } = getGridDimensions()
      const current = anchorCell.value ?? selectedCell.value
      const fallbackRow = current ? current.rowIndex : 0
      const targetRow = clamp(fallbackRow + rowDelta, 0, Math.max(rowCount - 1, 0))
      const anchorRow = options.extend
        ? clamp(resolveRowAnchor(fallbackRow), 0, Math.max(rowCount - 1, 0))
        : targetRow
      clearFullColumnSelection()
      setFullRowSelectionRange(anchorRow, targetRow, {
        focus: true,
        preserveAnchor: options.extend,
        anchorRow,
      })
      nextTick(() => scrollSelectionIntoView())
      return
    }
    clearFullRowSelection()
    clearFullColumnSelection()
    if (!hasGridData()) return
    const current = getSelectionCursor()
    if (!current) {
      selectCell(0, localColumns.value[0].key, true, { colIndex: 0 })
      return
    }

    const rowCount = totalRowCount.value
    const colCount = localColumns.value.length
    const newRow = clamp(current.rowIndex + rowDelta, 0, rowCount - 1)
    const newCol = clamp(current.colIndex + colDelta, 0, colCount - 1)

    const point = createSelectionPointByIndex(newRow, newCol)
    debugSelectionLog("moveSelection targeting", { point, extend: options.extend })
    if (options.extend) {
      extendActiveRangeTo(point)
    } else {
      setSingleCellSelection(point)
    }
    focusContainer()
    nextTick(() => scrollSelectionIntoView())
  }

  function performSelectionMove(source: SelectionArea, target: SelectionArea): boolean {
    const { rowCount, colCount } = getGridDimensions()
    if (rowCount <= 0 || colCount <= 0) return false
    const sourceHeight = source.endRow - source.startRow + 1
    const sourceWidth = source.endCol - source.startCol + 1
    if (sourceHeight <= 0 || sourceWidth <= 0) return false
    const targetHeight = target.endRow - target.startRow + 1
    const targetWidth = target.endCol - target.startCol + 1
    if (targetHeight !== sourceHeight || targetWidth !== sourceWidth) return false

    const columns: UiTableColumn[] = []
    for (let offset = 0; offset < sourceWidth; offset += 1) {
      const column = localColumns.value[source.startCol + offset]
      if (!column || !canPasteIntoColumn(column)) {
        return false
      }
      columns.push(column)
    }

    const events: CellEditEvent[] = []
    const historyEntries: HistoryEntry[] = []
    const moveResult = moveSelectionArea<RowKey>({
      source,
      target,
      context: getSelectionContext(),
      handlers: {
        read: (rowIndex, colIndex) => getCellRawValue(rowIndex, colIndex),
        clear: (rowIndex, colIndex) => setCellValueDirect(rowIndex, colIndex, null, {
          collector: events,
          historyCollector: historyEntries,
        }),
        write: (rowIndex, colIndex, value) => setCellValueDirect(rowIndex, colIndex, value, {
          collector: events,
          historyCollector: historyEntries,
        }),
      },
    })

    if (!moveResult || !moveResult.changed) {
      return false
    }

    dispatchEvents(events)
    if (historyEntries.length) {
      recordHistory(historyEntries)
    }

    const nextRange: UiTableSelectionRangeInput = {
      startRow: moveResult.appliedArea.startRow,
      endRow: moveResult.appliedArea.endRow,
      startCol: moveResult.appliedArea.startCol,
      endCol: moveResult.appliedArea.endCol,
      anchor: createSelectionPointByIndex(moveResult.appliedArea.startRow, moveResult.appliedArea.startCol),
      focus: createSelectionPointByIndex(moveResult.appliedArea.endRow, moveResult.appliedArea.endCol),
    }

    setSelection(nextRange, { focus: true })
    focusContainer()
    scheduleOverlayUpdate()
    refreshViewport?.(true)
    return true
  }

  function canMoveActiveSelection() {
    if (isFullRowMode.value) return false
    if (isEditingCell.value) return false
    if (fullRowSelection.value) return false
    if (fullColumnSelection.value !== null) return false
    if (selectionRanges.value.length !== 1) return false
    return getActiveSelectionArea() != null
  }

  function moveActiveSelectionTo(rowStart: number, colStart: number) {
    const source = getActiveSelectionArea()
    if (!source) return false
    if (!canMoveActiveSelection()) return false
    const { rowCount, colCount } = getGridDimensions()
    if (rowCount <= 0 || colCount <= 0) return false
    const height = source.endRow - source.startRow + 1
    const width = source.endCol - source.startCol + 1
    const maxRowStart = Math.max(0, rowCount - height)
    const maxColStart = Math.max(0, colCount - width)
    const nextRowStart = clamp(rowStart, 0, maxRowStart)
    const nextColStart = clamp(colStart, 0, maxColStart)
    if (nextRowStart === source.startRow && nextColStart === source.startCol) {
      return false
    }
    const target: SelectionArea = {
      startRow: nextRowStart,
      endRow: nextRowStart + height - 1,
      startCol: nextColStart,
      endCol: nextColStart + width - 1,
    }
    return performSelectionMove(source, target)
  }

  function moveByTab(forward: boolean) {
    if (isFullRowMode.value) {
      if (!hasGridData()) return false
      const { rowCount } = getGridDimensions()
      const current = anchorCell.value ?? selectedCell.value
      const baseRow = current ? current.rowIndex : 0
      const delta = forward ? 1 : -1
      const targetRow = clamp(baseRow + delta, 0, Math.max(rowCount - 1, 0))
      if (targetRow === baseRow) {
        return false
      }
      clearFullColumnSelection()
      setFullRowSelectionRange(targetRow, targetRow, { focus: true, anchorRow: targetRow })
      nextTick(() => scrollSelectionIntoView())
      return true
    }
    clearFullRowSelection()
    clearFullColumnSelection()
    if (!hasGridData()) return false
    const current = getSelectionCursor()
    if (!current) {
      selectCell(0, localColumns.value[0].key, true, { colIndex: 0 })
      return true
    }

    const rowCount = totalRowCount.value
    const colCount = localColumns.value.length
    let { rowIndex, colIndex } = current

    if (forward) {
      if (rowIndex === rowCount - 1 && colIndex === colCount - 1) {
        return false
      }
      colIndex += 1
      if (colIndex >= colCount) {
        colIndex = 0
        rowIndex = clamp(rowIndex + 1, 0, rowCount - 1)
      }
    } else {
      if (rowIndex === 0 && colIndex === 0) {
        return false
      }
      colIndex -= 1
      if (colIndex < 0) {
        colIndex = colCount - 1
        rowIndex = clamp(rowIndex - 1, 0, rowCount - 1)
      }
    }

    setSingleCellSelection({ rowIndex, colIndex })
    focusContainer()
    nextTick(() => scrollSelectionIntoView())
    return true
  }

  function triggerEditForSelection() {
    if (isFullRowMode.value) return null
    const active = getSelectionCursor()
    if (!active) return null
    const column = localColumns.value[active.colIndex]
    if (!column) return null
    if (!canPasteIntoColumn(column)) return null
    return {
      rowIndex: active.rowIndex,
      columnKey: column.key,
    }
  }

  watch(
    () => processedRows.value,
    () => {
      if (!processedRows.value.length) {
        clearCellSelection()
        clearFullRowSelection(true)
        clearFullColumnSelection()
        return
      }

      const remapped = remapSelectionState<RowKey>({
        ranges: selectionRanges.value,
        selected: selectedCell.value,
        anchor: anchorCell.value,
        dragAnchor: dragAnchorCell.value,
        fillSession: fillSession.value,
      }, {
        context: getSelectionContext(),
        findRowIndexById,
      })

      const resolved = setSelectionRangesHeadless<RowKey>({
        ranges: remapped.ranges,
        context: getSelectionContext(),
        activeRangeIndex: activeRangeIndex.value,
        selectedPoint: remapped.selected ?? undefined,
        anchorPoint: remapped.anchor ?? undefined,
        dragAnchorPoint: remapped.dragAnchor ?? undefined,
      })

      commitSelectionResult(resolved)
      setFillSession(remapped.fillSession ?? null)

      if (!resolved.selectedPoint && !resolved.anchorPoint && resolved.ranges.length === 0) {
        clearCellSelection()
        return
      }
    }
  )

  watch(
    () => viewportWidthRef.value,
    () => {
      invalidateSelectionMetrics()
      const controllerState = selectionControllerAdapter.state.value
      const resources = resolveOverlayComputationInputs(controllerState)
      updateCursorOverlayFromState(controllerState, resources)
      updateActiveRangeOverlayFromState(controllerState, resources, { commit: true })
      updateFillPreviewOverlayFromState(controllerState, resources, { commit: true })
      updateCutPreviewOverlayFromState(controllerState, resources, { commit: true })
      scheduleOverlayUpdate({ priority: "background" })
    }
  )

  watch(
    () => viewportHeightRef.value,
    () => {
      invalidateSelectionMetrics()
      const controllerState = selectionControllerAdapter.state.value
      const resources = resolveOverlayComputationInputs(controllerState)
      updateCursorOverlayFromState(controllerState, resources)
      updateActiveRangeOverlayFromState(controllerState, resources, { commit: true })
      updateFillPreviewOverlayFromState(controllerState, resources, { commit: true })
      updateCutPreviewOverlayFromState(controllerState, resources, { commit: true })
      scheduleOverlayUpdate({ priority: "background" })
    }
  )

  watch(
    () => effectiveRowHeightRef.value,
    () => {
      const controllerState = selectionControllerAdapter.state.value
      const resources = resolveOverlayComputationInputs(controllerState)
      updateCursorOverlayFromState(controllerState, resources)
      updateActiveRangeOverlayFromState(controllerState, resources, { commit: true })
      updateFillPreviewOverlayFromState(controllerState, resources, { commit: true })
      updateCutPreviewOverlayFromState(controllerState, resources, { commit: true })
      scheduleOverlayUpdate({ priority: "background" })
    }
  )

  watch(
    () => [
      viewport.scrollLeft.value,
      viewport.scrollTop.value,
      viewport.visibleStartCol.value,
      viewport.visibleEndCol.value,
      viewport.startIndex.value,
      viewport.endIndex.value,
    ],
    () => {
      const controllerState = selectionControllerAdapter.state.value
      const resources = resolveOverlayComputationInputs(controllerState)
      updateCursorOverlayFromState(controllerState, resources)
      updateFillPreviewOverlayFromState(controllerState, resources, { commit: true })
      updateCutPreviewOverlayFromState(controllerState, resources, { commit: true })
      scheduleOverlayUpdate()
    },
    { flush: "post" },
  )

  watch(
    () => containerRef.value,
    () => {
      invalidateSelectionMetrics()
      const controllerState = selectionControllerAdapter.state.value
      const resources = resolveOverlayComputationInputs(controllerState)
      updateCursorOverlayFromState(controllerState, resources)
      updateActiveRangeOverlayFromState(controllerState, resources, { commit: true })
      updateFillPreviewOverlayFromState(controllerState, resources, { commit: true })
      updateCutPreviewOverlayFromState(controllerState, resources, { commit: true })
      scheduleOverlayUpdate({ priority: "background" })
    }
  )

  if (overlayContainerRef) {
    watch(
      () => overlayContainerRef.value,
      () => {
        invalidateSelectionMetrics()
        const controllerState = selectionControllerAdapter.state.value
        const resources = resolveOverlayComputationInputs(controllerState)
        updateCursorOverlayFromState(controllerState, resources)
        updateActiveRangeOverlayFromState(controllerState, resources, { commit: true })
        updateFillPreviewOverlayFromState(controllerState, resources, { commit: true })
        updateCutPreviewOverlayFromState(controllerState, resources, { commit: true })
        scheduleOverlayUpdate({ priority: "background" })
      }
    )
  }

  watch(
    [selectedCell, anchorCell],
    ([focus, anchor]) => {
      const controllerState = selectionControllerAdapter.state.value
      const resources = resolveOverlayComputationInputs(controllerState)
      updateCursorOverlayFromState(controllerState, resources)
      updateActiveRangeOverlayFromState(controllerState, resources, { commit: true })
      updateFillPreviewOverlayFromState(controllerState, resources, { commit: true })
      updateCutPreviewOverlayFromState(controllerState, resources, { commit: true })
      const target = anchor ?? focus
      if (!target || isDraggingSelection.value || isFillDragging.value || isRowSelectionDragging.value) return
      nextTick(() => scrollSelectionIntoView())
    }
  )

  watch(
    () => isEditingCell.value,
    () => {
      const controllerState = selectionControllerAdapter.state.value
      const resources = resolveOverlayComputationInputs(controllerState)
      updateCursorOverlayFromState(controllerState, resources)
      updateActiveRangeOverlayFromState(controllerState, resources, { commit: true })
      updateFillPreviewOverlayFromState(controllerState, resources, { commit: true })
      updateCutPreviewOverlayFromState(controllerState, resources, { commit: true })
      scheduleOverlayUpdate()
    }
  )

  watch(
    () => localColumns.value.length,
    () => ensureSelectionBounds()
  )

  watch(
    () => totalRowCount.value,
    () => ensureSelectionBounds()
  )

  watch(
    rowSelectionMode,
    mode => {
      if (mode === "row") {
        if (!hasGridData()) {
          clearFullRowSelection(true)
          clearCellSelection()
          return
        }
        const { rowCount } = getGridDimensions()
        const targetRow = clamp((anchorCell.value ?? selectedCell.value)?.rowIndex ?? 0, 0, Math.max(rowCount - 1, 0))
        clearFullColumnSelection()
        setFullRowSelectionRange(targetRow, targetRow, { focus: false, anchorRow: targetRow })
      } else {
        const current = anchorCell.value ?? selectedCell.value
        clearFullRowSelection(true)
        if (!hasGridData()) {
          clearCellSelection()
          return
        }
        const { rowCount, colCount } = getGridDimensions()
        const nextRow = clamp(current?.rowIndex ?? 0, 0, Math.max(rowCount - 1, 0))
        const nextCol = clamp(current?.colIndex ?? 0, 0, Math.max(colCount - 1, 0))
        setSingleCellSelection({ rowIndex: nextRow, colIndex: nextCol })
      }
      scheduleOverlayUpdate()
    },
    { flush: "post" }
  )

  onBeforeUnmount(() => {
    if (overlayScheduleHandle !== null) {
      if (overlayScheduleHandle === OVERLAY_MICROTASK_HANDLE) {
        overlayMicrotaskScheduled = false
      } else {
        selectionEnvironment.scheduler.cancel(overlayScheduleHandle)
      }
      overlayScheduleHandle = null
      overlayScheduleMode = null
    }
    if (fillHandleStyle.value) {
      fillHandleStyle.value = null
    }
    cancelPendingControllerFrame()
    controllerAutoscrollUnsubscribe()
    disposeSelectionEnvironment()
    cancelFillHandleMeasurement()
    disposeStaticOverlayCache()
    window.removeEventListener("mouseup", handleSelectionMouseUp)
    window.removeEventListener("mousemove", onSelectionMouseMove)
    window.removeEventListener("mousemove", onFillMouseMove)
    window.removeEventListener("mouseup", onFillMouseUp)
    window.removeEventListener("mousemove", onRowSelectionMouseMove)
    window.removeEventListener("mouseup", handleRowSelectionMouseUp)
    document.removeEventListener("mousedown", handleDocumentMouseDown)
    if (selectionDragSession.value) {
      endSelectionDragSession(selectionDragSession.value)
      selectionDragSession.value = null
    }
    selectionSharedState.dispose()
  })

  if (typeof document !== "undefined") {
    document.addEventListener("mousedown", handleDocumentMouseDown)
  }

  return {
    selectedCell,
    anchorCell,
    selectionRanges,
    activeRangeIndex,
    fullRowSelection,
    fullColumnSelection,
    isDraggingSelection,
    isFillDragging,
    fillPreviewRange,
    cutPreviewRanges,
  cutPreviewActiveIndex,
    fillHandleStyle,
  selectionOverlayRects,
  activeSelectionOverlayRects,
  selectionCursorOverlay,
  fillPreviewOverlayRects,
  cutPreviewOverlayRects,
  overlayScrollState,
  selectionOverlayAdapter: selectionOverlayAdapterHandle,
    scheduleOverlayUpdate,
    setSelection,
    clearSelection,
    focusCell,
    getActiveCell,
    getSelectionSnapshot,
    getActiveRange,
    getActiveSelectionArea,
    selectCell,
    iterSelectionCells,
    getSelectedCells,
    isCellSelected,
  isSelectionCursorCell,
  isSelectionAnchorCell,
  isCellInSelectionRange: (rowIndex: number, colIndex: number) => findRangeIndexContainingPoint({ rowIndex, colIndex }) !== -1,
    isCellInCutPreview,
    isRowFullySelected,
    isColumnFullySelected,
    isRowInSelectionRect,
    isColumnInSelectionRect,
    isCellInFillPreview,
    getSelectionEdges,
    getFillPreviewEdges,
    getCutPreviewEdges,
    rowHeaderClass,
    onCellSelect,
    onRowIndexClick,
    onColumnHeaderClick,
    onCellDragStart,
    onCellDragEnter,
    handleSelectionMouseUp,
    moveSelection,
    canMoveActiveSelection,
    moveActiveSelectionTo,
    moveByTab,
    moveByPage,
    goToRowEdge,
    goToColumnEdge,
    goToGridEdge,
    triggerEditForSelection,
    clearSelectionValues,
    beginCutPreview,
    clearCutPreview,
    commitPendingCut,
    hasCutPreview,
    applyMatrixToSelection,
    buildSelectionMatrix,
    buildSelectionSnapshot,
    getBoundsForRange,
    startFillDrag,
    autoFillDownFromActiveRange,
    handleAutoScrollFrame,
    lastCommittedFillArea,
    selectionEnvironment,
  }
}
