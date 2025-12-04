import type { ComputedRef } from "vue"
import type { RowData } from "@/composables/useSelectableRows"
import type {
  UiTableApiExpose,
  UiTableNavigationApi,
  UiTableSelectionApi,
  UiTableClipboardApi,
  UiTableDataApi,
  UiTablePagingApi,
  UiTableHistoryApi,
  UiTableFilterApi,
  UiTableColumnsApi,
  UiTableGroupingApi,
  UiTableLegacyExpose,
} from "../../context"

interface UseUiTableApiSelectionDeps {
  setSelection: UiTableSelectionApi["setSelection"]
  clearSelection: UiTableSelectionApi["clearSelection"]
  getSelectionSnapshot: UiTableSelectionApi["getSelectionSnapshot"]
  getSelectedCells: UiTableSelectionApi["getSelectedCells"]
  selectionMetrics: ComputedRef<ReturnType<UiTableSelectionApi["getSelectionMetrics"]>>
  selectAllRows: UiTableSelectionApi["selectAllRows"]
  clearRowSelection: UiTableSelectionApi["clearRowSelection"]
  toggleRowSelection: UiTableSelectionApi["toggleRowSelection"]
  selectedRows: ComputedRef<RowData[]>
}

interface UseUiTableApiClipboardDeps {
  copySelectionToClipboardWithFlash: UiTableClipboardApi["copySelectionToClipboard"]
  copySelectionToClipboard: UiTableClipboardApi["copySelectionToClipboardImmediate"]
  pasteClipboardDataWithFlash: UiTableClipboardApi["pasteClipboardData"]
  pasteClipboardData: UiTableClipboardApi["pasteClipboardDataImmediate"]
}

interface UseUiTableApiFilterDeps {
  onApplyFilter: UiTableFilterApi["onApplyFilter"]
  onCancelFilter: UiTableFilterApi["onCancelFilter"]
  onSortColumn: UiTableFilterApi["onSortColumn"]
  onResetFilter: UiTableFilterApi["onResetFilter"]
  resetAllFilters: UiTableFilterApi["resetAllFilters"]
  openAdvancedFilterModal: UiTableFilterApi["openAdvancedFilterModal"]
  handleAdvancedModalApply: UiTableFilterApi["handleAdvancedModalApply"]
  handleAdvancedModalClear: UiTableFilterApi["handleAdvancedModalClear"]
  handleAdvancedModalCancel: UiTableFilterApi["handleAdvancedModalCancel"]
  handleAdvancedFilterApply: UiTableFilterApi["handleAdvancedFilterApply"]
  handleAdvancedFilterClear: UiTableFilterApi["handleAdvancedFilterClear"]
  setMultiSortState: UiTableFilterApi["setMultiSortState"]
  getFilterStateSnapshot: UiTableFilterApi["getFilterStateSnapshot"]
  setFilterStateSnapshot: UiTableFilterApi["setFilterStateSnapshot"]
}

interface UseUiTableApiArgs {
  navigation: UiTableNavigationApi
  selection: UseUiTableApiSelectionDeps
  clipboard: UseUiTableApiClipboardDeps
  data: Pick<UiTableDataApi, "getCellValue" | "setCellValue" | "exportCSV" | "importCSV">
  paging: UiTablePagingApi
  history: UiTableHistoryApi
  filter: UseUiTableApiFilterDeps
  columns: UiTableColumnsApi
  grouping: UiTableGroupingApi
}

export function useUiTableApi({
  navigation,
  selection,
  clipboard,
  data,
  paging,
  history,
  filter,
  columns,
  grouping,
}: UseUiTableApiArgs): UiTableApiExpose {
  return {
    navigation,
    selection: {
      setSelection: selection.setSelection,
      clearSelection: selection.clearSelection,
      getSelectionSnapshot: selection.getSelectionSnapshot,
      getSelectedCells: selection.getSelectedCells,
      getSelectionMetrics: () => selection.selectionMetrics.value,
      selectAllRows: selection.selectAllRows,
      clearRowSelection: selection.clearRowSelection,
      toggleRowSelection: selection.toggleRowSelection,
      getSelectedRows: () => selection.selectedRows.value,
    },
    clipboard: {
      copySelectionToClipboard: clipboard.copySelectionToClipboardWithFlash,
      copySelectionToClipboardImmediate: clipboard.copySelectionToClipboard,
      pasteClipboardData: clipboard.pasteClipboardDataWithFlash,
      pasteClipboardDataImmediate: clipboard.pasteClipboardData,
    },
    data: {
      getCellValue: data.getCellValue,
      setCellValue: data.setCellValue,
      exportCSV: data.exportCSV,
      importCSV: data.importCSV,
    },
    paging,
    history,
    filter,
    columns,
    grouping,
  }
}

const LEGACY_WARNING = "[UiTable] legacy API is deprecated. Use tableApi.navigation / selection / etc."

function createLegacyWrapper<T extends (...args: any[]) => any>(fn: T, warn: () => void): T {
  return ((...args: Parameters<T>) => {
    warn()
    return fn(...args)
  }) as T
}

