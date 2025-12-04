import { computed } from "vue"
import type { ComputedRef } from "vue"
import type { NormalizedTableProps } from "../../core/config/tableConfig"
import { BUILTIN_SELECTION_METRIC_LABELS } from "../../core/config/tableConfig"
import type { UiTableSelectedCell, UiTableSelectionMetricResult, VisibleRow } from "../../core/types"
import {
  computeSelectionMetrics,
  type NumberFormatterResolver,
} from "../../core/selection/selectionMetrics"
import type { RowData, UseSelectableRowsResult } from "@/composables/useSelectableRows"

export interface UseTableStatusMetricsOptions {
  normalizedProps: ComputedRef<NormalizedTableProps>
  sortedRows: ComputedRef<VisibleRow[]>
  rowSelection: Pick<UseSelectableRowsResult<RowData>, "selectedKeySet">
  rowCountFormatter: Intl.NumberFormat
  selectionMetricsConfig: ComputedRef<NormalizedTableProps["selectionMetrics"]>
  getSelectedCells: () => UiTableSelectedCell[]
  resolveSelectionMetricFormatter: NumberFormatterResolver
}

export function useTableStatusMetrics(options: UseTableStatusMetricsOptions) {
  const totalRowCountDisplay = computed(() => options.normalizedProps.value.totalRows ?? options.sortedRows.value.length)

  const formattedRowCount = computed(() => options.rowCountFormatter.format(Math.max(0, totalRowCountDisplay.value)))

  const selectedRowCount = computed(() => options.rowSelection.selectedKeySet.value.size)

  const formattedSelectedRowCount = computed(() => options.rowCountFormatter.format(selectedRowCount.value))

  const selectionMetrics = computed<UiTableSelectionMetricResult[]>(() => {
    const cells = options.getSelectedCells()

    return computeSelectionMetrics({
      config: options.selectionMetricsConfig.value,
      cells,
      labelMap: BUILTIN_SELECTION_METRIC_LABELS,
      getNumberFormatter: options.resolveSelectionMetricFormatter,
    })
  })

  return {
    totalRowCountDisplay,
    formattedRowCount,
    selectedRowCount,
    formattedSelectedRowCount,
    selectionMetrics,
  }
}
