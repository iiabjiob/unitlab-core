import type { UiTableColumn, UiTableColumnGroupDef } from "./column"
import type { UiTableStyleConfig } from "../../theme"
import type { UiTablePluginDefinition } from "../../../ui-table/plugins/types"
import type { UiTableSettingsAdapter } from "../tableSettingsAdapter"

export type {
  UiTableColumn,
  UiTableColumnAlignment,
  UiTableColumnEditor,
  UiTableColumnGroupDef,
  UiTableColumnSticky,
} from "./column"
export type { UiTableThemeTokens } from "../../theme"
export type { UiTableSettingsAdapter } from "../tableSettingsAdapter"

export type UiTableRowId = string | number

export type UiTableLazyLoadReason =
  | "mount"
  | "scroll"
  | "manual"
  | "filter-change"
  | "sort-change"
  | "refresh"

export interface UiTableLazyLoadContext {
  page: number
  pageSize: number
  offset: number
  totalLoaded: number
  reason: UiTableLazyLoadReason
  sorts?: UiTableSortState[]
  filters?: UiTableFilterSnapshot | null
  mode?: "page" | "block"
  signal?: AbortSignal
  background?: boolean
  blockSize?: number
}

export interface UiTableServerLoadResult<T = any> {
  rows: T[]
  total?: number
}

export type UiTableLazyLoader = (
  context: UiTableLazyLoadContext
) =>
  | Promise<void | UiTableServerLoadResult | any[]>
  | void
  | UiTableServerLoadResult
  | any[]

export type UiTableSortDirection = "asc" | "desc"

export interface UiTableSortState {
  key: string
  field?: string
  direction: UiTableSortDirection
}

export interface UiTableFilterClause {
  operator: string
  value: any
  value2?: any
  join?: "and" | "or"
}

export interface UiTableAdvancedFilter {
  type: "text" | "number" | "date"
  clauses: UiTableFilterClause[]
}

export interface UiTableFilterSnapshot {
  columnFilters: Record<string, string[]>
  advancedFilters: Record<string, UiTableAdvancedFilter>
}

export interface UiTableServerFilterOptionRequest {
  columnKey: string
  search: string
  filters: UiTableFilterSnapshot | null
  limit?: number
}

export interface UiTableServerFilterOption {
  value: unknown
  label?: string
  count?: number
}

export type UiTableFilterOptionLoader = (
  context: UiTableServerFilterOptionRequest
) => Promise<UiTableServerFilterOption[]> | UiTableServerFilterOption[]

export type {
  UiTableStyleSection,
  UiTableHeaderStyle,
  UiTableBodyStyle,
  UiTableGroupStyle,
  UiTableSummaryStyle,
  UiTableStateStyle,
  UiTableThemeTokenVariants,
  UiTableStyleConfig,
} from "../../theme"

export interface CellEditEvent<T = any> {
  rowId: UiTableRowId
  rowIndex: number
  key: keyof T | string
  value: unknown
  originalRowIndex?: number
  displayRowIndex?: number
  row?: T
}

export interface UiTableSelectionPoint {
  rowId: UiTableRowId | null
  rowIndex: number
  colIndex: number
}

export interface UiTableSelectionRangeInput {
  startRow: number
  endRow: number
  startCol: number
  endCol: number
  anchor?: UiTableSelectionPoint
  focus?: UiTableSelectionPoint
}

export interface UiTableSelectionSnapshotRange extends UiTableSelectionRangeInput {
  anchor: UiTableSelectionPoint
  focus: UiTableSelectionPoint
  startRowId?: UiTableRowId | null
  endRowId?: UiTableRowId | null
}

export interface UiTableSelectionSnapshot {
  ranges: UiTableSelectionSnapshotRange[]
  activeRangeIndex: number
  activeCell: UiTableSelectionPoint | null
  clone(): UiTableSelectionSnapshot
}

export interface UiTableSelectedCell<T = any> {
  rowId: UiTableRowId
  rowIndex: number
  colIndex: number
  columnKey: string
  value: unknown
  row?: T
}

export interface UiTableSelectionMetricContext<T = any> {
  cells: UiTableSelectedCell<T>[]
  cellCount: number
  numericValues: number[]
  numericCount: number
  sum: number | null
  min: number | null
  max: number | null
  average: number | null
}

export interface UiTableSelectionMetricFormatterPayload<T = any> {
  id: string
  label: string
  value: number | null
  context: UiTableSelectionMetricContext<T>
}

export interface UiTableSelectionMetricDefinition<T = any> {
  id: string
  label?: string
  precision?: number
  compute?: (context: UiTableSelectionMetricContext<T>) => number | null | undefined
  formatter?: (payload: UiTableSelectionMetricFormatterPayload<T>) => string
}

