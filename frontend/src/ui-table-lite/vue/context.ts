import type { InjectionKey, Ref, ComputedRef, CSSProperties, ComponentPublicInstance, Slots } from "vue"
import type {
  UiTableColumn,
  VisibleRow,
  UiTableSelectionSnapshot,
  UiTableSelectionRangeInput,
  UiTableSelectedCell,
  UiTableSelectionMetricResult,
  UiTableLazyLoadReason,
} from "../core/types"
import type { ColumnMetric } from "./composables/useAxisVirtualizer"
import type { ColumnGroupRenderRows } from "../core/columns/columnGroup"
import type { FilterMenuState, FilterOption, FilterCondition, FilterStateSnapshot } from "./composables/useTableFilters"
import type { HeaderRenderableEntry } from "../core/types/internal"
import type { UseSelectableRowsResult, RowData, RowKey } from "@/composables/useSelectableRows"
import type { RowPoolItem } from "./composables/useTableViewport"
import type { SortState } from "./composables/useTableSorting"
import type { SelectionPoint, SelectionRange } from "./composables/useTableSelection"
import type { ImperativeSelectionSnapshot } from "../core/imperative/types"

export type UiTableRowRegion = "pinned-left" | "center" | "pinned-right"

export type UiTableRowCellKind = "data" | "selection" | "index"

export interface GroupSelectionState {
  checked: boolean
  indeterminate: boolean
  selectable: boolean
  total: number
  selected: number
}

export interface UiTableRowCellDescriptor {
  key: string
  kind: UiTableRowCellKind
  classList: Array<string | string[] | Record<string, boolean> | undefined>
  style: CSSProperties
  columnKey: string
  binding: UiTableColumnBinding
  cellProps?: Record<string, unknown>
  hasCustomRenderer?: boolean
  selectionState?: {
    rowData: unknown
    checked: boolean
    columnIndex: number
  }
  indexDisplay?: number
}

export type UiTableRowViewModel =
  | { kind: "empty" }
  | {
      kind: "group"
      classList: Array<string | string[] | Record<string, boolean> | undefined>
      cellClassList: Array<string | string[] | Record<string, boolean> | undefined>
      caretClassList: Array<string | string[] | Record<string, boolean> | undefined>
      ariaColCount: number
      cellStyle: Record<string, string>
      rowKey: string
      label: string
      size: number
      level: number | null | undefined
      expanded: boolean
    }
  | {
      kind: "data"
      classList: Array<string | string[] | Record<string, boolean> | undefined>
      style: CSSProperties
      ariaRowIndex: number | null
      displayIndex: number
      cells: UiTableRowCellDescriptor[]
    }

export type ColumnPinPosition = "left" | "right" | "none"

export interface ColumnVisibilitySnapshot {
  key: string
  label?: string
  visible: boolean
}

export interface UiTableColumnStorage {
  get: (storageKey: string) => ColumnVisibilitySnapshot[] | null | undefined
  set: (storageKey: string, snapshot: ColumnVisibilitySnapshot[]) => void
  remove?: (storageKey: string) => void
}

export const UiTableColumnStorageKey: InjectionKey<UiTableColumnStorage> = Symbol("UiTableColumnStorage")

