import { computed } from "vue"
import type { ComputedRef, Ref } from "vue"
import type { ColumnMetric } from "./useAxisVirtualizer"
import type { RowPoolItem, TableViewportState } from "./useTableViewport"
import type { ServerRowModelDiagnostics } from "./useServerRowModel"

export type VirtualDebugRowSegment = "leading" | "visible" | "trailing"

export type VirtualDebugColumnSegment = "leading" | "visible" | "trailing"
export type VirtualDebugColumnBand = "pinned-left" | "scrollable" | "pinned-right"

export interface VirtualDebugRowPoolItem {
  poolIndex: number
  rowIndex: number
  displayIndex: number
  originalIndex: number | null
  active: boolean
  segment: VirtualDebugRowSegment
}

export interface VirtualDebugColumnPoolItem {
  key: string
  band: VirtualDebugColumnBand
  poolIndex: number
  colIndex: number
  originalIndex: number | null
  active: boolean
  segment: VirtualDebugColumnSegment
}

interface VirtualDebugColumnState {
  start: number
  end: number
  visibleStart: number
  visibleEnd: number
  overscanLeading: number
  overscanTrailing: number
  poolSize: number
  visibleCount: number
  totalCount: number
}

export interface VirtualDebugMetrics {
  timestamp: number
  fps: number
  frameTime: number
  droppedFrames: number
  virtualizationEnabled: boolean
  scrollTop: number
  scrollLeft: number
  viewportHeight: number
  viewportWidth: number
  totalContentHeight: number
  layoutReads: number
  layoutWrites: number
  dom: {
    totalElements: number
    lastUpdated: number
  }
  syncScrollRate: number
  heavyUpdateRate: number
  virtualizerUpdates: number
  virtualizerSkips: number
  rows: {
    startIndex: number
    endIndex: number
    visibleStartIndex: number
    visibleEndIndex: number
    visibleCount: number
    poolSize: number
    totalRowCount: number
    overscanLeading: number
    overscanTrailing: number
    overscanSize: number
    poolUtilization: number
    estimatedRowHeight: number
    poolRange: { startPercent: number; sizePercent: number }
    visibleRange: { startPercent: number; sizePercent: number }
    overscanLead: number
    overscanTrail: number
    version: number
  }
  columns: {
    totalVisible: number
    pinnedLeft: number
    pinnedRight: number
    scrollable: number
    visibleStart: number
    visibleEnd: number
    totalWidth: number
    pinnedLeftWidth: number
    pinnedRightWidth: number
    scrollableWidth: number
    poolSize: number
    visibleCount: number
    poolUtilization: number
  }
  rowPool: VirtualDebugRowPoolItem[]
  columnPool: VirtualDebugColumnPoolItem[]
  server?: {
    enabled: boolean
    progress: number | null
    loadedRanges: Array<{ start: number; end: number }>
    cacheBlocks: number
    cachedRows: number
    pendingBlocks: number
    pendingRequests: number
    abortedRequests: number
    cacheHits: number
    cacheMisses: number
    effectivePreloadThreshold: number
  }
}

interface UseVirtualDebugOptions {
  debugMode: Ref<boolean>
  fps: Ref<number>
  frameTime: Ref<number>
  droppedFrames: Ref<number>
  scrollTop: Ref<number>
  scrollLeft: Ref<number>
  viewportHeight: Ref<number>
  viewportWidth: Ref<number>
  totalContentHeight: Ref<number>
  totalRowCount: Ref<number>
  effectiveRowHeight: Ref<number>
  state: Ref<TableViewportState>
  visibleRowsPool: Ref<RowPoolItem[]>
  rowPoolVersion: Ref<number>
  visibleColumnEntries: Ref<ColumnMetric[]>
  visibleScrollableEntries: Ref<ColumnMetric[]>
  pinnedLeftEntries: Ref<ColumnMetric[]>
  pinnedRightEntries: Ref<ColumnMetric[]>
  visibleStartCol: Ref<number>
  visibleEndCol: Ref<number>
  columnState: Ref<VirtualDebugColumnState>
  virtualizationEnabled: Ref<boolean>
  layoutReads: Ref<number>
  layoutWrites: Ref<number>
  syncScrollRate: Ref<number>
  heavyUpdateRate: Ref<number>
  virtualizerUpdates: Ref<number>
  virtualizerSkips: Ref<number>
  domStats?: {
    totalElements: Ref<number>
    lastUpdated: Ref<number>
  }
  serverState?: {
    enabled: Ref<boolean> | ComputedRef<boolean>
    progress: Ref<number | null> | ComputedRef<number | null>
    loadedRanges: Ref<Array<{ start: number; end: number }>> | ComputedRef<Array<{ start: number; end: number }>>
    diagnostics: Ref<ServerRowModelDiagnostics | null> | ComputedRef<ServerRowModelDiagnostics | null>
  }
}