export function createLegacyExpose(api: UiTableApiExpose): UiTableLegacyExpose {
  const warnOnce = (() => {
    let displayed = false
    return () => {
      if (!import.meta.env?.DEV || displayed) return
      displayed = true
      if (typeof console !== "undefined" && typeof console.warn === "function") {
        console.warn(LEGACY_WARNING)
      }
    }
  })()

  return {
    scrollToRow: createLegacyWrapper(api.navigation.scrollToRow, warnOnce),
    scrollToColumn: createLegacyWrapper(api.navigation.scrollToColumn, warnOnce),
    isRowVisible: createLegacyWrapper(api.navigation.isRowVisible, warnOnce),
    focusCell: createLegacyWrapper(api.navigation.focusCell, warnOnce),
    setSelection: createLegacyWrapper(api.selection.setSelection, warnOnce),
    clearSelection: createLegacyWrapper(api.selection.clearSelection, warnOnce),
    getSelectionSnapshot: createLegacyWrapper(api.selection.getSelectionSnapshot, warnOnce),
    copySelectionToClipboard: createLegacyWrapper(api.clipboard.copySelectionToClipboard, warnOnce),
    pasteClipboardData: createLegacyWrapper(api.clipboard.pasteClipboardData, warnOnce),
    copySelectionToClipboardWithFlash: createLegacyWrapper(api.clipboard.copySelectionToClipboard, warnOnce),
    pasteClipboardDataWithFlash: createLegacyWrapper(api.clipboard.pasteClipboardData, warnOnce),
    getCellValue: createLegacyWrapper(api.data.getCellValue, warnOnce),
    setCellValue: createLegacyWrapper(api.data.setCellValue, warnOnce),
    getSelectedCells: createLegacyWrapper(api.selection.getSelectedCells, warnOnce),
    getSelectionMetrics: createLegacyWrapper(api.selection.getSelectionMetrics, warnOnce),
    exportCSV: createLegacyWrapper(api.data.exportCSV, warnOnce),
    importCSV: createLegacyWrapper(api.data.importCSV, warnOnce),
    requestPage: createLegacyWrapper(api.paging.requestPage, warnOnce),
    requestNextPage: createLegacyWrapper(api.paging.requestNextPage, warnOnce),
    resetLazyPaging: createLegacyWrapper(api.paging.resetLazyPaging, warnOnce),
    undo: createLegacyWrapper(api.history.undo, warnOnce),
    redo: createLegacyWrapper(api.history.redo, warnOnce),
    onApplyFilter: createLegacyWrapper(api.filter.onApplyFilter, warnOnce),
    onCancelFilter: createLegacyWrapper(api.filter.onCancelFilter, warnOnce),
    onSortColumn: createLegacyWrapper(api.filter.onSortColumn, warnOnce),
    onResetFilter: createLegacyWrapper(api.filter.onResetFilter, warnOnce),
    openAdvancedFilterModal: createLegacyWrapper(api.filter.openAdvancedFilterModal, warnOnce),
    handleAdvancedModalApply: createLegacyWrapper(api.filter.handleAdvancedModalApply, warnOnce),
    handleAdvancedModalClear: createLegacyWrapper(api.filter.handleAdvancedModalClear, warnOnce),
    handleAdvancedModalCancel: createLegacyWrapper(api.filter.handleAdvancedModalCancel, warnOnce),
    handleAdvancedFilterApply: createLegacyWrapper(api.filter.handleAdvancedFilterApply, warnOnce),
    handleAdvancedFilterClear: createLegacyWrapper(api.filter.handleAdvancedFilterClear, warnOnce),
    setMultiSortState: createLegacyWrapper(api.filter.setMultiSortState, warnOnce),
    handleColumnHeaderClick: createLegacyWrapper(api.columns.handleColumnHeaderClick, warnOnce),
    resetColumnVisibility: createLegacyWrapper(api.columns.resetColumnVisibility, warnOnce),
    openVisibilityPanel: createLegacyWrapper(api.columns.openVisibilityPanel, warnOnce),
    closeVisibilityPanel: createLegacyWrapper(api.columns.closeVisibilityPanel, warnOnce),
    resetAllFilters: createLegacyWrapper(api.filter.resetAllFilters, warnOnce),
    onGroupColumn: createLegacyWrapper(api.grouping.onGroupColumn, warnOnce),
    toggleGroupRow: createLegacyWrapper(api.grouping.toggleGroupRow, warnOnce),
    getFilteredRowEntries: createLegacyWrapper(api.grouping.getFilteredRowEntries, warnOnce),
    getFilteredRows: createLegacyWrapper(api.grouping.getFilteredRows, warnOnce),
    getFilterStateSnapshot: createLegacyWrapper(api.filter.getFilterStateSnapshot, warnOnce),
    setFilterStateSnapshot: createLegacyWrapper(api.filter.setFilterStateSnapshot, warnOnce),
    selectAllRows: createLegacyWrapper(api.selection.selectAllRows, warnOnce),
    clearRowSelection: createLegacyWrapper(api.selection.clearRowSelection, warnOnce),
    toggleRowSelection: createLegacyWrapper(api.selection.toggleRowSelection, warnOnce),
    getSelectedRows: createLegacyWrapper(api.selection.getSelectedRows, warnOnce),
  }
}