export interface UiTableHeaderBindings {
  headerRef: Ref<HTMLElement | null>
  headerRowStickyStyle: ComputedRef<CSSProperties>
  hasColumnGroups: ComputedRef<boolean>
  headerGroupRows: ComputedRef<ColumnGroupRenderRows>
  headerPinnedLeftGroupRows: ComputedRef<ColumnGroupRenderRows>
  headerMainGroupRows: ComputedRef<ColumnGroupRenderRows>
  headerPinnedRightGroupRows: ComputedRef<ColumnGroupRenderRows>
  headerRowClass: ComputedRef<string | string[] | Record<string, boolean> | undefined>
  headerRenderableEntries: ComputedRef<HeaderRenderableEntry[]>
  headerPinnedLeftEntries: ComputedRef<HeaderRenderableEntry[]>
  headerMainEntries: ComputedRef<HeaderRenderableEntry[]>
  headerPinnedRightEntries: ComputedRef<HeaderRenderableEntry[]>
  systemColumnStyle: (column: UiTableColumn) => CSSProperties
  headerSelectionCellClass: ComputedRef<string | string[] | Record<string, boolean> | undefined>
  setHeaderSelectionCheckboxRef: (element: Element | ComponentPublicInstance | null) => void
  rowSelection: UseSelectableRowsResult<RowData>
  selectableRowCount: ComputedRef<number>
  headerSelectionName: ComputedRef<string>
  handleHeaderSelectionChange: (event: Event) => void
  headerCellClass: (column: UiTableColumn) => string | undefined
  isColumnFullySelected: (columnIndex: number) => boolean
  getColumnIndex: (columnKey: string) => number
  isColumnHeaderHighlighted: (columnIndex: number) => boolean
  getSortDirectionForColumn: (columnKey: string) => "asc" | "desc" | null
  getSortPriorityForColumn: (columnKey: string) => number | null
  isFilterActiveForColumn: (columnKey: string) => boolean
  filterMenuState: FilterMenuState
  activeMenuOptions: ComputedRef<FilterOption[]>
  effectiveMenuSelectedKeys: ComputedRef<string[]>
  zoom: Ref<number>
  columnWidthMap: Ref<Map<string, number>>
  columnWidthDomMap: ComputedRef<Map<string, number>>
  getHeaderTabIndex: (columnIndex: number) => number
  getAriaColIndex: (columnIndex: number) => number
  containerRef: Ref<HTMLDivElement | null>
  splitPinnedLayoutEnabled: ComputedRef<boolean>
  isColumnSticky: (column: UiTableColumn) => boolean
  getStickySide: (column: UiTableColumn) => "left" | "right" | null
  getStickyLeftOffset: (column: UiTableColumn) => number | undefined
  getStickyRightOffset: (column: UiTableColumn) => number | undefined
  groupedColumnSet: ComputedRef<Set<string>>
  groupOrderMap: ComputedRef<Map<string, number>>
  handleHeaderReorder: (entries: HeaderRenderableEntry[]) => void
  isSelectAllChecked: ComputedRef<boolean>
  isSelectAllIndeterminate: ComputedRef<boolean>
  setFilterMenuSearchRef: (element: Element | { $el?: Element | null } | null) => void
  getAdvancedFilter: (columnKey: string) => FilterCondition | null | undefined
  resolveColumnPinState: (column: UiTableColumn) => ColumnPinPosition
  loadFilterOptions: (args: { search: string; offset?: number; limit?: number }) => Promise<FilterOption[]> | FilterOption[]
  filterSearchDebounce: ComputedRef<number | undefined>
  groupedColumns: ComputedRef<string[]>
  toggleFilterOption: (optionKey: string) => void
  toggleSelectAll: (checked: boolean) => void
  onApplyFilter: () => void
  onCancelFilter: () => void
  onSortColumn: (direction: string) => void
  onResetFilter: () => void
  onGroupColumn: (column: UiTableColumn) => void
  handleColumnPin: (column: UiTableColumn, position: ColumnPinPosition) => void
  openAdvancedFilterModal: (columnKey: string) => void
  onColumnResize: (columnKey: string, width: number) => void
  autoResizeColumn: (column: UiTableColumn) => void
  handleColumnHeaderClick: (column: UiTableColumn, columnIndex: number, event: MouseEvent | KeyboardEvent) => void
  onColumnMenuOpen: (columnKey: string, close: () => void) => void
  onColumnMenuClose: (columnKey: string) => void
  hideColumn: (columnKey: string) => void
  SELECTION_COLUMN_KEY: string
  resolveColumnSurface: (region: UiTableRowRegion, columnKey: string) => { left: number; width: number } | null
}

export const UiTableHeaderContextKey: InjectionKey<UiTableHeaderBindings> = Symbol("UiTableHeaderContext")

