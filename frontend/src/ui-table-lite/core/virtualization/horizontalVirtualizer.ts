import {
  COLUMN_VIRTUALIZATION_BUFFER,
  calculateVisibleColumns,
  type ColumnSizeLike,
  type ColumnWidthMetrics,
} from "./columnSizing"
import { clamp, SCROLL_EDGE_PADDING } from "../utils/constants"
import { createAxisVirtualizer, type AxisVirtualizerStrategy } from "./axisVirtualizer"

export interface HorizontalVirtualizerMeta<TColumn extends ColumnSizeLike> {
  scrollableColumns: readonly TColumn[]
  scrollableIndices: readonly number[]
  metrics: ColumnWidthMetrics
  pinnedLeftWidth: number
  pinnedRightWidth: number
  containerWidthForColumns: number
  nativeScrollLimit: number
  zoom: number
  buffer: number
  scrollDirection: number
  scrollVelocity?: number
  isRTL?: boolean
  version?: number
}

export interface HorizontalVirtualizerPayload {
  visibleStart: number
  visibleEnd: number
  leftPadding: number
  rightPadding: number
  totalScrollableWidth: number
  visibleScrollableWidth: number
  averageWidth: number
  scrollSpeed: number
  effectiveViewport: number
}

function resolveEffectiveViewport(containerWidth: number, pinnedLeftWidth: number, pinnedRightWidth: number): number {
  return Math.max(0, containerWidth - pinnedLeftWidth - pinnedRightWidth)
}

export function createHorizontalAxisStrategy<TColumn extends ColumnSizeLike>(): AxisVirtualizerStrategy<
  HorizontalVirtualizerMeta<TColumn>,
  HorizontalVirtualizerPayload
