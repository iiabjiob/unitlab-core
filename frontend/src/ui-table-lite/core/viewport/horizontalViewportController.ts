import { COLUMN_VIRTUALIZATION_BUFFER, resolveColumnWidth } from "../virtualization/columnSizing"
import {
  computeColumnLayout,
  type ColumnLayoutMetric,
  type ColumnLayoutOutput,
  type LayoutColumnLike,
} from "../virtualization/horizontalLayout"
import {
  createEmptyColumnSnapshot,
  updateColumnSnapshot,
  type ColumnMetric,
  type ColumnVirtualizationSnapshot,
} from "../virtualization/columnSnapshot"
import type { ColumnPinMode } from "../virtualization/types"
import {
  createHorizontalAxisStrategy,
  type HorizontalVirtualizerMeta,
  type HorizontalVirtualizerPayload,
} from "../virtualization/horizontalVirtualizer"
import {
  createAxisVirtualizer,
  type AxisVirtualizerState,
  type AxisVirtualizerStrategy,
} from "../virtualization/axisVirtualizer"
import {
  INDEX_COLUMN_WIDTH,
  SCROLL_EDGE_PADDING,
  SCROLL_EPSILON,
  clamp,
} from "../utils/constants"

export interface HorizontalViewportMetrics {
  containerWidth: number
  scrollLeft: number
  scrollWidth: number
}

export interface HorizontalViewportOptions<TColumn> {
  getViewportWidth: () => number
  getZoom: () => number
  getColumnKey: (column: TColumn) => string
  resolvePinMode: (column: TColumn) => ColumnPinMode
  overscan?: number
  indexColumnWidth?: number
  virtualizationEnabled?: boolean
  onSnapshot?: (snapshot: ColumnVirtualizationSnapshot<TColumn>) => void
  onScrollApplied?: (payload: { appliedScrollLeft: number; offset: number }) => void
  getTimestamp?: () => number
}

export interface HorizontalViewportState<TColumn> {
  scrollLeft: number
  viewportWidth: number
  effectiveViewportWidth: number
  containerWidthForColumns: number
  indexColumnWidth: number
  totalColumnCount: number
  columnSnapshot: ColumnVirtualizationSnapshot<TColumn>
  virtualizerState: AxisVirtualizerState<HorizontalVirtualizerPayload>
}

interface HorizontalViewportContext<TColumn> {
  metrics: HorizontalViewportMetrics
  columns: readonly TColumn[]
}

interface ColumnLookupEntry {
  pin: ColumnPinMode
  index: number
  scrollableIndex?: number
}

