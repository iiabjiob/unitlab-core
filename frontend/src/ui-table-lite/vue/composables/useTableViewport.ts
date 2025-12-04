import { computed, onBeforeUnmount, onMounted, shallowRef, unref, watch } from "vue"
import type { ComputedRef, Ref, ShallowRef } from "vue"

import type { UiTableColumn, VisibleRow } from "../../core/types"
import { BASE_ROW_HEIGHT } from "../../core/utils/constants"
import type { WritableSignal } from "../../core/runtime/signals"
import {
  createTableViewportController,
  type RowPoolItem,
  type TableViewportImperativeCallbacks,
  type TableViewportServerIntegration,
  type TableViewportState,
  type ViewportMetricsSnapshot,
  type TableViewportRuntimeOverrides,
  type ViewportSyncTargets,
} from "../../core/viewport/tableViewportController"
import { resolvePinMode, resolveRowHeightModeValue } from "../core/virtualization/viewportConfig"
import type { ColumnMetric } from "../core/virtualization/types"
import type { ServerRowModel } from "./useServerRowModel"
import { createViewportHostEnvironment } from "../adapters/viewportHostEnvironment"

export type { ColumnMetric, ColumnPinMode, ColumnVirtualizationSnapshot } from "../core/virtualization/types"
export type {
  ImperativeColumnUpdatePayload,
  ImperativeRowUpdatePayload,
  ImperativeScrollSyncPayload,
  RowPoolItem,
  TableViewportImperativeCallbacks,
  TableViewportState,
  ViewportMetricsSnapshot,
} from "../../core/viewport/tableViewportController"

type SignalRegistry = WeakMap<WritableSignal<unknown>, ShallowRef<unknown>>

function createVueWritableSignal<T>(registry: SignalRegistry, initial: T): WritableSignal<T> {
  const target = shallowRef(initial) as ShallowRef<T>
  const signal = {} as WritableSignal<T>

  Object.defineProperty(signal, "value", {
    get: () => target.value,
    set: (next: T) => {
      target.value = next
    },
    enumerable: true,
    configurable: true,
  })

  registry.set(signal, target)
  return signal
}

function getSignalRef<T>(registry: SignalRegistry, signal: WritableSignal<T>): ShallowRef<T> {
  const ref = registry.get(signal)
  if (!ref) {
    throw new Error("Missing signal binding for table viewport controller")
  }
  return ref as ShallowRef<T>
}

function resolveBaseRowHeight(source: UseTableViewportOptions["baseRowHeight"]): number {
  const value = typeof source === "number" ? source : unref(source ?? BASE_ROW_HEIGHT)
  if (Number.isFinite(value) && value > 0) {
    return value
  }
  return BASE_ROW_HEIGHT
}

function resolveServerIntegration(
  integration: UseTableViewportOptions["serverIntegration"],
): TableViewportServerIntegration | null {
  if (!integration) {
    return null
  }

  const enabledSource = integration.enabled ?? true
  const enabled = typeof enabledSource === "boolean" ? enabledSource : Boolean(unref(enabledSource))

  return {
    rowModel: integration.rowModel ?? null,
    enabled,
  }
}

export interface UseTableViewportOptions {
  containerRef: Ref<HTMLDivElement | null>
  headerRef: Ref<HTMLElement | null>
  processedRows: ComputedRef<VisibleRow[]>
  columns: Ref<UiTableColumn[]> | ComputedRef<UiTableColumn[]>
  onAfterScroll?: () => void
  onNearBottom?: () => void
  isLoading?: Ref<boolean>
  rowHeightMode?: Ref<"fixed" | "auto"> | "fixed" | "auto"
  baseRowHeight?: Ref<number> | number
  viewportMetrics?: Ref<ViewportMetricsSnapshot | null> | null
  imperativeCallbacks?: TableViewportImperativeCallbacks
  serverIntegration?: {
    enabled?: Ref<boolean> | ComputedRef<boolean> | boolean
    rowModel: ServerRowModel<unknown> | null
  }
  normalizeAndClampScroll?: boolean
  runtime?: TableViewportRuntimeOverrides
  syncTargets?: Ref<ViewportSyncTargets | null> | ComputedRef<ViewportSyncTargets | null>
  virtualizationEnabled?: Ref<boolean> | ComputedRef<boolean> | boolean
  horizontalVirtualizationEnabled?: Ref<boolean> | ComputedRef<boolean> | boolean
}