export interface UiTableColumnBinding {
  column: UiTableColumn
  columnIndex: number
  visualWidth: number
  cellClass?: string
  sticky: boolean
  stickySide: "left" | "right" | null
  stickyLeftOffset?: number
  stickyRightOffset?: number
  ariaColIndex: number
}

export interface SelectionAreaDescriptor {
  startRow: number
  endRow: number
  startCol: number
  endCol: number
}

export interface UiTableBodyBindings {
  virtualContainerStyle: ComputedRef<CSSProperties>
  pooledRows: ComputedRef<RowPoolItem[]>
  rowLayerStyle: (pooled: RowPoolItem) => CSSProperties
  isHoverableTable: ComputedRef<boolean>
  isGroupRowEntry: (entry: VisibleRow | null | undefined) => boolean
  groupRowClass: ComputedRef<string>
  groupCellClass: ComputedRef<string>
  groupCaretClass: ComputedRef<string>
  groupCellStyle: (level: number | null | undefined) => Record<string, string>
  toggleGroupRow: (key: string) => void
  isGroupExpanded: (key: string) => boolean
  ariaColCount: ComputedRef<number>
  rowGridClass: (row: any) => string | undefined
  splitPinnedLayoutEnabled: ComputedRef<boolean>
  headerPinnedLeftEntries: ComputedRef<HeaderRenderableEntry[]>
  headerMainEntries: ComputedRef<HeaderRenderableEntry[]>
  headerPinnedRightEntries: ComputedRef<HeaderRenderableEntry[]>
  headerRenderableEntries: ComputedRef<HeaderRenderableEntry[]>
  columnBindings: ComputedRef<Map<string, UiTableColumnBinding>>
  getColumnBinding: (columnKey: string) => UiTableColumnBinding
  bodySelectionCellClass: ComputedRef<string | string[] | Record<string, boolean> | undefined>
  bodyRowClass: ComputedRef<string | string[] | Record<string, boolean> | undefined>
  bodyCellClass: (column: UiTableColumn) => string | undefined
  bodyIndexCellClass: ComputedRef<string | string[] | Record<string, boolean> | undefined>
  rowHeightDom: ComputedRef<number>
  rowSelectionName: ComputedRef<string>
  isCheckboxRowSelected: (row: any) => boolean
  handleRowCheckboxToggle: (row: any) => void
  groupSelectionVisible: ComputedRef<boolean>
  getGroupSelectionState: (groupKey: string) => GroupSelectionState
  toggleGroupSelection: (groupKey: string, next?: boolean) => void
  isRowFullySelected: (rowIndex: number) => boolean
  isColumnFullySelected: (columnIndex: number) => boolean
  isCellSelected: (rowIndex: number, columnIndex: number) => boolean
  isSelectionCursorCell: (rowIndex: number, columnIndex: number) => boolean
  isSelectionAnchorCell: (rowIndex: number, columnIndex: number) => boolean
  isCellInSelectionRange: (rowIndex: number, columnIndex: number) => boolean
  isCellInFillPreview: (rowIndex: number, columnIndex: number) => boolean
  isCellInCutPreview: (rowIndex: number, columnIndex: number) => boolean
  getFillPreviewEdges: (rowIndex: number, columnIndex: number) => {
    top: boolean
    bottom: boolean
    left: boolean
    right: boolean
    active?: boolean
  } | null
  getSelectionEdges: (rowIndex: number, columnIndex: number) => {
    top: boolean
    bottom: boolean
    left: boolean
    right: boolean
    active?: boolean
  } | null
  getCutPreviewEdges: (rowIndex: number, columnIndex: number) => {
    top: boolean
    bottom: boolean
    left: boolean
    right: boolean
    active?: boolean
  } | null
  hasCutPreview: () => boolean
  buildSelectionSnapshot?: (payload: { rowIndices: number[]; columnIndices: number[] }) => ImperativeSelectionSnapshot
  getColumnIndex: (columnKey: string) => number
  getAriaColIndex: (columnIndex: number) => number
  getAriaRowIndex: (rowIndex: number) => number
  getHeaderTabIndex: (columnIndex: number) => number
  getCellTabIndex: (rowIndex: number, columnIndex: number) => number
  getCellDomId: (rowIndex: number, columnKey: string) => string
  columnWidthDomMap: ComputedRef<Map<string, number>>
  supportsCssZoom: boolean
  zoom: Ref<number>
  editCommand: Ref<{ rowIndex: number; key: string; token: number } | null>
  isColumnEditable: (column: UiTableColumn | undefined) => boolean
  getValidationError: (rowIndex: number, columnIndex: number) => string | null
  onCellEdit: (...args: any[]) => void
  onCellSelect: (...args: any[]) => void
  focusNextCell: (...args: any[]) => void
  handleCellEditingChange: (editing: boolean, columnKey: string, originalIndex: number | null | undefined) => void
  onCellDragStart: (...args: any[]) => void
  onCellDragEnter: (...args: any[]) => void
  onCellComponentFocus: (payload: { rowIndex: number; colIndex: number; columnKey: string }) => void
  startFillDrag: (event: MouseEvent) => void
  autoFillDown: (event?: MouseEvent) => void
  isFillDragging: () => boolean
  hasCustomRenderer: (columnKey: string) => boolean
  tableSlots: Slots
  getStickyTopOffset: (row: any) => number | undefined
  isSearchMatchCell: (rowIndex: number | null | undefined, columnKey: string) => boolean
  isActiveSearchMatchCell: (rowIndex: number | null | undefined, columnKey: string) => boolean
  SELECTION_COLUMN_KEY: string
  totalRowCountDisplay: ComputedRef<number | null | undefined>
  loadingState: ComputedRef<boolean>
  rowHeaderClass: (rowIndex: number) => string | string[] | Record<string, boolean> | undefined
  onRowIndexClick: (rowIndex: number, event?: MouseEvent) => void
  getActiveSelectionRange: () => SelectionAreaDescriptor | null
  canMoveActiveSelection: () => boolean
  moveActiveSelectionTo: (rowStart: number, colStart: number) => boolean
  hoverOverlayVisible: ComputedRef<boolean>
  hoverOverlayStyle: ComputedRef<CSSProperties | null>
  pinnedLeftEntries: ComputedRef<ColumnMetric[]>
  visibleScrollableEntries: ComputedRef<ColumnMetric[]>
  pinnedRightEntries: ComputedRef<ColumnMetric[]>
  leftPaddingDom: ComputedRef<number>
  rightPaddingDom: ComputedRef<number>
  viewportWidthDom: ComputedRef<number>
  buildRowViewModel: (options: {
    pooled: RowPoolItem
    entries: HeaderRenderableEntry[]
    region: UiTableRowRegion
  }) => UiTableRowViewModel
  renderCellSlot: (slotName: string, slotProps: Record<string, unknown>) => any[]
  resolveColumnSurface: (region: UiTableRowRegion, columnKey: string) => { left: number; width: number } | null
  getRegionSurfaceWidth: (region: UiTableRowRegion) => number
}

