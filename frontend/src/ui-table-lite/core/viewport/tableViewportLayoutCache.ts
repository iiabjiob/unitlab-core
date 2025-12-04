import type { TableViewportContainerMetrics, TableViewportHeaderMetrics } from "./viewportHostEnvironment"

export interface LayoutViewportRect {
  top: number
  left: number
  width: number
  height: number
}

export interface LayoutMeasurementSnapshot {
  version: number
  containerWidth: number
  containerHeight: number
  headerHeight: number
  scrollWidth: number
  scrollHeight: number
  scrollTop: number
  scrollLeft: number
  contentWidth: number
  contentHeight: number
  viewportRect: LayoutViewportRect
}

export interface LayoutMeasurementCache {
  snapshot(): LayoutMeasurementSnapshot
  updateContainer(metrics: TableViewportContainerMetrics | null, rect: DOMRect | null): void
  updateHeader(metrics: TableViewportHeaderMetrics | null): void
  updateContentDimensions(width: number, height: number): void
  reset(): void
}

function createEmptySnapshot(): LayoutMeasurementSnapshot {
  return {
    version: 0,
    containerWidth: 0,
    containerHeight: 0,
    headerHeight: 0,
    scrollWidth: 0,
    scrollHeight: 0,
    scrollTop: 0,
    scrollLeft: 0,
    contentWidth: 0,
    contentHeight: 0,
    viewportRect: { top: 0, left: 0, width: 0, height: 0 },
  }
}

export function createLayoutMeasurementCache(): LayoutMeasurementCache {
  let snapshot = createEmptySnapshot()

  function bumpVersion() {
    snapshot = {
      ...snapshot,
      version: snapshot.version + 1,
    }
  }

  return {
    snapshot() {
      const { viewportRect, ...rest } = snapshot
      return {
        ...rest,
        viewportRect: { ...viewportRect },
      }
    },
    updateContainer(metrics, rect) {
      if (!metrics && !rect) {
        return
      }
      let changed = false
      if (metrics) {
        const {
          clientWidth,
          clientHeight,
          scrollWidth,
          scrollHeight,
          scrollTop,
          scrollLeft,
        } = metrics
        if (
          clientWidth !== snapshot.containerWidth ||
          clientHeight !== snapshot.containerHeight ||
          scrollWidth !== snapshot.scrollWidth ||
          scrollHeight !== snapshot.scrollHeight ||
          scrollTop !== snapshot.scrollTop ||
          scrollLeft !== snapshot.scrollLeft
        ) {
          snapshot = {
            ...snapshot,
            containerWidth: clientWidth,
            containerHeight: clientHeight,
            scrollWidth,
            scrollHeight,
            scrollTop,
            scrollLeft,
          }
          changed = true
        }
      }
      if (rect) {
        const nextRect: LayoutViewportRect = {
          top: rect.top,
          left: rect.left,
          width: rect.width,
          height: rect.height,
        }
        const prevRect = snapshot.viewportRect
        if (
          nextRect.top !== prevRect.top ||
          nextRect.left !== prevRect.left ||
          nextRect.width !== prevRect.width ||
          nextRect.height !== prevRect.height
        ) {
          snapshot = {
            ...snapshot,
            viewportRect: nextRect,
          }
          changed = true
        }
      }
      if (changed) {
        bumpVersion()
      }
    },
    updateHeader(metrics) {
      if (!metrics) {
        return
      }
      if (metrics.height !== snapshot.headerHeight) {
        snapshot = {
          ...snapshot,
          headerHeight: metrics.height,
        }
        bumpVersion()
      }
    },
    updateContentDimensions(width, height) {
      const normalizedWidth = Number.isFinite(width) && width >= 0 ? width : 0
      const normalizedHeight = Number.isFinite(height) && height >= 0 ? height : 0
      if (
        normalizedWidth !== snapshot.contentWidth ||
        normalizedHeight !== snapshot.contentHeight
      ) {
        snapshot = {
          ...snapshot,
          contentWidth: normalizedWidth,
          contentHeight: normalizedHeight,
        }
        bumpVersion()
      }
    },
    reset() {
      snapshot = createEmptySnapshot()
    },
  }
}
