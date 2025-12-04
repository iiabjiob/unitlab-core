/**
 * Lightweight pointer hover delegation for UiTable.
 */
import type { HoverDelegationEnvironment } from "./domAdapters"

export interface HoverDelegationMetrics {
  scrollTop: number
  scrollLeft: number
  rowHeight: number
  totalRows: number
  pinnedLeftWidth: number
  scrollableWidth: number
  pinnedRightWidth: number
  viewportWidth: number
  columnRects: Array<{ index: number; left: number; width: number }>
}

export interface HoverDelegationHandle {
  updateMetrics(metrics: HoverDelegationMetrics | null): void
  destroy(): void
}

export type HoverDelegationCallback = (rowIndex: number | null, columnIndex: number | null) => void

export function createHoverDelegation(
  env: HoverDelegationEnvironment,
  container: HTMLElement,
  callback: HoverDelegationCallback,
): HoverDelegationHandle {
  let metrics: HoverDelegationMetrics | null = null
  let rafId = 0
  let pointerX = 0
  let pointerY = 0
  let pointerActive = false
  let lastRow: number | null = null
  let lastCol: number | null = null
  let pendingMeasurement: Promise<DOMRect | null> | null = null

  function cancelMeasurement() {
    pendingMeasurement = null
  }

  function emitHover(row: number | null, col: number | null) {
    if (row === lastRow && col === lastCol) {
      return
    }
    lastRow = row
    lastCol = col
    callback(row, col)
  }

  function resolveHover() {
    rafId = 0
    if (!metrics || !pointerActive) {
      cancelMeasurement()
      return
    }

    cancelMeasurement()
    const measurement = env.measurement.measure(() => {
      if (!container.isConnected) {
        return null
      }
      return container.getBoundingClientRect()
    })

    pendingMeasurement = measurement
    void measurement
      .then(rect => {
        if (pendingMeasurement !== measurement) {
          return
        }
        pendingMeasurement = null

        const currentMetrics = metrics
        if (!rect || !currentMetrics || !pointerActive) {
          emitHover(null, null)
          return
        }

        const localX = pointerX - rect.left
        const localY = pointerY - rect.top

        if (localX < 0 || localY < 0 || localX > rect.width || localY > rect.height) {
          emitHover(null, null)
          return
        }

        let rowIndex: number | null = null
        if (currentMetrics.rowHeight > 0 && currentMetrics.totalRows > 0) {
          const absoluteY = localY + currentMetrics.scrollTop
          const candidate = Math.floor(absoluteY / currentMetrics.rowHeight)
          if (candidate >= 0 && candidate < currentMetrics.totalRows) {
            rowIndex = candidate
          }
        }

        const tableX = mapPointerToTableSpace(localX, currentMetrics)
        if (tableX == null) {
          emitHover(null, null)
          return
        }

        let columnIndex: number | null = null
        if (currentMetrics.columnRects.length > 0) {
          for (const rectEntry of currentMetrics.columnRects) {
            if (tableX >= rectEntry.left && tableX < rectEntry.left + rectEntry.width) {
              columnIndex = rectEntry.index
              break
            }
          }
        }

        if (rowIndex == null || columnIndex == null) {
          emitHover(null, null)
          return
        }

        emitHover(rowIndex, columnIndex)
      })
      .catch(() => {
        if (pendingMeasurement === measurement) {
          pendingMeasurement = null
        }
      })
  }

  function scheduleResolve() {
    if (rafId === 0) {
      rafId = env.frame.schedule(resolveHover)
    }
  }

  function handlePointerMove(event: PointerEvent) {
    pointerX = event.clientX
    pointerY = event.clientY
    pointerActive = true
    scheduleResolve()
  }

  function resetHover() {
    pointerActive = false
    cancelMeasurement()
    if (rafId !== 0) {
      env.frame.cancel(rafId)
      rafId = 0
    }
    emitHover(null, null)
  }

  container.addEventListener("pointermove", handlePointerMove, { passive: true })
  container.addEventListener("pointerdown", handlePointerMove, { passive: true })
  container.addEventListener("pointerleave", resetHover, { passive: true })
  container.addEventListener("pointercancel", resetHover, { passive: true })

  return {
    updateMetrics(next: HoverDelegationMetrics | null) {
      metrics = next
      if (pointerActive && metrics) {
        scheduleResolve()
      } else if (!metrics) {
        resetHover()
      }
    },
    destroy() {
      container.removeEventListener("pointermove", handlePointerMove)
      container.removeEventListener("pointerdown", handlePointerMove)
      container.removeEventListener("pointerleave", resetHover)
      container.removeEventListener("pointercancel", resetHover)
      cancelMeasurement()
      if (rafId !== 0) {
        env.frame.cancel(rafId)
        rafId = 0
      }
      metrics = null
      lastRow = null
      lastCol = null
    },
  }
}

function mapPointerToTableSpace(localX: number, metrics: HoverDelegationMetrics): number | null {
  if (!Number.isFinite(localX)) {
    return null
  }
  const viewportWidth = Math.max(0, metrics.viewportWidth)
  if (localX < 0 || localX > viewportWidth) {
    return null
  }

  const pinnedLeftWidth = Math.max(0, metrics.pinnedLeftWidth)
  const pinnedRightWidth = Math.max(0, metrics.pinnedRightWidth)
  const scrollableStart = pinnedLeftWidth
  const scrollableEnd = Math.max(scrollableStart, viewportWidth - pinnedRightWidth)

  if (localX <= pinnedLeftWidth) {
    return localX
  }

  if (pinnedRightWidth > 0 && localX >= scrollableEnd) {
    const offsetWithinPinnedRight = localX - scrollableEnd
    return pinnedLeftWidth + metrics.scrollableWidth + offsetWithinPinnedRight
  }

  const offsetWithinScrollable = localX - pinnedLeftWidth
  return pinnedLeftWidth + metrics.scrollLeft + offsetWithinScrollable
}
