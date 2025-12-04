import { resolveColumnWidth } from "../virtualization/columnSizing"
import type { ColumnSizeLike } from "../virtualization/columnSizing"
import type { ColumnMetric } from "../virtualization/columnSnapshot"
import type { UiTableColumn } from "../types"
import { buildPinnedGeometry, buildScrollableOffsetMap, type FillHandleViewportState } from "./fillHandle"
import {
  acquireOverlayRect,
  acquireOverlayRectArray,
  type MutableSelectionOverlayRect,
} from "./selectionOverlayRectPool"
import type {
  GridSelectionPoint,
  GridSelectionRange,
  SelectionArea,
} from "./selectionState"

export interface SelectionOverlayRect {
  id: string
  left: number
  top: number
  width: number
  height: number
  active?: boolean
  pin?: "left" | "none" | "right"
}

export interface SelectionOverlayColumnSurface {
  left: number
  width: number
  pin: "left" | "none" | "right"
}

export interface SelectionOverlayComputationContext<RowKey = unknown> {
  ranges: readonly GridSelectionRange<RowKey>[]
  activeRangeIndex: number
  activeCell: GridSelectionPoint<RowKey> | null
  fillPreview: SelectionArea | null
  cutPreview: readonly SelectionArea[]
  cutPreviewActiveIndex: number
  columns: readonly UiTableColumn[]
  columnWidthMap: Map<string, number>
  pinnedLeft: readonly ColumnMetric<UiTableColumn>[]
  pinnedRight: readonly ColumnMetric<UiTableColumn>[]
  viewport: FillHandleViewportState
  rowHeight: number
  getColumnKey: (column: UiTableColumn) => string
  isSystemColumn?: (column: UiTableColumn) => boolean
  rowIndexColumnKey?: string
  columnSurfaces?: Map<string, SelectionOverlayColumnSurface>
}

export interface SelectionOverlayComputationResult {
  ranges: SelectionOverlayRect[]
  cursor: SelectionOverlayRect | null
  fillPreview: SelectionOverlayRect[]
  cutPreview: SelectionOverlayRect[]
}

export interface SelectionOverlayComputationPrepared {
  geometry: ReturnType<typeof buildPinnedGeometry<UiTableColumn>>
  scrollableOffsets: Map<string, number>
}

export function prepareSelectionOverlayResources<RowKey = unknown>(
  context: SelectionOverlayComputationContext<RowKey>,
): SelectionOverlayComputationPrepared | null {
  if (!context.viewport.width || !context.viewport.height || context.rowHeight <= 0) {
    return null
  }

  const geometry = buildPinnedGeometry<UiTableColumn>(
    context.viewport.width,
    context.pinnedLeft,
    context.pinnedRight,
    context.getColumnKey,
    context.columnWidthMap,
  )

  const pinnedKeys = new Set<string>([...geometry.leftMap.keys(), ...geometry.rightMap.keys()])
  const scrollableOffsets = buildScrollableOffsetMap(
    context.columns,
    pinnedKeys,
    context.getColumnKey,
    context.columnWidthMap,
  )

  return {
    geometry,
    scrollableOffsets,
  }
}

const PIXEL_EPSILON = 0.5

function normalizeIndices(a: number, b: number): [number, number] {
  const start = Math.min(a, b)
  const end = Math.max(a, b)
  return [start, end]
}

function createRowSpan(
  startRow: number,
  endRow: number,
  rowHeight: number,
): { top: number; height: number } | null {
  if (!Number.isFinite(rowHeight) || rowHeight <= 0) {
    return null
  }
  const [normalizedStart, normalizedEnd] = normalizeIndices(startRow, endRow)
  if (normalizedEnd < 0) {
    return null
  }

  const clampedStart = Math.max(0, normalizedStart)
  const worldTop = Math.round(clampedStart * rowHeight * 1000) / 1000
  const worldBottom = Math.round((Math.max(clampedStart, normalizedEnd) + 1) * rowHeight * 1000) / 1000
  const height = Math.round((worldBottom - worldTop) * 1000) / 1000

  if (height <= PIXEL_EPSILON) {
    return null
  }

  return {
    top: worldTop,
    height,
  }
}

type ColumnSegment = { left: number; right: number; pin: "left" | "none" | "right" }

