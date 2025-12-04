import { DEFAULT_COLUMN_WIDTH } from "./columnSizing"

const MIN_AUTO_COLUMN_WIDTH = 80
const WIDTH_EPSILON = 0.5

export interface AutoColumnLike {
  key: string
  visible?: boolean
  sticky?: "left" | "right" | "both" | boolean | null
  stickyLeft?: boolean | number | null
  stickyRight?: boolean | number | null
  isSystem?: boolean
  width?: number | null
  minWidth?: number | null
  maxWidth?: number | null
  userResized?: boolean
  autoWidth?: boolean
  pinned?: unknown
  pin?: unknown
  lock?: unknown
  locked?: unknown
}

export interface AutoColumnResizeOptions {
  minWidth?: number
  resolvePinMode?: (column: AutoColumnLike) => "left" | "right" | "none"
}

export interface AutoColumnResizeState {
  autoResizeApplied: boolean
  manualResizeDetected: boolean
  lastViewportWidth: number
  lastSignature: string
}

export interface AutoColumnResizeContext<TColumn extends AutoColumnLike> {
  columns: TColumn[]
  viewportWidth: number
  zoom: number
}

export interface AutoColumnResizeResult {
  updates: Map<string, number>
  appliedColumns: string[]
  shareWidth: number
  viewportWidth: number
}

function defaultResolvePinMode(column: AutoColumnLike): "left" | "right" | "none" {
  if (column.isSystem) return "left"
  if (column.sticky === "left") return "left"
  if (column.sticky === "right") return "right"
  if (column.stickyLeft) return "left"
  if (column.stickyRight) return "right"

  const rawPinned = column.pinned
  const rawPin = column.pin
  const rawLock = column.lock
  const rawLocked = column.locked

  if (rawPinned === true || rawPinned === "left" || rawPin === "left" || rawLock === "left" || rawLocked === true) {
    return "left"
  }
  if (rawPinned === "right" || rawPin === "right" || rawLock === "right") {
    return "right"
  }
  return "none"
}

function resolveBaseWidth(column: AutoColumnLike): number {
  if (typeof column.width === "number" && Number.isFinite(column.width)) {
    return column.width
  }
  if (typeof column.minWidth === "number" && Number.isFinite(column.minWidth)) {
    return column.minWidth
  }
  if (typeof column.maxWidth === "number" && Number.isFinite(column.maxWidth)) {
    return column.maxWidth
  }
  return DEFAULT_COLUMN_WIDTH
}

function buildSignature(columns: AutoColumnLike[], resolvePinMode: (column: AutoColumnLike) => "left" | "right" | "none") {
  return columns
    .filter(column => column.visible !== false)
    .map(column => `${column.key}:${resolvePinMode(column)}`)
    .join("|")
}

function hasManualResize(columns: AutoColumnLike[]): boolean {
  return columns.some(column => column.userResized)
}

