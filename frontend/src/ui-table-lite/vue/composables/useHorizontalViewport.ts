import { computed, ref, shallowRef, watch, watchEffect } from "vue"
import type { ComputedRef, Ref } from "vue"
import type { UiTableColumn } from "../../core/types"
import {
  createEmptyColumnSnapshot,
  type ColumnMetric,
  type ColumnVirtualizationSnapshot,
} from "../../core/virtualization/columnSnapshot"
import { createHorizontalViewportController } from "../../core/viewport/horizontalViewportController"
import { SCROLL_EPSILON } from "../../core/utils/constants"
import { resolvePinMode } from "../core/virtualization/viewportConfig"
import type { ColumnPinMode } from "../core/virtualization/types"

export interface UseHorizontalViewportOptions {
  containerRef: Ref<HTMLDivElement | null>
  columns: Ref<UiTableColumn[]> | ComputedRef<UiTableColumn[]>
  zoom: Ref<number>
  viewportWidth: Ref<number>
}

export function useHorizontalViewport({ containerRef, columns, zoom, viewportWidth }: UseHorizontalViewportOptions) {
  const scrollLeft = ref(0)
  const pendingScrollLeft = ref<number | null>(null)
  const scrollFrame = ref<number | null>(null)
  const columnRef = "value" in columns ? columns : ref(columns)
  const columnSnapshot = shallowRef<ColumnVirtualizationSnapshot<UiTableColumn>>(
    createEmptyColumnSnapshot<UiTableColumn>(),
  )

  const controller = createHorizontalViewportController<UiTableColumn>({
    getViewportWidth: () => viewportWidth.value,
    getZoom: () => zoom.value,
    getColumnKey: column => column.key,
  resolvePinMode: resolvePinMode as (column: UiTableColumn) => ColumnPinMode,
    onSnapshot: snapshot => {
      columnSnapshot.value = {
        pinnedLeft: snapshot.pinnedLeft.map(entry => ({ ...entry })),
        pinnedRight: snapshot.pinnedRight.map(entry => ({ ...entry })),
        visibleScrollable: snapshot.visibleScrollable.map(entry => ({ ...entry })),
        visibleColumns: snapshot.visibleColumns.map(entry => ({ ...entry })),
        columnWidthMap: new Map(snapshot.columnWidthMap),
        leftPadding: snapshot.leftPadding,
        rightPadding: snapshot.rightPadding,
        totalScrollableWidth: snapshot.totalScrollableWidth,
        visibleScrollableWidth: snapshot.visibleScrollableWidth,
        scrollableStart: snapshot.scrollableStart,
        scrollableEnd: snapshot.scrollableEnd,
        visibleStart: snapshot.visibleStart,
        visibleEnd: snapshot.visibleEnd,
        pinnedLeftWidth: snapshot.pinnedLeftWidth,
        pinnedRightWidth: snapshot.pinnedRightWidth,
        metrics: {
          widths: [...snapshot.metrics.widths],
          offsets: [...snapshot.metrics.offsets],
          totalWidth: snapshot.metrics.totalWidth,
        },
        containerWidthForColumns: snapshot.containerWidthForColumns,
        indexColumnWidth: snapshot.indexColumnWidth,
        scrollDirection: snapshot.scrollDirection,
  } as ColumnVirtualizationSnapshot<UiTableColumn>
    },
    onScrollApplied: payload => {
      const container = containerRef.value
      if (container && Math.abs(container.scrollLeft - payload.appliedScrollLeft) > SCROLL_EPSILON) {
        container.scrollLeft = payload.appliedScrollLeft
      }
      scrollLeft.value = container?.scrollLeft ?? payload.appliedScrollLeft
    },
  })

  const effectiveViewportWidth = ref(controller.state.effectiveViewportWidth)

  function syncState() {
    effectiveViewportWidth.value = controller.state.effectiveViewportWidth
  }

  function updateControllerMetrics(): boolean {
    const container = containerRef.value
    if (!container) return false

    controller.updateMetrics({
      metrics: {
        containerWidth: container.clientWidth,
        scrollLeft: container.scrollLeft,
        scrollWidth: container.scrollWidth,
      },
      columns: columnRef.value ?? [],
    })

    scrollLeft.value = container.scrollLeft
    syncState()
    return true
  }

  function runVirtualizer(scrollOverride?: number) {
    controller.runVirtualizer(scrollOverride)
    syncState()
  }

  function clampScrollLeftValue(value: number) {
    if (!Number.isFinite(value)) return 0
    if (!updateControllerMetrics()) {
      return Math.max(0, value)
    }
    return controller.clampScrollLeftValue(value)
  }

  function handleHorizontalScroll(event: Event) {
    const target = event.target as HTMLElement | null
    if (!target) return

    if (!updateControllerMetrics()) return

    const nextLeft = controller.clampScrollLeftValue(target.scrollLeft)
    pendingScrollLeft.value = nextLeft

    if (scrollFrame.value !== null) return

    scrollFrame.value = requestAnimationFrame(() => {
      scrollFrame.value = null
      if (pendingScrollLeft.value != null) {
        const value = pendingScrollLeft.value
        pendingScrollLeft.value = null
        controller.runVirtualizer(value)
        syncState()
      }
    })
  }

  function cancelScrollFrame() {
    if (scrollFrame.value !== null) {
      cancelAnimationFrame(scrollFrame.value)
      scrollFrame.value = null
    }
  }

  function scrollToColumn(key: string) {
    controller.scrollToColumn(key)
    syncState()
  }

  watchEffect(() => {
    const container = containerRef.value
    if (!container) return
    if (!updateControllerMetrics()) return
    controller.runVirtualizer()
    syncState()
  })

  watch(
    () => viewportWidth.value,
    () => {
      if (!updateControllerMetrics()) return
      runVirtualizer()
    }
  )

  watch(
    () => columnRef.value,
    () => {
      if (!updateControllerMetrics()) return
      runVirtualizer()
    }
  )

  watch(
    () => zoom.value,
    () => {
      if (!updateControllerMetrics()) return
      runVirtualizer()
    }
  )

  if (updateControllerMetrics()) {
    runVirtualizer()
  }

  return {
    scrollLeft,
    effectiveViewportWidth: computed(() => effectiveViewportWidth.value),
    columnVirtualization: computed(() => columnSnapshot.value),
    clampScrollLeftValue,
    handleHorizontalScroll,
    scrollToColumn,
    cancelScrollFrame,
    pinnedLeftColumns: computed(() =>
      columnSnapshot.value.pinnedLeft.map((entry: ColumnMetric<UiTableColumn>) => entry.column),
    ),
    pinnedLeftEntries: computed(() => columnSnapshot.value.pinnedLeft),
    pinnedRightColumns: computed(() =>
      columnSnapshot.value.pinnedRight.map((entry: ColumnMetric<UiTableColumn>) => entry.column),
    ),
    pinnedRightEntries: computed(() => columnSnapshot.value.pinnedRight),
    visibleColumns: computed(() =>
      columnSnapshot.value.visibleColumns.map((entry: ColumnMetric<UiTableColumn>) => entry.column),
    ),
    visibleColumnEntries: computed(() => columnSnapshot.value.visibleColumns),
    visibleScrollableColumns: computed(() =>
      columnSnapshot.value.visibleScrollable.map((entry: ColumnMetric<UiTableColumn>) => entry.column),
    ),
    visibleScrollableEntries: computed(() => columnSnapshot.value.visibleScrollable),
    leftPadding: computed(() => columnSnapshot.value.leftPadding),
    rightPadding: computed(() => columnSnapshot.value.rightPadding),
    columnWidthMap: computed(() => columnSnapshot.value.columnWidthMap),
    visibleStartCol: computed(() => columnSnapshot.value.visibleStart),
    visibleEndCol: computed(() => columnSnapshot.value.visibleEnd),
    scrollableRange: computed(() => ({
      start: columnSnapshot.value.scrollableStart,
      end: columnSnapshot.value.scrollableEnd,
    })),
  }
}