function createColumnSegments<RowKey = unknown>(
  startCol: number,
  endCol: number,
  context: SelectionOverlayComputationContext<RowKey>,
  geometry: ReturnType<typeof buildPinnedGeometry<UiTableColumn>>,
  scrollableOffsets: Map<string, number>,
): ColumnSegment[] {
  const [normalizedStart, normalizedEnd] = normalizeIndices(startCol, endCol)
  const segments: ColumnSegment[] = []
  let current: ColumnSegment | null = null

  const columnByKey = new Map<string, UiTableColumn>()
  for (const column of context.columns) {
    columnByKey.set(context.getColumnKey(column), column)
  }

  const widthCache = new Map<string, number>()
  const resolveColumnWidthByKey = (key: string): number | null => {
    const cached = widthCache.get(key)
    if (cached != null) {
      return cached
    }

    let width: number | null = null
    const mappedWidth = context.columnWidthMap.get(key)
    if (typeof mappedWidth === "number" && Number.isFinite(mappedWidth) && mappedWidth > 0) {
      width = mappedWidth
    } else {
      const column = columnByKey.get(key)
      if (column) {
        const resolved = resolveColumnWidth(column as unknown as ColumnSizeLike)
        if (Number.isFinite(resolved) && resolved > 0) {
          width = resolved
        }
      }
    }

    if (width == null) {
      return null
    }

    widthCache.set(key, width)
    return width
  }

  const pinnedLeftPositions = new Map<string, { left: number; right: number }>()
  geometry.leftMap.forEach((value, key) => {
    pinnedLeftPositions.set(key, { left: value.visualLeft, right: value.visualRight }) // world coordinate
  })

  // removed baseline correction — rely purely on world coordinates for all segments

  let totalScrollableWidth = 0
  scrollableOffsets.forEach((offset, key) => {
    const width = resolveColumnWidthByKey(key)
    if (width == null) {
      return
    }
    totalScrollableWidth = Math.max(totalScrollableWidth, offset + width)
  })

  const pinnedRightPositions = new Map<string, { left: number; right: number }>()
  let pinnedRightCursor = 0
  for (const metric of context.pinnedRight) {
    const key = context.getColumnKey(metric.column)
    let width: number | null = null
    if (typeof metric.width === "number" && Number.isFinite(metric.width) && metric.width > 0) {
      width = metric.width
    } else {
      width = resolveColumnWidthByKey(key)
    }
    if (width == null) {
      continue
    }
    const left = geometry.leftTotal + totalScrollableWidth + pinnedRightCursor // world coordinate
    pinnedRightPositions.set(key, { left, right: left + width })
    pinnedRightCursor += width
  }
  for (let columnIndex = normalizedStart; columnIndex <= normalizedEnd; columnIndex += 1) {
    const column = context.columns[columnIndex]
    if (!column) {
      continue
    }

    if (context.isSystemColumn?.(column)) {
      continue
    }

    const columnKey = context.getColumnKey(column)
    if (context.rowIndexColumnKey && columnKey === context.rowIndexColumnKey) {
      continue
    }

    const surface = context.columnSurfaces?.get(columnKey)
    const surfaceWidth = surface?.width
    const surfaceLeft = surface?.left

    let segmentLeft: number | null = null
    let segmentRight: number | null = null
    let pin: ColumnSegment["pin"] = "none"

    if (
      surface &&
      Number.isFinite(surfaceWidth ?? NaN) &&
      Number.isFinite(surfaceLeft ?? NaN) &&
      (surfaceWidth ?? 0) > 0
    ) {
      segmentLeft = surfaceLeft ?? 0
      segmentRight = segmentLeft + (surfaceWidth ?? 0)
      pin = surface.pin
    } else {
      const columnWidth = resolveColumnWidthByKey(columnKey)
      if (columnWidth == null) {
        continue
      }

      const leftPosition = pinnedLeftPositions.get(columnKey)
      const rightPosition = pinnedRightPositions.get(columnKey)

      if (leftPosition) {
        segmentLeft = leftPosition.left
        segmentRight = leftPosition.right
        pin = "left"
      } else if (rightPosition) {
        segmentLeft = rightPosition.left
        segmentRight = rightPosition.right
        pin = "right"
      } else {
        const offset = scrollableOffsets.get(columnKey)
        if (offset == null) {
          continue
        }
        segmentLeft = geometry.leftTotal + offset // world coordinate
        segmentRight = segmentLeft + columnWidth
      }
    }

    if (segmentLeft == null || segmentRight == null) {
      continue
    }

    if (current && current.pin === pin && Math.abs(segmentLeft - current.right) <= PIXEL_EPSILON) {
      current.right = segmentRight
    } else {
      current = { left: segmentLeft, right: segmentRight, pin }
      segments.push(current)
    }
  }

  return segments
}

