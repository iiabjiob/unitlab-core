import { computed, onBeforeUnmount, onMounted, ref, shallowRef, watch, watchEffect } from "vue"
import type { ComputedRef, Ref } from "vue"
import type { VisibleRow } from "../../core/types"
import { SCROLL_EPSILON } from "../../core/utils/constants"
import {
  createVerticalViewportController,
  type VerticalViewportOptions,
} from "../../core/viewport/verticalViewportController"

type RowHeightModeInput = Ref<"fixed" | "auto"> | "fixed" | "auto" | undefined

function resolveRowHeightModeValue(mode: RowHeightModeInput): "fixed" | "auto" {
  if (mode && typeof mode === "object") {
    const candidate = (mode as Ref<"fixed" | "auto">).value
    return (candidate ?? "fixed") as "fixed" | "auto"
  }
  return (mode ?? "fixed") as "fixed" | "auto"
}

export interface UseVerticalViewportOptions {
  containerRef: Ref<HTMLDivElement | null>
  headerRef?: Ref<HTMLElement | null>
  processedRows: ComputedRef<VisibleRow[]>
  zoom: Ref<number>
  rowHeightMode?: Ref<"fixed" | "auto"> | "fixed" | "auto"
  overscan?: number
}

interface ScrollFrame {
  id: number | null
}

