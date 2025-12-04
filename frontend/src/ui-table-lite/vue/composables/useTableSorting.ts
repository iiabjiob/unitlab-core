import { computed, ref, watch } from "vue"
import type { Ref } from "vue"
import { createTableSorting } from "../../core/sorting/tableSorting"
import type { SortDirection, SortState } from "../../core/sorting/tableSorting"
import type { UiTableColumn } from "../../core/types"

interface VisibleRow {
  row: any
  originalIndex: number
}

export type { SortDirection, SortState } from "../../core/sorting/tableSorting"

interface UseTableSortingOptions {
  rows: () => any[]
  localColumns: Ref<UiTableColumn[]>
  emitSortChange: (state: SortState | null) => void
}

export function useTableSorting({ rows, localColumns, emitSortChange }: UseTableSortingOptions) {
  const sorting = createTableSorting({
    getRows: rows,
    getColumnByKey(key: string) {
      return localColumns.value.find(column => column.key === key)
    },
    onPrimarySortChange: emitSortChange,
  })

  const multiSortState = ref<SortState[]>(sorting.getMultiSortState())
  const sortedOrder = ref<number[] | null>(sorting.getSortedOrder())

  const sortState = computed<SortState | null>(() => multiSortState.value[0] ?? null)

  function syncState() {
    multiSortState.value = sorting.getMultiSortState()
    sortedOrder.value = sorting.getSortedOrder()
  }

  function setMultiSortState(next: SortState[]) {
    sorting.setMultiSortState(next)
    syncState()
  }

  function applySort(columnKey: string, direction: SortDirection) {
    sorting.applySort(columnKey, direction)
    syncState()
  }

  function getSortDirectionForColumn(columnKey: string): SortDirection | null {
    const match = multiSortState.value.find(entry => entry.key === columnKey)
    return match?.direction ?? null
  }

  function getSortPriorityForColumn(columnKey: string): number | null {
    const index = multiSortState.value.findIndex(entry => entry.key === columnKey)
    return index === -1 ? null : index + 1
  }

  function applySorting<T extends VisibleRow>(entries: T[]): T[] {
    const hasActiveSorts = multiSortState.value.length > 0
    if (hasActiveSorts) {
      // Touch sorted order so reactive consumers (e.g., computed sorted rows) update when the order changes.
      void sortedOrder.value
    }
    return sorting.applySorting(entries)
  }

  function toggleColumnSort(columnKey: string, additive: boolean) {
    sorting.toggleColumnSort(columnKey, additive)
    syncState()
  }

  function clearSortForColumn(columnKey: string) {
    sorting.clearSortForColumn(columnKey)
    syncState()
  }

  function applyMultiSort<T extends Record<string, unknown>>(data: T[]): T[] {
    if (multiSortState.value.length > 0) {
      // Ensure consumers react to sort order changes even when data reference stays the same length.
      void sortedOrder.value
    }
    return sorting.applyMultiSort(data)
  }

  function ensureSortedOrder() {
    sorting.ensureSortedOrder()
    syncState()
  }

  function recomputeSortedOrder() {
    sorting.recomputeSortedOrder()
    syncState()
  }

  watch(
    () => rows().length,
    () => {
      sorting.notifyDatasetChanged()
      syncState()
    },
  )

  return {
    sortState,
    multiSortState,
    sortedOrder,
    applySort,
    toggleColumnSort,
    clearSortForColumn,
    getSortDirectionForColumn,
    getSortPriorityForColumn,
    ensureSortedOrder,
    recomputeSortedOrder,
    applySorting,
    applyMultiSort,
    setMultiSortState,
  }
}
