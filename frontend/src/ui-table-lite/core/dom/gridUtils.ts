import type { UiTableColumn } from "../types/column"
import type { ColumnMetric } from "../virtualization/columnSnapshot"
import type { ColumnPinMode } from "../virtualization/types"
import {
  accumulateColumnWidths as accumulateColumnWidthsCore,
  calculateVisibleColumns as calculateVisibleColumnsCore,
  COLUMN_VIRTUALIZATION_BUFFER,
  DEFAULT_COLUMN_WIDTH,
  resolveColumnWidth as resolveColumnWidthCore,
  type ColumnWidthMetrics,
  type VisibleColumnRange,
} from "../virtualization/columnSizing"

export { COLUMN_VIRTUALIZATION_BUFFER, DEFAULT_COLUMN_WIDTH }
export type { ColumnWidthMetrics, VisibleColumnRange }

export const supportsCssZoom =
  typeof CSS !== "undefined" &&
  typeof CSS.supports === "function" &&
  CSS.supports("zoom: 1")

export const resolveColumnWidth = (column: UiTableColumn, zoom = 1) => resolveColumnWidthCore(column, zoom)

export const accumulateColumnWidths = (columns: UiTableColumn[], zoom = 1): ColumnWidthMetrics =>
  accumulateColumnWidthsCore(columns, zoom)

export function calculateVisibleColumns(
  scrollLeft: number,
  containerWidth: number,
  columns: UiTableColumn[],
  options: {
    zoom?: number
    pinnedLeftWidth?: number
    pinnedRightWidth?: number
    metrics?: ColumnWidthMetrics
  } = {},
): VisibleColumnRange & ColumnWidthMetrics {
  return calculateVisibleColumnsCore(scrollLeft, containerWidth, columns, options)
}

export function getCellElement(
  container: HTMLElement | null,
  rowIndex: number,
  columnKey: string,
) {
  if (!container) return null
  return container.querySelector<HTMLElement>(
    `[data-row-index="${rowIndex}"][data-col-key="${columnKey}"]`,
  )
}

export interface FocusHandle<T = HTMLElement> {
  value: T | null
}

export function focusElement(elementRef: FocusHandle<HTMLElement>) {
  const element = elementRef.value
  if (!element) return
  element.focus({ preventScroll: true })
}

interface ScrollIntoViewInput {
  container: HTMLElement | null
  targetRowIndex: number
  rowHeight: number
  viewportHeight: number
  currentScrollTop: number
  clampScrollTop: (value: number) => number
}

export function scrollCellIntoView({
  container,
  targetRowIndex,
  rowHeight,
  viewportHeight,
  currentScrollTop,
  clampScrollTop,
}: ScrollIntoViewInput) {
  if (!container) return currentScrollTop
  const targetTop = targetRowIndex * rowHeight
  const targetBottom = targetTop + rowHeight
  const viewTop = currentScrollTop
  const viewBottom = viewTop + viewportHeight
  let nextScrollTop = viewTop
  if (targetTop < viewTop) {
    nextScrollTop = targetTop
  } else if (targetBottom > viewBottom) {
    nextScrollTop = targetBottom - viewportHeight
  }
  nextScrollTop = clampScrollTop(nextScrollTop)
  if (nextScrollTop !== viewTop) {
    container.scrollTop = nextScrollTop
  }
  return nextScrollTop
}

export function elementFromPoint(clientX: number, clientY: number) {
  if (typeof document === "undefined") return null
  return document.elementFromPoint(clientX, clientY)
}

function isPositiveFinite(value: unknown): value is number {
  return typeof value === "number" && Number.isFinite(value) && value > 0
}

