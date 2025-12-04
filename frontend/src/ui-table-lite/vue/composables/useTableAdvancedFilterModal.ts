import { computed, reactive, nextTick, type ComputedRef } from "vue"
import type { UiTableColumn } from "../../core/types"
import type { FilterCondition } from "./useTableFilters"

interface UseTableAdvancedFilterModalOptions {
  visibleColumns: ComputedRef<UiTableColumn[]>
  getAdvancedFilter: (columnKey: string) => FilterCondition | null
  setAdvancedFilter: (columnKey: string, condition: FilterCondition | null) => void
  clearAdvancedFilter: (columnKey: string) => void
  refresh: (force?: boolean) => void
  scheduleOverlayUpdate: () => void
  closeActiveMenu: () => void
}

export interface UseTableAdvancedFilterModalResult {
  advancedModalState: { open: boolean; columnKey: string | null }
  advancedModalColumn: ComputedRef<UiTableColumn | null>
  advancedModalType: ComputedRef<FilterCondition["type"]>
  advancedModalCondition: ComputedRef<FilterCondition | null>
  openAdvancedFilterModal: (columnKey: string) => void
  handleAdvancedModalApply: (condition: FilterCondition | null) => void
  handleAdvancedModalClear: () => void
  handleAdvancedModalCancel: () => void
  handleAdvancedFilterApply: (columnKey: string, condition: FilterCondition | null) => void
  handleAdvancedFilterClear: (columnKey: string) => void
}

export function useTableAdvancedFilterModal(options: UseTableAdvancedFilterModalOptions): UseTableAdvancedFilterModalResult {
  const advancedModalState = reactive<{ open: boolean; columnKey: string | null }>({
    open: false,
    columnKey: null,
  })

  const advancedModalColumn = computed(() => {
    if (!advancedModalState.columnKey) return null
    return options.visibleColumns.value.find(column => column.key === advancedModalState.columnKey) ?? null
  })

  const advancedModalType = computed<FilterCondition["type"]>(() => {
    const type = advancedModalColumn.value?.filterType
    if (type === "number" || type === "date") {
      return type
    }
    return "text"
  })

  const advancedModalCondition = computed(() => {
    if (!advancedModalState.columnKey) return null
    return options.getAdvancedFilter(advancedModalState.columnKey)
  })

  const applyAdvancedFilterCondition = (columnKey: string, condition: FilterCondition | null) => {
    if (condition && condition.clauses?.length) {
      options.setAdvancedFilter(columnKey, condition)
    } else {
      options.clearAdvancedFilter(columnKey)
    }

    nextTick(() => {
      options.refresh(true)
      options.scheduleOverlayUpdate()
    })
  }

  const openAdvancedFilterModal = (columnKey: string) => {
    advancedModalState.columnKey = columnKey
    advancedModalState.open = true
    options.closeActiveMenu()
  }

  const handleAdvancedModalCancel = () => {
    advancedModalState.open = false
    advancedModalState.columnKey = null
  }

  const handleAdvancedModalApply = (condition: FilterCondition | null) => {
    const targetColumn = advancedModalState.columnKey
    if (!targetColumn) {
      handleAdvancedModalCancel()
      return
    }

    applyAdvancedFilterCondition(targetColumn, condition)
    handleAdvancedModalCancel()
  }

  const handleAdvancedModalClear = () => {
    const targetColumn = advancedModalState.columnKey
    if (!targetColumn) return
    options.clearAdvancedFilter(targetColumn)
  }

  const handleAdvancedFilterApply = (columnKey: string, condition: FilterCondition | null) => {
    applyAdvancedFilterCondition(columnKey, condition)
  }

  const handleAdvancedFilterClear = (columnKey: string) => {
    options.clearAdvancedFilter(columnKey)
  }

  return {
    advancedModalState,
    advancedModalColumn,
    advancedModalType,
    advancedModalCondition,
    openAdvancedFilterModal,
    handleAdvancedModalApply,
    handleAdvancedModalClear,
    handleAdvancedModalCancel,
    handleAdvancedFilterApply,
    handleAdvancedFilterClear,
  }
}
