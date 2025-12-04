import {
  computed,
  onBeforeUnmount,
  shallowRef,
  triggerRef,
  watch,
  watchEffect,
  type ComputedRef,
  type CSSProperties,
  type Ref,
} from "vue"
import {
  createHoverDelegation,
  type HoverDelegationHandle,
  type HoverDelegationMetrics,
} from "../../core/dom/hoverDelegation"
import {
  buildTableSpaceColumnLayout,
  type TableSpaceColumnLayout,
} from "../../core/dom/gridUtils"
import { createHoverDelegationEnvironment } from "../adapters/hoverEnvironment"
import type { UiTableColumn, VisibleRow } from "../../core/types"
import type { HeaderRenderableEntry } from "../../core/types/internal"
import type { UiTableColumnBinding } from "../context"
import type { TableOverlayScrollEmitter, TableOverlayScrollSnapshot } from "./useTableOverlayScrollState"

interface HoverColumnRect {
  index: number
  key: string
  left: number
  width: number
  pin: "left" | "right" | "none"
}

interface UseTableHoverOverlayOptions {
  containerRef: Ref<HTMLDivElement | null>
  pointerContainerRef?: Ref<HTMLElement | null>
  headerRef: Ref<HTMLElement | null>
  isHoverableTable: ComputedRef<boolean>
  headerRenderableEntries: ComputedRef<HeaderRenderableEntry[]>
  bodyColumnBindings: ComputedRef<Map<string, UiTableColumnBinding>>
  columnWidthDomMap: ComputedRef<Map<string, number>>
  leftPaddingDom: ComputedRef<number>
  rightPaddingDom: ComputedRef<number>
  viewportWidth: Ref<number>
  viewportHeight: Ref<number>
  scrollTop: Ref<number>
  scrollLeft: Ref<number>
  rowHeightDom: ComputedRef<number>
  processedRows: ComputedRef<VisibleRow[]>
  totalRowCount: Ref<number>
  stickyBottomOffsets: ComputedRef<Map<number, number>>
  toDomUnits: (value: number) => number
  getStickyTopOffset: (row: any) => number | undefined
  columnIndexToKey: ComputedRef<Map<number, string>>
  overlayScrollState?: TableOverlayScrollEmitter
  scheduleOverlayUpdate?: () => void
}

export interface UseTableHoverOverlayResult {
  hoverOverlayStyle: ComputedRef<CSSProperties | null>
  hoverOverlayVisible: ComputedRef<boolean>
}

function resolveFallbackWidth(column: UiTableColumn, toDomUnits: (value: number) => number): number {
  const candidates = [column.width, column.minWidth, column.maxWidth]
  for (const candidate of candidates) {
    if (typeof candidate === "number" && Number.isFinite(candidate) && candidate > 0) {
      return toDomUnits(candidate)
    }
  }
  return 0
}

