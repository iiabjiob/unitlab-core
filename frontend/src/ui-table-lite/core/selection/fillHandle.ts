import { DEFAULT_COLUMN_WIDTH } from "../virtualization/columnSizing"
import type { ColumnMetric } from "../virtualization/columnSnapshot"
import type { ColumnPinMode } from "../virtualization/types"
import type { GridSelectionRange } from "./selectionState"

const FILL_HANDLE_ENABLED = false

export interface FillHandleViewportState {
  width: number
  height: number
  scrollLeft: number
  scrollTop: number
  startRow: number
  endRow: number
  visibleStartCol: number
  visibleEndCol: number
  virtualizationEnabled: boolean
}

interface ColumnSizeLike {
  width?: number | null
  minWidth?: number | null
  maxWidth?: number | null
}

function resolveColumnWidth<TColumn>(
  column: TColumn,
  getColumnKey: (column: TColumn) => string,
  widthMap: Map<string, number>,
): number {
  const key = getColumnKey(column)
  const mapped = widthMap.get(key)
  if (typeof mapped === "number" && Number.isFinite(mapped) && mapped > 0) {
    return mapped
  }
  const candidate = column as ColumnSizeLike
  const options = [candidate.width, candidate.minWidth, candidate.maxWidth]
  for (const value of options) {
    if (typeof value === "number" && Number.isFinite(value) && value > 0) {
      return value
    }
  }
  return DEFAULT_COLUMN_WIDTH
}

export interface ColumnVisualBounds {
  visualLeft: number
  visualRight: number
  width: number
  pin: ColumnPinMode
}

export interface PinnedGeometry {
  leftMap: Map<string, ColumnVisualBounds>
  rightMap: Map<string, ColumnVisualBounds>
  leftTotal: number
}

export function buildPinnedGeometry<TColumn>(
  viewportWidth: number,
  pinnedLeft: readonly ColumnMetric<TColumn>[],
  pinnedRight: readonly ColumnMetric<TColumn>[],
  getColumnKey: (column: TColumn) => string,
  widthMap: Map<string, number>,
): PinnedGeometry {
  const leftMap = new Map<string, ColumnVisualBounds>()
  let leftCursor = 0
  for (const metric of pinnedLeft) {
    const width = Number.isFinite(metric.width) && metric.width > 0
      ? metric.width
      : resolveColumnWidth(metric.column, getColumnKey, widthMap)
    const visualLeft = leftCursor
    const visualRight = visualLeft + width
    leftMap.set(getColumnKey(metric.column), {
      visualLeft,
      visualRight,
      width,
      pin: "left",
    })
    leftCursor = visualRight
  }

  const rightMap = new Map<string, ColumnVisualBounds>()
  let rightCursor = Math.max(0, viewportWidth)
  for (let index = pinnedRight.length - 1; index >= 0; index -= 1) {
    const metric = pinnedRight[index]
    const width = Number.isFinite(metric.width) && metric.width > 0
      ? metric.width
      : resolveColumnWidth(metric.column, getColumnKey, widthMap)
    rightCursor = Math.max(0, rightCursor - width)
    const visualLeft = rightCursor
    const visualRight = visualLeft + width
    rightMap.set(getColumnKey(metric.column), {
      visualLeft,
      visualRight,
      width,
      pin: "right",
    })
  }

  return {
    leftMap,
    rightMap,
    leftTotal: leftCursor,
  }
}

export function buildScrollableOffsetMap<TColumn>(
  columns: readonly TColumn[],
  pinnedKeys: Set<string>,
  getColumnKey: (column: TColumn) => string,
  widthMap: Map<string, number>,
): Map<string, number> {
  const map = new Map<string, number>()
  let offset = 0
  for (const column of columns) {
    const key = getColumnKey(column)
    if (pinnedKeys.has(key)) continue
    map.set(key, offset)
    const width = resolveColumnWidth(column, getColumnKey, widthMap)
    offset += width
  }
  return map
}

export function computeColumnVisualBounds(
  columnKey: string,
  columnWidth: number,
  scrollLeft: number,
  geometry: PinnedGeometry,
  scrollableOffsets: Map<string, number>,
): ColumnVisualBounds | null {
  const leftEntry = geometry.leftMap.get(columnKey)
  if (leftEntry) {
    return leftEntry
  }
  const rightEntry = geometry.rightMap.get(columnKey)
  if (rightEntry) {
    return rightEntry
  }
  const offset = scrollableOffsets.get(columnKey)
  if (offset == null) {
    return null
  }
  const visualLeft = geometry.leftTotal + offset - scrollLeft
  const visualRight = visualLeft + columnWidth
  return {
    visualLeft,
    visualRight,
    width: columnWidth,
    pin: "none",
  }
}