export function createHorizontalViewportController<TColumn extends LayoutColumnLike>(
  options: HorizontalViewportOptions<TColumn>,
) {
  const strategy = createHorizontalAxisStrategy<TColumn>() as AxisVirtualizerStrategy<
    HorizontalVirtualizerMeta<TColumn>,
    HorizontalVirtualizerPayload
  >
  const virtualizer = createAxisVirtualizer("horizontal", strategy, {
    visibleStart: 0,
    visibleEnd: 0,
    leftPadding: 0,
    rightPadding: 0,
    totalScrollableWidth: 0,
    visibleScrollableWidth: 0,
    averageWidth: 0,
    scrollSpeed: 0,
    effectiveViewport: 0,
  })

  const columnSnapshot = createEmptyColumnSnapshot<TColumn>()

  const state: HorizontalViewportState<TColumn> = {
    scrollLeft: 0,
    viewportWidth: 0,
    effectiveViewportWidth: 0,
    containerWidthForColumns: 0,
    indexColumnWidth: INDEX_COLUMN_WIDTH,
    totalColumnCount: 0,
    columnSnapshot,
    virtualizerState: virtualizer.getState(),
  }

  let lastMetrics: HorizontalViewportMetrics | null = null
  let lastColumns: readonly TColumn[] = []
  let layout: ColumnLayoutOutput<TColumn> | null = null
  let pinnedLeftMetrics: ColumnMetric<TColumn>[] = []
  let pinnedRightMetrics: ColumnMetric<TColumn>[] = []
  const columnLookup = new Map<string, ColumnLookupEntry>()
  let layoutVersion = 0
  let lastOffsetSample = 0
  let lastOffsetSampleTs = 0
  let smoothedScrollVelocity = 0
  let lastAppliedScrollLeft = 0
  let lastProcessedLayoutVersion = -1

  function resolveTimestamp(): number {
    if (typeof options.getTimestamp === "function") {
      const value = options.getTimestamp()
      if (Number.isFinite(value)) {
        return value
      }
    }
    if (typeof performance !== "undefined" && typeof performance.now === "function") {
      return performance.now()
    }
    return Date.now()
  }

  function resolveZoom(): number {
    const value = options.getZoom?.()
    if (typeof value === "number" && Number.isFinite(value) && value > 0) {
      return value
    }
    return 1
  }

  function resolveIndexColumnWidth(zoom: number): number {
    const base = typeof options.indexColumnWidth === "number" ? options.indexColumnWidth : INDEX_COLUMN_WIDTH
    return Math.max(0, base) * zoom
  }

  function resolveOverscan(): number {
    if (typeof options.overscan === "number" && Number.isFinite(options.overscan)) {
      return Math.max(0, options.overscan)
    }
    return COLUMN_VIRTUALIZATION_BUFFER
  }

  function isVirtualizationEnabled(): boolean {
    if (options.virtualizationEnabled === false) {
      return false
    }
    return true
  }

  function updateMetrics(context: HorizontalViewportContext<TColumn>): void {
    lastMetrics = context.metrics
    lastColumns = context.columns

    const zoom = resolveZoom()
    const layoutResult = computeColumnLayout<TColumn>({
      columns: lastColumns,
      zoom,
      resolvePinMode: options.resolvePinMode,
    })
    layout = layoutResult
    layoutVersion += 1

    const fallbackViewportWidth = options.getViewportWidth?.()
    const viewportCandidate = context.metrics.containerWidth
    const viewportWidth = Number.isFinite(viewportCandidate) && viewportCandidate >= 0
      ? viewportCandidate
      : (Number.isFinite(fallbackViewportWidth) && fallbackViewportWidth != null
          ? Math.max(0, fallbackViewportWidth as number)
          : lastMetrics?.containerWidth ?? 0)
    const indexWidth = resolveIndexColumnWidth(layoutResult.zoom)
    const containerWidthForColumns = Math.max(0, viewportWidth - indexWidth)
    const effectiveViewportWidth = Math.max(
      0,
      containerWidthForColumns - layoutResult.pinnedLeftWidth - layoutResult.pinnedRightWidth,
    )

    state.viewportWidth = viewportWidth
    state.indexColumnWidth = indexWidth
    state.containerWidthForColumns = containerWidthForColumns
    state.effectiveViewportWidth = effectiveViewportWidth
    state.totalColumnCount = layoutResult.scrollableColumns.length
    state.scrollLeft = context.metrics.scrollLeft
    lastAppliedScrollLeft = state.scrollLeft
    lastOffsetSample = state.scrollLeft
    lastOffsetSampleTs = resolveTimestamp()

    pinnedLeftMetrics = layoutResult.pinnedLeft.map(copyLayoutMetric)
    pinnedRightMetrics = layoutResult.pinnedRight.map(copyLayoutMetric)

    columnLookup.clear()
    layoutResult.pinnedLeft.forEach(metric => {
      const key = options.getColumnKey(metric.column)
      columnLookup.set(key, { pin: "left", index: metric.index })
    })
    layoutResult.pinnedRight.forEach(metric => {
      const key = options.getColumnKey(metric.column)
      columnLookup.set(key, { pin: "right", index: metric.index })
    })
    layoutResult.scrollableColumns.forEach((column, scrollableIndex) => {
      const key = options.getColumnKey(column)
      const index = layoutResult.scrollableIndices[scrollableIndex] ?? scrollableIndex
      columnLookup.set(key, { pin: "none", index, scrollableIndex })
    })
  }

  function applyScrollOffset(target: number, offset: number): void {
    const delta = Math.abs(lastAppliedScrollLeft - target)
    if (delta > SCROLL_EPSILON) {
      lastAppliedScrollLeft = target
    }
    state.scrollLeft = lastAppliedScrollLeft
    options.onScrollApplied?.({ appliedScrollLeft: state.scrollLeft, offset })
  }

  function emitSnapshot(): void {
    options.onSnapshot?.(state.columnSnapshot)
  }

  function resetSnapshot(): void {
    const snapshot = state.columnSnapshot
    snapshot.pinnedLeft.length = 0
    snapshot.pinnedRight.length = 0
    snapshot.visibleScrollable.length = 0
    snapshot.visibleColumns.length = 0
    snapshot.columnWidthMap.clear()
    snapshot.leftPadding = 0
    snapshot.rightPadding = 0
    snapshot.totalScrollableWidth = 0
    snapshot.visibleScrollableWidth = 0
    snapshot.scrollableStart = 0
    snapshot.scrollableEnd = 0
    snapshot.visibleStart = 0
    snapshot.visibleEnd = 0
    snapshot.pinnedLeftWidth = 0
    snapshot.pinnedRightWidth = 0
    snapshot.metrics = { widths: [], offsets: [], totalWidth: 0 }
    snapshot.containerWidthForColumns = state.containerWidthForColumns
    snapshot.indexColumnWidth = state.indexColumnWidth
    snapshot.scrollDirection = 0
    emitSnapshot()
  }

  function runVirtualizer(scrollOverride?: number): void {
    if (!lastMetrics || !layout) return

    const virtualizationEnabled = isVirtualizationEnabled()
    const total = layout.scrollableColumns.length
    const metrics = layout.scrollableMetrics
    const averageWidth = metrics.widths.length
      ? metrics.totalWidth / Math.max(metrics.widths.length, 1)
      : 1
    const estimatedItemSize = Math.max(averageWidth, 1)
    const viewportSize = Math.max(state.effectiveViewportWidth || estimatedItemSize, estimatedItemSize)

    if (total <= 0 || viewportSize <= 0) {
      state.virtualizerState = virtualizer.getState()
      state.scrollLeft = 0
      resetSnapshot()
      return
    }

    const override = typeof scrollOverride === "number" ? scrollOverride : undefined
    if (override == null && layoutVersion === lastProcessedLayoutVersion) {
      return
    }
    const fallbackOffset = Number.isFinite(lastAppliedScrollLeft) ? lastAppliedScrollLeft : 0
    const desiredOffsetRaw = override ?? fallbackOffset
    const desiredOffset = clampScrollLeftValue(desiredOffsetRaw)
    const previousOffset = state.virtualizerState.offset
    const direction = desiredOffset > previousOffset ? 1 : desiredOffset < previousOffset ? -1 : 0
    const overscan = resolveOverscan()
    const nativeScrollLimit = Math.max(0, lastMetrics.scrollWidth - lastMetrics.containerWidth)

    const nowTs = resolveTimestamp()
    if (Number.isFinite(desiredOffset)) {
      if (lastOffsetSampleTs > 0) {
        const deltaTime = Math.max(nowTs - lastOffsetSampleTs, 1)
        const instantVelocity = (desiredOffset - lastOffsetSample) / (deltaTime / 1000)
        if (Number.isFinite(instantVelocity)) {
          const smoothing = 0.35
          smoothedScrollVelocity = smoothedScrollVelocity * (1 - smoothing) + instantVelocity * smoothing
        } else {
          smoothedScrollVelocity *= 0.5
        }
      }
      lastOffsetSample = desiredOffset
      lastOffsetSampleTs = nowTs
    }

    const virtualizerState = virtualizer.update({
      axis: "horizontal",
      viewportSize,
      scrollOffset: desiredOffset,
      virtualizationEnabled,
      estimatedItemSize,
      totalCount: total,
      overscan,
      meta: {
        scrollableColumns: layout.scrollableColumns,
        scrollableIndices: layout.scrollableIndices,
        metrics,
        pinnedLeftWidth: layout.pinnedLeftWidth,
        pinnedRightWidth: layout.pinnedRightWidth,
        containerWidthForColumns: state.containerWidthForColumns,
        nativeScrollLimit,
        zoom: layout.zoom,
        buffer: overscan,
        scrollDirection: direction,
        scrollVelocity: smoothedScrollVelocity,
        version: layoutVersion,
      },
    })

  state.virtualizerState = virtualizerState
  applyScrollOffset(virtualizerState.offset, virtualizerState.offset)
    lastProcessedLayoutVersion = layoutVersion

    const payload = virtualizerState.payload ?? {
      visibleStart: virtualizerState.startIndex,
      visibleEnd: virtualizerState.endIndex,
      leftPadding: 0,
      rightPadding: 0,
      totalScrollableWidth: metrics.totalWidth,
      visibleScrollableWidth: metrics.totalWidth,
      averageWidth,
      scrollSpeed: Math.abs(smoothedScrollVelocity),
      effectiveViewport: state.effectiveViewportWidth,
    }

    updateColumnSnapshot({
      snapshot: state.columnSnapshot,
      meta: {
        scrollableColumns: layout.scrollableColumns,
        scrollableIndices: layout.scrollableIndices,
        metrics,
        pinnedLeft: pinnedLeftMetrics,
        pinnedRight: pinnedRightMetrics,
        pinnedLeftWidth: layout.pinnedLeftWidth,
        pinnedRightWidth: layout.pinnedRightWidth,
        containerWidthForColumns: state.containerWidthForColumns,
        indexColumnWidth: state.indexColumnWidth,
        scrollDirection: direction,
        zoom: layout.zoom,
      },
      range: {
        start: virtualizerState.startIndex,
        end: virtualizerState.endIndex,
      },
      payload,
      getColumnKey: options.getColumnKey,
      resolveColumnWidth,
    })

    emitSnapshot()
  }

  function clampScrollLeftValue(value: number): number {
    if (!Number.isFinite(value)) return 0
    if (!lastMetrics || !layout) {
      return Math.max(0, value)
    }

    const nativeLimit = Math.max(0, lastMetrics.scrollWidth - lastMetrics.containerWidth)
    if (!isVirtualizationEnabled()) {
      return clamp(value, 0, nativeLimit)
    }

    const metrics = layout.scrollableMetrics
    if (!metrics.widths.length) return 0

    const averageWidth = metrics.totalWidth / Math.max(metrics.widths.length, 1)
    if (!Number.isFinite(averageWidth) || averageWidth <= 0) {
      return clamp(value, 0, nativeLimit)
    }

    const effectiveViewport = Math.max(
      0,
      state.containerWidthForColumns - layout.pinnedLeftWidth - layout.pinnedRightWidth,
    )
    const baseMax = Math.max(0, metrics.totalWidth - effectiveViewport)
    const trailingGap = Math.max(0, effectiveViewport - metrics.totalWidth)
    const overscan = resolveOverscan()
    const overscanLeading = state.virtualizerState.overscanLeading
    const overscanTrailing = state.virtualizerState.overscanTrailing
    const bufferColumns = Math.max(overscan, overscanLeading, overscanTrailing)
    const bufferPx = bufferColumns * averageWidth
    const extendedMax = Math.max(0, baseMax + bufferPx + trailingGap + SCROLL_EDGE_PADDING)
    const virtualizationLimit = Math.max(baseMax, extendedMax)
    const tolerance = averageWidth + SCROLL_EDGE_PADDING

    let maxScroll = virtualizationLimit
    if (Number.isFinite(nativeLimit) && nativeLimit > 0) {
      if (Math.abs(nativeLimit - virtualizationLimit) > tolerance) {
        maxScroll = nativeLimit
      } else {
        maxScroll = Math.max(nativeLimit, virtualizationLimit)
      }
    }

    if (!Number.isFinite(maxScroll) || maxScroll <= 0) {
      return 0
    }
    return clamp(value, 0, maxScroll)
  }

  function scrollToColumn(key: string): void {
    if (!layout || !lastMetrics) return
    const entry = columnLookup.get(key)
    if (!entry) return

    if (entry.pin === "left") {
      runVirtualizer(0)
      return
    }

    const nativeLimit = Math.max(0, lastMetrics.scrollWidth - lastMetrics.containerWidth)
    if (entry.pin === "right") {
      runVirtualizer(nativeLimit)
      return
    }

    if (typeof entry.scrollableIndex === "number") {
      const offsets = layout.scrollableMetrics.offsets
      const target = offsets[entry.scrollableIndex] ?? 0
      const clamped = clampScrollLeftValue(target)
      runVirtualizer(clamped)
    }
  }

  function isColumnVisible(key: string): boolean {
    const snapshot = state.columnSnapshot
    return snapshot.visibleColumns.some(entry => options.getColumnKey(entry.column) === key)
  }

  return {
    state,
    updateMetrics,
    clampScrollLeftValue,
    runVirtualizer,
    scrollToColumn,
    isColumnVisible,
  }
}

function copyLayoutMetric<TColumn>(metric: ColumnLayoutMetric<TColumn>): ColumnMetric<TColumn> {
  return {
    column: metric.column,
    index: metric.index,
    width: metric.width,
    pin: metric.pin,
    offset: metric.offset,
  }
}