export interface UiTableSelectionMetricResult {
  id: string
  label: string
  value: number | null
  displayValue: string
}

export type UiTableSelectionMetricsProp<T = any> =
  | boolean
  | UiTableSelectionMetricDefinition<T>[]
  | {
      enabled?: boolean
      metrics?: UiTableSelectionMetricDefinition<T>[]
    }

export interface UiTableSelectionMetricsConfig<T = any> {
  enabled: boolean
  metrics: UiTableSelectionMetricDefinition<T>[]
}

export interface VisibleRow<T = any> {
  row: T
  rowId: UiTableRowId
  originalIndex: number
  displayIndex?: number
  stickyTop?: boolean | number
  stickyBottom?: boolean | number
}

export interface UiTableRowClickEvent<T = any> {
  row: T
  rowId: UiTableRowId
  rowIndex: number
  displayIndex?: number
  originalIndex?: number
}

export interface UiTableEventHandlers<T = any> {
  reachBottom?: () => void
  rowClick?: (payload: UiTableRowClickEvent<T>) => void
  cellEdit?: (event: CellEditEvent<T>) => void
  batchEdit?: (events: CellEditEvent<T>[]) => void
  selectionChange?: (snapshot: UiTableSelectionSnapshot) => void
  sortChange?: (state: UiTableSortState | null) => void
  filterChange?: (filters: Record<string, string[]>) => void
  filtersReset?: () => void
  zoomChange?: (value: number) => void
  columnResize?: (payload: { key: string; width: number }) => void
  groupFilterToggle?: (open: boolean) => void
  rowsDelete?: (rows: unknown) => void
  lazyLoad?: (context: UiTableLazyLoadContext) => void
  lazyLoadComplete?: (context: UiTableLazyLoadContext) => void
  lazyLoadError?: (context: UiTableLazyLoadContext & { error: unknown }) => void
  autoResizeApplied?: (payload: { columns: string[]; shareWidth: number; viewportWidth: number }) => void
  selectAllRequest?: (payload: UiTableSelectAllRequestPayload) => void
}

export interface UiTableSelectAllRequestPayload {
  checked: boolean
  filters: UiTableFilterSnapshot | null
  sorts: UiTableSortState[]
  selection: {
    allSelected: boolean
    isIndeterminate: boolean
    selectedRowKeys: Array<string | number>
    visibleRowCount: number
    totalRowCount: number | null
  }
}

export interface UiTableSelectionConfig<T = any> {
  enabled?: boolean
  mode?: "cell" | "row"
  showSelectionColumn?: boolean
  selected?: (T | string | number)[]
}

export interface UiTableFeatureConfig<T = any> {
  inlineControls?: boolean
  hoverable?: boolean
  rowIndexColumn?: boolean
  zoom?: boolean
  selection?: UiTableSelectionConfig<T>
  selectionMetrics?: UiTableSelectionMetricsProp<T>
}

export interface UiTableAppearanceConfig {
  rowHeightMode?: "fixed" | "auto"
  rowHeight?: number
  styleConfig?: UiTableStyleConfig | null
}

export interface UiTableLoadConfig {
  hasMore?: boolean
  pageSize?: number
  autoLoadOnScroll?: boolean
  loadOnMount?: boolean
  lazyLoader?: UiTableLazyLoader
  serverSideModel?: boolean
  filterOptionLoader?: UiTableFilterOptionLoader
}

export interface UiTableDebugConfig {
  viewport?: boolean
}

export interface UiTableDataConfig<T = any> {
  rows?: T[]
  totalRows?: number
  summaryRow?: Record<string, any> | null
}

export interface UiTableColumnConfig {
  definitions?: UiTableColumn[]
  groups?: UiTableColumnGroupDef[]
}

export interface UiTableStateConfig<T = any> {
  selected?: (T | string | number)[]
  loading?: boolean
}

export interface UiTableConfig<T = any> {
  tableId?: string
  data?: UiTableDataConfig<T>
  columns?: UiTableColumnConfig
  features?: UiTableFeatureConfig<T>
  appearance?: UiTableAppearanceConfig
  load?: UiTableLoadConfig
  debug?: UiTableDebugConfig
  state?: UiTableStateConfig<T>
  selection?: UiTableSelectionConfig<T>
  selectionMetrics?: UiTableSelectionMetricsProp<T>
  plugins?: UiTablePluginDefinition[]
  defaultSort?: UiTableSortState | null
  events?: UiTableEventHandlers<T>
  settingsAdapter?: UiTableSettingsAdapter
}