export const UiTableBodyContextKey: InjectionKey<UiTableBodyBindings> = Symbol("UiTableBodyContext")

export interface UiTableSummaryBindings {
  headerRenderableEntries: ComputedRef<HeaderRenderableEntry[]>
  summaryRowClass: ComputedRef<string | string[] | Record<string, boolean> | undefined>
  summaryCellClass: ComputedRef<string | string[] | Record<string, boolean> | undefined>
  summaryLabelCellClass: ComputedRef<string | string[] | Record<string, boolean> | undefined>
  summaryCellStyle: (column: UiTableColumn) => CSSProperties
  summaryRowAriaIndex: ComputedRef<number>
  summaryRowData: ComputedRef<Record<string, unknown> | null>
  tableSlots: Slots
  columnBindings: ComputedRef<Map<string, UiTableColumnBinding>>
  getColumnBinding: (columnKey: string) => UiTableColumnBinding
  selectionColumnKey: string
  resolveColumnSurface: (region: UiTableRowRegion, columnKey: string) => { left: number; width: number } | null
}

export const UiTableSummaryContextKey: InjectionKey<UiTableSummaryBindings> = Symbol("UiTableSummaryContext")

export interface UiTableNavigationApi {
  scrollToRow: (rowIndex: number) => void
  scrollToColumn: (columnKey: string) => void
  isRowVisible: (rowIndex: number) => boolean
  focusCell: (rowIndex: number, column: number | string, options?: { extend?: boolean }) => boolean
}