export function useTableHoverOverlay(options: UseTableHoverOverlayOptions): UseTableHoverOverlayResult {
  const hoverEnvironment = createHoverDelegationEnvironment()

  const overlayScrollSnapshot = shallowRef<TableOverlayScrollSnapshot | null>(null)

  if (options.overlayScrollState) {
    const unsubscribe = options.overlayScrollState.subscribe(snapshot => {
      overlayScrollSnapshot.value = { ...snapshot }
    })
    onBeforeUnmount(() => {
      unsubscribe()
    })
  }

  const scrollTopRef = computed(() =>
    options.overlayScrollState ? overlayScrollSnapshot.value?.scrollTop ?? 0 : options.scrollTop.value,
  )
  const scrollLeftRef = computed(() =>
    options.overlayScrollState ? overlayScrollSnapshot.value?.scrollLeft ?? 0 : options.scrollLeft.value,
  )
  const viewportWidthRef = computed(() =>
    options.overlayScrollState
      ? overlayScrollSnapshot.value?.viewportWidth ?? options.viewportWidth.value
      : options.viewportWidth.value,
  )

  const columnRectsCache: HoverColumnRect[] = []
  const hoverLayout = shallowRef<TableSpaceColumnLayout<UiTableColumn> | null>(null)

  const hoverColumnRects = computed<HoverColumnRect[]>(() => {
    if (!options.isHoverableTable.value) {
      columnRectsCache.length = 0
      hoverLayout.value = null
      return columnRectsCache
    }

    const rects = columnRectsCache
    let writeIndex = 0
    const widthMap = options.columnWidthDomMap.value
    const bindings = options.bodyColumnBindings.value

    const orderedBindings = Array.from(bindings.values())
      .filter(binding => Number.isInteger(binding.columnIndex))
      .sort((a, b) => (a.columnIndex ?? 0) - (b.columnIndex ?? 0))

    const orderedColumns = orderedBindings.map(binding => binding.column)

    const pinnedLeftMetrics = options.headerRenderableEntries.value
      .filter(entry => entry.metric.pin === "left")
      .map(entry => entry.metric)

    const pinnedRightMetrics = options.headerRenderableEntries.value
      .filter(entry => entry.metric.pin === "right")
      .map(entry => entry.metric)

    const layout = buildTableSpaceColumnLayout({
      columns: orderedColumns,
      getColumnKey: column => column.key,
      columnWidthMap: widthMap,
      pinnedLeft: pinnedLeftMetrics,
      pinnedRight: pinnedRightMetrics,
      resolveColumnWidth: column => resolveFallbackWidth(column, options.toDomUnits),
    })

    const columnIndexMap = new Map<string, number>()
    orderedBindings.forEach(binding => {
      if (Number.isInteger(binding.columnIndex)) {
        columnIndexMap.set(binding.column.key, binding.columnIndex)
      }
    })

    options.headerRenderableEntries.value.forEach(entry => {
      const column = entry.metric.column
      const columnKey = column.key
      const layoutInfo = layout.byKey.get(columnKey)
      if (!layoutInfo) {
        return
      }
      const columnIndex = columnIndexMap.get(columnKey)
      if (!Number.isInteger(columnIndex)) {
        return
      }
      const width = layoutInfo.width
      if (!(width > 0)) {
        return
      }
      let rect = rects[writeIndex]
      if (!rect) {
        rect = { index: columnIndex!, key: columnKey, left: layoutInfo.left, width, pin: layoutInfo.pin }
        rects[writeIndex] = rect
      } else {
        rect.index = columnIndex!
        rect.key = columnKey
        rect.left = layoutInfo.left
        rect.width = width
        rect.pin = layoutInfo.pin
      }
      writeIndex += 1
    })

    rects.length = writeIndex
    rects.sort((a, b) => a.left - b.left)
    hoverLayout.value = layout
    return rects
  })

  const columnRectMapCache = new Map<number, HoverColumnRect>()
  const hoverColumnRectMap = computed(() => {
    const map = columnRectMapCache
    map.clear()
    const rects = hoverColumnRects.value
    for (let index = 0; index < rects.length; index += 1) {
      const rect = rects[index]
      map.set(rect.index, rect)
    }
    return map
  })

  const hoveredCell = shallowRef<{ rowIndex: number; columnIndex: number } | null>(null)

  const setHoveredCell = (next: { rowIndex: number; columnIndex: number } | null) => {
    const current = hoveredCell.value
    if (current?.rowIndex === next?.rowIndex && current?.columnIndex === next?.columnIndex) {
      return
    }
    hoveredCell.value = next ? { rowIndex: next.rowIndex, columnIndex: next.columnIndex } : null
    options.scheduleOverlayUpdate?.()
  }

  function resolveHoverRowTop(rowIndex: number): number | null {
    const rowHeight = options.rowHeightDom.value
    if (!(rowHeight > 0)) {
      return null
    }
    if (rowIndex < 0) {
      return null
    }

    const rows = options.processedRows.value
    const totalRows = options.totalRowCount.value
    if (rowIndex >= totalRows) {
      return null
    }

    const rowEntry = rows[rowIndex]

    if (rowEntry?.stickyBottom) {
      const bottomKey = typeof rowEntry.originalIndex === "number" ? rowEntry.originalIndex : rowIndex
      const bottomOffset = options.stickyBottomOffsets.value.get(bottomKey)
      if (bottomOffset != null) {
        return options.viewportHeight.value - bottomOffset - rowHeight
      }
    }

    if (rowEntry?.stickyTop) {
      const explicitOffset = typeof rowEntry.stickyTop === "number" ? rowEntry.stickyTop : null
      const computedOffset = explicitOffset != null ? explicitOffset : options.getStickyTopOffset(rowEntry.row) ?? 0
      const headerHeightPx = options.headerRef.value?.offsetHeight ?? 0
      return computedOffset + headerHeightPx
    }

    return rowIndex * rowHeight - scrollTopRef.value
  }

  const hoverOverlayStyle = computed<CSSProperties | null>(() => {
    if (!options.isHoverableTable.value) {
      return null
    }

    const cell = hoveredCell.value
    if (!cell) {
      return null
    }

    const rect = hoverColumnRectMap.value.get(cell.columnIndex)
    if (!rect) {
      return null
    }

    if (!(rect.width > 0)) {
      return null
    }

    const layout = hoverLayout.value
    if (!layout) {
      return null
    }

    const layoutInfo = layout.byKey.get(rect.key)
    if (!layoutInfo) {
      return null
    }

    const top = resolveHoverRowTop(cell.rowIndex)
    if (top == null) {
      return null
    }

    const height = options.rowHeightDom.value
    if (!(height > 0)) {
      return null
    }

    const viewportWidth = viewportWidthRef.value
    let left: number
    if (layoutInfo.pin === "left") {
      left = layoutInfo.left
    } else if (layoutInfo.pin === "right") {
      const base = layout.pinnedLeftWidth + layout.scrollableWidth
      const offsetWithinPinned = layoutInfo.left - base
      left = viewportWidth - layout.pinnedRightWidth + offsetWithinPinned
    } else {
      left = layoutInfo.left - scrollLeftRef.value
    }

    return {
      transform: `translate3d(${left}px, ${top}px, 0)`,
      width: `${rect.width}px`,
      height: `${height}px`,
    }
  })

  const hoverOverlayVisible = computed(() => hoverOverlayStyle.value !== null)

  const hoverMetrics = shallowRef<HoverDelegationMetrics | null>(null)
  const hoverMetricsRectCache: HoverDelegationMetrics["columnRects"] = []
  const hoverMetricsCache: HoverDelegationMetrics = {
    scrollTop: 0,
    scrollLeft: 0,
    rowHeight: 0,
    totalRows: 0,
    pinnedLeftWidth: 0,
    scrollableWidth: 0,
    pinnedRightWidth: 0,
    viewportWidth: 0,
    columnRects: hoverMetricsRectCache,
  }

  watchEffect(() => {
    if (!options.isHoverableTable.value) {
      if (hoverMetrics.value !== null) {
        hoverMetrics.value = null
        hoverMetricsRectCache.length = 0
        triggerRef(hoverMetrics)
      }
      setHoveredCell(null)
      return
    }

    const rowHeight = options.rowHeightDom.value
    if (!(rowHeight > 0)) {
      if (hoverMetrics.value !== null) {
        hoverMetrics.value = null
        hoverMetricsRectCache.length = 0
        triggerRef(hoverMetrics)
      }
      setHoveredCell(null)
      return
    }

    const columnRects = hoverColumnRects.value
    const layout = hoverLayout.value
    if (!columnRects.length || !layout) {
      if (hoverMetrics.value !== null) {
        hoverMetrics.value = null
        hoverMetricsRectCache.length = 0
        triggerRef(hoverMetrics)
      }
      setHoveredCell(null)
      return
    }

    let writeIndex = 0
    for (let index = 0; index < columnRects.length; index += 1) {
      const source = columnRects[index]
      let rect = hoverMetricsRectCache[writeIndex]
      if (!rect) {
        rect = { index: source.index, left: source.left, width: source.width }
        hoverMetricsRectCache[writeIndex] = rect
      } else {
        rect.index = source.index
        rect.left = source.left
        rect.width = source.width
      }
      writeIndex += 1
    }
    hoverMetricsRectCache.length = writeIndex

    hoverMetricsCache.scrollTop = scrollTopRef.value
    hoverMetricsCache.scrollLeft = scrollLeftRef.value
    hoverMetricsCache.rowHeight = rowHeight
    hoverMetricsCache.totalRows = options.totalRowCount.value
    hoverMetricsCache.pinnedLeftWidth = layout.pinnedLeftWidth
    hoverMetricsCache.scrollableWidth = layout.scrollableWidth
    hoverMetricsCache.pinnedRightWidth = layout.pinnedRightWidth
    hoverMetricsCache.viewportWidth = viewportWidthRef.value
    hoverMetrics.value = hoverMetricsCache
    triggerRef(hoverMetrics)
  })

  let hoverDelegation: HoverDelegationHandle | null = null

  function disposeHoverDelegation() {
    if (hoverDelegation) {
      hoverDelegation.destroy()
      hoverDelegation = null
    }
  }

  function handleHoverChange(rowIndex: number | null, columnIndex: number | null) {
    if (rowIndex == null || columnIndex == null) {
      setHoveredCell(null)
      return
    }
    if (rowIndex < 0 || rowIndex >= options.totalRowCount.value) {
      setHoveredCell(null)
      return
    }
    if (!options.columnIndexToKey.value.has(columnIndex)) {
      setHoveredCell(null)
      return
    }

    setHoveredCell({ rowIndex, columnIndex })
  }

  const pointerContainerRef = options.pointerContainerRef ?? options.containerRef

  watch(
    [pointerContainerRef, options.isHoverableTable],
    ([pointerHost, hoverable]) => {
      disposeHoverDelegation()
      setHoveredCell(null)
      if (!pointerHost || !hoverable) {
        return
      }
      hoverDelegation = createHoverDelegation(hoverEnvironment, pointerHost, handleHoverChange)
      hoverDelegation.updateMetrics(hoverMetrics.value)
    },
    { immediate: true },
  )

  watch(hoverMetrics, metrics => {
    if (!hoverDelegation) {
      return
    }
    hoverDelegation.updateMetrics(metrics)
    if (!metrics) {
      setHoveredCell(null)
      return
    }
    if (hoveredCell.value) {
      options.scheduleOverlayUpdate?.()
    }
  })

  onBeforeUnmount(() => {
    disposeHoverDelegation()
  })

  return {
    hoverOverlayStyle,
    hoverOverlayVisible,
  }
}
