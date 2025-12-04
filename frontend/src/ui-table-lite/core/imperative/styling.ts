import type { UiTableBodyBindings, ClassLike } from "./bindings.js"
import type { UiTableColumn } from "../types/index.js"
import { mergeClasses } from "./dom.js"
import type { CellSlot, CellRenderState } from "./types.js"
import type { ResolvedImperativeBodyClassMap } from "./viewConfig.js"

interface ComputeCellClassOptions {
  cell: CellSlot
  state: CellRenderState
  column: UiTableColumn
  editing: boolean
  body: UiTableBodyBindings
  classMap: ResolvedImperativeBodyClassMap
}

export function computeCellClass(options: ComputeCellClassOptions): string {
  const { cell, state, editing, body, classMap } = options

  if (cell.type === "selection") {
    return mergeClasses(
      cell.baseClass,
      body.bodySelectionCellClass.value,
      state.isRowSelected ? "ui-table__selection-cell--row-selected" : "",
      state.isSelected ? "ui-table__selection-cell--active" : "",
      state.isRangeSelected ? "ui-table__selection-cell--range" : "",
    )
  }

  if (cell.type === "index") {
    return mergeClasses(
      cell.baseClass,
      body.bodyIndexCellClass.value,
      state.rowHeaderClass,
      state.sticky ? `ui-table-cell--sticky-${state.stickySide ?? "left"}` : "",
    )
  }

  const classes: ClassLike[] = [
    cell.baseClass,
    state.sticky ? `ui-table-cell--sticky-${state.stickySide ?? "left"}` : "",
    state.isCursor ? classMap.state.cursorCell : "",
    state.isAnchor ? classMap.state.anchorCell : "",
    state.validationError ? "ui-table-cell--invalid" : "",
    state.activeSearchMatch ? "ui-table-cell--search-active" : state.searchMatch ? "ui-table-cell--search-match" : "",
    editing ? classMap.state.editing : classMap.state.readOnly,
  ] as const

  const extra: ClassLike[] = []

  if (!editing) {
    if (state.isCursor && classMap.state.activeCell) {
      extra.push(classMap.state.activeCell)
    } else if (state.isRowSelected && classMap.state.rowSelected) {
      extra.push(classMap.state.rowSelected)
    } else if (state.isColumnSelected && classMap.state.columnSelected) {
      extra.push(classMap.state.columnSelected)
    } else if (state.isRangeSelected && classMap.state.rangeSelected) {
      extra.push(classMap.state.rangeSelected)
    }

    if (state.activeSearchMatch && classMap.state.searchMatchActive) {
      extra.push(classMap.state.searchMatchActive)
    } else if (state.searchMatch && classMap.state.searchMatch) {
      extra.push(classMap.state.searchMatch)
    }

    if (state.validationError && classMap.state.validationError) {
      extra.push(classMap.state.validationError)
    }
  }

  return mergeClasses(...classes, ...extra)
}

interface HighlightStateOptions {
  cell: CellSlot
  state: CellRenderState
  column: UiTableColumn
  editing: boolean
}

export function computeHighlightState(options: HighlightStateOptions): boolean {
  const { cell, state, column, editing } = options
  if (cell.type !== "data") return false
  if (column.isSystem) return false
  if (editing) return false
  if (state.isRowSelected || state.isColumnSelected || state.isRangeSelected) return true
  if (state.searchMatch || state.activeSearchMatch) return true
  if (state.validationError) return true
  return false
}

interface PinnedBackgroundOptions {
  state: CellRenderState
  editing: boolean
}

function shouldSuppressPinnedBackground(options: PinnedBackgroundOptions): boolean {
  const { state, editing } = options
  if (!state.stickySide) return true
  if (editing) return true
  if (state.isRowSelected || state.isColumnSelected || state.isRangeSelected) return true
  if (state.validationError) return true
  if (state.searchMatch || state.activeSearchMatch) return true
  return false
}

export function resolvePinnedBackground(options: PinnedBackgroundOptions): string {
  if (shouldSuppressPinnedBackground(options)) return ""
  const { state } = options
  if (state.stickySide === "left") {
    return "var(--ui-table-pinned-left-bg, var(--ui-table-pinned-bg, var(--ui-table-row-background-color)))"
  }
  if (state.stickySide === "right") {
    return "var(--ui-table-pinned-right-bg, var(--ui-table-pinned-bg, var(--ui-table-row-background-color)))"
  }
  return ""
}

// Uses a gradient instead of a physical border so pinned dividers avoid repeated layout.
export function resolvePinnedDividerGradient(state: CellRenderState): string {
  if (!state.stickySide) return ""
  if (state.stickySide === "left") {
    return "linear-gradient(to left, transparent calc(100% - var(--ui-table-pinned-left-divider-width, var(--ui-table-pinned-left-border-width, 1px))), var(--ui-table-pinned-left-divider-color, var(--ui-table-pinned-left-border-color, var(--ui-table-cell-border-color))) calc(100% - var(--ui-table-pinned-left-divider-width, var(--ui-table-pinned-left-border-width, 1px))))"
  }
  if (state.stickySide === "right") {
    return "linear-gradient(to right, transparent calc(100% - var(--ui-table-pinned-right-divider-width, var(--ui-table-pinned-right-border-width, 1px))), var(--ui-table-pinned-right-divider-color, var(--ui-table-pinned-right-border-color, var(--ui-table-cell-border-color))) calc(100% - var(--ui-table-pinned-right-divider-width, var(--ui-table-pinned-right-border-width, 1px))))"
  }
  return ""
}

interface ComputeBoxShadowOptions {
  cell: CellSlot
  state: CellRenderState
  column: UiTableColumn
  editing: boolean
}

export function computeBoxShadow(options: ComputeBoxShadowOptions): string {
  const { cell, state, column, editing } = options
  const parts: string[] = []

  if (cell.type !== "data" || column.isSystem) {
    return parts.join(", ")
  }

  if (state.validationError && !editing) {
    parts.push("inset 0 0 0 1px rgba(248, 113, 113, 0.75)")
  }

  if (state.activeSearchMatch) {
    parts.push("inset 0 0 0 2px rgba(37, 99, 235, 0.85)")
  } else if (state.searchMatch) {
    parts.push("inset 0 0 0 2px rgba(250, 204, 21, 0.7)")
  }

  return parts.join(", ")
}

interface ApplyCutPreviewOptions {
  state: CellRenderState
  style: Record<string, unknown>
}

export function applyCutPreviewStyles(options: ApplyCutPreviewOptions) {
  const { style } = options
  style["border-top"] = ""
  style["border-bottom"] = ""
  style["border-left"] = ""
  style["border-right"] = ""
}
