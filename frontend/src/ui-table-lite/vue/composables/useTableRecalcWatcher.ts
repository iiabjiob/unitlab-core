import { watch, nextTick, type ComputedRef, type Ref } from "vue"
import type { UiTableColumn, VisibleRow } from "../../core/types"
import type { FilterCondition } from "./useTableFilters"

export interface TableRecalcRequestOptions {
  refresh?: boolean
  overlay?: boolean
  viewport?: boolean
}

interface UseTableRecalcWatcherOptions {
  resolvedRows: Ref<VisibleRow[]>
  processedRows: Ref<VisibleRow[]>
  columnFilters: Ref<Record<string, string[]>>
  filtersState: Ref<Record<string, FilterCondition>>
  totalRowCount: Ref<number>
  visibleColumns: Ref<UiTableColumn[]>
  summaryRow: ComputedRef<unknown>
  validationErrors: Record<string, string | null>
  ensureSortedOrder: () => void
  measureRowHeight: () => void
  requestTableRecalc: (reason: string, options?: TableRecalcRequestOptions) => void
}

export function useTableRecalcWatcher(options: UseTableRecalcWatcherOptions) {
  const {
    resolvedRows,
    processedRows,
    columnFilters,
    filtersState,
    totalRowCount,
    visibleColumns,
    summaryRow,
    validationErrors,
    ensureSortedOrder,
    measureRowHeight,
    requestTableRecalc,
  } = options

  watch(
    () => resolvedRows.value.length,
    () => {
      ensureSortedOrder()
      nextTick(() => requestTableRecalc("resolved-rows", { overlay: true, viewport: true }))
    },
  )

  watch(
    () => processedRows.value,
    () => requestTableRecalc("processed-rows"),
  )

  watch(
    () => [columnFilters.value, filtersState.value],
    () => {
      requestTableRecalc("filters-state", { overlay: true })
    },
    { deep: true },
  )

  watch(
    totalRowCount,
    () => {
      nextTick(() => {
        const maxRow = totalRowCount.value - 1
        for (const key of Object.keys(validationErrors)) {
          const [row] = key.split(":")
          const index = Number(row)
          if (Number.isInteger(index) && index > maxRow) {
            delete validationErrors[key]
          }
        }
      })
    },
  )

  watch(
    summaryRow,
    () => {
      nextTick(() => measureRowHeight())
    },
  )

  watch(
    () => visibleColumns.value.length,
    () => {
      nextTick(() => requestTableRecalc("visible-columns", { overlay: true, viewport: true }))
    },
  )
}
