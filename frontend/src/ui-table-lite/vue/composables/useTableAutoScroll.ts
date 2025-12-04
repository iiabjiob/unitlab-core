import { onBeforeUnmount, ref } from "vue"
import type { Ref } from "vue"
import { createAutoScrollController, type PointerCoordinates } from "@/ui-table/core/selection/autoScroll"
import { AUTO_SCROLL_MAX_SPEED, AUTO_SCROLL_MIN_SPEED, AUTO_SCROLL_THRESHOLD } from "../../core/utils/constants"
import { scheduleMeasurement, type MeasurementHandle } from "../../core/runtime/measurementQueue"

// Manages pointer-driven auto scrolling for UiTable drag interactions.

interface UseTableAutoScrollOptions {
  containerRef: Ref<HTMLDivElement | null>
  onFrame?: (event: { lastPointer: PointerCoordinates | null }) => void
}

/**
 * Provides helpers to start, maintain, and stop auto scrolling while the user drags across the grid.
 */
export function useTableAutoScroll({ containerRef, onFrame }: UseTableAutoScrollOptions) {
  const WINDOW_SCROLL_OPTIONS: AddEventListenerOptions = { passive: true, capture: true }
  const WINDOW_RESIZE_OPTIONS: AddEventListenerOptions = { passive: true }
  const lastPointer = ref<PointerCoordinates | null>(null)
  type RectSnapshot = { left: number; top: number; right: number; bottom: number }

  let cachedContainerRect: RectSnapshot | null = null
  let rectDirty = true
  let pendingRectMeasurement: MeasurementHandle<RectSnapshot | null> | null = null
  let rectListenerCleanup: (() => void) | null = null
  let listenersBoundTo: HTMLElement | null = null

  const controller = createAutoScrollController({
    threshold: AUTO_SCROLL_THRESHOLD,
    minSpeed: AUTO_SCROLL_MIN_SPEED,
    maxSpeed: AUTO_SCROLL_MAX_SPEED,
    adapter: {
      getViewportRect: () => {
        const container = containerRef.value
        if (!container) return null
        ensureRectMeasurement(container)
        const rect = cachedContainerRect
        if (!rect) {
          return null
        }
        return { ...rect }
      },
      scrollBy: (deltaX, deltaY) => {
        const container = containerRef.value
        if (!container) return
        if (deltaX) {
          container.scrollLeft += deltaX
        }
        if (deltaY) {
          container.scrollTop += deltaY
        }
      },
    },
    onStep: () => {
      onFrame?.({ lastPointer: lastPointer.value })
    },
  })

  function cancelRectMeasurement() {
    if (pendingRectMeasurement) {
      pendingRectMeasurement.cancel()
      pendingRectMeasurement = null
    }
  }

  function scheduleRectMeasurement(container: HTMLElement) {
    cancelRectMeasurement()
    const handle = scheduleMeasurement<RectSnapshot | null>(() => {
      if (!container.isConnected) {
        return null
      }
      const rect = container.getBoundingClientRect()
      return {
        left: rect.left,
        top: rect.top,
        right: rect.right,
        bottom: rect.bottom,
      }
    })

    pendingRectMeasurement = handle
    void handle.promise
      .then(result => {
        if (pendingRectMeasurement !== handle) {
          return
        }
        pendingRectMeasurement = null
        if (!result) {
          rectDirty = true
          return
        }
        cachedContainerRect = result
        rectDirty = false
      })
      .catch(() => {
        if (pendingRectMeasurement === handle) {
          pendingRectMeasurement = null
        }
        rectDirty = true
      })
  }

  function ensureRectMeasurement(container: HTMLElement) {
    if (!rectDirty && cachedContainerRect) {
      return
    }
    if (pendingRectMeasurement) {
      return
    }
    scheduleRectMeasurement(container)
  }

  function invalidateCachedRect() {
    rectDirty = true
    const container = listenersBoundTo ?? containerRef.value
    if (container) {
      ensureRectMeasurement(container)
    }
  }

  function detachRectListeners() {
    rectListenerCleanup?.()
    rectListenerCleanup = null
    listenersBoundTo = null
    cancelRectMeasurement()
    cachedContainerRect = null
    rectDirty = true
  }

  function ensureRectListeners(container: HTMLElement) {
    if (listenersBoundTo === container && rectListenerCleanup) {
      return
    }

    detachRectListeners()
    invalidateCachedRect()

    const handleWindowResize = () => invalidateCachedRect()
    const handleWindowScroll = () => invalidateCachedRect()
    const handleContainerScroll = () => invalidateCachedRect()

    if (typeof window !== "undefined") {
      window.addEventListener("resize", handleWindowResize, WINDOW_RESIZE_OPTIONS)
      window.addEventListener("scroll", handleWindowScroll, WINDOW_SCROLL_OPTIONS)
    }
    container.addEventListener("scroll", handleContainerScroll, { passive: true })

    listenersBoundTo = container
    rectListenerCleanup = () => {
      if (typeof window !== "undefined") {
        window.removeEventListener("resize", handleWindowResize, WINDOW_RESIZE_OPTIONS)
        window.removeEventListener("scroll", handleWindowScroll, WINDOW_SCROLL_OPTIONS)
      }
      container.removeEventListener("scroll", handleContainerScroll)
    }
  }

  function updateAutoScroll(pointer: PointerCoordinates) {
    lastPointer.value = pointer
    const container = containerRef.value
    if (!container) return
    ensureRectListeners(container)
    ensureRectMeasurement(container)
    controller.update({ clientX: pointer.clientX, clientY: pointer.clientY })
  }

  /**
   * Cancels the RAF loop and clears any stored pointer state.
   */
  function stopAutoScroll() {
    controller.stop()
    lastPointer.value = null
    cachedContainerRect = null
    rectDirty = true
    cancelRectMeasurement()
    detachRectListeners()
  }

  onBeforeUnmount(() => {
    controller.stop()
    cancelRectMeasurement()
    detachRectListeners()
  })

  return {
    updateAutoScroll,
    stopAutoScroll,
    lastPointer,
  }
}
