export const DEFAULT_COLUMN_WIDTH = 160
export const COLUMN_VIRTUALIZATION_BUFFER = 2

export interface ColumnSizeLike {
  width?: number | null
  minWidth?: number | null
  maxWidth?: number | null
}

export interface ColumnWidthMetrics {
  widths: number[]
  offsets: number[]
  totalWidth: number
}

export interface VisibleColumnRange {
  startIndex: number
  endIndex: number
  leftPadding: number
  rightPadding: number
}

const visibleColumnResultPool: Array<VisibleColumnRange & ColumnWidthMetrics> = [
  {
    startIndex: 0,
    endIndex: 0,
    leftPadding: 0,
    rightPadding: 0,
    widths: [],
    offsets: [],
    totalWidth: 0,
  },
  {
    startIndex: 0,
    endIndex: 0,
    leftPadding: 0,
    rightPadding: 0,
    widths: [],
    offsets: [],
    totalWidth: 0,
  },
]

let visibleColumnResultIndex = 0

export function resolveColumnWidth<T extends ColumnSizeLike>(column: T, zoom = 1): number {
  const fallbackWidth = column.width ?? column.minWidth ?? column.maxWidth ?? DEFAULT_COLUMN_WIDTH
  const baseWidth = column.width ?? fallbackWidth
  const minWidth = column.minWidth ?? fallbackWidth
  const maxWidth = column.maxWidth ?? fallbackWidth
  const normalized = Math.max(minWidth, Math.min(maxWidth, baseWidth)) * zoom
  if (!Number.isFinite(normalized)) {
    return DEFAULT_COLUMN_WIDTH * zoom
  }
  return normalized
}

export function accumulateColumnWidths<T extends ColumnSizeLike>(columns: readonly T[], zoom = 1): ColumnWidthMetrics {
  const widths: number[] = []
  const offsets: number[] = []
  let totalWidth = 0

  for (const column of columns) {
    offsets.push(totalWidth)
    const width = resolveColumnWidth(column, zoom)
    widths.push(width)
    totalWidth += width
  }

  return { widths, offsets, totalWidth }
}

function findFirstVisibleColumn(scrollLeft: number, widths: readonly number[], offsets: readonly number[]) {
  let low = 0
  let high = widths.length - 1
  let candidate = widths.length

  while (low <= high) {
    const mid = Math.floor((low + high) / 2)
    const columnStart = offsets[mid]
    const columnEnd = columnStart + widths[mid]
    if (columnEnd >= scrollLeft) {
      candidate = mid
      high = mid - 1
    } else {
      low = mid + 1
    }
  }

  return Math.min(candidate, widths.length)
}

function findLastVisibleColumn(scrollRight: number, widths: readonly number[], offsets: readonly number[]) {
  let low = 0
  let high = widths.length - 1
  let candidate = -1

  while (low <= high) {
    const mid = Math.floor((low + high) / 2)
    const columnStart = offsets[mid]
    if (columnStart <= scrollRight) {
      candidate = mid
      low = mid + 1
    } else {
      high = mid - 1
    }
  }

  return Math.min(candidate, widths.length - 1)
}

export function calculateVisibleColumns<T extends ColumnSizeLike>(
  scrollLeft: number,
  containerWidth: number,
  columns: readonly T[],
  options: {
    zoom?: number
    pinnedLeftWidth?: number
    pinnedRightWidth?: number
    metrics?: ColumnWidthMetrics
  } = {},
): VisibleColumnRange & ColumnWidthMetrics {
  const {
    zoom = 1,
    pinnedLeftWidth = 0,
    pinnedRightWidth = 0,
    metrics: suppliedMetrics,
  } = options

  const metrics = suppliedMetrics ?? accumulateColumnWidths(columns, zoom)
  const { widths, offsets, totalWidth } = metrics

  const result = visibleColumnResultPool[visibleColumnResultIndex]
  visibleColumnResultIndex = (visibleColumnResultIndex + 1) % visibleColumnResultPool.length
  result.widths = widths
  result.offsets = offsets
  result.totalWidth = totalWidth

  if (!widths.length) {
    result.startIndex = 0
    result.endIndex = 0
    result.leftPadding = 0
    result.rightPadding = 0
    return result
  }

  const effectiveViewportWidth = Math.max(0, containerWidth - pinnedLeftWidth - pinnedRightWidth)
  if (totalWidth <= effectiveViewportWidth) {
    result.startIndex = 0
    result.endIndex = widths.length
    result.leftPadding = 0
    result.rightPadding = 0
    return result
  }

  const effectiveScrollLeft = Math.max(0, scrollLeft - pinnedLeftWidth)
  const scrollRight = effectiveScrollLeft + effectiveViewportWidth

  const firstVisible = findFirstVisibleColumn(effectiveScrollLeft, widths, offsets)
  const lastVisible = findLastVisibleColumn(scrollRight, widths, offsets)

  const visibleStartIndex = Math.min(firstVisible, widths.length)
  const visibleEndIndex = Math.min(lastVisible + 1, widths.length)

  const leftPadding = offsets[visibleStartIndex] ?? 0
  const rightPadding = Math.max(0, totalWidth - (offsets[visibleEndIndex] ?? totalWidth))

  result.startIndex = visibleStartIndex
  result.endIndex = visibleEndIndex
  result.leftPadding = leftPadding
  result.rightPadding = rightPadding

  return result
}
