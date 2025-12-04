import { createAxisVirtualizer, type AxisVirtualizerStrategy } from "./axisVirtualizer"

const MIN_ROW_HEIGHT = 0.0001
const VIRTUAL_PADDING = 1

function clamp(value: number, min: number, max: number): number {
  if (Number.isNaN(value)) return min
  if (value < min) return min
  if (value > max) return max
  return value
}

export interface VerticalVirtualizerMeta {
  zoom: number
  scrollDirection?: number
  nativeScrollLimit?: number | null
  debug?: boolean
  debugNativeScrollLimit?: number | null
}

export type VerticalVirtualizerPayload = undefined

export function createVerticalAxisStrategy(): AxisVirtualizerStrategy<VerticalVirtualizerMeta, VerticalVirtualizerPayload> {
  return {
    computeVisibleCount(context) {
      const zoom = context.meta.zoom || 1
      const rowHeight = context.estimatedItemSize || 1
      const adjustedViewport = zoom >= 1 ? context.viewportSize : context.viewportSize / Math.max(zoom, 0.01)
      return Math.max(1, Math.ceil((adjustedViewport || rowHeight) / Math.max(rowHeight, 1)))
    },
    clampScroll(value, context) {
      const rowHeight = Math.max(context.estimatedItemSize || 1, MIN_ROW_HEIGHT)

      if (!context.virtualizationEnabled) {
        const nativeLimit = Math.max(0, context.meta.nativeScrollLimit ?? 0)
        if (!Number.isFinite(nativeLimit) || nativeLimit <= 0) {
          return 0
        }
        return clamp(value, 0, nativeLimit)
      }

      const visibleSpan = Math.max(1, context.visibleCount)
      const baseMax = Math.max(0, context.totalCount * rowHeight - context.viewportSize)
      const overscanPx = Math.max(0, context.overscanTrailing) * rowHeight
      const trailingGap = Math.max(0, context.viewportSize - visibleSpan * rowHeight)
      const virtualMax = Math.max(baseMax, baseMax + overscanPx + trailingGap + VIRTUAL_PADDING)

      if (context.meta.debug) {
        const debugLimit = context.meta.debugNativeScrollLimit
        if (typeof debugLimit === "number" && Number.isFinite(debugLimit)) {
          const tolerance = rowHeight + VIRTUAL_PADDING
          if (Math.abs(debugLimit - virtualMax) > tolerance) {
            return clamp(value, 0, Math.max(virtualMax, debugLimit))
          }
        }
      }

      if (!Number.isFinite(virtualMax) || virtualMax <= 0) {
        return 0
      }
      return clamp(value, 0, virtualMax)
    },
    computeRange(offset, context, target) {
      if (!context.virtualizationEnabled) {
        target.start = 0
        target.end = context.totalCount
        target.payload = undefined
        return target
      }

      const rowHeight = context.estimatedItemSize || 1
      const rawStart = Math.floor(offset / Math.max(rowHeight, 1))
      const desiredStart = rawStart - context.overscanLeading
      const maxPoolStart = Math.max(context.totalCount - context.poolSize, 0)
      const start = clamp(desiredStart, 0, maxPoolStart)
      const end = Math.min(context.totalCount, start + context.poolSize)

      target.start = start
      target.end = end
      target.payload = undefined
      return target
    },
    getOffsetForIndex(index, context) {
      const rowHeight = context.estimatedItemSize || 1
      const clampedIndex = clamp(index, 0, Math.max(context.totalCount - 1, 0))
      return clampedIndex * rowHeight
    },
  }
}

export function createVerticalAxisVirtualizer() {
  return createAxisVirtualizer<VerticalVirtualizerMeta, VerticalVirtualizerPayload>(
    "vertical",
    createVerticalAxisStrategy(),
    undefined,
  )
}
