import { BASE_ROW_HEIGHT } from "../utils/constants"
import type {
  UiTableColumn,
  UiTableConfig,
  UiTableEventHandlers,
  UiTableFilterOptionLoader,
  UiTableLazyLoader,
  UiTableSelectionMetricDefinition,
  UiTableSelectionMetricsConfig,
  UiTableSelectionMetricsProp,
  UiTableStyleConfig,
} from "../types"
import type { UiTableColumnGroupDef } from "../types/column"
import type { UiTablePluginDefinition } from "../../../ui-table/plugins/types"
import type { RowData, RowKey } from "../../../composables/useSelectableRows"

export interface NormalizedSelectionState {
  enabled: boolean
  controlled: boolean
  mode: "cell" | "row"
  showSelectionColumn: boolean
  selected?: (RowData | RowKey)[]
}

export interface NormalizedTableProps {
  rows: any[]
  columns: UiTableColumn[]
  columnGroups: UiTableColumnGroupDef[]
  config: UiTableConfig
  tableId: string
  loading: boolean
  rowHeightMode: "fixed" | "auto"
  rowHeight: number
  summaryRow: Record<string, any> | null
  debugViewport: boolean
  inlineControls: boolean
  showRowIndexColumn: boolean
  selection: NormalizedSelectionState
  selectionMetrics: UiTableSelectionMetricsConfig
  hoverable: boolean
  styleConfig: UiTableStyleConfig | null
  showZoomControl: boolean
  hasMore?: boolean
  totalRows?: number
  pageSize: number
  autoLoadOnScroll: boolean
  loadOnMount: boolean
  lazyLoader?: UiTableLazyLoader
  serverSideModel: boolean
  filterOptionLoader?: UiTableFilterOptionLoader
  events: UiTableEventHandlers
  plugins: UiTablePluginDefinition[]
}

export interface UiTableProps {
  rows?: any[]
  totalRows?: number
  loading?: boolean
  rowHeightMode?: "fixed" | "auto"
  columns?: UiTableColumn[]
  columnGroups?: UiTableColumnGroupDef[]
  tableId?: string
  summaryRow?: Record<string, any> | null
  debugViewport?: boolean
  inlineControls?: boolean
  showRowIndexColumn?: boolean
  selectable?: boolean
  fullRowSelectionMode?: boolean
  selected?: (RowData | RowKey)[]
  hoverable?: boolean
  styleConfig?: UiTableStyleConfig
  rowHeight?: number
  showSelectionColumn?: boolean
  showZoom?: boolean
  selectionMetrics?: UiTableSelectionMetricsProp
  hasMore?: boolean
  pageSize?: number
  autoLoadOnScroll?: boolean
  loadOnMount?: boolean
  lazyLoader?: UiTableLazyLoader
  serverSideModel?: boolean
  filterOptionLoader?: UiTableFilterOptionLoader
  config?: UiTableConfig
  events?: UiTableEventHandlers
  plugins?: UiTablePluginDefinition[]
}

export const DEFAULT_LAZY_PAGE_SIZE = 200

export const DEFAULT_SELECTION_METRIC_DEFINITIONS: ReadonlyArray<UiTableSelectionMetricDefinition> = [
  { id: "count", label: "Count", precision: 0 },
  { id: "sum", label: "Sum" },
  { id: "min", label: "Min" },
  { id: "max", label: "Max" },
  { id: "avg", label: "Average", precision: 2 },
]

export const BUILTIN_SELECTION_METRIC_LABELS: Record<string, string> = DEFAULT_SELECTION_METRIC_DEFINITIONS.reduce(
  (acc, metric) => {
    acc[metric.id] = metric.label ?? metric.id
    return acc
  },
  {} as Record<string, string>,
)

const DEFAULT_SELECTION_METRICS_DISABLED: UiTableSelectionMetricsConfig = {
  enabled: false,
  metrics: [],
}

type SelectionMetricLike = UiTableSelectionMetricDefinition | undefined | null

function pickFirst<T>(...values: (T | null | undefined)[]): T | undefined {
  for (const value of values) {
    if (value !== undefined && value !== null) {
      return value
    }
  }
  return undefined
}