function appendRectanglePayload<RowKey = unknown>(
  target: MutableSelectionOverlayRect[],
  startRow: number,
  endRow: number,
  startCol: number,
  endCol: number,
  keyPrefix: string,
  context: SelectionOverlayComputationContext<RowKey>,
  geometry: ReturnType<typeof buildPinnedGeometry<UiTableColumn>>,
  scrollableOffsets: Map<string, number>,
  active = false,
): void {
  const rowSpan = createRowSpan(startRow, endRow, context.rowHeight)
  if (!rowSpan) {
    return
  }

  const segments = createColumnSegments(startCol, endCol, context, geometry, scrollableOffsets)
  if (!segments.length) {
    return
  }

  for (let index = 0; index < segments.length; index += 1) {
    const segment = segments[index]

    // 1. Normalize both left & right *before* width calc
    const normalizedLeft = Math.round(segment.left * 1000) / 1000
    const normalizedRight = Math.round(segment.right * 1000) / 1000

    // 2. Recompute width using normalized values
    const width = normalizedRight - normalizedLeft
    if (width <= PIXEL_EPSILON) {
      continue
    }

    // 3. Use ONLY normalized values in rect
    const rect = acquireOverlayRect()
    rect.id = `${keyPrefix}-${index}`
    rect.left = normalizedLeft
    rect.top = rowSpan.top
    rect.width = width
    rect.height = rowSpan.height
    rect.active = active
    rect.pin = segment.pin

    target.push(rect)
  }
}

function createCursorRect<RowKey = unknown>(
  point: GridSelectionPoint<RowKey>,
  context: SelectionOverlayComputationContext<RowKey>,
  geometry: ReturnType<typeof buildPinnedGeometry<UiTableColumn>>,
  scrollableOffsets: Map<string, number>,
): SelectionOverlayRect | null {
  const rowSpan = createRowSpan(point.rowIndex, point.rowIndex, context.rowHeight)
  if (!rowSpan) {
    return null
  }

  const segments = createColumnSegments(point.colIndex, point.colIndex, context, geometry, scrollableOffsets)
  if (!segments.length) {
    return null
  }

  for (let index = 0; index < segments.length; index += 1) {
    const segment = segments[index]

    // Normalize coords BEFORE computing width
    const normalizedLeft = Math.round(segment.left * 1000) / 1000
    const normalizedRight = Math.round(segment.right * 1000) / 1000
    const width = normalizedRight - normalizedLeft

    if (width <= PIXEL_EPSILON) {
      continue
    }

    const rect = acquireOverlayRect()
    rect.id = `cursor-${index}`
    rect.left = normalizedLeft
    rect.top = rowSpan.top
    rect.width = width
    rect.height = rowSpan.height
    rect.active = true
    rect.pin = segment.pin

    return rect
  }

  return null
}

export function computeSelectionOverlayRects<RowKey = unknown>(
  context: SelectionOverlayComputationContext<RowKey>,
): SelectionOverlayComputationResult {
  const prepared = prepareSelectionOverlayResources(context)
  if (!prepared) {
    return {
      ranges: acquireOverlayRectArray(),
      cursor: null,
      fillPreview: acquireOverlayRectArray(),
      cutPreview: acquireOverlayRectArray(),
    }
  }

  const { geometry, scrollableOffsets } = prepared

  const ranges = acquireOverlayRectArray()
  context.ranges.forEach((range, index) => {
    appendRectanglePayload(
      ranges,
      range.startRow,
      range.endRow,
      range.startCol,
      range.endCol,
      `range-${index}`,
      context,
      geometry,
      scrollableOffsets,
      index === context.activeRangeIndex,
    )
  })

  const cursor = context.activeCell
    ? createCursorRect(context.activeCell, context, geometry, scrollableOffsets)
    : null

  const fillPreview = acquireOverlayRectArray()
  if (context.fillPreview) {
    appendRectanglePayload(
      fillPreview,
      context.fillPreview.startRow,
      context.fillPreview.endRow,
      context.fillPreview.startCol,
      context.fillPreview.endCol,
      "fill-preview",
      context,
      geometry,
      scrollableOffsets,
    )
  }

  const cutPreview = acquireOverlayRectArray()
  context.cutPreview.forEach((area, index) => {
    appendRectanglePayload(
      cutPreview,
      area.startRow,
      area.endRow,
      area.startCol,
      area.endCol,
      `cut-preview-${index}`,
      context,
      geometry,
      scrollableOffsets,
      index === context.cutPreviewActiveIndex,
    )
  })

  return {
    ranges,
    cursor,
    fillPreview,
    cutPreview,
  }
}