function sumColumnWidths(entries: ColumnMetric[]): number {
  return entries.reduce((total, entry) => {
    const width = Number(entry.width)
    if (!Number.isFinite(width)) {
      return total
    }
    return total + width
  }, 0)
}

export function useVirtualDebug(options: UseVirtualDebugOptions): {
  active: ComputedRef<boolean>
  metrics: ComputedRef<VirtualDebugMetrics | null>
  wrapperClass: ComputedRef<Record<string, boolean>>
  containerClass: ComputedRef<Record<string, boolean>>
} {
  const active = computed(() => Boolean(options.debugMode.value))

  const metrics = computed<VirtualDebugMetrics | null>(() => {
    if (!active.value) {
      return null
    }

    const state = options.state.value
    const totalRowCount = Math.max(0, state.totalRowCount)
    const safeRowCount = Math.max(totalRowCount, 1)

    const visibleStartIndex = Math.min(
      state.endIndex,
      state.startIndex + Math.max(0, state.overscanLeading)
    )
    const visibleEndIndex = Math.min(
      state.endIndex,
      visibleStartIndex + Math.max(0, state.visibleCount)
    )
    const poolEndIndex = Math.min(
      state.endIndex,
      state.startIndex + Math.max(0, state.poolSize)
    )
    const poolSpan = Math.max(poolEndIndex - state.startIndex, 0)
    const visibleSpan = Math.max(visibleEndIndex - visibleStartIndex, 0)

    const poolRange = {
      startPercent: (state.startIndex / safeRowCount) * 100,
      sizePercent: (poolSpan / safeRowCount) * 100,
    }

    const visibleRange = {
      startPercent: (visibleStartIndex / safeRowCount) * 100,
      sizePercent: (visibleSpan / safeRowCount) * 100,
    }

    const poolVersion = options.rowPoolVersion.value
    const poolItems = options.visibleRowsPool.value
    const overscanLeading = Math.max(0, Math.min(state.overscanLeading, poolItems.length))
    const visibleCount = Math.max(0, Math.min(state.visibleCount, poolItems.length - overscanLeading))
    const rowPool: VirtualDebugRowPoolItem[] = poolItems.map((item, index) => {
      let segment: VirtualDebugRowSegment = "visible"
      if (index < overscanLeading) {
        segment = "leading"
      } else if (index >= overscanLeading + visibleCount) {
        segment = "trailing"
      }

      return {
        poolIndex: item.poolIndex,
        rowIndex: item.rowIndex,
        displayIndex: item.displayIndex,
        originalIndex: item.entry?.originalIndex ?? null,
        active: Boolean(item.entry),
        segment,
      }
    })

    const activeCount = rowPool.reduce((total, item) => (item.active ? total + 1 : total), 0)
    const poolUtilization = state.poolSize > 0 ? activeCount / state.poolSize : 0
    const overscanSize = Math.max(0, state.poolSize - state.visibleCount)

    const visibleColumnEntries = options.visibleColumnEntries.value
    const visibleScrollableEntries = options.visibleScrollableEntries.value
    const pinnedLeftEntries = options.pinnedLeftEntries.value
    const pinnedRightEntries = options.pinnedRightEntries.value
    const columnState = options.columnState.value

    const totalWidth = sumColumnWidths(visibleColumnEntries)
    const scrollableWidth = sumColumnWidths(visibleScrollableEntries)
    const pinnedLeftWidth = sumColumnWidths(pinnedLeftEntries)
    const pinnedRightWidth = sumColumnWidths(pinnedRightEntries)

    const scrollableEntryByIndex = new Map<number, ColumnMetric>()
    for (const entry of visibleScrollableEntries) {
      if (typeof entry.scrollableIndex === "number") {
        scrollableEntryByIndex.set(entry.scrollableIndex, entry)
      }
    }

    const columnPool: VirtualDebugColumnPoolItem[] = []

    pinnedLeftEntries.forEach((entry, idx) => {
      columnPool.push({
        key: `pinned-left-${entry.column.key ?? idx}`,
        band: "pinned-left",
        poolIndex: idx,
        colIndex: entry.index,
        originalIndex: entry.index,
        active: true,
        segment: "visible",
      })
    })

    const poolStart = Math.max(0, columnState.start)
    const poolEnd = Math.max(poolStart, columnState.end)
    for (let offset = 0; offset < poolEnd - poolStart; offset += 1) {
      const scrollableIndex = poolStart + offset
      const segment: VirtualDebugColumnSegment =
        scrollableIndex < columnState.visibleStart
          ? "leading"
          : scrollableIndex >= columnState.visibleEnd
            ? "trailing"
            : "visible"
      const metric = scrollableEntryByIndex.get(scrollableIndex) ?? null
      columnPool.push({
        key: `scroll-${scrollableIndex}`,
        band: "scrollable",
        poolIndex: offset,
        colIndex: scrollableIndex,
        originalIndex: metric?.index ?? null,
        active: Boolean(metric),
        segment,
      })
    }

    pinnedRightEntries.forEach((entry, idx) => {
      columnPool.push({
        key: `pinned-right-${entry.column.key ?? idx}`,
        band: "pinned-right",
        poolIndex: idx,
        colIndex: entry.index,
        originalIndex: entry.index,
        active: true,
        segment: "visible",
      })
    })

    let serverMetrics: VirtualDebugMetrics["server"] | undefined
    if (options.serverState) {
      const enabled = Boolean(options.serverState.enabled.value)
      const diagnostics = options.serverState.diagnostics.value
      const safeDiagnostics: ServerRowModelDiagnostics | null = diagnostics ?? null
      serverMetrics = {
        enabled,
        progress: options.serverState.progress.value ?? null,
        loadedRanges: (options.serverState.loadedRanges.value ?? []).slice(),
        cacheBlocks: safeDiagnostics?.cacheBlocks ?? 0,
        cachedRows: safeDiagnostics?.cachedRows ?? 0,
        pendingBlocks: safeDiagnostics?.pendingBlocks ?? 0,
        pendingRequests: safeDiagnostics?.pendingRequests ?? 0,
        abortedRequests: safeDiagnostics?.abortedRequests ?? 0,
        cacheHits: safeDiagnostics?.cacheHits ?? 0,
        cacheMisses: safeDiagnostics?.cacheMisses ?? 0,
        effectivePreloadThreshold: safeDiagnostics?.effectivePreloadThreshold ?? 0,
      }
    }

    return {
      timestamp: Date.now(),
      fps: Number.isFinite(options.fps.value) ? Number(options.fps.value) : 0,
      frameTime: options.frameTime.value,
      droppedFrames: options.droppedFrames.value,
      virtualizationEnabled: Boolean(options.virtualizationEnabled.value),
  layoutReads: options.layoutReads.value,
  layoutWrites: options.layoutWrites.value,
      scrollTop: options.scrollTop.value,
      scrollLeft: options.scrollLeft.value,
      viewportHeight: options.viewportHeight.value,
      viewportWidth: options.viewportWidth.value,
      totalContentHeight: options.totalContentHeight.value,
      dom: {
        totalElements: options.domStats?.totalElements.value ?? 0,
        lastUpdated: options.domStats?.lastUpdated.value ?? 0,
      },
      syncScrollRate: options.syncScrollRate.value,
      heavyUpdateRate: options.heavyUpdateRate.value,
      virtualizerUpdates: options.virtualizerUpdates.value,
      virtualizerSkips: options.virtualizerSkips.value,
      rows: {
        startIndex: state.startIndex,
        endIndex: state.endIndex,
        visibleStartIndex,
        visibleEndIndex,
        visibleCount: state.visibleCount,
        poolSize: state.poolSize,
        totalRowCount,
        overscanLeading: state.overscanLeading,
        overscanTrailing: state.overscanTrailing,
        overscanSize,
        poolUtilization,
        estimatedRowHeight: options.effectiveRowHeight.value,
        poolRange,
        visibleRange,
        overscanLead: state.overscanLeading,
        overscanTrail: state.overscanTrailing,
        version: poolVersion,
      },
      columns: {
        totalVisible: visibleColumnEntries.length,
        pinnedLeft: pinnedLeftEntries.length,
        pinnedRight: pinnedRightEntries.length,
        scrollable: visibleScrollableEntries.length,
        visibleStart: options.visibleStartCol.value,
        visibleEnd: options.visibleEndCol.value,
        totalWidth,
        pinnedLeftWidth,
        pinnedRightWidth,
        scrollableWidth,
        poolSize: columnState.poolSize,
        visibleCount: columnState.visibleCount,
        poolUtilization: columnState.poolSize > 0 ? columnState.visibleCount / columnState.poolSize : 0,
      },
      rowPool,
      columnPool,
      server: serverMetrics,
    }
  })

  const wrapperClass = computed(() => ({
    "ui-table__viewport-wrapper--debug": active.value,
  }))

  const containerClass = computed(() => ({
    "ui-table__viewport--debug": active.value,
  }))

  return {
    active,
    metrics,
    wrapperClass,
    containerClass,
  }
}