function normalizeSelectionMetricDefinition(
  definition: SelectionMetricLike,
): UiTableSelectionMetricDefinition | null {
  if (!definition || typeof definition !== "object") {
    return null
  }
  const id = String(definition.id ?? "").trim()
  if (!id) {
    return null
  }
  const label = typeof definition.label === "string" && definition.label.trim().length ? definition.label : undefined
  const precision = typeof definition.precision === "number" && Number.isFinite(definition.precision)
    ? Math.max(0, Math.floor(definition.precision))
    : undefined
  const compute = typeof definition.compute === "function" ? definition.compute : undefined
  const formatter = typeof definition.formatter === "function" ? definition.formatter : undefined
  return {
    id,
    label,
    precision,
    compute,
    formatter,
  }
}

export function normalizeSelectionMetrics(
  input: UiTableSelectionMetricsProp | undefined,
): UiTableSelectionMetricsConfig {
  if (input === undefined || input === null || input === false) {
    return DEFAULT_SELECTION_METRICS_DISABLED
  }

  if (input === true) {
    return {
      enabled: true,
      metrics: DEFAULT_SELECTION_METRIC_DEFINITIONS.map(def => ({ ...def })),
    }
  }

  let enabled = true
  let definitions: UiTableSelectionMetricDefinition[] = []

  if (Array.isArray(input)) {
    const normalized: Array<UiTableSelectionMetricDefinition | null> = input.map(normalizeSelectionMetricDefinition)
    definitions = normalized.filter((metric): metric is UiTableSelectionMetricDefinition => metric !== null)
  } else {
    const candidateEnabled = typeof input.enabled === "boolean" ? input.enabled : undefined
    enabled = candidateEnabled ?? true
    const normalized: Array<UiTableSelectionMetricDefinition | null> = Array.isArray(input.metrics)
      ? input.metrics.map(normalizeSelectionMetricDefinition)
      : []
    definitions = normalized.filter((metric): metric is UiTableSelectionMetricDefinition => metric !== null)
  }

  if (!definitions.length && enabled) {
    definitions = DEFAULT_SELECTION_METRIC_DEFINITIONS.map(def => ({ ...def }))
  }

  return {
    enabled: enabled && definitions.length > 0,
    metrics: definitions,
  }
}

export function normalizePlugins(input: UiTablePluginDefinition | UiTablePluginDefinition[] | null | undefined): UiTablePluginDefinition[] {
  if (!input) return []
  const list = Array.isArray(input) ? input : [input]
  return list.filter((entry): entry is UiTablePluginDefinition => Boolean(entry))
}