export type FillHandleComputationResult =
  | { kind: "style"; left: number; top: number; size: number }
  | { kind: "fallback" }

export interface FillHandleStyleContext<TColumn> {
  activeRange: GridSelectionRange
  columns: readonly TColumn[]
  columnWidthMap: Map<string, number>
  pinnedLeft: readonly ColumnMetric<TColumn>[]
  pinnedRight: readonly ColumnMetric<TColumn>[]
  rowHeight: number
  fillHandleSize: number
  viewport: FillHandleViewportState
  getColumnKey: (column: TColumn) => string
  isSystemColumn?: (column: TColumn) => boolean
  rowIndexColumnKey?: string
}

export function computeFillHandleStyle<TColumn>(
  context: FillHandleStyleContext<TColumn>,
): FillHandleComputationResult | null {
  if (!FILL_HANDLE_ENABLED) {
    void context
    return null
  }
  const {
    activeRange,
    columns,
    columnWidthMap,
    pinnedLeft,
    pinnedRight,
    rowHeight,
    fillHandleSize,
    viewport,
    getColumnKey,
    isSystemColumn,
    rowIndexColumnKey,
  } = context

  if (!Number.isFinite(rowHeight) || rowHeight <= 0) {
    return null
  }

  const columnIndex = activeRange.endCol
  const rowIndex = activeRange.endRow
  const column = columns[columnIndex]
  if (!column) {
    return null
  }
  if (isSystemColumn?.(column)) {
    return null
  }

  const geometry = buildPinnedGeometry(viewport.width, pinnedLeft, pinnedRight, getColumnKey, columnWidthMap)
  const pinnedKeys = new Set<string>([
    ...geometry.leftMap.keys(),
    ...geometry.rightMap.keys(),
  ])
  const scrollableOffsets = buildScrollableOffsetMap(columns, pinnedKeys, getColumnKey, columnWidthMap)
  const columnKey = getColumnKey(column)
  const columnWidth = resolveColumnWidth(column, getColumnKey, columnWidthMap)
  const columnVisual = computeColumnVisualBounds(columnKey, columnWidth, viewport.scrollLeft, geometry, scrollableOffsets)
  if (!columnVisual) {
    return { kind: "fallback" }
  }

  const virtualizationActive = viewport.virtualizationEnabled !== false
  if (virtualizationActive) {
    const rowInPool = rowIndex >= viewport.startRow && rowIndex < viewport.endRow
    if (!rowInPool) {
      return { kind: "fallback" }
    }
    const columnVisible =
      columnVisual.pin !== "none" ||
      (columnIndex >= viewport.visibleStartCol && columnIndex < viewport.visibleEndCol)
    if (!columnVisible) {
      return { kind: "fallback" }
    }
  }

  const rowVisualBottom = (rowIndex + 1) * rowHeight - viewport.scrollTop
  const rowVisualTop = rowVisualBottom - rowHeight
  if (rowVisualBottom <= 0 || rowVisualTop >= viewport.height) {
    return null
  }

  if (columnVisual.visualRight <= 0 || columnVisual.visualLeft >= viewport.width) {
    return null
  }

  if (rowIndexColumnKey) {
    const indexColumn = columns.find(col => getColumnKey(col) === rowIndexColumnKey)
    if (indexColumn) {
      const indexWidth = resolveColumnWidth(indexColumn, getColumnKey, columnWidthMap)
      if (indexWidth > 0) {
        const indexVisual = computeColumnVisualBounds(
          rowIndexColumnKey,
          indexWidth,
          viewport.scrollLeft,
          geometry,
          scrollableOffsets,
        )
        if (indexVisual && columnVisual.visualRight - fillHandleSize <= indexVisual.visualRight + 0.5) {
          return null
        }
      }
    }
  }

  const left = columnVisual.visualRight - fillHandleSize
  const top = rowVisualBottom - fillHandleSize

  if (!Number.isFinite(left) || !Number.isFinite(top)) {
    return null
  }

  return {
    kind: "style",
    left,
    top,
    size: fillHandleSize,
  }
}