export function computeFillPreviewOverlayRects<RowKey = unknown>(
  context: SelectionOverlayComputationContext<RowKey>,
  prepared?: SelectionOverlayComputationPrepared,
): SelectionOverlayRect[] {
  if (!context.fillPreview) {
    return acquireOverlayRectArray()
  }

  const resources = prepared ?? prepareSelectionOverlayResources(context)
  if (!resources) {
    return acquireOverlayRectArray()
  }

  const area = context.fillPreview
  const rectangles = acquireOverlayRectArray()
  appendRectanglePayload(
    rectangles,
    area.startRow,
    area.endRow,
    area.startCol,
    area.endCol,
    "fill-preview",
    context,
    resources.geometry,
    resources.scrollableOffsets,
  )
  return rectangles
}

export function computeCutPreviewOverlayRects<RowKey = unknown>(
  context: SelectionOverlayComputationContext<RowKey>,
  prepared?: SelectionOverlayComputationPrepared,
): SelectionOverlayRect[] {
  if (!context.cutPreview.length) {
    return acquireOverlayRectArray()
  }

  const resources = prepared ?? prepareSelectionOverlayResources(context)
  if (!resources) {
    return acquireOverlayRectArray()
  }

  const rectangles = acquireOverlayRectArray()
  context.cutPreview.forEach((area, index) => {
    appendRectanglePayload(
      rectangles,
      area.startRow,
      area.endRow,
      area.startCol,
      area.endCol,
      `cut-preview-${index}`,
      context,
      resources.geometry,
      resources.scrollableOffsets,
      index === context.cutPreviewActiveIndex,
    )
  })

  return rectangles
}

export function computeCursorOverlayRect<RowKey = unknown>(
  context: SelectionOverlayComputationContext<RowKey>,
  prepared?: SelectionOverlayComputationPrepared,
): SelectionOverlayRect | null {
  if (!context.activeCell) {
    return null
  }

  const resources = prepared ?? prepareSelectionOverlayResources(context)
  if (!resources) {
    return null
  }

  return createCursorRect(context.activeCell, context, resources.geometry, resources.scrollableOffsets)
}

export function computeStaticRangeOverlayRects<RowKey = unknown>(
  context: SelectionOverlayComputationContext<RowKey>,
  prepared?: SelectionOverlayComputationPrepared,
): SelectionOverlayRect[] {
  if (!context.ranges.length) {
    return acquireOverlayRectArray()
  }

  const resources = prepared ?? prepareSelectionOverlayResources(context)
  if (!resources) {
    return acquireOverlayRectArray()
  }

  const rectangles = acquireOverlayRectArray()
  context.ranges.forEach((range, index) => {
    if (index === context.activeRangeIndex) {
      return
    }

    appendRectanglePayload(
      rectangles,
      range.startRow,
      range.endRow,
      range.startCol,
      range.endCol,
      `range-${index}`,
      context,
      resources.geometry,
      resources.scrollableOffsets,
      false,
    )
  })

  return rectangles
}

export function computeActiveRangeOverlayRects<RowKey = unknown>(
  context: SelectionOverlayComputationContext<RowKey>,
  prepared?: SelectionOverlayComputationPrepared,
): SelectionOverlayRect[] {
  if (!context.ranges.length) {
    return acquireOverlayRectArray()
  }

  const activeIndex = Math.min(Math.max(context.activeRangeIndex ?? -1, 0), context.ranges.length - 1)
  if (activeIndex < 0) {
    return acquireOverlayRectArray()
  }

  const range = context.ranges[activeIndex]
  if (!range) {
    return acquireOverlayRectArray()
  }

  const resources = prepared ?? prepareSelectionOverlayResources(context)
  if (!resources) {
    return acquireOverlayRectArray()
  }

  const rectangles = acquireOverlayRectArray()
  appendRectanglePayload(
    rectangles,
    range.startRow,
    range.endRow,
    range.startCol,
    range.endCol,
    `range-${activeIndex}`,
    context,
    resources.geometry,
    resources.scrollableOffsets,
    true,
  )
  return rectangles
}