export function useVerticalViewport({
  containerRef,
  headerRef,
  processedRows,
  zoom,
  rowHeightMode = "fixed",
  overscan,
}: UseVerticalViewportOptions) {
  const scrollTop = ref(0)
  const pendingScrollTop = ref<number | null>(null)
  const scrollFrame: ScrollFrame = { id: null }
  const resizeObserverRef = ref<ResizeObserver | null>(null)

  const totalRowCount = computed(() => processedRows.value.length)
  const virtualizationEnabled = computed(() => resolveRowHeightModeValue(rowHeightMode) === "fixed")

  const visibleRowsPool = shallowRef<VisibleRow[]>([])
  const visibleRows = computed(() => visibleRowsPool.value)

  const controllerOptions: VerticalViewportOptions = {
    overscan,
    rowHeightMode: resolveRowHeightModeValue(rowHeightMode),
    getRowCount: () => totalRowCount.value,
    getZoom: () => zoom.value,
    onVisibleRows: payload => {
      const withDisplayIndex = payload.pool.map((entry, offset) => ({
        ...entry,
        displayIndex: payload.startIndex + offset,
      })) as VisibleRow[]
      visibleRowsPool.value = withDisplayIndex
    },
    onScrollApplied: payload => {
      const container = containerRef.value
      if (container && Math.abs(container.scrollTop - payload.appliedScrollTop) > SCROLL_EPSILON) {
        container.scrollTop = payload.appliedScrollTop
      }
      scrollTop.value = container?.scrollTop ?? payload.appliedScrollTop
    },
  }

  const controller = createVerticalViewportController(controllerOptions)
  const { state } = controller

  const effectiveRowHeight = ref(state.effectiveRowHeight)
  const totalContentHeight = ref(state.totalContentHeight)
  const viewportHeight = ref(state.viewportHeight)
  const viewportWidth = ref(state.viewportWidth)
  const startIndex = ref(state.virtualizerState.startIndex)
  const endIndex = ref(state.virtualizerState.endIndex)
  const visibleCount = ref(state.virtualizerState.visibleCount)
  const poolSize = ref(state.virtualizerState.poolSize)

  function syncControllerState() {
    effectiveRowHeight.value = state.effectiveRowHeight
    totalContentHeight.value = state.totalContentHeight
    viewportHeight.value = state.viewportHeight
    viewportWidth.value = state.viewportWidth
    startIndex.value = state.virtualizerState.startIndex
    endIndex.value = state.virtualizerState.endIndex
    visibleCount.value = state.virtualizerState.visibleCount
    poolSize.value = state.virtualizerState.poolSize
  }

  function refreshControllerOptions() {
    controllerOptions.rowHeightMode = resolveRowHeightModeValue(rowHeightMode)
  }

  function updateControllerMetrics(): boolean {
    const container = containerRef.value
    if (!container) return false

    refreshControllerOptions()

    const headerHeight = headerRef?.value?.offsetHeight ?? 0
    controller.updateMetrics({
      metrics: {
        containerHeight: container.clientHeight,
        containerWidth: container.clientWidth,
        headerHeight,
        scrollTop: container.scrollTop,
        scrollHeight: container.scrollHeight,
      },
      rows: processedRows.value,
    })

    scrollTop.value = container.scrollTop
    syncControllerState()
    return true
  }

  function runVirtualizer(scrollOverride?: number) {
    if (!updateControllerMetrics()) return
    controller.runVirtualizer(scrollOverride)
    syncControllerState()
  }

  function updateViewportMetrics() {
    runVirtualizer()
  }

  function updateVisibleRange(force = false) {
    if (force) {
      runVirtualizer(scrollTop.value)
      return
    }
    runVirtualizer()
  }

  function handleVerticalScroll(event: Event) {
    const target = event.target as HTMLElement | null
    if (!target) return
    if (!updateControllerMetrics()) return

    const nextTop = target.scrollTop

    pendingScrollTop.value = nextTop

    if (scrollFrame.id !== null) return

    scrollFrame.id = requestAnimationFrame(() => {
      scrollFrame.id = null
      if (pendingScrollTop.value == null) return
      const value = pendingScrollTop.value
      pendingScrollTop.value = null
      runVirtualizer(value)
    })
  }

  function measureRowHeight() {
    runVirtualizer()
  }

  function cancelScrollFrame() {
    if (scrollFrame.id !== null) {
      cancelAnimationFrame(scrollFrame.id)
      scrollFrame.id = null
    }
  }

  function clampScrollTopValue(value: number) {
    if (!Number.isFinite(value)) return 0
    if (!updateControllerMetrics()) {
      return Math.max(0, value)
    }

    if (!virtualizationEnabled.value) {
      const container = containerRef.value
      if (!container) return Math.max(0, value)
      const maxScroll = Math.max(0, container.scrollHeight - container.clientHeight)
      return Math.min(Math.max(value, 0), maxScroll)
    }

    return controller.clampScrollTopValue(value)
  }

  function scrollToRow(index: number) {
    if (!updateControllerMetrics()) return
    controller.scrollToRow(index)
    syncControllerState()
  }

  function isRowVisible(index: number) {
    return controller.isRowVisible(index)
  }

  onMounted(() => {
    runVirtualizer()
  })

  watchEffect(() => {
    const container = containerRef.value
    if (!container) return
    if (resizeObserverRef.value) {
      resizeObserverRef.value.disconnect()
      resizeObserverRef.value = null
    }
    if (typeof ResizeObserver !== "undefined") {
      const observer = new ResizeObserver(() => updateViewportMetrics())
      observer.observe(container)
      if (headerRef?.value) {
        observer.observe(headerRef.value)
      }
      resizeObserverRef.value = observer
    }
  })

  watch(
    () => [totalRowCount.value, virtualizationEnabled.value],
    () => {
      runVirtualizer()
    },
    { immediate: true }
  )

  watch(
    () => zoom.value,
    () => {
      runVirtualizer()
    }
  )

  watch(
    () => processedRows.value,
    () => {
      runVirtualizer()
    }
  )

  onBeforeUnmount(() => {
    resizeObserverRef.value?.disconnect()
    resizeObserverRef.value = null
    cancelScrollFrame()
  })

  return {
    scrollTop,
    viewportHeight,
    viewportWidth,
    totalRowCount,
    effectiveRowHeight,
    visibleCount,
    poolSize,
    totalContentHeight,
    virtualizationEnabled,
    startIndex,
    endIndex,
    visibleRows,
    visibleRowsPool,
    clampScrollTopValue,
    updateVisibleRange,
    handleVerticalScroll,
    updateViewportMetrics,
    measureRowHeight,
    cancelScrollFrame,
    scrollToRow,
    isRowVisible,
  }
}