function resolveMetricWidth<TColumn>(
  key: string,
  column: TColumn,
  widthMap: ReadonlyMap<string, number>,
  metricWidth: number | undefined,
  resolveFallback?: (column: TColumn) => number,
): number {
  const mapped = widthMap.get(key)
  if (isPositiveFinite(mapped)) {
    return mapped
  }
  if (isPositiveFinite(metricWidth)) {
    return metricWidth!
  }
  if (resolveFallback) {
    const fallback = resolveFallback(column)
    if (isPositiveFinite(fallback)) {
      return fallback
    }
  }
  return 0
}

export interface TableSpaceColumnInfo<TColumn> {
  column: TColumn
  key: string
  pin: ColumnPinMode
  left: number
  width: number
}

export interface TableSpaceColumnLayout<TColumn> {
  ordered: TableSpaceColumnInfo<TColumn>[]
  byKey: Map<string, TableSpaceColumnInfo<TColumn>>
  pinnedLeftWidth: number
  scrollableWidth: number
  pinnedRightWidth: number
}

export interface BuildTableSpaceLayoutInput<TColumn> {
  columns: readonly TColumn[]
  getColumnKey: (column: TColumn) => string
  columnWidthMap: ReadonlyMap<string, number>
  pinnedLeft: readonly ColumnMetric<TColumn>[]
  pinnedRight: readonly ColumnMetric<TColumn>[]
  resolveColumnWidth?: (column: TColumn) => number
}

export function buildTableSpaceColumnLayout<TColumn>(
  input: BuildTableSpaceLayoutInput<TColumn>,
): TableSpaceColumnLayout<TColumn> {
  const {
    columns,
    getColumnKey,
    columnWidthMap,
    pinnedLeft,
    pinnedRight,
    resolveColumnWidth: resolveFallback,
  } = input

  const infoByKey = new Map<string, TableSpaceColumnInfo<TColumn>>()
  const ordered: TableSpaceColumnInfo<TColumn>[] = []
  const pinnedKeySet = new Set<string>()

  let pinnedLeftWidth = 0
  for (const metric of pinnedLeft) {
    const column = metric.column
    const key = getColumnKey(column)
    pinnedKeySet.add(key)
    const width = resolveMetricWidth(key, column, columnWidthMap, metric.width, resolveFallback)
    const info: TableSpaceColumnInfo<TColumn> = {
      column,
      key,
      pin: "left",
      left: pinnedLeftWidth,
      width,
    }
    pinnedLeftWidth += width
    infoByKey.set(key, info)
    ordered.push(info)
  }

  const scrollableInfos: TableSpaceColumnInfo<TColumn>[] = []
  let scrollableWidth = 0
  for (const column of columns) {
    const key = getColumnKey(column)
    if (pinnedKeySet.has(key)) {
      continue
    }
    const width = resolveMetricWidth(key, column, columnWidthMap, undefined, resolveFallback)
    const info: TableSpaceColumnInfo<TColumn> = {
      column,
      key,
      pin: "none",
      left: pinnedLeftWidth + scrollableWidth,
      width,
    }
    scrollableWidth += width
    infoByKey.set(key, info)
    scrollableInfos.push(info)
  }

  const pinnedRightInfos: TableSpaceColumnInfo<TColumn>[] = []
  let pinnedRightWidth = 0
  const pinnedRightBase = pinnedLeftWidth + scrollableWidth
  for (const metric of pinnedRight) {
    const column = metric.column
    const key = getColumnKey(column)
    pinnedKeySet.add(key)
    const width = resolveMetricWidth(key, column, columnWidthMap, metric.width, resolveFallback)
    const info: TableSpaceColumnInfo<TColumn> = {
      column,
      key,
      pin: "right",
      left: pinnedRightBase + pinnedRightWidth,
      width,
    }
    pinnedRightWidth += width
    infoByKey.set(key, info)
    pinnedRightInfos.push(info)
  }

  ordered.push(...scrollableInfos, ...pinnedRightInfos)

  return {
    ordered,
    byKey: infoByKey,
    pinnedLeftWidth,
    scrollableWidth,
    pinnedRightWidth,
  }
}