export function normalizeTableProps(raw: UiTableProps): NormalizedTableProps {
  const config: UiTableConfig = raw.config ?? {}
  const featureConfig = config.features ?? {}
  const selectionOverride = config.selection ?? {}
  const selectionFeature = featureConfig.selection ?? {}
  const selectionConfig = {
    ...selectionFeature,
    ...selectionOverride,
  }
  const appearanceConfig = config.appearance ?? {}
  const loadConfig = config.load ?? {}
  const debugConfig = config.debug ?? {}
  const dataConfig = config.data ?? {}
  const columnConfig = config.columns ?? {}
  const stateConfig = config.state ?? {}

  const rows = Array.isArray(raw.rows)
    ? raw.rows
    : Array.isArray(dataConfig.rows)
      ? dataConfig.rows
      : []

  const columns = Array.isArray(raw.columns)
    ? raw.columns
    : Array.isArray(columnConfig.definitions)
      ? columnConfig.definitions
      : []

  const columnGroups = Array.isArray(raw.columnGroups)
    ? raw.columnGroups
    : Array.isArray(columnConfig.groups)
      ? columnConfig.groups
      : []

  const rowHeightModeCandidate = pickFirst(raw.rowHeightMode, appearanceConfig.rowHeightMode)
  const rowHeightMode: "fixed" | "auto" = rowHeightModeCandidate === "auto" ? "auto" : "fixed"

  const rowHeightCandidate = pickFirst(raw.rowHeight, appearanceConfig.rowHeight)
  const rowHeight = Math.max(1, typeof rowHeightCandidate === "number" && Number.isFinite(rowHeightCandidate) ? rowHeightCandidate : BASE_ROW_HEIGHT)

  const inlineControlsCandidate = pickFirst(raw.inlineControls, featureConfig.inlineControls)
  const inlineControls = inlineControlsCandidate !== undefined ? Boolean(inlineControlsCandidate) : true

  const showRowIndexCandidate = pickFirst(raw.showRowIndexColumn, featureConfig.rowIndexColumn)
  const showRowIndexColumn = Boolean(showRowIndexCandidate)

  const hoverableCandidate = pickFirst(raw.hoverable, featureConfig.hoverable)
  const hoverable = hoverableCandidate === undefined ? true : Boolean(hoverableCandidate)

  const showZoomCandidate = pickFirst(raw.showZoom, featureConfig.zoom)
  const showZoomControl = Boolean(showZoomCandidate ?? false)

  const hasMoreCandidate = pickFirst(raw.hasMore, loadConfig.hasMore)
  const hasMore = typeof hasMoreCandidate === "boolean" ? hasMoreCandidate : undefined

  const totalRowsCandidate = pickFirst(raw.totalRows, dataConfig.totalRows)
  const totalRows = typeof totalRowsCandidate === "number" && Number.isFinite(totalRowsCandidate) ? totalRowsCandidate : undefined

  const pageSizeCandidate = pickFirst(raw.pageSize, loadConfig.pageSize)
  const pageSize = typeof pageSizeCandidate === "number" && Number.isFinite(pageSizeCandidate)
    ? Math.max(1, Math.floor(pageSizeCandidate))
    : DEFAULT_LAZY_PAGE_SIZE

  const autoLoadOnScroll = Boolean(pickFirst(raw.autoLoadOnScroll, loadConfig.autoLoadOnScroll) ?? false)
  const loadOnMount = Boolean(pickFirst(raw.loadOnMount, loadConfig.loadOnMount) ?? false)
  const serverSideModel = Boolean(pickFirst(raw.serverSideModel, loadConfig.serverSideModel) ?? false)

  const lazyLoader = raw.lazyLoader ?? loadConfig.lazyLoader
  const filterOptionLoader = raw.filterOptionLoader ?? loadConfig.filterOptionLoader

  const styleConfig = pickFirst(raw.styleConfig, appearanceConfig.styleConfig) ?? null

  const summaryRow = pickFirst(raw.summaryRow, dataConfig.summaryRow) ?? null

  const debugViewport = Boolean(pickFirst(raw.debugViewport, debugConfig.viewport) ?? false)

  const tableId =
    pickFirst(raw.tableId, config.tableId) ??
    "default"

  const loading = Boolean(pickFirst(raw.loading, stateConfig.loading) ?? false)

  const selectionEnabledCandidate = pickFirst(raw.selectable, selectionConfig.enabled)
  const selectionEnabled = selectionEnabledCandidate !== undefined ? Boolean(selectionEnabledCandidate) : false

  const selectionModeCandidate = selectionConfig.mode ?? (raw.fullRowSelectionMode ? "row" : undefined)
  const selectionMode: "cell" | "row" = selectionModeCandidate === "row" ? "row" : "cell"

  const selectionShowColumnCandidate = pickFirst(raw.showSelectionColumn, selectionConfig.showSelectionColumn)
  const selectionShowSelectionColumn = selectionEnabled
    ? selectionShowColumnCandidate !== undefined
      ? Boolean(selectionShowColumnCandidate)
      : true
    : false

  const selectionSelectedSource: Array<RowData | RowKey | undefined> | undefined = Array.isArray(raw.selected)
    ? raw.selected
    : Array.isArray(selectionConfig.selected)
      ? selectionConfig.selected
      : Array.isArray(stateConfig.selected)
        ? stateConfig.selected
        : undefined

  const selectionSelected = selectionSelectedSource
    ? selectionSelectedSource.filter((item): item is RowData | RowKey => item !== undefined && item !== null)
    : undefined

  const selectionControlled = selectionEnabled && selectionSelected !== undefined

  const selectionMetricsConfig = normalizeSelectionMetrics(
    pickFirst(raw.selectionMetrics, featureConfig.selectionMetrics),
  )

  const plugins = normalizePlugins(pickFirst(raw.plugins, config.plugins))

  const events: UiTableEventHandlers = {
    ...(config.events ?? {}),
    ...(raw.events ?? {}),
  }

  return {
    rows,
    columns,
    columnGroups,
    config,
    tableId,
    loading,
    rowHeightMode,
    rowHeight,
    summaryRow,
    debugViewport,
    inlineControls,
    showRowIndexColumn,
    selection: {
      enabled: selectionEnabled,
      controlled: selectionControlled,
      mode: selectionMode,
      showSelectionColumn: selectionShowSelectionColumn,
      selected: selectionSelected,
    },
    selectionMetrics: selectionMetricsConfig,
    hoverable,
    styleConfig,
    showZoomControl,
    hasMore,
    totalRows,
    pageSize,
    autoLoadOnScroll,
    loadOnMount,
    lazyLoader,
    serverSideModel,
    filterOptionLoader,
    events,
    plugins,
  }
}
