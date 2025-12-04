import { nextTick, watch, type ComputedRef, type Ref } from "vue"
import type { UiTableColumn } from "../../core/types"
import type { SelectionPoint } from "./useTableSelection"

interface UseTableFocusManagementOptions {
  anchorCell: Ref<SelectionPoint | null>
  selectedCell: Ref<SelectionPoint | null>
  isCellInSelectionRange: (rowIndex: number, colIndex: number) => boolean
  selectCell: (rowIndex: number, columnKey: string, extend: boolean, options?: { colIndex?: number }) => void
  visibleColumns: ComputedRef<UiTableColumn[]>
  containerRef: Ref<HTMLDivElement | null>
  focusableContainers: ComputedRef<HTMLElement[]>
  resolveCellElement: (rowIndex: number, columnKey: string) => HTMLElement | null
  isGridFocused: Ref<boolean>
}

interface CellFocusPayload {
  rowIndex: number
  colIndex: number
  columnKey: string
}

export function useTableFocusManagement({
  anchorCell,
  selectedCell,
  isCellInSelectionRange,
  selectCell,
  visibleColumns,
  containerRef,
  focusableContainers,
  resolveCellElement,
  isGridFocused,
}: UseTableFocusManagementOptions) {
  const isActiveCell = (rowIndex: number, colIndex: number): boolean => {
    const active = anchorCell.value ?? selectedCell.value
    return Boolean(active && active.rowIndex === rowIndex && active.colIndex === colIndex)
  }

  const getCellTabIndex = (rowIndex: number, colIndex: number): number => {
    return isActiveCell(rowIndex, colIndex) ? 0 : -1
  }

  const getCellDomId = (rowIndex: number, columnKey: string): string => {
    return `ui-table-cell-${rowIndex}-${columnKey}`
  }

  const getAriaRowIndex = (displayIndex: number): number => {
    return displayIndex + 1
  }

  const getAriaColIndex = (colIndex: number): number => {
    return colIndex + 1
  }

  const getHeaderTabIndex = (colIndex: number): number => {
    const active = anchorCell.value ?? selectedCell.value
    if (active?.colIndex === colIndex) return 0
    if (!active && colIndex === 0) return 0
    return -1
  }

  const focusActiveCellElement = (preventScroll = true): boolean => {
    const active = anchorCell.value ?? selectedCell.value
    if (!active) return false
    const column = visibleColumns.value[active.colIndex]
    if (!column) return false
    const cell = resolveCellElement(active.rowIndex, column.key)
    if (!cell) return false
    cell.focus({ preventScroll })
    isGridFocused.value = true
    return true
  }

  const onCellComponentFocus = (payload: CellFocusPayload) => {
    const focused = selectedCell.value ?? anchorCell.value
    const withinRange = isCellInSelectionRange(payload.rowIndex, payload.colIndex)
    if (focused && focused.rowIndex === payload.rowIndex && focused.colIndex === payload.colIndex) {
      isGridFocused.value = true
      return
    }

    if (withinRange) {
      isGridFocused.value = true
      return
    }

    selectCell(payload.rowIndex, payload.columnKey, false, { colIndex: payload.colIndex })
    isGridFocused.value = true
  }

  const onGridFocusIn = (event: FocusEvent) => {
    const container = containerRef.value
    if (!container) return
    isGridFocused.value = true
    if (event.target === container) {
      nextTick(() => focusActiveCellElement())
    }
  }

  const isTargetWithinTable = (node: Node | null): boolean => {
    if (!node) {
      return false
    }
    if (containerRef.value?.contains(node)) {
      return true
    }
    for (const surface of focusableContainers.value) {
      if (surface?.contains(node as Node)) {
        return true
      }
    }
    return false
  }

  const onGridFocusOut = (event: FocusEvent) => {
    const container = containerRef.value
    if (!container) {
      isGridFocused.value = false
      return
    }
    const nextTarget = event.relatedTarget as Node | null
    if (!isTargetWithinTable(nextTarget)) {
      isGridFocused.value = false
    }
  }

  watch(anchorCell, () => {
    if (!isGridFocused.value) return
    nextTick(() => focusActiveCellElement())
  })

  return {
    isActiveCell,
    getCellTabIndex,
    getCellDomId,
    getAriaRowIndex,
    getAriaColIndex,
    getHeaderTabIndex,
    focusActiveCellElement,
    onCellComponentFocus,
    onGridFocusIn,
    onGridFocusOut,
  }
}
