import { computed, onBeforeUnmount, ref, shallowRef, unref, watch, type Ref, type ShallowRef } from "vue"

import { scheduleMeasurement, type MeasurementHandle } from "../../core/runtime/measurementQueue"

import type { ViewportMetricsSnapshot } from "./useTableViewport"

interface UseTableViewportMetricsOptions {
  containerRef: Ref<HTMLDivElement | null>
  headerRef: Ref<HTMLElement | null>
  fallbackHeaderHeight?: Ref<number> | number
}

function clampDomMeasure(value: number | undefined | null, fallback: number): number {
  return typeof value === "number" && Number.isFinite(value) ? value : fallback
}

export function useTableViewportMetrics(options: UseTableViewportMetricsOptions) {
  const fallbackHeaderHeightRef = computed(() => {
    const raw = unref(options.fallbackHeaderHeight)
    return typeof raw === "number" && Number.isFinite(raw) && raw > 0 ? raw : 0
  })

  const headerHeight = ref(fallbackHeaderHeightRef.value)
  const viewportMetrics: ShallowRef<ViewportMetricsSnapshot> = shallowRef({
    containerWidth: 0,
    containerHeight: 0,
    headerHeight: fallbackHeaderHeightRef.value,
  })

  let headerResizeObserver: ResizeObserver | null = null
  let containerResizeObserver: ResizeObserver | null = null
  let headerMeasurementHandle: MeasurementHandle<number> | null = null
  let containerMeasurementHandle: MeasurementHandle<{ width: number; height: number }> | null = null
  const cancelHeaderMeasurement = () => {
    headerMeasurementHandle?.cancel()
    headerMeasurementHandle = null
  }

  const cancelContainerMeasurement = () => {
    containerMeasurementHandle?.cancel()
    containerMeasurementHandle = null
  }

  const requestHeaderMeasurement = (element: HTMLElement) => {
    cancelHeaderMeasurement()
    const current = element
    const handle = scheduleMeasurement(() => {
      const rect = current.getBoundingClientRect()
      const fallback = fallbackHeaderHeightRef.value || current.offsetHeight || 0
      return clampDomMeasure(rect.height, fallback)
    })
    headerMeasurementHandle = handle
    void handle.promise
      .then(value => {
        if (headerMeasurementHandle !== handle) return
        if (options.headerRef.value !== current) return
        headerMeasurementHandle = null
        applyHeaderHeight(value)
      })
      .catch(() => {
        if (headerMeasurementHandle === handle) {
          headerMeasurementHandle = null
        }
      })
  }

  const requestContainerMeasurement = (container: HTMLDivElement) => {
    cancelContainerMeasurement()
    const current = container
    const handle = scheduleMeasurement(() => {
      const rect = current.getBoundingClientRect()
      return {
        width: clampDomMeasure(current.clientWidth, rect.width),
        height: clampDomMeasure(current.clientHeight, rect.height),
      }
    })
    containerMeasurementHandle = handle
    void handle.promise
      .then(result => {
        if (containerMeasurementHandle !== handle) return
        if (options.containerRef.value !== current) return
        containerMeasurementHandle = null
        updateViewportMetrics({
          containerWidth: result.width,
          containerHeight: result.height,
        })
      })
      .catch(() => {
        if (containerMeasurementHandle === handle) {
          containerMeasurementHandle = null
        }
      })
  }

  const updateViewportMetrics = (partial: Partial<ViewportMetricsSnapshot>) => {
    const current = viewportMetrics.value
    const next: ViewportMetricsSnapshot = {
      containerWidth: partial.containerWidth ?? current.containerWidth,
      containerHeight: partial.containerHeight ?? current.containerHeight,
      headerHeight: partial.headerHeight ?? current.headerHeight,
    }
    if (
      next.containerWidth !== current.containerWidth ||
      next.containerHeight !== current.containerHeight ||
      next.headerHeight !== current.headerHeight
    ) {
      viewportMetrics.value = next
    }
  }

  const applyHeaderHeight = (value: number) => {
    const fallback = fallbackHeaderHeightRef.value
    const normalized = clampDomMeasure(value, fallback || 0)
    const resolved = fallback > 0 ? Math.max(normalized, fallback) : normalized
    headerHeight.value = resolved
    const container = options.containerRef.value
    if (container) {
      container.style.setProperty("--ui-table-header-height", `${Math.round(resolved)}px`)
    }
    updateViewportMetrics({ headerHeight: resolved })
  }

  watch(
    fallbackHeaderHeightRef,
    fallback => {
      if (fallback <= 0) {
        return
      }
      if (headerHeight.value < fallback) {
        applyHeaderHeight(Math.max(headerHeight.value, fallback))
      } else {
        const container = options.containerRef.value
        if (container) {
          container.style.setProperty("--ui-table-header-height", `${Math.round(headerHeight.value)}px`)
        }
        updateViewportMetrics({ headerHeight: headerHeight.value })
      }
    },
    { immediate: fallbackHeaderHeightRef.value > 0 },
  )

  watch(
    () => options.headerRef.value,
    element => {
      headerResizeObserver?.disconnect()
      headerResizeObserver = null
      cancelHeaderMeasurement()
      if (!element) return

      requestHeaderMeasurement(element)
      if (typeof ResizeObserver !== "undefined") {
        headerResizeObserver = new ResizeObserver(entries => {
          const entry = entries[0]
          if (entry && typeof entry.contentRect?.height === "number") {
            applyHeaderHeight(entry.contentRect.height)
            return
          }
          requestHeaderMeasurement(element)
        })
        headerResizeObserver.observe(element)
      }
    },
    { immediate: true },
  )

  watch(
    () => options.containerRef.value,
    container => {
      containerResizeObserver?.disconnect()
      containerResizeObserver = null
      cancelContainerMeasurement()
      if (!container) return

      const fallback = fallbackHeaderHeightRef.value
      const currentHeader = headerHeight.value > 0 ? headerHeight.value : fallback
      if (currentHeader > 0) {
        container.style.setProperty("--ui-table-header-height", `${Math.round(currentHeader)}px`)
        updateViewportMetrics({ headerHeight: currentHeader })
      }
      requestContainerMeasurement(container)

      if (typeof ResizeObserver === "undefined") {
        return
      }

      containerResizeObserver = new ResizeObserver(entries => {
        const entry = entries[0]
        const measuredWidth = clampDomMeasure(entry?.contentRect?.width, container.clientWidth)
        const measuredHeight = clampDomMeasure(entry?.contentRect?.height, container.clientHeight)
        updateViewportMetrics({
          containerWidth: measuredWidth,
          containerHeight: measuredHeight,
        })
      })
      containerResizeObserver.observe(container)
    },
    { immediate: true },
  )

  onBeforeUnmount(() => {
    headerResizeObserver?.disconnect()
    containerResizeObserver?.disconnect()
    headerResizeObserver = null
    containerResizeObserver = null
    cancelHeaderMeasurement()
    cancelContainerMeasurement()
  })

  return {
    headerHeight,
    viewportMetrics,
  }
}