export interface UseTableViewportResult {
  scrollTop: Ref<number>
  scrollLeft: Ref<number>
  viewportHeight: Ref<number>
  viewportWidth: Ref<number>
  totalRowCount: Ref<number>
  effectiveRowHeight: Ref<number>
  visibleCount: Ref<number>
  poolSize: Ref<number>
  totalContentHeight: Ref<number>
  startIndex: Ref<number>
  endIndex: Ref<number>
  visibleRows: Ref<VisibleRow[]>
  visibleRowsPool: Ref<RowPoolItem[]>
  rowPoolVersion: Ref<number>
  virtualizationEnabled: Ref<boolean>
  visibleColumns: Ref<UiTableColumn[]>
  visibleColumnEntries: Ref<ColumnMetric[]>
  visibleScrollableColumns: Ref<UiTableColumn[]>
  visibleScrollableEntries: Ref<ColumnMetric[]>
  pinnedLeftColumns: Ref<UiTableColumn[]>
  pinnedLeftEntries: Ref<ColumnMetric[]>
  pinnedRightColumns: Ref<UiTableColumn[]>
  pinnedRightEntries: Ref<ColumnMetric[]>
  leftPadding: Ref<number>
  rightPadding: Ref<number>
  columnWidthMap: Ref<Map<string, number>>
  visibleStartCol: Ref<number>
  visibleEndCol: Ref<number>
  scrollableRange: Ref<{ start: number; end: number }>
  columnVirtualState: Ref<{
    start: number
    end: number
    visibleStart: number
    visibleEnd: number
    overscanLeading: number
    overscanTrailing: number
    poolSize: number
    visibleCount: number
    totalCount: number
    indexColumnWidth: number
    pinnedRightWidth: number
  }>
  clampScrollTopValue: (value: number) => number
  handleScroll: (event: Event) => void
  updateViewportHeight: () => void
  measureRowHeight: () => void
  cancelScrollRaf: () => void
  scrollToRow: (index: number) => void
  scrollToColumn: (key: string) => void
  isRowVisible: (index: number) => boolean
  debugMode: Ref<boolean>
  fps: Ref<number>
  frameTime: Ref<number>
  droppedFrames: Ref<number>
  layoutReads: Ref<number>
  layoutWrites: Ref<number>
  syncScrollRate: Ref<number>
  heavyUpdateRate: Ref<number>
  virtualizerUpdates: Ref<number>
  virtualizerSkips: Ref<number>
  state: Ref<TableViewportState>
  visibleRange: Ref<{ start: number; end: number }>
  attach: () => void
  detach: () => void
  refresh: (force?: boolean) => void
}