export interface UiTableSelectionApi {
  setSelection: (
    ranges: UiTableSelectionRangeInput | UiTableSelectionRangeInput[],
    options?: { activeRangeIndex?: number; focus?: boolean },
  ) => void
  clearSelection: () => void
  getSelectionSnapshot: () => UiTableSelectionSnapshot
  getSelectedCells: () => UiTableSelectedCell[]
  getSelectionMetrics: () => UiTableSelectionMetricResult[]
  selectAllRows: () => void
  clearRowSelection: () => void
  toggleRowSelection: (row: RowData) => void
  getSelectedRows: () => RowData[]
}

export interface UiTableClipboardApi {
  copySelectionToClipboard: () => Promise<void>
  copySelectionToClipboardImmediate: () => Promise<void>
  pasteClipboardData: () => Promise<void>
  pasteClipboardDataImmediate: () => Promise<void>
}

export interface UiTableDataApi {
  getCellValue: (rowRef: number | RowKey, column: number | string) => unknown
  setCellValue: (rowRef: number | RowKey, column: number | string, value: unknown, options?: { force?: boolean }) => boolean
  exportCSV: (options?: { includeHeaders?: boolean; range?: SelectionRange | null }) => string
  importCSV: (text: string, options?: { base?: SelectionPoint | null }) => boolean
}

export interface UiTablePagingApi {
  requestPage: (page: number, reason?: UiTableLazyLoadReason) => Promise<void>
  requestNextPage: (reason?: UiTableLazyLoadReason) => Promise<void>
  resetLazyPaging: (options?: { clearRows?: boolean }) => void
}

export interface UiTableHistoryApi {
  undo: () => void
  redo: () => void
}

export interface UiTableFilterApi {
  onApplyFilter: () => void
  onCancelFilter: () => void
  onSortColumn: (direction: string) => void
  onResetFilter: () => void
  resetAllFilters: () => void
  openAdvancedFilterModal: (columnKey: string) => void
  handleAdvancedModalApply: (condition: FilterCondition | null) => void
  handleAdvancedModalClear: () => void
  handleAdvancedModalCancel: () => void
  handleAdvancedFilterApply: (columnKey: string, condition: FilterCondition | null) => void
  handleAdvancedFilterClear: (columnKey: string) => void
  setMultiSortState: (state: SortState[]) => void
  getFilterStateSnapshot: () => FilterStateSnapshot
  setFilterStateSnapshot: (snapshot: FilterStateSnapshot | null | undefined) => void
}

export interface UiTableColumnsApi {
  handleColumnHeaderClick: (column: UiTableColumn, columnIndex: number, event: MouseEvent | KeyboardEvent) => void
  resetColumnVisibility: () => void
  openVisibilityPanel: () => void
  closeVisibilityPanel: () => void
}

export interface UiTableGroupingApi {
  onGroupColumn: (column: UiTableColumn) => void
  toggleGroupRow: (key: string) => void
  getFilteredRowEntries: () => Array<{ row: RowData; originalIndex: number }>
  getFilteredRows: () => RowData[]
}

export interface UiTableApiExpose {
  navigation: UiTableNavigationApi
  selection: UiTableSelectionApi
  clipboard: UiTableClipboardApi
  data: UiTableDataApi
  paging: UiTablePagingApi
  history: UiTableHistoryApi
  filter: UiTableFilterApi
  columns: UiTableColumnsApi
  grouping: UiTableGroupingApi
}