export function createAutoColumnResizer<TColumn extends AutoColumnLike>(
  options: AutoColumnResizeOptions = {},
) {
  const minWidth = Math.max(0, options.minWidth ?? MIN_AUTO_COLUMN_WIDTH)
  const resolvePin = options.resolvePinMode ?? defaultResolvePinMode

  const state: AutoColumnResizeState = {
    autoResizeApplied: false,
    manualResizeDetected: false,
    lastViewportWidth: 0,
    lastSignature: "",
  }

  function reset(columns?: TColumn[]) {
    state.autoResizeApplied = false
    state.lastViewportWidth = 0
    state.lastSignature = ""
    if (columns) {
      state.manualResizeDetected = hasManualResize(columns)
    } else {
      state.manualResizeDetected = false
    }
  }

  function invalidate() {
    state.autoResizeApplied = false
  }

  function markManualResize() {
    state.manualResizeDetected = true
  }

  function syncManualState(columns: TColumn[]) {
    state.manualResizeDetected = hasManualResize(columns)
  }

  function apply(context: AutoColumnResizeContext<TColumn>): AutoColumnResizeResult | null {
    const { columns, viewportWidth, zoom } = context
    if (!columns.length || viewportWidth <= 0) return null
    if (zoom <= 0) return null
    if (state.manualResizeDetected) return null

    const visibleColumns = columns.filter(column => column.visible !== false)
    if (!visibleColumns.length) return null

    const currentSignature = buildSignature(visibleColumns, resolvePin)
    const shouldAttempt =
      !state.autoResizeApplied ||
      Math.abs(state.lastViewportWidth - viewportWidth) > WIDTH_EPSILON ||
      currentSignature !== state.lastSignature

    if (!shouldAttempt) {
      return null
    }

    const pinnedColumns: TColumn[] = []
    const scrollableColumns: TColumn[] = []
    visibleColumns.forEach(column => {
      const pin = resolvePin(column)
      if (pin === "left" || pin === "right") {
        pinnedColumns.push(column)
      } else {
        scrollableColumns.push(column)
      }
    })

    const flexibleColumns = scrollableColumns.filter(column => {
      if (column.userResized) return false
      if (column.autoWidth === true) return true
      return typeof column.width !== "number" || !Number.isFinite(column.width)
    })

    if (!flexibleColumns.length) {
      state.autoResizeApplied = true
      state.lastViewportWidth = viewportWidth
      state.lastSignature = currentSignature
      return null
    }

    const zoomScale = Math.max(zoom, 0.01)
    const pinnedWidthPx = pinnedColumns.reduce((sum, column) => sum + resolveBaseWidth(column) * zoomScale, 0)
    const fixedColumns = scrollableColumns.filter(column => !flexibleColumns.includes(column))
    const fixedWidthPx = fixedColumns.reduce((sum, column) => sum + resolveBaseWidth(column) * zoomScale, 0)
    const flexibleWidthPx = flexibleColumns.reduce((sum, column) => sum + resolveBaseWidth(column) * zoomScale, 0)
    const currentTotalWidthPx = pinnedWidthPx + fixedWidthPx + flexibleWidthPx

    if (currentTotalWidthPx >= viewportWidth - WIDTH_EPSILON) {
      state.autoResizeApplied = true
      state.lastViewportWidth = viewportWidth
      state.lastSignature = currentSignature
      return null
    }

    const effectiveViewportPx = Math.max(0, viewportWidth - pinnedWidthPx)
    if (effectiveViewportPx <= 0) {
      return null
    }

    const availablePx = Math.max(0, effectiveViewportPx - fixedWidthPx)
    if (availablePx <= 0) {
      return null
    }

    const sharePx = availablePx / flexibleColumns.length
    if (!Number.isFinite(sharePx) || sharePx <= 0) {
      return null
    }

    const shareBase = sharePx / zoomScale
    const updates = new Map<string, number>()
    const appliedColumns: string[] = []

    flexibleColumns.forEach(column => {
      const currentWidth = resolveBaseWidth(column)
      const nextWidth = Math.max(column.minWidth ?? minWidth, shareBase)
      if (Math.abs(currentWidth - nextWidth) > WIDTH_EPSILON) {
        updates.set(column.key, nextWidth)
        appliedColumns.push(column.key)
      }
    })

    if (!updates.size) {
      state.autoResizeApplied = true
      state.lastViewportWidth = viewportWidth
      state.lastSignature = currentSignature
      return null
    }

    state.autoResizeApplied = true
    state.lastViewportWidth = viewportWidth
    state.lastSignature = currentSignature

    return {
      updates,
      appliedColumns,
      shareWidth: shareBase,
      viewportWidth,
    }
  }

  return {
    getState(): AutoColumnResizeState {
      return state
    },
    reset,
    invalidate,
    markManualResize,
    syncManualState,
    apply,
  }
}
