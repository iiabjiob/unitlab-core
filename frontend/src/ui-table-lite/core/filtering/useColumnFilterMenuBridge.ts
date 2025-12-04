import type { SortDirection } from "../sorting/tableSorting"
import type { FilterMenuState } from "../types/filters"

interface ColumnFilterMenuOptions {
  filterMenuState: FilterMenuState
  confirmFilterSelection: () => void
  cancelFilterSelection: () => void
  applySort: (columnKey: string, direction: SortDirection) => void
  clearFilterForColumn: (columnKey: string) => void
}

export function useColumnFilterMenuBridge({
  filterMenuState,
  confirmFilterSelection,
  cancelFilterSelection,
  applySort,
  clearFilterForColumn,
}: ColumnFilterMenuOptions) {
  function onApplyFilter() {
    confirmFilterSelection()
  }

  function onCancelFilter() {
    cancelFilterSelection()
  }

  function onSortColumn(direction: string) {
    const columnKey = filterMenuState.columnKey
    if (!columnKey) return
    const normalized = normalizeDirection(direction)
    if (!normalized) return
    applySort(columnKey, normalized)
    cancelFilterSelection()
  }

  function onResetFilter() {
    const columnKey = filterMenuState.columnKey
    if (!columnKey) return
    clearFilterForColumn(columnKey)
  }

  return {
    onApplyFilter,
    onCancelFilter,
    onSortColumn,
    onResetFilter,
  }
}

function normalizeDirection(direction: string): SortDirection | null {
  if (direction === "asc" || direction === "desc") {
    return direction
  }
  return null
}
