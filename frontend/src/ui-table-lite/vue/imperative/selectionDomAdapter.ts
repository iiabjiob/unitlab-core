import {
  elementFromPoint,
  getCellElement,
  scrollCellIntoView,
  buildTableSpaceColumnLayout,
  type TableSpaceColumnLayout,
} from "../../core/dom/gridUtils"
import { computeFillHandleStyle } from "../../core/selection/fillHandle"
import {
  acquireFillHandleStyle,
  type FillHandleStylePayload,
} from "../../core/selection/fillHandleStylePool"
import { scheduleMeasurement, type MeasurementHandle } from "../../core/runtime/measurementQueue"
import type { GridSelectionPoint, GridSelectionRange } from "../../core/selection/selectionState"
import type { UiTableColumn } from "../../core/types"
import type { Ref } from "vue"
import type { ColumnMetric } from "../composables/useAxisVirtualizer"
import { clamp } from "../../core/utils/constants"

interface SelectionViewportBindings {
  effectiveRowHeight: Ref<number>
  viewportHeight: Ref<number>
  viewportWidth: Ref<number>
  scrollTop: Ref<number>
  scrollLeft: Ref<number>
  startIndex: Ref<number>
  endIndex: Ref<number>
  visibleStartCol: Ref<number>
  visibleEndCol: Ref<number>
  columnWidthMap: Ref<Map<string, number>>
  pinnedLeftEntries: Ref<ColumnMetric[]>
  pinnedRightEntries: Ref<ColumnMetric[]>
  virtualizationEnabled: Ref<boolean>
  clampScrollTopValue: (value: number) => number
  scrollToColumn?: (key: string) => void
}

export interface SelectionDomAdapterOptions<RowKey = unknown> {
  containerRef: Ref<HTMLDivElement | null>
  overlayContainerRef?: Ref<HTMLElement | null>
  localColumns: Ref<UiTableColumn[]>
  totalRowCount: Ref<number>
  viewport: SelectionViewportBindings
  rowIndexColumnKey?: string
  getRowIdByIndex: (rowIndex: number) => RowKey | null
  resolveCellElement?: (rowIndex: number, columnKey: string) => HTMLElement | null
  resolveHeaderCellElement?: (columnKey: string) => HTMLElement | null
}

interface FillHandleStyleInput<RowKey = unknown> {
  range: GridSelectionRange<RowKey>
  fillHandleSize: number
}

interface ScrollInput<RowKey = unknown> {
  range: GridSelectionRange<RowKey> | null
  cursor: GridSelectionPoint<RowKey> | null
  attempt?: number
  maxAttempts?: number
}

interface FallbackResult {
  handled: boolean
  style: FillHandleStylePayload | null
}

interface ScrollMeasurementResult {
  container: HTMLDivElement
  resolvedColumn: boolean
  minLeftTable: number | null
  maxRightTable: number | null
  viewLeftTable: number
  viewRightTable: number
  visibilityKeys: string[]
}

export interface SelectionDomAdapter<RowKey = unknown> {
  invalidateMetrics(): void
  resolveCellFromPoint(clientX: number, clientY: number): GridSelectionPoint<RowKey> | null
  resolveRowIndexFromPoint(clientX: number, clientY: number): number | null
  resolveFillHandleStyle(
    input: FillHandleStyleInput<RowKey>,
  ): MeasurementHandle<FillHandleStylePayload | null>
  scrollSelectionIntoView(input: ScrollInput<RowKey>): void
}