export interface UiTableLegacyExpose {
  scrollToRow: UiTableNavigationApi["scrollToRow"]
  scrollToColumn: UiTableNavigationApi["scrollToColumn"]
  isRowVisible: UiTableNavigationApi["isRowVisible"]
  focusCell: UiTableNavigationApi["focusCell"]
  setSelection: UiTableSelectionApi["setSelection"]
  clearSelection: UiTableSelectionApi["clearSelection"]
  getSelectionSnapshot: UiTableSelectionApi["getSelectionSnapshot"]
  copySelectionToClipboard: UiTableClipboardApi["copySelectionToClipboard"]
  pasteClipboardData: UiTableClipboardApi["pasteClipboardData"]
  copySelectionToClipboardWithFlash: UiTableClipboardApi["copySelectionToClipboard"]
  pasteClipboardDataWithFlash: UiTableClipboardApi["pasteClipboardData"]
  getCellValue: UiTableDataApi["getCellValue"]
  setCellValue: UiTableDataApi["setCellValue"]
  getSelectedCells: UiTableSelectionApi["getSelectedCells"]
  getSelectionMetrics: UiTableSelectionApi["getSelectionMetrics"]
  exportCSV: UiTableDataApi["exportCSV"]
  importCSV: UiTableDataApi["importCSV"]
  requestPage: UiTablePagingApi["requestPage"]
  requestNextPage: UiTablePagingApi["requestNextPage"]
  resetLazyPaging: UiTablePagingApi["resetLazyPaging"]
  undo: UiTableHistoryApi["undo"]
  redo: UiTableHistoryApi["redo"]
  onApplyFilter: UiTableFilterApi["onApplyFilter"]
  onCancelFilter: UiTableFilterApi["onCancelFilter"]
  onSortColumn: UiTableFilterApi["onSortColumn"]
  onResetFilter: UiTableFilterApi["onResetFilter"]
  openAdvancedFilterModal: UiTableFilterApi["openAdvancedFilterModal"]
  handleAdvancedModalApply: UiTableFilterApi["handleAdvancedModalApply"]
  handleAdvancedModalClear: UiTableFilterApi["handleAdvancedModalClear"]
  handleAdvancedModalCancel: UiTableFilterApi["handleAdvancedModalCancel"]
  handleAdvancedFilterApply: UiTableFilterApi["handleAdvancedFilterApply"]
  handleAdvancedFilterClear: UiTableFilterApi["handleAdvancedFilterClear"]
  setMultiSortState: UiTableFilterApi["setMultiSortState"]
  handleColumnHeaderClick: UiTableColumnsApi["handleColumnHeaderClick"]
  resetColumnVisibility: UiTableColumnsApi["resetColumnVisibility"]
  openVisibilityPanel: UiTableColumnsApi["openVisibilityPanel"]
  closeVisibilityPanel: UiTableColumnsApi["closeVisibilityPanel"]
  resetAllFilters: UiTableFilterApi["resetAllFilters"]
  onGroupColumn: UiTableGroupingApi["onGroupColumn"]
  toggleGroupRow: UiTableGroupingApi["toggleGroupRow"]
  getFilteredRowEntries: UiTableGroupingApi["getFilteredRowEntries"]
  getFilteredRows: UiTableGroupingApi["getFilteredRows"]
  getFilterStateSnapshot: UiTableFilterApi["getFilterStateSnapshot"]
  setFilterStateSnapshot: UiTableFilterApi["setFilterStateSnapshot"]
  selectAllRows: UiTableSelectionApi["selectAllRows"]
  clearRowSelection: UiTableSelectionApi["clearRowSelection"]
  toggleRowSelection: UiTableSelectionApi["toggleRowSelection"]
  getSelectedRows: UiTableSelectionApi["getSelectedRows"]
}

export interface UiTableExposeBindings extends UiTableApiExpose, UiTableLegacyExpose {}

export const UiTableExposeContextKey: InjectionKey<UiTableExposeBindings> = Symbol("UiTableExposeContext")