> {
  let lastClampInput = Number.NaN
  let lastClampResult = 0
  let lastClampVersion = -1
  let lastRangeOffset = Number.NaN
  let lastRangeStart = 0
  let lastRangeEnd = 0
  let lastRangePayload: HorizontalVirtualizerPayload | null = null
  let lastVersion = -1

  return {
    computeVisibleCount(context) {
      if (!context.virtualizationEnabled) {
        return context.totalCount
      }
      const { metrics, pinnedLeftWidth, pinnedRightWidth, containerWidthForColumns } = context.meta
      if (!metrics.widths.length) return 0
      const averageWidth = metrics.totalWidth / Math.max(metrics.widths.length, 1)
      if (!Number.isFinite(averageWidth) || averageWidth <= 0) {
        return metrics.widths.length
      }
      const effectiveViewport = resolveEffectiveViewport(containerWidthForColumns, pinnedLeftWidth, pinnedRightWidth)
      if (effectiveViewport <= 0) return 1
      return Math.max(1, Math.ceil(effectiveViewport / averageWidth))
    },
    clampScroll(value, context) {
      const {
        metrics,
        containerWidthForColumns,
        pinnedLeftWidth,
        pinnedRightWidth,
        nativeScrollLimit,
        buffer,
        version = -1,
      } = context.meta

      if (version !== lastClampVersion) {
        lastClampVersion = version
        lastClampInput = Number.NaN
      }

      if (!metrics.widths.length) return 0

      const nativeLimit = Math.max(0, nativeScrollLimit)

      if (!context.virtualizationEnabled) {
        lastClampInput = value
        lastClampResult = clamp(value, 0, nativeLimit)
        return lastClampResult
      }

      if (Number.isFinite(lastClampInput) && Math.abs(value - lastClampInput) < 1) {
        return lastClampResult
      }

      const offsets = metrics.offsets
      const widths = metrics.widths
      const lastIndex = Math.max(0, widths.length - 1)
      const totalScrollableWidth = Number.isFinite(metrics.totalWidth)
        ? metrics.totalWidth
        : offsets.length
          ? (offsets[lastIndex] ?? 0) + (widths[lastIndex] ?? 0)
          : 0

      if (!Number.isFinite(totalScrollableWidth) || totalScrollableWidth <= 0) {
        lastClampInput = value
        lastClampResult = clamp(value, 0, nativeLimit)
        return lastClampResult
      }

      const effectiveViewport = resolveEffectiveViewport(containerWidthForColumns, pinnedLeftWidth, pinnedRightWidth)
      if (effectiveViewport <= 0) {
        lastClampInput = value
        lastClampResult = 0
        return 0
      }

      const baseMax = Math.max(0, totalScrollableWidth - effectiveViewport)
      const trailingGap = Math.max(0, effectiveViewport - totalScrollableWidth)

      const baseBuffer = Number.isFinite(buffer) ? buffer : COLUMN_VIRTUALIZATION_BUFFER
      const overscanColumns = Math.max(baseBuffer, context.overscanLeading, context.overscanTrailing)
      const overscanCount = Math.min(widths.length, Math.max(0, Math.ceil(overscanColumns)))
      let overscanWidth = 0
      if (overscanCount > 0) {
        const overscanStartIndex = Math.max(0, widths.length - overscanCount)
        const overscanStartOffset = offsets[overscanStartIndex] ?? totalScrollableWidth
        overscanWidth = Math.max(0, totalScrollableWidth - overscanStartOffset)
      }

      const virtualizationLimit = Math.max(0, baseMax + overscanWidth + trailingGap + SCROLL_EDGE_PADDING)
      const maxScroll = Math.max(nativeLimit, virtualizationLimit)

      if (!Number.isFinite(maxScroll) || maxScroll <= 0) {
        lastClampInput = value
        lastClampResult = 0
        return 0
      }

      lastClampInput = value
      lastClampResult = clamp(value, 0, maxScroll)
      return lastClampResult
    },
    computeRange(offset, context, target) {
      const {
        scrollableColumns,
        containerWidthForColumns,
        pinnedLeftWidth,
        pinnedRightWidth,
        metrics,
        zoom,
        buffer,
        scrollVelocity = 0,
        version = -1,
      } = context.meta
      const payload = target.payload

      const isRTL = Boolean(context.meta.isRTL)

      if (version !== lastVersion) {
        lastVersion = version
        lastRangeOffset = Number.NaN
        lastRangePayload = null
      }

      if (!scrollableColumns.length) {
        payload.visibleStart = 0
        payload.visibleEnd = 0
        payload.leftPadding = 0
        payload.rightPadding = 0
        payload.totalScrollableWidth = 0
        payload.visibleScrollableWidth = 0
        payload.averageWidth = 0
        payload.scrollSpeed = 0
        payload.effectiveViewport = 0
        target.start = 0
        target.end = 0
        lastRangeOffset = offset
        lastRangeStart = 0
        lastRangeEnd = 0
        lastRangePayload = { ...payload }
        return target
      }

      if (Number.isFinite(lastRangeOffset) && Math.abs(offset - lastRangeOffset) < 1 && lastRangePayload) {
        payload.visibleStart = lastRangePayload.visibleStart
        payload.visibleEnd = lastRangePayload.visibleEnd
        payload.leftPadding = lastRangePayload.leftPadding
        payload.rightPadding = lastRangePayload.rightPadding
        payload.totalScrollableWidth = lastRangePayload.totalScrollableWidth
        payload.visibleScrollableWidth = lastRangePayload.visibleScrollableWidth
        payload.averageWidth = lastRangePayload.averageWidth
        payload.scrollSpeed = lastRangePayload.scrollSpeed
        payload.effectiveViewport = lastRangePayload.effectiveViewport
        target.start = lastRangeStart
        target.end = lastRangeEnd
        return target
      }

      if (!context.virtualizationEnabled) {
        const totalWidth = metrics.totalWidth
        payload.visibleStart = 0
        payload.visibleEnd = scrollableColumns.length
        const leftPadding = 0
        const rightPadding = 0
        payload.leftPadding = isRTL ? rightPadding : leftPadding
        payload.rightPadding = isRTL ? leftPadding : rightPadding
        payload.totalScrollableWidth = totalWidth
        payload.visibleScrollableWidth = totalWidth
        payload.averageWidth = metrics.widths.length ? totalWidth / Math.max(metrics.widths.length, 1) : 0
        payload.scrollSpeed = scrollVelocity
        payload.effectiveViewport = resolveEffectiveViewport(containerWidthForColumns, pinnedLeftWidth, pinnedRightWidth)
        target.start = 0
        target.end = scrollableColumns.length
        lastRangeOffset = offset
        lastRangeStart = target.start
        lastRangeEnd = target.end
        lastRangePayload = { ...payload }
        return target
      }

      const baseBuffer = Number.isFinite(buffer) ? buffer : COLUMN_VIRTUALIZATION_BUFFER
      const dynamicOverscanMultiplier = 1 + Math.min(Math.abs(scrollVelocity) / 1500, 1)
      const dynamicOverscan = Math.min(256, Math.max(0, Math.ceil(baseBuffer * dynamicOverscanMultiplier)))
      const overscanLeading = Math.max(context.overscanLeading, dynamicOverscan)
      const overscanTrailing = Math.max(context.overscanTrailing, dynamicOverscan)
      const visibleRange = calculateVisibleColumns(offset, containerWidthForColumns, scrollableColumns, {
        zoom,
        pinnedLeftWidth,
        pinnedRightWidth,
        metrics,
      })

      const visibleStart = visibleRange.startIndex
      const visibleEnd = visibleRange.endIndex
      const overscanStart = Math.max(0, visibleStart - overscanLeading)
      const overscanEnd = Math.min(context.totalCount, visibleEnd + overscanTrailing)
      const visibleScrollableWidth = Math.max(
        0,
        visibleRange.totalWidth - (visibleRange.leftPadding + visibleRange.rightPadding),
      )

      const averageWidth = metrics.widths.length ? metrics.totalWidth / Math.max(metrics.widths.length, 1) : 0
      const effectiveViewport = resolveEffectiveViewport(containerWidthForColumns, pinnedLeftWidth, pinnedRightWidth)
      const leftPadding = isRTL ? visibleRange.rightPadding : visibleRange.leftPadding
      const rightPadding = isRTL ? visibleRange.leftPadding : visibleRange.rightPadding

      payload.visibleStart = visibleStart
      payload.visibleEnd = visibleEnd
      payload.leftPadding = leftPadding
      payload.rightPadding = rightPadding
      payload.totalScrollableWidth = metrics.totalWidth
      payload.visibleScrollableWidth = visibleScrollableWidth
      payload.averageWidth = Number.isFinite(averageWidth) ? averageWidth : 0
      payload.scrollSpeed = scrollVelocity
      payload.effectiveViewport = effectiveViewport

      target.start = overscanStart
      target.end = overscanEnd

      lastRangeOffset = offset
      lastRangeStart = target.start
      lastRangeEnd = target.end
      lastRangePayload = { ...payload }
      return target
    },
    getOffsetForIndex(index, context) {
      const { metrics } = context.meta
      if (!metrics.offsets.length) return 0
      const clampedIndex = clamp(index, 0, Math.max(metrics.offsets.length - 1, 0))
      return metrics.offsets[clampedIndex] ?? 0
    },
  }
}

export function createHorizontalAxisVirtualizer<TColumn extends ColumnSizeLike>() {
  return createAxisVirtualizer<HorizontalVirtualizerMeta<TColumn>, HorizontalVirtualizerPayload>(
    "horizontal",
    createHorizontalAxisStrategy<TColumn>(),
    {
      visibleStart: 0,
      visibleEnd: 0,
      leftPadding: 0,
      rightPadding: 0,
      totalScrollableWidth: 0,
      visibleScrollableWidth: 0,
      averageWidth: 0,
      scrollSpeed: 0,
      effectiveViewport: 0,
    },
  )
}