export function createSelectionDomAdapter<RowKey = unknown>(
  options: SelectionDomAdapterOptions<RowKey>,
): SelectionDomAdapter<RowKey> {
  const {
    containerRef,
    overlayContainerRef,
    localColumns,
    totalRowCount,
    viewport,
    rowIndexColumnKey,
    getRowIdByIndex,
    resolveCellElement,
    resolveHeaderCellElement,
  } = options

  const effectiveOverlayRef = overlayContainerRef ?? (containerRef as Ref<HTMLElement | null>)

  let cachedOverlayRect: DOMRect | null = null
  let pendingScrollMeasurement: MeasurementHandle<ScrollMeasurementResult | null> | null = null

  function resolveColumnLayoutWidth(column: UiTableColumn): number {
    const widthMap = viewport.columnWidthMap.value
    const mapped = widthMap.get(column.key)
    if (typeof mapped === "number" && Number.isFinite(mapped) && mapped > 0) {
      return mapped
    }
    const candidates = [column.width, column.minWidth, column.maxWidth]
    for (const candidate of candidates) {
      if (typeof candidate === "number" && Number.isFinite(candidate) && candidate > 0) {
        return candidate
      }
    }
    return 0
  }

  function buildCurrentTableSpaceLayout(): TableSpaceColumnLayout<UiTableColumn> | null {
    const columns = localColumns.value
    if (!columns.length) {
      return null
    }
    return buildTableSpaceColumnLayout({
      columns,
      getColumnKey: column => column.key,
      columnWidthMap: viewport.columnWidthMap.value,
      pinnedLeft: viewport.pinnedLeftEntries.value,
      pinnedRight: viewport.pinnedRightEntries.value,
      resolveColumnWidth: resolveColumnLayoutWidth,
    })
  }

  function createImmediateHandle<T>(value: T): MeasurementHandle<T> {
    return {
      promise: Promise.resolve(value),
      cancel() {
        // no-op for already resolved handles
      },
    }
  }

  function invalidateMetrics() {
    cachedOverlayRect = null
    if (pendingScrollMeasurement) {
      pendingScrollMeasurement.cancel()
      pendingScrollMeasurement = null
    }
  }

  function getScrollContainer(): HTMLDivElement | null {
    return containerRef.value
  }

  function getOverlayContainer(): HTMLElement | null {
    return effectiveOverlayRef.value ?? null
  }

  function ensureOverlayRect(container: HTMLElement): DOMRect {
    if (!cachedOverlayRect) {
      cachedOverlayRect = container.getBoundingClientRect()
    }
    return cachedOverlayRect
  }

  function toTableSpaceX(clientX: number, containerRectLeft: number, scrollLeftValue: number): number {
    return clientX - containerRectLeft + scrollLeftValue
  }

  function toTableSpaceY(clientY: number, containerRectTop: number, scrollTopValue: number): number {
    return clientY - containerRectTop + scrollTopValue
  }

  function clampRowIndex(rowIndex: number) {
    return clamp(rowIndex, 0, Math.max(totalRowCount.value - 1, 0))
  }

  function resolveCellFromPoint(clientX: number, clientY: number): GridSelectionPoint<RowKey> | null {
    const container = getScrollContainer()
    if (!container) return null
    const element = elementFromPoint(clientX, clientY)
    if (!element) return null
    let current: Element | null = element
    while (current && current !== document.body) {
      if (
        current instanceof HTMLElement &&
        current.hasAttribute("data-row-index") &&
        current.hasAttribute("data-col-key")
      ) {
        const rowIndex = Number.parseInt(current.getAttribute("data-row-index") || "", 10)
        const colKey = current.getAttribute("data-col-key") || ""
        const colIndex = localColumns.value.findIndex(column => column.key === colKey)
        if (!Number.isNaN(rowIndex) && colIndex !== -1) {
          const normalizedRow = clampRowIndex(rowIndex)
          return {
            rowIndex: normalizedRow,
            colIndex,
            rowId: getRowIdByIndex(normalizedRow),
          }
        }
        break
      }
      current = current.parentElement
    }

    const rect = container.getBoundingClientRect()
    if (clientX < rect.left || clientX > rect.right || clientY < rect.top || clientY > rect.bottom) {
      return null
    }

    return null
  }

  function resolveRowIndexFromPoint(clientX: number, clientY: number): number | null {
    const container = getScrollContainer()
    if (!container) return null
    const element = elementFromPoint(clientX, clientY)
    if (!element) return null
    let current: Element | null = element
    while (current && current !== document.body) {
      if (current instanceof HTMLElement && current.hasAttribute("data-row-index")) {
        const value = Number.parseInt(current.getAttribute("data-row-index") || "", 10)
        if (!Number.isNaN(value)) {
          return clampRowIndex(value)
        }
        break
      }
      current = current.parentElement
    }
    const rect = container.getBoundingClientRect()
    if (clientY < rect.top) return 0
    if (clientY > rect.bottom) return Math.max(totalRowCount.value - 1, 0)
    return null
  }

  function resolveFillHandleStyle(
    input: FillHandleStyleInput<RowKey>,
  ): MeasurementHandle<FillHandleStylePayload | null> {
    if (!getScrollContainer() || !getOverlayContainer()) {
      return createImmediateHandle(null)
    }

    return scheduleMeasurement(() => {
      const scrollContainer = getScrollContainer()
      const overlayContainer = getOverlayContainer()
      if (!scrollContainer || !overlayContainer) {
        return null
      }

      const activeColumn = localColumns.value[input.range.endCol]
      if (!activeColumn || activeColumn.isSystem) {
        return null
      }

      const viewportWidth = viewport.viewportWidth.value || scrollContainer.clientWidth || 0
      const viewportHeight = viewport.viewportHeight.value || scrollContainer.clientHeight || 0
      const scrollLeftValue = viewport.scrollLeft.value
      const scrollTopValue = viewport.scrollTop.value

      const layout = buildCurrentTableSpaceLayout()
      const layoutInfo = layout?.byKey.get(activeColumn.key) ?? null

      const result = computeFillHandleStyle({
        activeRange: input.range,
        columns: localColumns.value,
        columnWidthMap: viewport.columnWidthMap.value,
        pinnedLeft: viewport.pinnedLeftEntries.value,
        pinnedRight: viewport.pinnedRightEntries.value,
        rowHeight: viewport.effectiveRowHeight.value || 0,
        fillHandleSize: input.fillHandleSize,
        viewport: {
          width: viewportWidth,
          height: viewportHeight,
          scrollLeft: scrollLeftValue,
          scrollTop: scrollTopValue,
          startRow: viewport.startIndex.value,
          endRow: viewport.endIndex.value,
          visibleStartCol: viewport.visibleStartCol.value,
          visibleEndCol: viewport.visibleEndCol.value,
          virtualizationEnabled: viewport.virtualizationEnabled.value !== false,
        },
        getColumnKey: column => column.key,
        isSystemColumn: column => column.isSystem === true,
        rowIndexColumnKey,
      })

      if (!result) {
        return null
      }

      const fallback = applyDomFallback({
        scrollContainer,
        overlayContainer,
        rowIndex: input.range.endRow,
        columnKey: activeColumn.key,
        fillHandleSize: input.fillHandleSize,
        scrollLeft: scrollLeftValue,
        scrollTop: scrollTopValue,
      })

      if (result.kind === "fallback") {
        return fallback.handled ? fallback.style : null
      }

      if (fallback.handled) {
        return fallback.style
      }

      if (!layout || !layoutInfo) {
        return null
      }

      const columnWidth = Math.max(0, layoutInfo.width)
      if (!(columnWidth > 0)) {
        return null
      }

      const normalizedSize = Math.max(0, Math.min(columnWidth, result.size))

      let actualColumnLeft: number
      if (layoutInfo.pin === "left") {
        actualColumnLeft = layoutInfo.left
      } else if (layoutInfo.pin === "right") {
        if (!(viewportWidth > 0)) {
          return null
        }
        const pinnedRightBase = layout.pinnedLeftWidth + layout.scrollableWidth
        const offsetWithinPinned = layoutInfo.left - pinnedRightBase
        actualColumnLeft = viewportWidth - layout.pinnedRightWidth + offsetWithinPinned
      } else {
        actualColumnLeft = layoutInfo.left - scrollLeftValue
      }

      const actualColumnRight = actualColumnLeft + columnWidth
      const actualLeft = actualColumnRight - normalizedSize
      const actualTop = result.top

      if (!Number.isFinite(actualLeft) || !Number.isFinite(actualTop)) {
        return null
      }

      const styleLeft = Math.round(actualLeft + scrollLeftValue)
      const styleTop = Math.round(actualTop + scrollTopValue)

      const stylePayload = acquireFillHandleStyle()
      stylePayload.width = `${normalizedSize}px`
      stylePayload.height = `${normalizedSize}px`
      stylePayload.left = `${styleLeft}px`
      stylePayload.top = `${styleTop}px`
      stylePayload.x = styleLeft
      stylePayload.y = styleTop
      stylePayload.widthValue = normalizedSize
      stylePayload.heightValue = normalizedSize

      return stylePayload
    })
  }

  function applyDomFallback(args: {
    scrollContainer: HTMLElement
    overlayContainer: HTMLElement
    rowIndex: number
    columnKey: string
    fillHandleSize: number
    scrollLeft: number
    scrollTop: number
  }): FallbackResult {
    const {
      scrollContainer,
      overlayContainer,
      rowIndex,
      columnKey,
      fillHandleSize,
      scrollLeft,
      scrollTop,
    } = args
    const cell =
      resolveCellElement?.(rowIndex, columnKey) ??
      getCellElement(scrollContainer, rowIndex, columnKey) ??
      getCellElement(overlayContainer, rowIndex, columnKey)
    if (!cell) {
      return { handled: false, style: null }
    }

    const overlayRect = ensureOverlayRect(overlayContainer)
    const cellRect = cell.getBoundingClientRect()
    const size = fillHandleSize

    if (rowIndexColumnKey) {
      const pinnedRowIndexCell =
        resolveCellElement?.(rowIndex, rowIndexColumnKey) ??
        getCellElement(scrollContainer, rowIndex, rowIndexColumnKey) ??
        getCellElement(overlayContainer, rowIndex, rowIndexColumnKey)
      if (pinnedRowIndexCell) {
        const pinnedRect = pinnedRowIndexCell.getBoundingClientRect()
        // `toTableSpaceX` converts from viewport-relative client coordinates into table space using the overlay rect and scroll offsets.
        const fillLeftTable = toTableSpaceX(cellRect.right, overlayRect.left, scrollLeft) - size
        const pinnedRightTable = toTableSpaceX(pinnedRect.right, overlayRect.left, scrollLeft)
        if (!Number.isFinite(fillLeftTable) || !Number.isFinite(pinnedRightTable)) {
          return { handled: true, style: null }
        }
        if (fillLeftTable <= pinnedRightTable + 0.5) {
          return { handled: true, style: null }
        }
      }
    }

    const tableRight = toTableSpaceX(cellRect.right, overlayRect.left, scrollLeft)
    const tableBottom = toTableSpaceY(cellRect.bottom, overlayRect.top, scrollTop)
    if (!Number.isFinite(tableRight) || !Number.isFinite(tableBottom)) {
      return { handled: true, style: null }
    }

    const styleLeft = Math.round(tableRight - size)
    const styleTop = Math.round(tableBottom - size)

    const style = acquireFillHandleStyle()
    style.width = `${size}px`
    style.height = `${size}px`
    style.left = `${styleLeft}px`
    style.top = `${styleTop}px`
    style.x = styleLeft
    style.y = styleTop
    style.widthValue = size
    style.heightValue = size

    return {
      handled: true,
      style,
    }
  }

  function scrollSelectionIntoView(input: ScrollInput<RowKey>) {
    const container = getScrollContainer()
    if (!container) return

    const { range, cursor } = input
    const attempt = input.attempt ?? 0
    const maxAttempts = input.maxAttempts ?? 4

    const rowTargets = new Set<number>()
    if (range) {
      rowTargets.add(range.startRow)
      rowTargets.add(range.endRow)
    } else if (cursor) {
      rowTargets.add(cursor.rowIndex)
    }

    const colTargets = new Set<number>()
    if (range) {
      colTargets.add(range.startCol)
      colTargets.add(range.endCol)
    } else if (cursor) {
      colTargets.add(cursor.colIndex)
    }

    if (rowTargets.size) {
      const rowHeight = viewport.effectiveRowHeight.value || 1
      const viewportHeight = viewport.viewportHeight.value || rowHeight
      const scrollArgsBase = {
        container,
        rowHeight,
        viewportHeight,
        clampScrollTop: viewport.clampScrollTopValue,
      }

      const sortedRows = Array.from(rowTargets).sort((a, b) => a - b)
      let nextScrollTop = viewport.scrollTop.value

      for (const rowIndex of sortedRows) {
        nextScrollTop = scrollCellIntoView({
          ...scrollArgsBase,
          targetRowIndex: rowIndex,
          currentScrollTop: nextScrollTop,
        })
      }

      viewport.scrollTop.value = nextScrollTop
    }

    if (!colTargets.size) return

    const padding = 2
    const referenceRows = rowTargets.size
      ? Array.from(rowTargets)
      : cursor
        ? [cursor.rowIndex]
        : []

    const columnMap = new Map<string, UiTableColumn>(localColumns.value.map(column => [column.key, column]))

    const columnKeys = Array.from(colTargets)
      .map(index => localColumns.value[index]?.key)
      .filter((key): key is string => Boolean(key))

    const visibilityKeys = columnKeys.filter(key => !isColumnPinned(columnMap.get(key)))

    if (!visibilityKeys.length) {
      return
    }

    const scheduleRetry = () => {
      if (attempt >= maxAttempts) {
        return
      }
      const nextInput: ScrollInput<RowKey> = {
        range,
        cursor,
        attempt: attempt + 1,
        maxAttempts,
      }
      if (typeof requestAnimationFrame === "function") {
        requestAnimationFrame(() => scrollSelectionIntoView(nextInput))
      } else {
        setTimeout(() => scrollSelectionIntoView(nextInput), 16)
      }
    }

    if (pendingScrollMeasurement) {
      pendingScrollMeasurement.cancel()
      pendingScrollMeasurement = null
    }

    const measurementHandle = scheduleMeasurement<ScrollMeasurementResult | null>(() => {
      if (!container || !container.isConnected) {
        return null
      }

      const containerRect = container.getBoundingClientRect()
      const viewLeftTable = container.scrollLeft
      const viewRightTable = container.scrollLeft + container.clientWidth
      let minLeftTable: number | null = null
      let maxRightTable: number | null = null
      let resolvedColumn = false

      // Translate any DOM-based measurements into the canonical table space so scroll math stays consistent.
      for (const key of visibilityKeys) {
        let rect: DOMRect | null = null

        for (const rowIndex of referenceRows) {
          const cell = resolveCellElement?.(rowIndex, key) ?? getCellElement(container, rowIndex, key)
          if (cell) {
            rect = cell.getBoundingClientRect()
            break
          }
        }

        if (!rect) {
          const headerCell = resolveHeaderCellElement?.(key) ?? container.querySelector<HTMLElement>(`.ui-table-header-cell[data-column-key="${key}"]`)
          if (headerCell) {
            rect = headerCell.getBoundingClientRect()
          }
        }

        if (!rect) {
          continue
        }

        resolvedColumn = true
        const rectLeftTable = toTableSpaceX(rect.left, containerRect.left, container.scrollLeft)
        const rectRightTable = toTableSpaceX(rect.right, containerRect.left, container.scrollLeft)
        minLeftTable = minLeftTable === null ? rectLeftTable : Math.min(minLeftTable, rectLeftTable)
        maxRightTable = maxRightTable === null ? rectRightTable : Math.max(maxRightTable, rectRightTable)
      }

      return {
        container,
        resolvedColumn,
        minLeftTable,
        maxRightTable,
        viewLeftTable,
        viewRightTable,
        visibilityKeys,
      }
    })

    pendingScrollMeasurement = measurementHandle

    void measurementHandle.promise
      .then(result => {
        if (pendingScrollMeasurement !== measurementHandle) {
          return
        }
        pendingScrollMeasurement = null

        if (!result) {
          return
        }

        const activeContainer = getScrollContainer()
        if (!activeContainer || activeContainer !== result.container) {
          return
        }

        if (!result.resolvedColumn) {
          if (viewport.scrollToColumn && result.visibilityKeys.length && attempt < maxAttempts) {
            viewport.scrollToColumn(result.visibilityKeys[0])
            scheduleRetry()
          }
          return
        }

        const viewWidth = result.viewRightTable - result.viewLeftTable
        let adjusted = false

        if (result.minLeftTable !== null) {
          if (result.minLeftTable < result.viewLeftTable + padding) {
            activeContainer.scrollLeft = Math.max(0, result.minLeftTable - padding)
            adjusted = true
          }
        }

        if (result.maxRightTable !== null) {
          if (result.maxRightTable > result.viewRightTable - padding) {
            activeContainer.scrollLeft = Math.max(0, result.maxRightTable - viewWidth + padding)
            adjusted = true
          }
        }

        if (adjusted) {
          cachedOverlayRect = null
          scheduleRetry()
        }
      })
      .catch(() => {
        if (pendingScrollMeasurement === measurementHandle) {
          pendingScrollMeasurement = null
        }
      })
  }

  function isColumnPinned(column: UiTableColumn | undefined | null): boolean {
    if (!column) return false
    if ((column as any).isSystem) return true
    const rawPin = (column as any).pin ?? (column as any).pinned ?? column.sticky
    if (rawPin === "left" || rawPin === "right") return true
    if (column.sticky === "left" || column.sticky === "right") return true
    if ((column as any).stickyLeft === true || typeof (column as any).stickyLeft === "number") return true
    if ((column as any).stickyRight === true || typeof (column as any).stickyRight === "number") return true
    return false
  }

  return {
    invalidateMetrics,
    resolveCellFromPoint,
    resolveRowIndexFromPoint,
    resolveFillHandleStyle,
    scrollSelectionIntoView,
  }
}