export function useTableViewport(options: UseTableViewportOptions): UseTableViewportResult {
  const signalRegistry: SignalRegistry = new WeakMap()

  const visibleRowsRef = shallowRef<VisibleRow[]>([])
  const visibleRowsPoolRef = shallowRef<RowPoolItem[]>([])
  const rowPoolVersionRef = shallowRef(0)

  const initialServerIntegration = resolveServerIntegration(options.serverIntegration)

  const composeImperativeCallbacks = (
    callbacks: TableViewportImperativeCallbacks | null | undefined,
  ): TableViewportImperativeCallbacks => {
    const base = callbacks ?? {}
    const onRowsHandler = base.onRows

    return {
      ...base,
      onRows: payload => {
        const sourceVisibleRows = Array.isArray(payload.visibleRows)
          ? payload.visibleRows
          : payload.pool.reduce<VisibleRow[]>((acc, item) => {
              if (item.entry) acc.push(item.entry)
              return acc
            }, [])

        visibleRowsRef.value = sourceVisibleRows.slice()
        visibleRowsPoolRef.value = payload.pool.slice()
        if (typeof payload.version === "number") {
          rowPoolVersionRef.value = payload.version
        }

        if (typeof onRowsHandler === "function") {
          onRowsHandler(payload)
        }
      },
    }
  }

  const controller = createTableViewportController({
    resolvePinMode,
    getColumnKey: column => column.key,
    createSignal: initial => createVueWritableSignal(signalRegistry, initial),
    onAfterScroll: options.onAfterScroll,
    onNearBottom: options.onNearBottom,
    imperativeCallbacks: composeImperativeCallbacks(options.imperativeCallbacks),
    serverIntegration: initialServerIntegration ?? undefined,
    hostEnvironment: createViewportHostEnvironment(),
    normalizeAndClampScroll: options.normalizeAndClampScroll,
    runtime: options.runtime,
  })

  const virtualizationEnabledResolved = computed(() => {
    const source = options.virtualizationEnabled
    if (typeof source === "boolean") {
      return source
    }
    if (source == null) {
      return true
    }
    return Boolean(unref(source))
  })

  watch(
    virtualizationEnabledResolved,
    value => {
      controller.setVirtualizationEnabled(value)
    },
    { immediate: true },
  )

  const horizontalVirtualizationResolved = computed(() => {
    const source = options.horizontalVirtualizationEnabled
    if (typeof source === "boolean") {
      return source
    }
    if (source == null) {
      return true
    }
    return Boolean(unref(source))
  })

  watch(
    horizontalVirtualizationResolved,
    value => {
      controller.setHorizontalVirtualizationEnabled(value)
    },
    { immediate: true },
  )

  const { input, core, derived } = controller
  const rowSignals = derived.rows
  const columnSignals = derived.columns
  const metricSignals = derived.metrics

  watch(
    options.processedRows,
    rows => {
      controller.setProcessedRows(Array.isArray(rows) ? rows : [])
    },
    { immediate: true },
  )

  watch(
    options.columns,
    value => {
      controller.setColumns(Array.isArray(value) ? value : [])
    },
    { immediate: true },
  )

  if (options.syncTargets) {
    watch(
      options.syncTargets,
      value => {
        controller.setViewportSyncTargets(value ?? null)
      },
      { immediate: true },
    )
  } else {
    controller.setViewportSyncTargets(null)
  }

  const rowHeightModeResolved = computed(() => resolveRowHeightModeValue(options.rowHeightMode ?? "fixed"))
  watch(
    rowHeightModeResolved,
    mode => {
      controller.setRowHeightMode(mode)
    },
    { immediate: true },
  )

  const baseRowHeightResolved = computed(() => resolveBaseRowHeight(options.baseRowHeight ?? BASE_ROW_HEIGHT))
  watch(
    baseRowHeightResolved,
    height => {
      controller.setBaseRowHeight(height)
    },
    { immediate: true },
  )

  const viewportMetricsRef = options.viewportMetrics
  if (viewportMetricsRef) {
    watch(
      viewportMetricsRef,
      metrics => {
        controller.setViewportMetrics(metrics ?? null)
      },
      { immediate: true },
    )
  } else {
    controller.setViewportMetrics(null)
  }

  if (options.isLoading) {
    watch(
      options.isLoading,
      value => {
        controller.setIsLoading(Boolean(value))
      },
      { immediate: true },
    )
  } else {
    controller.setIsLoading(false)
  }

  controller.setImperativeCallbacks(composeImperativeCallbacks(options.imperativeCallbacks))
  controller.setOnAfterScroll(options.onAfterScroll)
  controller.setOnNearBottom(options.onNearBottom)

  if (options.serverIntegration) {
    watch(
      () => resolveServerIntegration(options.serverIntegration),
      value => {
        controller.setServerIntegration(value)
      },
      { immediate: true },
    )
  } else {
    controller.setServerIntegration(null)
  }

  watch(
    [() => options.containerRef.value, () => options.headerRef.value],
    ([container, header]) => {
      controller.attach(container, header)
    },
    { immediate: true },
  )

  onMounted(() => {
    controller.refresh(true)
  })

  onBeforeUnmount(() => {
    controller.detach()
    controller.dispose()
  })

  const useSignal = <T,>(signal: WritableSignal<T>): ShallowRef<T> => getSignalRef(signalRegistry, signal)

  const scrollTop = useSignal(input.scrollTop)
  const scrollLeft = useSignal(input.scrollLeft)
  const viewportHeight = useSignal(input.viewportHeight)
  const viewportWidth = useSignal(input.viewportWidth)
  const virtualizationEnabled = useSignal(input.virtualizationEnabled)

  const totalRowCount = useSignal(core.totalRowCount)
  const effectiveRowHeight = useSignal(core.effectiveRowHeight)
  const visibleCount = useSignal(core.visibleCount)
  const poolSize = useSignal(core.poolSize)
  const totalContentHeight = useSignal(core.totalContentHeight)
  const startIndex = useSignal(core.startIndex)
  const endIndex = useSignal(core.endIndex)
  const overscanLeading = useSignal(core.overscanLeading)
  const overscanTrailing = useSignal(core.overscanTrailing)

  const visibleRows = visibleRowsRef
  const visibleRowsPool = visibleRowsPoolRef
  const rowPoolVersion = rowPoolVersionRef
  const visibleRange = useSignal(rowSignals.visibleRange)

  const visibleColumns = useSignal(columnSignals.visibleColumns)
  const visibleColumnEntries = useSignal(columnSignals.visibleColumnEntries)
  const visibleScrollableColumns = useSignal(columnSignals.visibleScrollableColumns)
  const visibleScrollableEntries = useSignal(columnSignals.visibleScrollableEntries)
  const pinnedLeftColumns = useSignal(columnSignals.pinnedLeftColumns)
  const pinnedLeftEntries = useSignal(columnSignals.pinnedLeftEntries)
  const pinnedRightColumns = useSignal(columnSignals.pinnedRightColumns)
  const pinnedRightEntries = useSignal(columnSignals.pinnedRightEntries)
  const leftPadding = useSignal(columnSignals.leftPadding)
  const rightPadding = useSignal(columnSignals.rightPadding)
  const columnWidthMap = useSignal(columnSignals.columnWidthMap)
  const visibleStartCol = useSignal(columnSignals.visibleStartCol)
  const visibleEndCol = useSignal(columnSignals.visibleEndCol)
  const scrollableRange = useSignal(columnSignals.scrollableRange)
  const columnVirtualState = useSignal(columnSignals.columnVirtualState)

  const fps = useSignal(metricSignals.fps)
  const frameTime = useSignal(metricSignals.frameTime)
  const droppedFrames = useSignal(metricSignals.droppedFrames)
  const layoutReads = useSignal(metricSignals.layoutReads)
  const layoutWrites = useSignal(metricSignals.layoutWrites)
  const syncScrollRate = useSignal(metricSignals.syncScrollRate)
  const heavyUpdateRate = useSignal(metricSignals.heavyUpdateRate)
  const virtualizerUpdates = useSignal(metricSignals.virtualizerUpdates)
  const virtualizerSkips = useSignal(metricSignals.virtualizerSkips)

  const debugModeSignal = useSignal(metricSignals.debugMode)

  const state = computed<TableViewportState>(() => ({
    startIndex: startIndex.value,
    endIndex: endIndex.value,
    visibleCount: visibleCount.value,
    poolSize: poolSize.value,
    totalRowCount: totalRowCount.value,
    overscanLeading: overscanLeading.value,
    overscanTrailing: overscanTrailing.value,
  }))
  const debugMode = computed<boolean>({
    get: () => debugModeSignal.value,
    set: value => {
      controller.setDebugMode(Boolean(value))
    },
  })

  const handleScroll = (event: Event) => {
    controller.handleScroll(event)
  }

  const updateViewportHeight = () => {
    controller.updateViewportHeight()
  }

  const measureRowHeight = () => {
    controller.measureRowHeight()
  }

  const cancelScrollRaf = () => {
    controller.cancelScrollRaf()
  }

  const scrollToRow = (index: number) => {
    controller.scrollToRow(index)
  }

  const scrollToColumn = (key: string) => {
    controller.scrollToColumn(key)
  }

  const isRowVisible = (index: number) => controller.isRowVisible(index)

  const attach = () => {
    controller.attach(options.containerRef.value, options.headerRef.value)
  }

  const detach = () => {
    controller.detach()
  }

  const refresh = (force?: boolean) => {
    controller.refresh(force)
  }

  return {
    scrollTop,
    scrollLeft,
    viewportHeight,
    viewportWidth,
    totalRowCount,
    effectiveRowHeight,
    visibleCount,
    poolSize,
    totalContentHeight,
    startIndex,
    endIndex,
    visibleRows,
    visibleRowsPool,
    rowPoolVersion,
    virtualizationEnabled,
    visibleColumns,
    visibleColumnEntries,
    visibleScrollableColumns,
    visibleScrollableEntries,
    pinnedLeftColumns,
    pinnedLeftEntries,
    pinnedRightColumns,
    pinnedRightEntries,
    leftPadding,
    rightPadding,
    columnWidthMap,
    visibleStartCol,
    visibleEndCol,
    scrollableRange,
    columnVirtualState,
    clampScrollTopValue: value => controller.clampScrollTopValue(value),
    handleScroll,
    updateViewportHeight,
    measureRowHeight,
    cancelScrollRaf,
    scrollToRow,
    scrollToColumn,
    isRowVisible,
    debugMode,
    fps,
    frameTime,
    droppedFrames,
    layoutReads,
    layoutWrites,
    syncScrollRate,
    heavyUpdateRate,
    virtualizerUpdates,
    virtualizerSkips,
    state,
    visibleRange,
    attach,
    detach,
    refresh,
  }
}
