import { BASE_ROW_HEIGHT, ROW_POOL_OVERSCAN, SCROLL_EDGE_PADDING, SCROLL_EPSILON, VIRTUALIZATION_BUFFER, clamp } from "../utils/constants"
import type { VisibleRow } from "../types"
import type { AxisVirtualizerState } from "../virtualization/axisVirtualizer"
import { createVerticalAxisStrategy } from "../virtualization/verticalVirtualizer"
import { createAxisVirtualizer } from "../virtualization/axisVirtualizer"


export interface VerticalViewportMetrics {
  containerHeight: number
  containerWidth: number
  headerHeight: number
  scrollTop: number
  scrollHeight: number
}

export interface VerticalViewportOptions {
  overscan?: number
  rowHeightMode?: "fixed" | "auto"
  getRowCount: () => number
  getZoom: () => number
  getEffectiveRowHeight?: () => number
  onVisibleRows?: (payload: VerticalViewportRowsPayload) => void
  onScrollApplied?: (payload: { appliedScrollTop: number; offset: number }) => void
  virtualizationEnabled?: boolean
}

export interface VerticalViewportRowsPayload {
  pool: VisibleRow[]
  startIndex: number
  endIndex: number
  visibleCount: number
  poolSize: number
}

export interface VerticalViewportState {
  scrollTop: number
  viewportHeight: number
  viewportWidth: number
  totalRowCount: number
  effectiveRowHeight: number
  totalContentHeight: number
  virtualizerState: AxisVirtualizerState<undefined>
}

interface VerticalViewportContext {
  metrics: VerticalViewportMetrics
  /**
   * Immutable, index-addressable snapshot of visible rows for the current render pass.
   * Consumers must treat this collection as read-only.
   */
  rows: VisibleRow[]
}

export function createVerticalViewportController(options: VerticalViewportOptions) {
  const strategy = createVerticalAxisStrategy()
  const virtualizer = createAxisVirtualizer("vertical", strategy, undefined)

  const state: VerticalViewportState = {
    scrollTop: 0,
    viewportHeight: 0,
    viewportWidth: 0,
    totalRowCount: 0,
    effectiveRowHeight: BASE_ROW_HEIGHT,
    totalContentHeight: 0,
    virtualizerState: virtualizer.getState(),
  }

  let lastMetrics: VerticalViewportMetrics | null = null
  let lastRows: VisibleRow[] = []
  let lastAppliedScrollTop = 0


  function getOverscan(virtualizationEnabled: boolean) {
    if (!virtualizationEnabled) return 0
    if (typeof options.overscan === "number") {
      return Math.max(0, options.overscan)
    }
    return ROW_POOL_OVERSCAN + VIRTUALIZATION_BUFFER
  }

  function resolveRowHeight(): number {
    if (typeof options.getEffectiveRowHeight === "function") {
      return options.getEffectiveRowHeight()
    }
    const zoomFactor = Math.max(options.getZoom() || 1, 0.01)
    return BASE_ROW_HEIGHT * (zoomFactor < 1 ? 1 : zoomFactor)
  }

  function updateMetrics(context: VerticalViewportContext): void {
    lastMetrics = context.metrics
    lastRows = context.rows

    state.totalRowCount = options.getRowCount()
    state.viewportHeight = Math.max(context.metrics.containerHeight - context.metrics.headerHeight, 0)
    state.viewportWidth = context.metrics.containerWidth
    state.effectiveRowHeight = resolveRowHeight()
    state.totalContentHeight = state.totalRowCount * state.effectiveRowHeight
    state.scrollTop = context.metrics.scrollTop
    lastAppliedScrollTop = context.metrics.scrollTop
  }

  function clampScrollTopValue(value: number): number {
    if (!Number.isFinite(value)) return 0
    const metrics = lastMetrics
    if (!metrics) return value

    const virtualizationEnabled = isVirtualizationEnabled()

    if (!virtualizationEnabled) {
      const nativeLimit = Math.max(0, metrics.scrollHeight - metrics.containerHeight)
      return clamp(value, 0, nativeLimit)
    }

    const virtualizerState = state.virtualizerState
    const rowHeight = state.effectiveRowHeight || BASE_ROW_HEIGHT
    const overscanPx = Math.max(0, virtualizerState.overscanTrailing) * rowHeight
    const baseMax = Math.max(0, state.totalRowCount * rowHeight - state.viewportHeight)
    const visibleSpan = Math.max(1, virtualizerState.visibleCount)
    const trailingGap = Math.max(0, state.viewportHeight - visibleSpan * rowHeight)
    const virtualMax = Math.max(baseMax, baseMax + overscanPx + trailingGap + SCROLL_EDGE_PADDING)
    const nativeLimit = Math.max(0, metrics.scrollHeight - metrics.containerHeight)
    const maxScroll = Math.max(baseMax, Math.min(virtualMax, nativeLimit))

    if (!Number.isFinite(maxScroll) || maxScroll <= 0) {
      return 0
    }
    return clamp(value, 0, maxScroll)
  }

  function applyScrollOffset(target: number, virtualizerState: AxisVirtualizerState<undefined>) {
    const delta = Math.abs(lastAppliedScrollTop - target)
    if (delta > SCROLL_EPSILON) {
      lastAppliedScrollTop = target
    }
    state.scrollTop = lastAppliedScrollTop
    options.onScrollApplied?.({ appliedScrollTop: state.scrollTop, offset: virtualizerState.offset })
  }

  function renderVisibleRows(virtualizerState: AxisVirtualizerState<undefined>) {
    const rows = lastRows
    const start = virtualizerState.startIndex
    const end = virtualizerState.endIndex
    const pool: VisibleRow[] = []

    for (let index = start; index < end; index += 1) {
      const entry = rows[index]
      if (!entry) continue
      pool.push(entry)
    }

    options.onVisibleRows?.({
      pool,
      startIndex: start,
      endIndex: end,
      visibleCount: virtualizerState.visibleCount,
      poolSize: virtualizerState.poolSize,
    })
  }

  function isVirtualizationEnabled(): boolean {
    const rowHeightMode = options.rowHeightMode ?? "fixed"
    if (options.virtualizationEnabled === false) {
      return false
    }
    return rowHeightMode === "fixed"
  }

  function publishEmptyState(virtualizationEnabled: boolean) {
    const virtualState = virtualizer.update({
      axis: "vertical",
      viewportSize: state.viewportHeight,
      scrollOffset: 0,
      virtualizationEnabled,
      estimatedItemSize: state.effectiveRowHeight || BASE_ROW_HEIGHT,
      totalCount: state.totalRowCount,
      overscan: 0,
      meta: {
        zoom: Math.max(options.getZoom() || 1, 0.01),
        scrollDirection: 0,
        nativeScrollLimit: Math.max(0, (lastMetrics?.scrollHeight ?? 0) - (lastMetrics?.containerHeight ?? 0)),
        debug: false,
        debugNativeScrollLimit: undefined,
      },
    })
    state.virtualizerState = virtualState
    state.scrollTop = 0
    lastAppliedScrollTop = 0
    options.onVisibleRows?.({ pool: [], startIndex: 0, endIndex: 0, visibleCount: 0, poolSize: 0 })
    options.onScrollApplied?.({ appliedScrollTop: 0, offset: 0 })
  }

  function runVirtualizer(scrollOverride?: number) {
    if (!lastMetrics) return

    const virtualizationEnabled = isVirtualizationEnabled()
    const rowHeight = state.effectiveRowHeight || BASE_ROW_HEIGHT
    const overscan = getOverscan(virtualizationEnabled)
    const total = state.totalRowCount
    const viewport = Math.max(state.viewportHeight || rowHeight, rowHeight)

    if (total <= 0 || viewport <= 0) {
      publishEmptyState(virtualizationEnabled)
      return
    }

    const override = typeof scrollOverride === "number" ? scrollOverride : undefined
    const fallbackOffset = typeof lastMetrics.scrollTop === "number" ? lastMetrics.scrollTop : lastAppliedScrollTop
    const desiredOffsetRaw = override ?? fallbackOffset
    const desiredOffset = clampScrollTopValue(desiredOffsetRaw)
    const previousOffset = state.virtualizerState.offset
    const direction = desiredOffset > previousOffset ? 1 : desiredOffset < previousOffset ? -1 : 0

    const virtualizerState = virtualizer.update({
      axis: "vertical",
      viewportSize: viewport,
      scrollOffset: desiredOffset,
      virtualizationEnabled,
      estimatedItemSize: rowHeight,
      totalCount: total,
      overscan,
      meta: {
        zoom: Math.max(options.getZoom() || 1, 0.01),
        scrollDirection: direction,
        nativeScrollLimit: Math.max(0, lastMetrics.scrollHeight - lastMetrics.containerHeight),
        debug: false,
        debugNativeScrollLimit: undefined,
      },
    })

    state.virtualizerState = virtualizerState
    renderVisibleRows(virtualizerState)
    applyScrollOffset(virtualizerState.offset, virtualizerState)
  }

  function scrollToRow(index: number) {
    const total = state.totalRowCount
    if (total <= 0) return
    const clampedIndex = clamp(index, 0, Math.max(total - 1, 0))
    const target = clampScrollTopValue(clampedIndex * state.effectiveRowHeight)
    runVirtualizer(target)
  }

  function isRowVisible(index: number) {
    const virtualizerState = state.virtualizerState
    return index >= virtualizerState.startIndex && index < virtualizerState.endIndex
  }

  return {
    state,
    updateMetrics,
    clampScrollTopValue,
    runVirtualizer,
    scrollToRow,
    isRowVisible,
  }
}
