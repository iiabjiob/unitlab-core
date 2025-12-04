<template>
  <div ref="tableRootRef" class="ui-table__workspace ui-table-theme-root">
    <div
      :class="['ui-table', tableWrapperClass, { 'ui-table--hoverable': isHoverableTable }]"
      :style="tableInlineStyle"
    >
      <UiTableToolbar
        :use-inline-controls="useInlineControls"
        :has-active-filters-or-groups="hasActiveFiltersOrGroups"
        :reset-filters-button-name="resetFiltersButtonName"
        :reset-targets-count="resetTargetsCount"
        :show-visibility-panel="showVisibilityPanel"
        :column-toggle-button-name="columnToggleButtonName"
        :visibility-storage-key="visibilityStorageKey"
        :columns="localColumns"
        @reset-filters="handleResetAllFilters"
        @toggle-visibility="toggleVisibilityPanel"
        @visibility-update="handleColumnsVisibilityUpdate"
        @visibility-close="closeVisibilityPanel"
        @visibility-reset="resetColumnVisibility"
      />

      <UiTableViewport
        ref="tableViewportRef"
        v-model:containerRef="containerRef"
        :table-container-class="tableContainerClass"
        :ariaRowCount="ariaRowCount"
        :ariaColCount="ariaColCount"
        :exposed-api="tableExpose"
        @focus-in="onGridFocusIn"
        @focus-out="onGridFocusOut"
        @keydown="handleKeydown"
        @scroll="handleViewportScrollEvent"
      >
        <template #header-main>
          <UiTableHeader section="main" />
        </template>
        <template #body-main>
          <UiTableVirtualBodyRegion section="main" />
          <div v-if="showVirtualEmptyState" class="ui-table__empty-state">No data</div>
          <UiTableSummary v-if="hasSummaryRow" />
        </template>
      </UiTableViewport>
    </div>
    <UiTableStatusBar
      :row-count-display="formattedRowCount"
      :selected-row-count="selectedRowCount"
      :selected-row-count-display="formattedSelectedRowCount"
      :selected-cell-count="selectedCellCount"
      :selected-cell-count-display="formattedSelectedCellCount"
    />

    <UiTableModals
      :advanced-open="advancedModalState.open"
      :advanced-column-label="advancedModalColumn?.label ?? ''"
      :advanced-type="advancedModalType"
      :advanced-condition="advancedModalCondition"
      @advanced-apply="handleAdvancedModalApply"
      @advanced-clear="handleAdvancedModalClear"
      @advanced-cancel="handleAdvancedModalCancel"
    />
  </div>
</template>

<script setup lang="ts">
import { ref, computed, watch, nextTick, useSlots, onBeforeUnmount, onMounted, watchEffect, shallowRef, provide, unref } from "vue"
import type { ComponentPublicInstance, ComputedRef, Ref, CSSProperties } from "vue"
import UiTableViewport from "./UiTableViewport.vue"
import UiTableVirtualBodyRegion from "./UiTableVirtualBodyRegion.vue"
import UiTableHeader from "./UiTableHeader.vue"
import UiTableStatusBar from "./UiTableStatusBar.vue"
import UiTableSummary from "./UiTableSummary.vue"
import { useTableClipboardBridge } from "../composables/useTableClipboardBridge"
import { useTableTheme } from "../composables/useTableTheme"
import { useTableSettingsStore } from "../tableSettingsStore"
import type { UiTableSettingsAdapter } from "@/ui-table/core/tableSettingsAdapter"
import { createPiniaTableSettingsAdapter } from "../piniaTableSettingsAdapter"
import type {
  CellEditEvent,
  UiTableColumn,
  VisibleRow,
  UiTableFilterSnapshot,
  UiTableSortState,
  UiTableSelectAllRequestPayload,
  UiTableSelectionMetricResult,
  UiTableSelectedCell,
  UiTableSelectionSnapshot,
} from "../../core/types"
import type { SelectionOverlayColumnSurface } from "../../core/selection/selectionOverlay"
import type { UiTableSortState as AdapterSortState } from "../../core/types/sort"
import { useTableFilters } from "../composables/useTableFilters"
import type { FilterStateSnapshot } from "../composables/useTableFilters"
import { useTableSorting, type SortState } from "../composables/useTableSorting"
import { useTableViewport, type ColumnMetric, type RowPoolItem } from "../composables/useTableViewport"
import { useTableHistory, type HistoryEntry } from "../composables/useTableHistory"
import { useTableEditing, isColumnEditable as baseIsColumnEditable } from "../composables/useTableEditing"
import { useTableSelection, type SelectionArea, type SelectionPoint, type SelectionRange } from "../composables/useTableSelection"
import type { TableOverlayScrollEmitter } from "../composables/useTableOverlayScrollState"
import type { UiTableOverlayHandle } from "../types/overlay"
import { useCellFlash } from "../../core/dom/useCellFlash"
import { getCellElement, supportsCssZoom } from "../../core/dom/gridUtils"
import { useTableHoverOverlay } from "../composables/useTableHoverOverlay"
import { useColumnVisibility } from "../composables/useColumnVisibility"
import { useTableAutoScroll } from "../composables/useTableAutoScroll"
import type { PointerCoordinates } from "@/ui-table/core/selection/autoScroll"
import { useTablePanels } from "../composables/useTablePanels"
import { useTableEvents } from "../../core/events/useTableEvents"
import { useAutoResizeColumn } from "@/composables/useAutoResizeColumn"
import { useAutoColumnResize } from "../../core/virtualization/useAutoColumnResize"
import { useTableAutoColumnScheduler } from "../composables/useTableAutoColumnScheduler"
import { useTableOverlayController } from "../composables/useTableOverlayController"
import { useTableGrouping, GROUP_INDENT_BASE, GROUP_INDENT_STEP } from "../composables/useTableGrouping"
import { useTableDataModel } from "../composables/useTableDataModel"
import { useTableServerSync } from "../composables/useTableServerSync"
import { useTableStickyColumns } from "../composables/useTableStickyColumns"
import { useTableHeaderLayout } from "../composables/useTableHeaderLayout"
import { useTableGridLayout } from "../composables/useTableGridLayout"
import { useTableColumnBindings } from "../composables/useTableColumnBindings"
import { useTableFocusManagement } from "../composables/useTableFocusManagement"
import { useTableColumnPinning } from "../composables/useTableColumnPinning"
import { useTableAdvancedFilterModal } from "../composables/useTableAdvancedFilterModal"
import { useTableLocalColumns } from "../composables/useTableLocalColumns"
import { useTableViewportMetrics } from "../composables/useTableViewportMetrics"
import { BASE_ROW_HEIGHT } from "../../core/utils/constants"
import { formatFilterLabel, serializeFilterValue } from "../../core/utils/validators"
import "../../core/styles/layout.css"
import "../../core/styles/theme.css"
import "../../core/styles/components.css"
import "../../core/styles/states.css"
import "../../core/styles/overlays.css"
import "../../core/styles/dark.css"
import "../../core/styles/effects.css"
import "../../core/styles/sidebar.css"
import "../../core/styles/toolbar.css"
import "../../core/styles/status.css"
import "../../core/styles/modal-overlay.css"
import "../../core/styles/fill-handle.css"
import "../../core/styles/icon-button.css"
import "../../core/styles/row-index.css"
import "../../core/styles/empty-state.css"
import "../../core/styles/summary-footer.css"
import "../../core/styles/cell-select-trigger.css"
import UiTableToolbar from "./UiTableToolbar.vue"
import UiTableModals from "./UiTableModals.vue"
import { useColumnGroups } from "../composables/useColumnGroups"
import type { UiTableColumnGroupDef } from "../../core/types/column"
import { useSelectableRows, type RowData, type RowKey } from "@/composables/useSelectableRows"
import { useColumnFilterMenuBridge } from "../../core/filtering/useColumnFilterMenuBridge"
import {
  UiTableHeaderContextKey,
  type UiTableHeaderBindings,
  UiTableBodyContextKey,
  type UiTableBodyBindings,
  type UiTableRowViewModel,
  type UiTableRowCellDescriptor,
  type UiTableRowRegion,
  type UiTableColumnBinding,
  type GroupSelectionState,
  UiTableSummaryContextKey,
  type UiTableSummaryBindings,
  UiTableExposeContextKey,
  type UiTableExposeBindings,
  type ColumnPinPosition,
} from "../context"
import type { HeaderRenderableEntry } from "../../core/types/internal"
import {
  BUILTIN_SELECTION_METRIC_LABELS,
  DEFAULT_LAZY_PAGE_SIZE,
  normalizeTableProps,
  type NormalizedTableProps,
  type UiTableProps,
} from "../../core/config/tableConfig"
import { useTableRuntime } from "../composables/useTableRuntime"
import { computeSelectionMetrics, createNumberFormatterResolver } from "../../core/selection/selectionMetrics"
import { useUiTableApi, createLegacyExpose } from "../core/api/useUiTableApi"
import { useTableRecalcWatcher } from "../composables/useTableRecalcWatcher"

const FILTER_OPTION_FETCH_LIMIT = 200
const SERVER_FILTER_SEARCH_DEBOUNCE_MS = 300

const props = withDefaults(defineProps<UiTableProps>(), {
  inlineControls: true,
  showRowIndexColumn: false,
  selectable: false,
  fullRowSelectionMode: false,
  hoverable: false,
  styleConfig: () => ({}),
  rowHeight: BASE_ROW_HEIGHT,
  showSelectionColumn: true,
  selectionMetrics: false,
  pageSize: DEFAULT_LAZY_PAGE_SIZE,
  autoLoadOnScroll: false,
  loadOnMount: false,
  serverSideModel: false,
  plugins: () => [],
})

const normalizedProps = computed<NormalizedTableProps>(() => normalizeTableProps(props))

const {
  tableRootRef,
  tableThemeVars,
  tableWrapperClass,
  tableContainerBaseClass,
  headerRowClass,
  headerCellBaseClass,
  headerSelectionCellClass,
  headerIndexCellClass,
  bodyRowClass,
  bodyCellBaseClass,
  bodySelectionCellClass,
  bodyIndexCellClass,
  groupRowClass,
  groupCellClass,
  groupCaretClass,
  summaryRowClass,
  summaryCellClass,
  summaryLabelCellClass,
  selectedRowClass,
} = useTableTheme({ normalizedProps })


const emit = defineEmits([
  "reach-bottom",
  "row-click",
  "cell-edit",
  "batch-edit",
  "selection-change",
  "sort-change",
  "filter-change",
  "filters-reset",
  "zoom-change",
  "column-resize",
  "group-filter-toggle",
  "rows-delete",
  "update:selected",
  "lazy-load",
  "lazy-load-complete",
  "lazy-load-error",
  "auto-resize-applied",
  "select-all-request",
])

const tableId = computed<string>(() => normalizedProps.value.tableId)
const tableSettings = useTableSettingsStore()
const defaultSettingsAdapter = createPiniaTableSettingsAdapter(tableSettings)
const settingsAdapter = computed<UiTableSettingsAdapter>(() => {
  return normalizedProps.value.config.settingsAdapter ?? defaultSettingsAdapter
})

const { fireEvent, setTableExpose } = useTableRuntime({
  normalizedProps,
  emit,
  tableId,
  tableRootRef,
})
const autoColumnResize = useAutoColumnResize({
  onApplied: payload => {
    emit("auto-resize-applied", payload)
    fireEvent("autoResizeApplied", payload)
  },
})

const splitPinnedLayoutEnabled = computed(() => false)

let updateViewportHeightCallback: (() => void) | null = null
let scheduleOverlayUpdateCallback: (() => void) | null = null

const scheduleAutoColumnResizeImpl = shallowRef<() => void>(() => {})
const scheduleAutoColumnResize = () => scheduleAutoColumnResizeImpl.value()

const rowCountFormatter = new Intl.NumberFormat()
const numericSummaryFormatter = new Intl.NumberFormat(undefined, {
  maximumFractionDigits: 2,
  minimumFractionDigits: 0,
})
const resolveSelectionMetricFormatter = createNumberFormatterResolver({ baseFormatter: numericSummaryFormatter })

const localColumns = ref<UiTableColumn[]>([])
const refreshViewportCallback = shallowRef<() => void>(() => {})
const overlayUpdateCallback = shallowRef<() => void>(() => {})
const viewportWidthForAutoResize = ref(0)

let applyStoredPinStateImpl: () => void = () => {}
let reorderPinnedColumnsImpl: () => void = () => {}

const applyStoredPinState = () => applyStoredPinStateImpl()
const reorderPinnedColumns = () => reorderPinnedColumnsImpl()

const {
  visibilityStorageKey,
  visibilityHydrated,
  updateVisibilityMapFromColumns,
  persistColumnState,
  loadColumnStateFromStorage,
  applyStoredColumnState,
  handleColumnsVisibilityUpdate,
  resetColumnVisibility,
  hideColumn,
} = useColumnVisibility({
  tableId,
  localColumns,
  autoColumnResize,
  scheduleAutoColumnResize,
  reorderPinnedColumns,
  updateViewportHeight: () => updateViewportHeightCallback?.(),
  scheduleOverlayUpdate: () => scheduleOverlayUpdateCallback?.(),
})

const ROW_INDEX_COLUMN_KEY = "__rowIndex__"
const SELECTION_COLUMN_KEY = "__select__"

function createRowIndexColumn(): UiTableColumn {
  return {
    key: ROW_INDEX_COLUMN_KEY,
    label: "#",
    width: 56,
    minWidth: 48,
    maxWidth: 72,
    sortable: false,
    resizable: false,
    editable: false,
    align: "right",
    headerAlign: "right",
    isSystem: true,
    visible: true,
  }
}

function createSelectionColumn(): UiTableColumn {
  return {
    key: SELECTION_COLUMN_KEY,
    label: "",
    width: 48,
    minWidth: 44,
    maxWidth: 56,
    sortable: false,
    resizable: false,
    editable: false,
    align: "center",
    headerAlign: "center",
    isSystem: true,
    visible: true,
  }
}

function ensureSystemColumns(columns: UiTableColumn[], includeRowIndex: boolean, includeSelection: boolean): UiTableColumn[] {
  const sanitized = columns.filter(
    column => column.key !== ROW_INDEX_COLUMN_KEY && column.key !== SELECTION_COLUMN_KEY,
  )

  const systemColumns: UiTableColumn[] = []
  if (includeRowIndex) {
    systemColumns.push(createRowIndexColumn())
  }
  if (includeSelection) {
    systemColumns.push(createSelectionColumn())
  }

  return [...systemColumns, ...sanitized]
}

const fallbackRowKeyMap = new WeakMap<object, RowKey>()
let fallbackRowKeySeed = 0

function resolveRowKey(row: RowData, originalIndex?: number): RowKey {
  const candidateId = (row as Record<string, unknown>).id
  if (typeof candidateId === "string" || typeof candidateId === "number") {
    return candidateId
  }
  const keyProp = (row as Record<string, unknown>).key
  if (typeof keyProp === "string" || typeof keyProp === "number") {
    return keyProp
  }
  const internalKey = (row as Record<string, unknown>).__key
  if (typeof internalKey === "string" || typeof internalKey === "number") {
    return internalKey
  }
  if (typeof originalIndex === "number") {
    return originalIndex
  }
  const existing = fallbackRowKeyMap.get(row as object)
  if (existing != null) {
    return existing
  }
  fallbackRowKeySeed += 1
  const generated: RowKey = `row_${fallbackRowKeySeed}`
  fallbackRowKeyMap.set(row as object, generated)
  return generated
}

function rowKeyResolver(row: RowData): RowKey {
  return resolveRowKey(row)
}

const rowHeightMode = computed(() => normalizedProps.value.rowHeightMode)
const baseRowHeight = computed(() => normalizedProps.value.rowHeight)

const serverSortResolver = shallowRef<() => UiTableSortState[]>(() => [])
const serverFilterResolver = shallowRef<() => UiTableFilterSnapshot | null>(() => null)

const {
  serverSideModel,
  serverSideFiltering,
  serverSideSorting,
  autoLazyOnScroll,
  resolvedRows,
  loadingState,
  requestPage,
  requestNextPage,
  resetLazyPaging,
  scheduleServerReload,
  serverRowModel,
  serverRowBlocks,
  isServerPlaceholderRow,
  lastServerFilterFingerprint,
  lastServerSortFingerprint,
  setScrollToTopForServer,
} = useTableDataModel({
  normalizedProps,
  emit: emit as (event: string, ...args: any[]) => void,
  fireEvent,
  getServerSorts: () => serverSortResolver.value(),
  getServerFilters: () => serverFilterResolver.value(),
})

watch(
  serverRowBlocks,
  () => {
    if (!serverSideModel.value) {
      return
    }
    nextTick(() => {
      requestTableRecalc("server-row-blocks", { overlay: true })
    })
  },
)

const useInlineControls = computed(() => normalizedProps.value.inlineControls)
const selectionEnabled = computed(() => normalizedProps.value.selection.enabled)
const selectionColumnVisible = computed(
  () => selectionEnabled.value && normalizedProps.value.selection.showSelectionColumn,
)
const isHoverableTable = computed(() => normalizedProps.value.hoverable)
const isFullRowSelectionMode = computed(() => normalizedProps.value.selection.mode === "row")
const selectionMetricsConfig = computed(() => normalizedProps.value.selectionMetrics)

useTableLocalColumns({
  normalizedProps,
  selectionColumnVisible,
  localColumns,
  ensureSystemColumns,
  getSavedColumnWidth: columnKey => settingsAdapter.value.getColumnWidth(tableId.value, columnKey),
  loadColumnStateFromStorage,
  applyStoredColumnState,
  updateVisibilityMapFromColumns,
  persistColumnState,
  visibilityHydrated,
  autoColumnResizeReset: columns => autoColumnResize.reset(columns),
  scheduleAutoColumnResize,
  applyStoredPinState,
  reorderPinnedColumns,
})

const resetFiltersButtonName = computed(() => `${tableId.value}-reset-filters`)
const columnToggleButtonName = computed(() => `${tableId.value}-column-toggle`)
const headerSelectionName = computed(() => `${tableId.value}-select-all`)
const rowSelectionName = computed(() => `${tableId.value}-row-select`)

function headerCellClass(column: UiTableColumn) {
  if (column.key === ROW_INDEX_COLUMN_KEY) {
    return headerIndexCellClass.value
  }
  return headerCellBaseClass.value
}

function bodyCellClass(_column: UiTableColumn) {
  return bodyCellBaseClass.value
}

type ExposedViewportElement = HTMLElement | null | Ref<HTMLElement | null>

type TableViewportExposeRefs = {
  containerRef: ExposedViewportElement
  headerPinnedLeftRef?: ExposedViewportElement
  headerPinnedRightRef?: ExposedViewportElement
  headerMainRef?: ExposedViewportElement
  headerMainContentRef?: ExposedViewportElement
  headerPinnedLeftContentRef?: ExposedViewportElement
  headerPinnedRightContentRef?: ExposedViewportElement
  pinnedLeftRef?: ExposedViewportElement
  pinnedRightRef?: ExposedViewportElement
  pinnedLeftContentRef?: ExposedViewportElement
  pinnedRightContentRef?: ExposedViewportElement
  bodyMainSurfaceRef?: ExposedViewportElement
  bodyMainContentRef?: ExposedViewportElement
  splitViewportRef?: ExposedViewportElement
  layoutRef?: ExposedViewportElement
  bodyLayerRef?: ExposedViewportElement
  backgroundSpacerRef?: ExposedViewportElement
  viewportContentRef?: ExposedViewportElement
  colsLayerRef?: ExposedViewportElement
  rowsLayerRef?: ExposedViewportElement
  overlayRef?: ExposedViewportElement
  overlayComponentRef?: Ref<UiTableOverlayHandle | null>
}

type TableSelectionFallback = {
  selectedCell: Ref<SelectionPoint | null>
  anchorCell: Ref<SelectionPoint | null>
  selectionRanges: Ref<SelectionRange[]>
  fillPreviewRange: ComputedRef<SelectionArea | null>
  cutPreviewRanges: ComputedRef<SelectionArea[]>
  isDraggingSelection: Ref<boolean>
  isFillDragging: Ref<boolean>
  setSelection: (...args: any[]) => any
  clearSelection: (...args: any[]) => any
  focusCell: (...args: any[]) => any
  moveByPage: (...args: any[]) => any
  getSelectionSnapshot: (...args: any[]) => UiTableSelectionSnapshot
  getActiveRange: (...args: any[]) => SelectionRange | null
  selectCell: (...args: any[]) => any
  getSelectedCells: (...args: any[]) => UiTableSelectedCell[]
  fullRowSelection: Ref<{ start: number; end: number } | null>
  isCellSelected: (...args: any[]) => boolean
  isSelectionCursorCell: (...args: any[]) => boolean
  isSelectionAnchorCell: (...args: any[]) => boolean
  isCellInSelectionRange: (...args: any[]) => boolean
  isCellInCutPreview: (...args: any[]) => boolean
  isCellInFillPreview: (...args: any[]) => boolean
  isRowFullySelected: (...args: any[]) => boolean
  isColumnFullySelected: (...args: any[]) => boolean
  getSelectionEdges: (...args: any[]) => any
  getFillPreviewEdges: (...args: any[]) => any
  getCutPreviewEdges: (...args: any[]) => any
  rowHeaderClass: (...args: any[]) => any
  onCellSelect: (...args: any[]) => any
  onRowIndexClick: (...args: any[]) => any
  onColumnHeaderClick: (...args: any[]) => any
  onCellDragStart: (...args: any[]) => any
  onCellDragEnter: (...args: any[]) => any
  getActiveSelectionArea: (...args: any[]) => SelectionArea | null
  canMoveActiveSelection: (...args: any[]) => boolean
  moveSelection: (...args: any[]) => any
  moveActiveSelectionTo: (...args: any[]) => boolean
  moveByTab: (...args: any[]) => boolean
  goToRowEdge: (...args: any[]) => any
  goToColumnEdge: (...args: any[]) => any
  goToGridEdge: (...args: any[]) => any
  triggerEditForSelection: (...args: any[]) => any
  clearSelectionValues: (...args: any[]) => any
  beginCutPreview: (...args: any[]) => any
  clearCutPreview: (...args: any[]) => any
  hasCutPreview: (...args: any[]) => boolean
  applyMatrixToSelection: (...args: any[]) => any
  buildSelectionMatrix: (...args: any[]) => any
  buildSelectionSnapshot: (...args: any[]) => any
  startFillDrag: (...args: any[]) => any
  autoFillDownFromActiveRange: (...args: any[]) => any
  scheduleOverlayUpdate: (...args: any[]) => any
  handleAutoScrollFrame: (...args: any[]) => any
  lastCommittedFillArea: Ref<SelectionArea | null>
  selectionOverlayAdapter: any
  overlayScrollState: TableOverlayScrollEmitter
}

type TableSelectionApi = ReturnType<typeof useTableSelection> extends void ? TableSelectionFallback : ReturnType<typeof useTableSelection>

const tableViewportRef = ref<TableViewportExposeRefs | null>(null)
const containerRef = ref<HTMLDivElement | null>(null)

const overlayComponentHandle = computed<UiTableOverlayHandle | null>(() => {
  const viewport = tableViewportRef.value
  if (!viewport?.overlayComponentRef) {
    return null
  }
  return unref(viewport.overlayComponentRef) ?? null
})

function unwrapViewportElement(candidate: ExposedViewportElement | undefined): HTMLElement | null {
  if (candidate == null) {
    return null
  }
  const resolved = unref(candidate)
  return (resolved ?? null) as HTMLElement | null
}

const bodyContainerSurfaces = computed<HTMLElement[]>(() => {
  const surfaces: HTMLElement[] = []
  if (containerRef.value) {
    surfaces.push(containerRef.value)
  }
  return surfaces
})

const headerContainerSurfaces = computed<HTMLElement[]>(() => {
  const surfaces: HTMLElement[] = []
  const viewport = tableViewportRef.value
  if (!viewport) {
    return surfaces
  }
  if (splitPinnedLayoutEnabled.value) {
    const left = unwrapViewportElement(viewport.headerPinnedLeftContentRef ?? viewport.headerPinnedLeftRef)
    const main =
      unwrapViewportElement(viewport.headerMainContentRef ?? viewport.headerMainRef) ??
      unwrapViewportElement(viewport.headerMainRef)
    const right = unwrapViewportElement(viewport.headerPinnedRightContentRef ?? viewport.headerPinnedRightRef)
    if (left) {
      surfaces.push(left)
    }
    if (main && !surfaces.includes(main)) {
      surfaces.push(main)
    }
    if (right) {
      surfaces.push(right)
    }
  } else {
    const legacy =
      unwrapViewportElement(viewport.headerMainContentRef ?? viewport.headerMainRef) ??
      unwrapViewportElement(viewport.headerMainRef)
    if (legacy) {
      surfaces.push(legacy)
    }
  }
  return surfaces
})

const layoutContainerRef = computed<HTMLElement | null>(() => unwrapViewportElement(tableViewportRef.value?.layoutRef))
const overlayLayerContainerRef = computed<HTMLElement | null>(() => unwrapViewportElement(tableViewportRef.value?.overlayRef))
const backgroundSpacerElement = computed<HTMLElement | null>(() =>
  unwrapViewportElement(tableViewportRef.value?.backgroundSpacerRef),
)
const viewportContentElement = computed<HTMLElement | null>(() =>
  unwrapViewportElement(tableViewportRef.value?.viewportContentRef),
)
const colsLayerElement = computed<HTMLElement | null>(() => unwrapViewportElement(tableViewportRef.value?.colsLayerRef))
const scrollContainerElement = computed<HTMLElement | null>(() => unwrapViewportElement(tableViewportRef.value?.splitViewportRef))
const rowsLayerElement = computed<HTMLElement | null>(() => unwrapViewportElement(tableViewportRef.value?.rowsLayerRef))

const dimensionVariableTargets = computed<HTMLElement[]>(() => {
  const nodes = [
    backgroundSpacerElement.value,
    viewportContentElement.value,
    colsLayerElement.value,
    rowsLayerElement.value,
    layoutContainerRef.value,
    scrollContainerElement.value,
  ]
  return nodes.filter((element): element is HTMLElement => Boolean(element))
})

function resolveBodyCellElement(rowIndex: number, columnKey: string): HTMLElement | null {
  const selector = `[data-row-index="${rowIndex}"][data-col-key="${columnKey}"]`
  for (const container of bodyContainerSurfaces.value) {
    const cell = container.querySelector<HTMLElement>(selector)
    if (cell) {
      return cell
    }
  }
  return null
}

function resolveHeaderCellElement(columnKey: string): HTMLElement | null {
  const selector = `[data-column-key="${columnKey}"]`
  for (const container of headerContainerSurfaces.value) {
    const cell = container.querySelector<HTMLElement>(selector)
    if (cell) {
      return cell
    }
  }
  return null
}
const headerRef = ref<HTMLElement | null>(null)
const fallbackHeaderHeightDom = ref(Math.max(0, baseRowHeight.value))
const { viewportMetrics } = useTableViewportMetrics({
  containerRef,
  headerRef,
  fallbackHeaderHeight: fallbackHeaderHeightDom,
})
const { autoResizeColumn } = useAutoResizeColumn(localColumns, resolvedRows, {
  onWidthChange: (column, width) => {
    onColumnResize(column.key, width)
  },
})
const tableSlots = useSlots()
const visibleColumns = computed(() => localColumns.value.filter(column => column.visible !== false))
const isGridFocused = ref(false)
const ariaColCount = computed(() => Math.max(1, visibleColumns.value.length))

const columnGroupDefs = computed<UiTableColumnGroupDef[]>(() => normalizedProps.value.columnGroups)

const { rootGroups: rootColumnGroups, ungroupedColumns } = useColumnGroups({
  columns: () => visibleColumns.value,
  groupDefs: () => columnGroupDefs.value,
})

// Move focus back to the grid container without scrolling
function focusContainer() {
  if (focusActiveCellElement()) return
  nextTick(() => {
    if (focusActiveCellElement()) return
    const el = containerRef.value
    if (!el) return
    el.focus({ preventScroll: true })
    isGridFocused.value = true
  })
}

const emitGroupFilterToggle = (open: boolean) => {
  emit("group-filter-toggle", open)
  fireEvent("groupFilterToggle", open)
}

const { showVisibilityPanel, toggleVisibilityPanel, openVisibilityPanel, closeVisibilityPanel } = useTablePanels({
  focusContainer,
  emitGroupToggle: emitGroupFilterToggle,
})

const baseEntries = computed(() =>
  resolvedRows.value.map((row, originalIndex) => ({
    row,
    rowId: resolveRowKey(row as RowData, originalIndex),
    originalIndex,
  }))
)

const suspendedFilterRows = shallowRef(new Map<string, Set<number>>())

const filters = useTableFilters({
  rows: () => resolvedRows.value,
  localColumns: computed(() => visibleColumns.value),
  emitFilterChange: payload => {
    emit("filter-change", payload)
    fireEvent("filterChange", payload)
  },
  getSuspendedRows: () => suspendedFilterRows.value,
})

function cloneSortStateForServer(source?: SortState[]): UiTableSortState[] {
  if (!serverSideSorting.value) return []
  const origin = source ?? multiSortState.value
  return origin.map(entry => ({ key: entry.key, direction: entry.direction }))
}

const {
  filterMenuState,
  onColumnMenuOpen,
  onColumnMenuClose: internalOnColumnMenuClose,
  clearFilterForColumn,
  closeActiveMenu,
  applyFilters,
  getAdvancedFilter,
  setAdvancedFilter,
  clearAdvancedFilter,
  columnFilters,
  filtersState,
  resetAllFilters,
} = filters

const baseIsFilterActiveForColumn = filters.isFilterActiveForColumn

const isFilterActiveForColumn = (columnKey: string): boolean => {
  const baseActive = baseIsFilterActiveForColumn(columnKey)
  if (!serverSideFiltering.value || baseActive) {
    return baseActive
  }
  // Server-side filtering operates on trimmed datasets, so selected keys can match the cached option list.
  const applied = columnFilters.value[columnKey]
  if (Array.isArray(applied) && applied.length > 0) {
    return true
  }
  const advanced = filtersState.value[columnKey]
  return Boolean(advanced?.clauses?.length)
}

const activeMenuOptions = filters.activeMenuOptions
const effectiveMenuSelectedKeys = computed(() => Array.from(filters.activeMenuSelectedSet.value))
const filterSearchDebounce = computed(() => (serverSideFiltering.value ? SERVER_FILTER_SEARCH_DEBOUNCE_MS : undefined))
const loadFilterOptions = async (args: { search: string; offset?: number; limit?: number }) => {
  const loader = normalizedProps.value.filterOptionLoader
  if (serverSideFiltering.value && typeof loader === "function") {
    const columnKey = filterMenuState.columnKey
    if (!columnKey) {
      return filters.loadActiveMenuOptions(args.search)
    }
    try {
      const snapshot = filterSnapshotForServer(getFilterStateSnapshot())
  const response = await loader({
        columnKey,
        search: args.search,
        filters: snapshot,
        limit: args.limit ?? FILTER_OPTION_FETCH_LIMIT,
      })
      const options = (Array.isArray(response) ? response : []).map(option => {
        const baseLabel = option.label ?? formatFilterLabel(option.value)
        const label = option.count != null ? `${baseLabel} (${option.count})` : baseLabel
        return {
          key: serializeFilterValue(option.value),
          raw: option.value,
          label,
          count: option.count,
        }
      })
      return filters.loadActiveMenuOptions(args.search, options)
    } catch (error) {
      console.warn("⚠️ Failed to load server filter options", error)
      return filters.loadActiveMenuOptions(args.search)
    }
  }
  return filters.loadActiveMenuOptions(args.search)
}
const isSelectAllChecked = filters.isSelectAllChecked
const isSelectAllIndeterminate = filters.isSelectAllIndeterminate
const toggleFilterOption = filters.toggleFilterOption
const toggleSelectAll = filters.toggleSelectAll
const setFilterMenuSearchRef = filters.setFilterMenuSearchRef
const confirmFilterSelection = filters.confirmFilterSelection
const cancelFilterSelection = filters.cancelFilterSelection
const getFilterStateSnapshot = filters.getFilterStateSnapshot
const setFilterStateSnapshot = filters.setFilterStateSnapshot

let filterSnapshotForServer: (source: FilterStateSnapshot | null) => UiTableFilterSnapshot | null = () => null
let snapshotHasActiveFilters: (snapshot: UiTableFilterSnapshot | null) => boolean = () => false
let computeSortFingerprint: (sorts: UiTableSortState[]) => string = () => ""
let computeFilterFingerprint: (snapshot: UiTableFilterSnapshot | null) => string = () => ""

const activeFilterColumns = computed(() => {
  const keys = new Set<string>()
  Object.keys(columnFilters.value).forEach(key => keys.add(key))
  Object.keys(filtersState.value).forEach(key => {
    const condition = filtersState.value[key]
    if (condition?.clauses?.length) {
      keys.add(key)
    }
  })
  return keys
})

const activeFiltersCount = computed(() => activeFilterColumns.value.size)
const activeGroupCount = computed(() => groupState.value.length)
const resetTargetsCount = computed(() => activeFiltersCount.value + activeGroupCount.value)
const hasActiveFilters = computed(() => activeFiltersCount.value > 0)
const hasActiveFiltersOrGroups = computed(() => resetTargetsCount.value > 0)

function addSuspendedFilterRow(columnKey: string, originalIndex: number) {
  const current = suspendedFilterRows.value
  const next = new Map(current)
  const set = new Set(next.get(columnKey) ?? [])
  set.add(originalIndex)
  next.set(columnKey, set)
  suspendedFilterRows.value = next
}

function clearSuspendedFilterRows() {
  if (!suspendedFilterRows.value.size) return
  suspendedFilterRows.value = new Map()
}

const filteredEntries = computed(() => {
  if (serverSideFiltering.value) {
    return baseEntries.value
  }
  return applyFilters(baseEntries.value)
})

function getFilteredRowEntries() {
  return filteredEntries.value.map(entry => ({ row: entry.row, originalIndex: entry.originalIndex }))
}

function getFilteredRows() {
  return filteredEntries.value.map(entry => entry.row)
}

const {
  applySort,
  toggleColumnSort,
  getSortDirectionForColumn,
  getSortPriorityForColumn,
  ensureSortedOrder,
  applySorting,
  multiSortState,
  setMultiSortState,
} = useTableSorting({
  rows: () => resolvedRows.value,
  localColumns: computed(() => visibleColumns.value),
  emitSortChange: state => {
    emit("sort-change", state)
    fireEvent("sortChange", state)
  },
})

const {
  onApplyFilter,
  onCancelFilter,
  onSortColumn,
  onResetFilter,
} = useColumnFilterMenuBridge({
  filterMenuState,
  confirmFilterSelection,
  cancelFilterSelection,
  applySort,
  clearFilterForColumn,
})

const sortHydrated = ref(false)
const filterHydrated = ref(false)

const serverSync = useTableServerSync({
  tableId,
  serverSideFiltering,
  serverSideSorting,
  columnFilters,
  filtersState,
  filterHydrated,
  sortHydrated,
  multiSortState,
  getFilterStateSnapshot,
  settingsAdapter,
  scheduleServerReload,
  lastServerFilterFingerprint,
  lastServerSortFingerprint,
  serverFilterResolver,
  serverSortResolver,
  cloneSortStateForServer,
})

filterSnapshotForServer = serverSync.filterSnapshotForServer
snapshotHasActiveFilters = serverSync.snapshotHasActiveFilters
computeFilterFingerprint = serverSync.computeFilterFingerprint
computeSortFingerprint = serverSync.computeSortFingerprint

function mapSortStateFromAdapter(state: AdapterSortState[] | undefined | null): SortState[] {
  if (!state || !state.length) {
    return []
  }
  const result: SortState[] = []
  state.forEach(entry => {
    if (entry.direction !== "asc" && entry.direction !== "desc") return
    const key = entry.key ?? entry.field
    if (!key) return
    result.push({ key, direction: entry.direction })
  })
  return result
}

onMounted(() => {
  const storedSort = settingsAdapter.value.getSortState(tableId.value)
  const restoredSorts = Array.isArray(storedSort) && storedSort.length > 0
  if (restoredSorts) {
    setMultiSortState(mapSortStateFromAdapter(storedSort))
  }
  const storedFilters = settingsAdapter.value.getFilterSnapshot(tableId.value)
  const restoredFilters = Boolean(storedFilters)
  if (storedFilters) {
    setFilterStateSnapshot(storedFilters)
  }
  const storedGrouping = settingsAdapter.value.getGroupState(tableId.value)
  if (storedGrouping?.columns?.length) {
    const expansion = storedGrouping.expansion ?? {}
    groupExpansion.value = { ...expansion }
    groupState.value = storedGrouping.columns.map(key => ({
      key,
      expanded: expansion[key] ?? true,
    }))
  }
  const initialServerSnapshot = serverSideFiltering.value
    ? filterSnapshotForServer(getFilterStateSnapshot())
    : null
  if (serverSideFiltering.value) {
    lastServerFilterFingerprint.value = computeFilterFingerprint(initialServerSnapshot)
  }
  const initialServerSorts = serverSideSorting.value ? cloneSortStateForServer() : []
  if (serverSideSorting.value) {
    lastServerSortFingerprint.value = computeSortFingerprint(initialServerSorts)
  }
  serverFilterResolver.value = () => initialServerSnapshot
  serverSortResolver.value = () => initialServerSorts
  if (serverSideModel.value) {
    const shouldReloadForFilters = restoredFilters && snapshotHasActiveFilters(initialServerSnapshot)
    const shouldReloadForSorts = restoredSorts && initialServerSorts.length > 0
    if (shouldReloadForFilters) {
      scheduleServerReload("filter-change")
    } else if (shouldReloadForSorts) {
      scheduleServerReload("sort-change")
    }
  }
  sortHydrated.value = true
  filterHydrated.value = true
  groupHydrated.value = true
})


const sortedRows = computed(() => {
  const sourceEntries = serverSideSorting.value ? filteredEntries.value : applySorting(filteredEntries.value)
  return sourceEntries.map((entry: VisibleRow) => {
    const stickyTopFlag = (entry as any).stickyTop ?? entry.row?.stickyTop
    const rawStickyBottom = (entry as any).stickyBottom ?? entry.row?.stickyBottom ?? (entry.row?.sticky ? true : undefined)
    return {
      ...entry,
      stickyTop: stickyTopFlag,
      stickyBottom: rawStickyBottom,
    }
  })
})

const {
  groupState,
  groupExpansion,
  groupHydrated,
  groupedColumns,
  groupedColumnSet,
  groupOrderMap,
  groupRowMembership,
  processedRows,
  findRowIndexByRowId,
  getRowIdForDisplayIndex,
  toggleGroupRow,
  isGroupExpanded,
  onGroupColumn,
  isGroupRowEntry,
  setGroupRefreshCallback,
} = useTableGrouping({
  sortedRows,
  settingsAdapter,
  tableId,
  isPlaceholderRow: isServerPlaceholderRow,
})

const selectableRows = computed<RowData[]>(() => {
  if (!selectionEnabled.value) {
    return []
  }
  const rows: RowData[] = []
  for (const entry of processedRows.value) {
    if (isGroupRowEntry(entry)) {
      continue
    }
    const row = (entry.row ?? {}) as RowData
    if (isServerPlaceholderRow(row)) {
      continue
    }
    resolveRowKey(row, entry.originalIndex)
    rows.push(row)
  }
  return rows
})

const isSelectionControlled = computed(() => selectionEnabled.value && normalizedProps.value.selection.controlled)
const selectionModel = computed<(RowData | RowKey)[] | undefined>(() => {
  if (!selectionEnabled.value) {
    return undefined
  }
  return normalizedProps.value.selection.selected
})

const rowSelection = useSelectableRows<RowData>({
  rows: selectableRows,
  modelValue: selectionModel,
  controlled: isSelectionControlled,
  emitUpdate: rows => {
    emit("update:selected", rows)
  },
  rowKey: rowKeyResolver,
})

const selectedRowCount = computed(() => rowSelection.selectedKeySet.value.size)
const formattedSelectedRowCount = computed(() => rowCountFormatter.format(selectedRowCount.value))

const groupSelectionVisible = computed(() => selectionColumnVisible.value)

const EMPTY_GROUP_SELECTION_STATE: GroupSelectionState = {
  checked: false,
  indeterminate: false,
  selectable: false,
  total: 0,
  selected: 0,
}

const groupSelectionStateMap = computed(() => {
  const map = new Map<string, GroupSelectionState>()
  if (!groupSelectionVisible.value || !selectionEnabled.value) {
    return map
  }

  const selectedKeys = rowSelection.selectedKeySet.value

  groupRowMembership.value.forEach((rows, groupKey) => {
    let selectableCount = 0
    let selectedCount = 0

    for (const row of rows) {
      if (!isSelectableDataRow(row)) {
        continue
      }
      selectableCount += 1
      const resolvedKey = rowKeyResolver(row)
      if (selectedKeys.has(resolvedKey)) {
        selectedCount += 1
      }
    }

    if (!selectableCount) {
      map.set(groupKey, EMPTY_GROUP_SELECTION_STATE)
      return
    }

    const checked = selectedCount === selectableCount
    const indeterminate = !checked && selectedCount > 0

    map.set(groupKey, {
      checked,
      indeterminate,
      selectable: true,
      total: selectableCount,
      selected: selectedCount,
    })
  })

  return map
})

function getGroupSelectionState(groupKey: string): GroupSelectionState {
  return groupSelectionStateMap.value.get(groupKey) ?? EMPTY_GROUP_SELECTION_STATE
}

function toggleGroupSelection(groupKey: string, next?: boolean) {
  if (!groupSelectionVisible.value || !selectionEnabled.value) {
    return
  }

  const membership = groupRowMembership.value.get(groupKey)
  if (!membership || membership.length === 0) {
    return
  }

  const candidates = membership.filter(row => isSelectableDataRow(row))
  if (!candidates.length) {
    return
  }

  const targetState = getGroupSelectionState(groupKey)
  const shouldSelect = typeof next === "boolean" ? next : !(targetState.checked && !targetState.indeterminate)
  const workingKeys = new Set<RowKey>(rowSelection.selectedKeySet.value)

  for (const row of candidates) {
    const key = rowKeyResolver(row)
    if (shouldSelect) {
      workingKeys.add(key)
    } else {
      workingKeys.delete(key)
    }
  }

  const nextRows: RowData[] = []
  for (const row of selectableRows.value) {
    const key = rowKeyResolver(row)
    if (workingKeys.has(key)) {
      nextRows.push(row)
    }
  }

  rowSelection.setSelection(nextRows)
}

watch(
  selectionEnabled,
  enabled => {
    if (!enabled) {
      rowSelection.clearSelection()
    }
  },
  { immediate: false },
)

const selectableRowCount = computed(() => selectableRows.value.length)
const headerSelectionCheckboxRef = ref<HTMLInputElement | null>(null)

function setHeaderSelectionCheckboxRef(element: Element | ComponentPublicInstance | null) {
  headerSelectionCheckboxRef.value = element instanceof HTMLInputElement ? element : null
}

function emitSelectAllRequest(checked: boolean) {
  const payload: UiTableSelectAllRequestPayload = {
    checked,
    filters: filterSnapshotForServer(getFilterStateSnapshot()),
    sorts: cloneSortStateForServer(),
    selection: {
      allSelected: rowSelection.allSelected.value,
      isIndeterminate: rowSelection.isIndeterminate.value,
      selectedRowKeys: Array.from(rowSelection.selectedKeySet.value),
      visibleRowCount: selectableRowCount.value,
      totalRowCount: normalizedProps.value.totalRows ?? null,
    },
  }
  emit("select-all-request", payload)
  fireEvent("selectAllRequest", payload)
}

watchEffect(() => {
  const checkbox = headerSelectionCheckboxRef.value
  if (!checkbox) {
    return
  }
  checkbox.indeterminate = selectionEnabled.value && rowSelection.isIndeterminate.value
})

function handleHeaderSelectionChange(event: Event) {
  if (!selectionEnabled.value) {
    return
  }
  event.stopPropagation()
  const target = event.target as HTMLInputElement | null
  const nextChecked = target?.checked ?? !(rowSelection.allSelected.value && !rowSelection.isIndeterminate.value)
  if (serverSideModel.value) {
    emitSelectAllRequest(nextChecked)
  } else if (nextChecked) {
    rowSelection.selectAll()
  } else {
    rowSelection.clearSelection()
  }
  if (target) {
    target.blur()
  }
}

function selectAllRows() {
  if (serverSideModel.value) {
    emitSelectAllRequest(true)
    return
  }
  rowSelection.selectAll()
}

function isSelectableDataRow(row: any): row is RowData {
  if (!row || (row as any).__group) {
    return false
  }
  if (isServerPlaceholderRow(row)) {
    return false
  }
  return true
}

function isCheckboxRowSelected(row: any): boolean {
  if (!selectionEnabled.value || !isSelectableDataRow(row)) {
    return false
  }
  return rowSelection.isRowSelected(row)
}

function handleRowCheckboxToggle(row: any) {
  if (!selectionEnabled.value || !isSelectableDataRow(row)) {
    return
  }
  rowSelection.toggleRow(row)
}

function rowGridClass(row: any) {
  if (!selectionEnabled.value || !isSelectableDataRow(row)) {
    return undefined
  }
  if (!rowSelection.isRowSelected(row)) return undefined
  return selectedRowClass.value || undefined
}

function handleNearBottom() {
  if (serverSideModel.value) {
    return
  }
  if (autoLazyOnScroll.value) {
    void requestNextPage("scroll")
    return
  }
  if (loadingState.value) return
  emit("reach-bottom")
  fireEvent("reachBottom")
}

let handleAutoScrollFrame: ((event: { lastPointer: PointerCoordinates | null }) => void) | null = null

const { updateAutoScroll, stopAutoScroll, lastPointer } = useTableAutoScroll({
  containerRef,
  onFrame: event => handleAutoScrollFrame?.(event),
})

const zoom = ref(1)
const zoomLayoutScale = computed(() => 1)
const toDomUnits = (value: number) => value

const { scheduleAutoColumnResize: scheduleAutoColumnResizeInternal, enableAutoColumnResize } =
  useTableAutoColumnScheduler({
    autoColumnResize,
    visibilityHydrated,
    localColumns,
    viewportWidth: viewportWidthForAutoResize,
    zoomLayoutScale,
    refreshViewport: () => refreshViewportCallback.value(),
    scheduleOverlayUpdate: () => overlayUpdateCallback.value(),
  })

scheduleAutoColumnResizeImpl.value = scheduleAutoColumnResizeInternal

let handleViewportAfterScroll: (() => void) | null = null

const viewport = useTableViewport({
  containerRef,
  headerRef,
  processedRows,
  columns: computed(() => visibleColumns.value),
  onAfterScroll: () => handleViewportAfterScroll?.(),
  onNearBottom: handleNearBottom,
  isLoading: loadingState,
  rowHeightMode,
  baseRowHeight,
  viewportMetrics,
  serverIntegration: {
    enabled: serverSideModel,
    rowModel: serverRowModel,
  },
  normalizeAndClampScroll: true,
  horizontalVirtualizationEnabled: false,
})

const {
  scrollTop,
  scrollLeft,
  viewportHeight,
  viewportWidth,
  totalRowCount,
  effectiveRowHeight,
  visibleRowsPool,
  rowPoolVersion,
  totalContentHeight,
  startIndex,
  endIndex,
  virtualizationEnabled,
  visibleStartCol,
  visibleEndCol,
  visibleScrollableEntries,
  leftPadding,
  rightPadding,
  columnWidthMap,
  handleScroll: handleViewportScroll,
  updateViewportHeight,
  measureRowHeight,
  clampScrollTopValue,
  cancelScrollRaf,
  scrollToRow,
  scrollToColumn,
  isRowVisible,
  debugMode,
  refresh,
} = viewport

const pinnedLeftEntries = computed<ColumnMetric[]>(() => [])
const pinnedRightEntries = computed<ColumnMetric[]>(() => [])

const pinnedRightWidth = computed(() => 0)

const pinnedLeftOffset = computed(() => 0)
const pinnedRightOffset = computed(() => 0)

type TableRecalcOptions = {
  refresh?: boolean
  overlay?: boolean
  viewport?: boolean
}

const requestTableRecalc = (_reason: string, options: TableRecalcOptions = {}) => {
  const { refresh: shouldRefresh = true, overlay = false, viewport = false } = options
  if (shouldRefresh) {
    refresh(true)
  }
  if (viewport) {
    updateViewportHeight()
  }
  if (overlay) {
    scheduleOverlayUpdate()
  }
}

refreshViewportCallback.value = () => requestTableRecalc("viewport-refresh")

updateViewportHeightCallback = updateViewportHeight

setScrollToTopForServer(() => {
  scrollToRow(0)
})

onBeforeUnmount(() => {
  setScrollToTopForServer(null)
})

watch(
  viewportWidth,
  value => {
    viewportWidthForAutoResize.value = value
  },
  { immediate: true },
)

enableAutoColumnResize()

const tableContainerClass = computed<Record<string, boolean>>(() => {
  const classes: Record<string, boolean> = {}
  const base = tableContainerBaseClass.value

  if (Array.isArray(base)) {
    base.forEach(entry => {
      if (typeof entry === "string" && entry.trim().length > 0) {
        classes[entry] = true
      } else if (entry && typeof entry === "object") {
        Object.assign(classes, entry)
      }
    })
  } else if (typeof base === "string" && base.trim().length > 0) {
    classes[base] = true
  } else if (base && typeof base === "object") {
    Object.assign(classes, base as Record<string, boolean>)
  }

  return classes
})

const refreshGroupedView = () => {
  nextTick(() => requestTableRecalc("group-refresh", { overlay: true, viewport: true }))
}

setGroupRefreshCallback(refreshGroupedView)

onBeforeUnmount(() => {
  setGroupRefreshCallback(null)
})

function groupCellStyle(level: number | null | undefined): Record<string, string> {
  const safeLevel = Number.isFinite(level as number) ? Number(level) : 0
  const indent = GROUP_INDENT_BASE + Math.max(0, safeLevel) * GROUP_INDENT_STEP
  return {
    paddingLeft: `${indent}px`,
  }
}

const rowHeightDom = computed(() => toDomUnits(effectiveRowHeight.value))

watch(
  rowHeightDom,
  value => {
    if (Number.isFinite(value) && value > 0) {
      fallbackHeaderHeightDom.value = value
    }
  },
  { immediate: true },
)
const totalContentHeightDom = computed(() => toDomUnits(totalContentHeight.value))
const leftPaddingDom = computed(() => toDomUnits(leftPadding.value))
const rightPaddingDom = computed(() => toDomUnits(rightPadding.value))
const viewportWidthDom = computed(() => toDomUnits(viewportWidth.value))

const rowPoolWindow = computed(() => {
  rowPoolVersion.value
  const pool = visibleRowsPool.value
  let minIndex = Number.POSITIVE_INFINITY
  let maxIndex = Number.NEGATIVE_INFINITY

  for (const item of pool) {
    const index = item.rowIndex
    if (typeof index === "number" && Number.isFinite(index) && index >= 0) {
      if (index < minIndex) {
        minIndex = index
      }
      if (index > maxIndex) {
        maxIndex = index
      }
    }
  }

  if (!Number.isFinite(minIndex) || !Number.isFinite(maxIndex)) {
    const fallbackStart = Math.max(0, startIndex.value)
    const fallbackCount = pool.length > 0 ? pool.length : 0
    return { start: fallbackStart, count: Math.max(0, fallbackCount) }
  }

  const spanCount = Math.max(0, maxIndex - minIndex + 1)
  return { start: minIndex, count: spanCount }
})

const rowWindowStart = computed(() => Math.max(0, rowPoolWindow.value.start))
const rowsLayerHeightDom = computed(() => totalContentHeightDom.value)
const columnWidthDomMap = computed(() => {
  const map = new Map<string, number>()
  columnWidthMap.value.forEach((width, key) => {
    map.set(key, toDomUnits(width))
  })
  return map
})

const totalColumnWidthDom = computed(() => {
  let total = 0
  columnWidthDomMap.value.forEach(value => {
    if (Number.isFinite(value)) {
      total += Math.max(0, value)
    }
  })
  return total
})

interface ColumnSurface {
  columnKey: string
  left: number
  width: number
  entry: HeaderRenderableEntry
}

interface RegionSurface {
  list: ColumnSurface[]
  map: Map<string, ColumnSurface>
  totalWidth: number
}

const resolveColumnDomWidth = (entry: HeaderRenderableEntry): number => {
  const columnKey = entry.metric.column.key
  const domWidth = columnWidthDomMap.value.get(columnKey)
  if (domWidth != null && Number.isFinite(domWidth)) {
    return Math.max(0, domWidth)
  }
  const layoutWidthCandidate =
    columnWidthMap.value.get(columnKey) ??
    entry.metric.width ??
    entry.metric.column.width ??
    entry.metric.column.minWidth ??
    0
  const layoutWidth = typeof layoutWidthCandidate === "number" && Number.isFinite(layoutWidthCandidate)
    ? layoutWidthCandidate
    : 0
  return Math.max(0, toDomUnits(layoutWidth))
}

const buildRegionSurface = (
  entries: HeaderRenderableEntry[],
  options: { leftPad?: number; rightPad?: number },
): RegionSurface => {
  const { leftPad = 0, rightPad = 0 } = options
  let offset = 0
  const list: ColumnSurface[] = []
  const map = new Map<string, ColumnSurface>()

  entries.forEach(entry => {
    if (entry.showLeftFiller) {
      offset += Math.max(0, leftPad)
    }

    const width = resolveColumnDomWidth(entry)
    const surface: ColumnSurface = {
      columnKey: entry.metric.column.key,
      left: offset,
      width,
      entry,
    }

    list.push(surface)
    map.set(surface.columnKey, surface)
    offset += width

    if (entry.showRightFiller) {
      offset += Math.max(0, rightPad)
    }
  })

  return {
    list,
    map,
    totalWidth: offset,
  }
}

const columnSurfaces = computed(() => {
  const pinnedLeft = buildRegionSurface(headerPinnedLeftEntries.value, { leftPad: 0, rightPad: 0 })
  const center = buildRegionSurface(headerMainEntries.value, {
    leftPad: leftPaddingDom.value,
    rightPad: rightPaddingDom.value,
  })
  const pinnedRight = buildRegionSurface(headerPinnedRightEntries.value, { leftPad: 0, rightPad: 0 })

  const aggregate = new Map<string, ColumnSurface>()
  for (const surface of pinnedLeft.map.values()) {
    aggregate.set(surface.columnKey, surface)
  }
  for (const surface of center.map.values()) {
    aggregate.set(surface.columnKey, surface)
  }
  for (const surface of pinnedRight.map.values()) {
    aggregate.set(surface.columnKey, surface)
  }

  return {
    byRegion: {
      "pinned-left": pinnedLeft,
      center,
      "pinned-right": pinnedRight,
    } as Record<UiTableRowRegion, RegionSurface>,
    map: aggregate,
  }
})

const selectionOverlayColumnSurfaces = computed<Map<string, SelectionOverlayColumnSurface>>(() => {
  const result = new Map<string, SelectionOverlayColumnSurface>()
  const surfaces = columnSurfaces.value

  const appendRegion = (region: UiTableRowRegion, pin: SelectionOverlayColumnSurface["pin"]) => {
    const regionSurface = surfaces.byRegion[region]
    if (!regionSurface) {
      return
    }
    for (const entry of regionSurface.list) {
      if (!Number.isFinite(entry.width) || entry.width <= 0) {
        continue
      }
      result.set(entry.columnKey, {
        left: entry.left,
        width: entry.width,
        pin,
      })
    }
  }

  appendRegion("pinned-left", "left")
  appendRegion("center", "none")
  appendRegion("pinned-right", "right")

  return result
})

const resolveColumnSurface = (region: UiTableRowRegion, columnKey: string): ColumnSurface | null => {
  const surfaces = columnSurfaces.value
  const regionSurface = surfaces.byRegion[region]?.map.get(columnKey)
  if (regionSurface) {
    return regionSurface
  }
  return surfaces.map.get(columnKey) ?? null
}

function computePinnedSectionWidthDom(entries: HeaderRenderableEntry[]): number {
  if (!entries.length) {
    return 0
  }

  const widthMap = columnWidthDomMap.value
  const layoutWidthMap = columnWidthMap.value
  const leftPad = leftPaddingDom.value ?? 0
  const rightPad = rightPaddingDom.value ?? 0

  let total = 0

  for (const entry of entries) {
    if (entry.showLeftFiller) {
      total += Math.max(0, leftPad)
    }

    const columnKey = entry.metric.column.key
    const domWidth = widthMap.get(columnKey)

    if (domWidth != null && Number.isFinite(domWidth)) {
      total += Math.max(0, domWidth)
    } else {
      const layoutWidthCandidate =
        layoutWidthMap.get(columnKey) ??
        entry.metric.width ??
        entry.metric.column.width ??
        entry.metric.column.minWidth ??
        0
      const layoutWidth = typeof layoutWidthCandidate === "number" && Number.isFinite(layoutWidthCandidate)
        ? layoutWidthCandidate
        : 0
      total += Math.max(0, toDomUnits(layoutWidth))
    }

    if (entry.showRightFiller) {
      total += Math.max(0, rightPad)
    }
  }

  return total
}

const pinnedLeftWidthDom = computed(() => computePinnedSectionWidthDom(headerPinnedLeftEntries.value))
const pinnedRightWidthDomValue = computed(() => computePinnedSectionWidthDom(headerPinnedRightEntries.value))

const viewportPinnedVars = computed(() => ({
  "--ui-table-pinned-left-width": `${Math.max(0, pinnedLeftWidthDom.value)}px`,
  "--ui-table-pinned-right-width": `${Math.max(0, pinnedRightWidthDomValue.value)}px`,
}))

const tableInlineStyle = computed(() => ({
  ...tableThemeVars.value,
  ...viewportPinnedVars.value,
}))

const clampDimension = (value: number) => (Number.isFinite(value) && value > 0 ? value : 0)

watch(
  () => ({
    targets: dimensionVariableTargets.value,
    width: clampDimension(totalColumnWidthDom.value),
    height: clampDimension(totalContentHeightDom.value),
  }),
  ({ targets, width, height }) => {
    if (!targets.length) {
      return
    }

    const widthValue = `${width}px`
    const heightValue = `${height}px`

    for (const target of targets) {
      target.style.setProperty("--ui-table-total-width", widthValue)
      target.style.setProperty("--ui-table-total-height", heightValue)
    }
  },
  { immediate: true },
)

watch(
  () => ({
    element: rowsLayerElement.value,
    height: clampDimension(rowsLayerHeightDom.value),
  }),
  ({ element, height }) => {
    if (!element) {
      return
    }
    if (element.style.transform) {
      element.style.removeProperty("transform")
    }
    element.style.height = `${height}px`
  },
  { immediate: true },
)

const pooledRows = computed<RowPoolItem[]>(() => {
  rowPoolVersion.value
  return visibleRowsPool.value
})

const isSearchMatchCell = (_rowIndex: number | null | undefined, _columnKey: string) => false
const isActiveSearchMatchCell = (_rowIndex: number | null | undefined, _columnKey: string) => false

const {
  stickyBottomOffsets,
  isColumnSticky: headerIsColumnSticky,
  getStickySide: headerGetStickySide,
  getStickyLeftOffset: headerGetStickyLeftOffset,
  getStickyRightOffset: headerGetStickyRightOffset,
  systemColumnStyle,
  summaryCellStyle,
  getStickyTopOffset,
} = useTableStickyColumns({
  visibleColumns,
  pinnedLeftEntries,
  pinnedRightEntries,
  columnWidthDomMap,
  columnWidthMap,
  toDomUnits,
  processedRows,
  rowHeightDom,
  pooledRows,
  viewportHeight,
  scrollTop,
  selectionColumnKey: SELECTION_COLUMN_KEY,
  splitPinnedLayoutEnabled,
})

const bodyIsColumnSticky = () => false
const bodyGetStickySide = () => null
const bodyGetStickyLeftOffset = () => undefined
const bodyGetStickyRightOffset = () => undefined

const totalRowCountDisplay = computed(() => normalizedProps.value.totalRows ?? sortedRows.value.length)

const formattedRowCount = computed(() => rowCountFormatter.format(Math.max(0, totalRowCountDisplay.value ?? 0)))

watch(
  () => normalizedProps.value.debugViewport,
  value => {
    debugMode.value = Boolean(value)
  },
  { immediate: true }
)

function reorderColumns(nextColumnsOrder: UiTableColumn[]) {
  const orderMap = new Map(nextColumnsOrder.map((column, index) => [column.key, index]))
  const nextColumns = localColumns.value
    .map((column, originalIndex) => ({ column: { ...column }, originalIndex }))
    .sort((a, b) => {
      const orderA = orderMap.has(a.column.key) ? orderMap.get(a.column.key)! : Number.MAX_SAFE_INTEGER
      const orderB = orderMap.has(b.column.key) ? orderMap.get(b.column.key)! : Number.MAX_SAFE_INTEGER
      if (orderA === orderB) {
        return a.originalIndex - b.originalIndex
      }
      return orderA - orderB
    })
    .map(entry => entry.column)

  localColumns.value = nextColumns
  const snapshot = updateVisibilityMapFromColumns(nextColumns)
  persistColumnState(snapshot)
}

const {
  resolveColumnPinState,
  applyStoredPinState: useApplyStoredPinState,
  reorderPinnedColumns: useReorderPinnedColumns,
  setColumnPin,
} = useTableColumnPinning()

applyStoredPinStateImpl = useApplyStoredPinState
reorderPinnedColumnsImpl = useReorderPinnedColumns

function handleHeaderReorder(entries: HeaderRenderableEntry[]) {
  const orderedColumns = entries.map(entry => entry.metric.column)
  reorderColumns(orderedColumns)
  reorderPinnedColumns()
}

function handleColumnPin(column: UiTableColumn, position: ColumnPinPosition) {
  // Pinning is disabled in lite build.
  if (column.isSystem) return
  setColumnPin(column.key, position)
  closeActiveMenu()
}

const {
  headerRenderableEntries,
  headerPinnedLeftEntries,
  headerMainEntries,
  headerPinnedRightEntries,
  headerGroupRows,
  headerPinnedLeftGroupRows,
  headerMainGroupRows,
  headerPinnedRightGroupRows,
  hasColumnGroups,
  visibleColumnEntries,
  columnTrackStartMap,
} = useTableHeaderLayout({
  rootColumnGroups,
  ungroupedColumns,
  pinnedLeftEntries,
  pinnedRightEntries,
  visibleScrollableEntries,
  leftPadding,
  rightPadding,
})

const { virtualContainerStyle, headerRowStickyStyle, rowLayerStyle } = useTableGridLayout({
  totalContentHeightDom,
  rowHeightDom,
  stickyBottomOffsets,
  startIndex,
  rowWindowStart,
})

let applyHistoryEntries: (entries: HistoryEntry[], direction: "undo" | "redo") => CellEditEvent[] = () => []
let historyAppliedCallback: ((direction: "undo" | "redo", events: CellEditEvent[]) => void) | null = null

const { undo, redo, recordHistory, isApplyingHistory } = useTableHistory({
  applyEntries: (entries, direction) => applyHistoryEntries(entries, direction),
  onHistoryApplied: (direction, events) => historyAppliedCallback?.(direction, events),
})

// Emit a single-cell edit event to parent listeners
function emitCellEdit(event: CellEditEvent) {
  emit("cell-edit", event)
  fireEvent("cellEdit", event)
}

// Emit a batch edit event with cloned payload to avoid mutations
function emitBatchEdit(events: CellEditEvent[]) {
  const payload = events.map(item => ({ ...item }))
  emit("batch-edit", payload)
  fireEvent("batchEdit", payload)
}

const editing = useTableEditing({
  processedRows,
  localColumns: computed(() => visibleColumns.value),
  emitCellEdit,
  emitBatchEdit,
  recordHistory,
  isApplyingHistory,
  findRowIndexById: rowId => findRowIndexByRowId(rowId) ?? null,
  getRowIdByIndex: index => getRowIdForDisplayIndex(index),
})

const {
  validationErrors,
  getValidationError,
  setCellValueDirect,
  setCellValue,
  setCellValueFromPaste,
  getCellValue,
  getCellRawValue,
  dispatchEvents,
  onCellEdit,
  onCellEditingChange: internalOnCellEditingChange,
  requestEdit,
  isEditingCell,
  editCommand,
} = editing

watch(
  isEditingCell,
  (value: boolean) => {
    if (!value) {
      nextTick(() => focusContainer())
    }
  }
)

function handleCellEditingChange(editingState: boolean, columnKey: string, originalIndex: number | null | undefined) {
  internalOnCellEditingChange(editingState)
  if (!editingState) return
  if (!activeFilterColumns.value.has(columnKey)) return
  if (typeof originalIndex !== "number") return
  addSuspendedFilterRow(columnKey, originalIndex)
}

const isColumnEditable = (column: UiTableColumn | undefined) => {
  if (isFullRowSelectionMode.value) return false
  if (column?.isSystem) return false
  return baseIsColumnEditable(column)
}

// Replay history entries into the grid and return dispatched events
applyHistoryEntries = (entries, direction) => {
  const events: CellEditEvent[] = []
  for (const entry of entries) {
    const value = direction === "undo" ? entry.oldValue : entry.newValue
    const columnIndex = visibleColumns.value.findIndex(column => column.key === entry.columnKey)
    if (columnIndex === -1) {
      continue
    }

    const targetRowId = entry.rowId as RowKey | undefined
    let rowRef: RowKey | number = entry.rowIndex

    if (targetRowId !== undefined && targetRowId !== null) {
      const currentIndex = findRowIndexByRowId(targetRowId)
      if (currentIndex !== null && currentIndex !== undefined) {
        rowRef = typeof targetRowId === "string" ? targetRowId : currentIndex
      }
    }

    setCellValueDirect(rowRef, columnIndex, value, {
      collector: events,
      suppressHistory: true,
      force: true,
    })
  }
  return events
}

const {
  selectedCell,
  anchorCell,
  overlayScrollState,
  isFillDragging,
  setSelection,
  clearSelection,
  focusCell,
  moveByPage,
  getSelectionSnapshot,
  getActiveRange,
  selectCell,
  getSelectedCells,
  fullRowSelection,
  isCellSelected,
  isSelectionCursorCell,
  isSelectionAnchorCell,
  isCellInSelectionRange,
  isCellInCutPreview,
  isCellInFillPreview,
  isRowFullySelected,
  isColumnFullySelected,
  getSelectionEdges,
  getFillPreviewEdges,
  getCutPreviewEdges,
  rowHeaderClass,
  onCellSelect,
  onRowIndexClick,
  onColumnHeaderClick,
  onCellDragStart,
  onCellDragEnter,
  getActiveSelectionArea,
  canMoveActiveSelection,
  moveSelection,
  moveActiveSelectionTo,
  moveByTab,
  goToRowEdge,
  goToColumnEdge,
  goToGridEdge,
  triggerEditForSelection,
  clearSelectionValues,
  beginCutPreview,
  clearCutPreview,
  commitPendingCut,
  hasCutPreview,
  applyMatrixToSelection,
  buildSelectionMatrix,
  buildSelectionSnapshot,
  startFillDrag,
  autoFillDownFromActiveRange: autoFillDown,
  scheduleOverlayUpdate,
  handleAutoScrollFrame: internalHandleAutoScrollFrame,
  lastCommittedFillArea,
} = useTableSelection({
  containerRef,
  overlayContainerRef: overlayLayerContainerRef,
  overlayComponentRef: overlayComponentHandle,
  localColumns: computed(() => visibleColumns.value),
  processedRows,
  totalRowCount,
  getRowIdByIndex: index => getRowIdForDisplayIndex(index),
  findRowIndexById: rowId => findRowIndexByRowId(rowId) ?? null,
  viewport: {
    effectiveRowHeight,
    viewportHeight,
    viewportWidth,
    scrollTop,
    scrollLeft,
    startIndex,
    endIndex,
    visibleStartCol,
    visibleEndCol,
    columnWidthMap,
    pinnedLeftEntries,
    pinnedRightEntries,
    virtualizationEnabled,
    clampScrollTopValue,
    scrollToColumn,
  },
  isEditingCell,
  columnSurfaces: selectionOverlayColumnSurfaces,
  overlayScrollControlledExternally: true,
  focusContainer,
  emitSelectionChange: snapshot => {
    const emittedSnapshot = snapshot.clone()
    emit("selection-change", emittedSnapshot)
    fireEvent("selectionChange", emittedSnapshot.clone())
  },
  setCellValueDirect,
  setCellValueFromPaste,
  getCellRawValue,
  dispatchEvents,
  recordHistory,
  refreshViewport: refresh,
  stopAutoScroll,
  updateAutoScroll,
  lastPointer,
  deleteRows: rows => {
    emit("rows-delete", rows)
    fireEvent("rowsDelete", rows)
  },
  rowIndexColumnKey: normalizedProps.value.showRowIndexColumn ? ROW_INDEX_COLUMN_KEY : undefined,
  rowSelectionMode: computed(() => normalizedProps.value.selection.mode),
  resolveCellElement: resolveBodyCellElement,
  resolveHeaderCellElement: resolveHeaderCellElement,
  pinnedRightWidth,
  pinnedLeftOffset,
  pinnedRightOffset,
}) as unknown as TableSelectionApi

const overlayController = useTableOverlayController({
  scrollHostRef: containerRef,
  overlayState: overlayScrollState,
  pinnedLeftOffset,
  pinnedRightOffset,
})

const {
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
} = useTableAdvancedFilterModal({
  visibleColumns,
  getAdvancedFilter,
  setAdvancedFilter,
  clearAdvancedFilter,
  refresh,
  scheduleOverlayUpdate,
  closeActiveMenu,
})

const scheduleOverlayFrame = () => {
  overlayController.requestSync()
  scheduleOverlayUpdate()
}

scheduleOverlayUpdateCallback = scheduleOverlayFrame
overlayUpdateCallback.value = () => scheduleOverlayFrame()

const {
  getCellTabIndex,
  getCellDomId,
  getAriaRowIndex,
  getAriaColIndex,
  getHeaderTabIndex,
  focusActiveCellElement,
  onCellComponentFocus,
  onGridFocusIn,
  onGridFocusOut,
} = useTableFocusManagement({
  anchorCell,
  selectedCell,
  isCellInSelectionRange,
  selectCell,
  visibleColumns,
  containerRef,
  focusableContainers: bodyContainerSurfaces,
  resolveCellElement: resolveBodyCellElement,
  isGridFocused,
})

const {
  getColumnIndex,
  columnIndexToKey,
  bodyColumnBindings,
  getBodyColumnBinding,
} = useTableColumnBindings({
  headerRenderableEntries,
  visibleColumnEntries,
  columnTrackStartMap,
  localColumns,
  columnWidthDomMap,
  bodyCellClass,
  isColumnSticky: bodyIsColumnSticky,
  getStickySide: bodyGetStickySide,
  getStickyLeftOffset: bodyGetStickyLeftOffset,
  getStickyRightOffset: bodyGetStickyRightOffset,
  getAriaColIndex,
})

const { hoverOverlayStyle, hoverOverlayVisible } = useTableHoverOverlay({
  containerRef,
  pointerContainerRef: layoutContainerRef,
  headerRef,
  isHoverableTable,
  headerRenderableEntries,
  bodyColumnBindings,
  columnWidthDomMap,
  leftPaddingDom,
  rightPaddingDom,
  viewportWidth,
  viewportHeight,
  scrollTop,
  scrollLeft,
  rowHeightDom,
  processedRows,
  totalRowCount,
  stickyBottomOffsets,
  toDomUnits,
  getStickyTopOffset,
  columnIndexToKey,
  overlayScrollState,
  scheduleOverlayUpdate,
})

const renderTableSlotNodes = (slotName: string, slotProps: Record<string, unknown>) => {
  const slot = tableSlots[slotName]
  if (!slot) {
    return []
  }
  const result = slot(slotProps)
  return Array.isArray(result) ? result : []
}

interface BuildRowViewOptions {
  pooled: RowPoolItem
  entries: HeaderRenderableEntry[]
  region: UiTableRowRegion
}

const buildDataCellProps = (
  pooledRow: RowPoolItem,
  binding: UiTableColumnBinding,
  columnKey: string,
): Record<string, unknown> => {
  const rowEntry = pooledRow.entry
  const rowIndex = pooledRow.displayIndex
  const columnIndex = binding.columnIndex

  if (!rowEntry?.row) {
    return {
      row: rowEntry?.row ?? {},
      col: binding.column,
      rowIndex,
      colIndex: columnIndex,
      zoomScale: 1,
      visualWidth: binding.visualWidth,
      editable: isColumnEditable(binding.column),
    }
  }

  const targetRowIndex = rowEntry.displayIndex ?? rowIndex

  return {
    row: rowEntry.row,
    col: binding.column,
    rowIndex,
    originalRowIndex: rowEntry.originalIndex ?? undefined,
    colIndex: columnIndex,
    zoomScale: 1,
    visualWidth: binding.visualWidth,
    isSelected: isSelectionCursorCell(rowIndex, columnIndex),
    isSelectionAnchor: isSelectionAnchorCell(rowIndex, columnIndex),
    isRowSelected: isRowFullySelected(rowIndex),
    isColumnSelected: isColumnFullySelected(columnIndex),
    isRangeSelected: isCellInSelectionRange(rowIndex, columnIndex),
    editCommand: editCommand.value,
    editable: isColumnEditable(binding.column),
    validationError: getValidationError(rowIndex, columnIndex),
    tabIndex: getCellTabIndex(rowIndex, columnIndex),
    ariaRowIndex: getAriaRowIndex(rowIndex),
    ariaColIndex: binding.ariaColIndex,
    cellId: getCellDomId(rowIndex, binding.column.key),
    sticky: binding.sticky,
    stickySide: binding.stickySide,
    stickyLeftOffset: binding.stickyLeftOffset,
    stickyRightOffset: binding.stickyRightOffset,
    stickyTopOffset: getStickyTopOffset(rowEntry),
    stickyTop: Boolean(rowEntry.stickyTop ?? rowEntry.row?.stickyTop),
    searchMatch: isSearchMatchCell(targetRowIndex, columnKey),
    activeSearchMatch: isActiveSearchMatchCell(targetRowIndex, columnKey),
  }
}

const createSelectionCellDescriptor = (
  options: {
    pooled: RowPoolItem
    entry: HeaderRenderableEntry
    surface: ColumnSurface
  },
): UiTableRowCellDescriptor => {
  const { pooled, entry, surface } = options
  const binding = getBodyColumnBinding(entry.metric.column.key)
  const rowIndex = pooled.displayIndex
  const rowEntry = pooled.entry
  const rowData = rowEntry?.row ?? null
  const classList: Array<string | string[] | Record<string, boolean> | undefined> = [
    "ui-table__selection-cell ui-table__sticky-divider flex items-center justify-center w-full h-full",
    bodySelectionCellClass.value,
    {
      "ui-table__selection-cell--row-selected": isRowFullySelected(rowIndex),
      "ui-table__selection-cell--active": isCellSelected(rowIndex, binding.columnIndex),
      "ui-table__selection-cell--range": isCellInSelectionRange(rowIndex, binding.columnIndex),
    },
  ]

  const style: CSSProperties = {
    position: "absolute",
    top: "0",
    bottom: "0",
    left: `${surface.left}px`,
    width: `${surface.width}px`,
    height: "100%",
  }

  return {
    key: `selection-${pooled.poolIndex}`,
    kind: "selection",
    classList,
    style,
    columnKey: entry.metric.column.key,
    binding,
    selectionState: {
      rowData,
      checked: rowData ? isCheckboxRowSelected(rowData) : false,
      columnIndex: binding.columnIndex,
    },
  }
}

const createIndexCellDescriptor = (
  options: {
    pooled: RowPoolItem
    entry: HeaderRenderableEntry
    surface: ColumnSurface
  },
): UiTableRowCellDescriptor => {
  const { pooled, entry, surface } = options
  const binding = getBodyColumnBinding(entry.metric.column.key)
  const rowIndex = pooled.displayIndex
  const classList: Array<string | string[] | Record<string, boolean> | undefined> = [
    "ui-table__row-index ui-table__sticky-divider",
    bodyIndexCellClass.value,
    rowHeaderClass(rowIndex),
  ]

  const style: CSSProperties = {
    position: "absolute",
    top: "0",
    bottom: "0",
    left: `${surface.left}px`,
    width: `${surface.width}px`,
    height: "100%",
  }

  return {
    key: `index-${pooled.poolIndex}`,
    kind: "index",
    classList,
    style,
    columnKey: entry.metric.column.key,
    binding,
    indexDisplay: rowIndex + 1,
  }
}

const createDataCellDescriptor = (
  options: {
    pooled: RowPoolItem
    entry: HeaderRenderableEntry
    surface: ColumnSurface
  },
): UiTableRowCellDescriptor => {
  const { pooled, entry, surface } = options
  const columnKey = entry.metric.column.key
  const binding = getBodyColumnBinding(columnKey)
  const rowIndex = pooled.displayIndex
  const classList: Array<string | string[] | Record<string, boolean> | undefined> = [
    binding.cellClass,
    {
      "ui-table-cell--cursor": isSelectionCursorCell(rowIndex, binding.columnIndex),
      "ui-table-cell--range": isCellInSelectionRange(rowIndex, binding.columnIndex),
      "ui-table-cell--column-selected": isColumnFullySelected(binding.columnIndex),
    },
  ]

  const style: CSSProperties = {
    position: "absolute",
    top: "0",
    bottom: "0",
    left: `${surface.left}px`,
    width: `${surface.width}px`,
    height: "100%",
  }

  return {
    key: `cell-${pooled.poolIndex}-${columnKey}`,
    kind: "data",
    classList,
    style,
    columnKey,
    binding,
    cellProps: buildDataCellProps(pooled, binding, columnKey),
    hasCustomRenderer: hasCustomRenderer(columnKey),
  }
}

const buildRowViewModel = ({
  pooled,
  entries,
  region,
}: BuildRowViewOptions): UiTableRowViewModel => {
  const rowEntry = pooled.entry
  if (!rowEntry) {
    return { kind: "empty" }
  }

  if (isGroupRowEntry(rowEntry)) {
    const group = rowEntry.row
    const expanded = isGroupExpanded(group.key)
    return {
      kind: "group",
      classList: ["ui-table__group-row", groupRowClass.value],
      cellClassList: ["ui-table__group-cell", groupCellClass.value],
      caretClassList: ["ui-table__group-caret", groupCaretClass.value, { expanded }],
      ariaColCount: ariaColCount.value,
      cellStyle: groupCellStyle(group.level),
      rowKey: group.key,
      label: group.value,
      size: group.size,
      level: group.level,
      expanded,
    }
  }

  const rowIndex = pooled.displayIndex
  const baseStyle: CSSProperties = {
    position: "relative",
    width: "100%",
    height: "100%",
  }

  const classList: Array<string | string[] | Record<string, boolean> | undefined> = [
    bodyRowClass.value,
    rowGridClass(rowEntry.row ?? null),
  ]

  if (isRowFullySelected(rowIndex)) {
    classList.push("ui-table__row--selected")
  }

  const cells: UiTableRowCellDescriptor[] = []

  for (const entry of entries) {
    const columnKey = entry.metric.column.key
    const surface = resolveColumnSurface(region, columnKey)
    if (!surface) {
      continue
    }

    if (entry.metric.column.isSystem) {
      if (columnKey === SELECTION_COLUMN_KEY) {
        cells.push(createSelectionCellDescriptor({ pooled, entry, surface }))
      } else {
        cells.push(createIndexCellDescriptor({ pooled, entry, surface }))
      }
      continue
    }

    cells.push(createDataCellDescriptor({ pooled, entry, surface }))
  }

  return {
    kind: "data",
    classList,
    style: baseStyle,
    ariaRowIndex: getAriaRowIndex(rowIndex),
    displayIndex: rowIndex,
    cells,
  }
}

const headerBindings: UiTableHeaderBindings = {
  headerRef,
  headerRowStickyStyle,
  hasColumnGroups,
  headerGroupRows,
  headerPinnedLeftGroupRows,
  headerMainGroupRows,
  headerPinnedRightGroupRows,
  headerRowClass,
  headerRenderableEntries,
  headerPinnedLeftEntries,
  headerMainEntries,
  headerPinnedRightEntries,
  systemColumnStyle,
  headerSelectionCellClass,
  setHeaderSelectionCheckboxRef,
  rowSelection,
  selectableRowCount,
  headerSelectionName,
  handleHeaderSelectionChange,
  headerCellClass,
  isColumnFullySelected,
  getColumnIndex,
  isColumnHeaderHighlighted,
  getSortDirectionForColumn,
  getSortPriorityForColumn,
  isFilterActiveForColumn,
  filterMenuState,
  activeMenuOptions,
  effectiveMenuSelectedKeys,
  zoom,
  columnWidthMap,
  columnWidthDomMap,
  getHeaderTabIndex,
  getAriaColIndex,
  containerRef,
  splitPinnedLayoutEnabled,
  isColumnSticky: headerIsColumnSticky,
  getStickySide: headerGetStickySide,
  getStickyLeftOffset: headerGetStickyLeftOffset,
  getStickyRightOffset: headerGetStickyRightOffset,
  groupedColumnSet,
  groupOrderMap,
  handleHeaderReorder,
  isSelectAllChecked,
  isSelectAllIndeterminate,
  setFilterMenuSearchRef,
  getAdvancedFilter,
  resolveColumnPinState,
  loadFilterOptions,
  filterSearchDebounce,
  groupedColumns,
  toggleFilterOption,
  toggleSelectAll,
  onApplyFilter,
  onCancelFilter,
  onSortColumn,
  onResetFilter,
  onGroupColumn,
  handleColumnPin,
  openAdvancedFilterModal,
  onColumnResize,
  autoResizeColumn,
  handleColumnHeaderClick,
  onColumnMenuOpen,
  onColumnMenuClose,
  hideColumn,
  SELECTION_COLUMN_KEY,
  resolveColumnSurface,
}

provide(UiTableHeaderContextKey, headerBindings)

handleAutoScrollFrame = () => internalHandleAutoScrollFrame()
handleViewportAfterScroll = () => scheduleOverlayUpdate()
const { flash } = useCellFlash()
historyAppliedCallback = (direction, events) => {
  dispatchEvents(events)
  scheduleOverlayUpdate()
  if (direction === "undo") {
    clearCutPreview()
  }
  if (!events.length) return
  const historyDirection = direction
  nextTick(() => {
    const container = containerRef.value
    if (!container) return
    const changedCells: HTMLElement[] = []
    for (const event of events) {
      const displayRowIndex = event.displayRowIndex ?? event.rowIndex
      if (displayRowIndex == null || displayRowIndex < 0) continue
      const columnKey = String(event.key)
      const cell = getCellElement(container, displayRowIndex, columnKey)
      if (cell) {
        changedCells.push(cell)
      }
    }
    if (changedCells.length) {
      const flashType = historyDirection === "undo" ? "undo" : "redo"
      flash(changedCells, flashType)
    }
  })
}

watch(
  lastCommittedFillArea,
  area => {
    if (!area) return
    const cells = getCellsForArea(area)
    if (cells.length) {
      flash(cells, 'fill')
    }
    lastCommittedFillArea.value = null
  },
  { flush: 'post' }
)

const {
  copySelectionToClipboard,
  cutSelectionToClipboard,
  pasteClipboardData,
  exportCSV,
  importCSV,
  copySelectionToClipboardWithFlash,
  cutSelectionToClipboardWithFlash,
  pasteClipboardDataWithFlash,
  getCellsForArea,
  cancelCutPreview,
} = useTableClipboardBridge({
  containerRef,
  cellContainers: bodyContainerSurfaces,
  visibleColumns,
  anchorCell,
  selectedCell,
  fullRowSelection,
  getSelectionSnapshot,
  getActiveRange,
  buildSelectionMatrix,
  applyMatrixToSelection,
  beginCutPreview,
  clearCutPreview,
  commitPendingCut,
  flash,
})

const selectedCells = computed(() => getSelectedCells())
const selectedCellCount = computed(() => selectedCells.value.length)
const formattedSelectedCellCount = computed(() => rowCountFormatter.format(selectedCellCount.value))

const selectionMetrics = computed<UiTableSelectionMetricResult[]>(() => {
  const cells = selectedCells.value

  return computeSelectionMetrics({
    config: selectionMetricsConfig.value,
    cells,
    labelMap: BUILTIN_SELECTION_METRIC_LABELS,
    getNumberFormatter: resolveSelectionMetricFormatter,
  })
})

const { handleKeydown: handleTableKeydown, focusNextCell } = useTableEvents({
  isEditingCell,
  selection: {
    moveSelection,
    moveByTab,
    moveByPage,
    triggerEditForSelection,
    clearSelectionValues,
    selectCell,
    scheduleOverlayUpdate,
    goToRowEdge,
    goToColumnEdge,
    goToGridEdge,
  },
  clipboard: {
    copySelectionToClipboard,
    cutSelectionToClipboard,
    pasteClipboardData,
    copySelectionToClipboardWithFlash,
    cutSelectionToClipboardWithFlash,
    pasteClipboardDataWithFlash,
    cancelCutPreview,
  },

  history: {
    undo,
    redo,
  },

  requestEdit,
})

const handleViewportScrollEvent = (event: Event) => {
  const target = event.target as HTMLElement | null
  if (!target) {
    return
  }

  handleViewportScroll(event)

  overlayScrollState.emit({
    scrollLeft: target.scrollLeft,
    scrollTop: target.scrollTop,
    pinnedOffsetLeft: pinnedLeftOffset.value,
    pinnedOffsetRight: pinnedRightOffset.value,
  })
}

function handleKeydown(event: KeyboardEvent) {
  handleTableKeydown(event)
}

const hasSummaryRow = computed(() => Boolean(normalizedProps.value.summaryRow) || Boolean(tableSlots.summary))

const summaryRowData = computed(() => normalizedProps.value.summaryRow)

const showVirtualEmptyState = computed(() => !totalRowCountDisplay.value && !loadingState.value)

const ariaRowCount = computed(() => {
  const total = totalRowCountDisplay.value ?? resolvedRows.value.length
  const summaryOffset = hasSummaryRow.value ? 1 : 0
  return Math.max(1, (total ?? 0) + summaryOffset)
})

const summaryRowAriaIndex = computed(() => {
  const total = totalRowCountDisplay.value ?? resolvedRows.value.length
  return total + 1
})

// Determine if a scoped slot overrides the default cell renderer
function hasCustomRenderer(columnKey: string) {
  return Boolean(tableSlots[`cell-${columnKey}`])
}

const bodyBindings: UiTableBodyBindings = {
  virtualContainerStyle,
  pooledRows,
  rowLayerStyle,
  isHoverableTable,
  isGroupRowEntry,
  groupRowClass,
  groupCellClass,
  groupCaretClass,
  groupCellStyle,
  toggleGroupRow,
  isGroupExpanded,
  ariaColCount,
  rowGridClass,
  splitPinnedLayoutEnabled,
  headerPinnedLeftEntries,
  headerMainEntries,
  headerPinnedRightEntries,
  headerRenderableEntries,
  columnBindings: bodyColumnBindings,
  getColumnBinding: getBodyColumnBinding,
  bodySelectionCellClass,
  bodyRowClass,
  bodyCellClass,
  bodyIndexCellClass,
  rowHeightDom,
  rowSelectionName,
  isCheckboxRowSelected,
  handleRowCheckboxToggle,
  groupSelectionVisible,
  getGroupSelectionState,
  toggleGroupSelection,
  isRowFullySelected,
  isColumnFullySelected,
  isCellSelected,
  isSelectionCursorCell,
  isSelectionAnchorCell,
  isCellInSelectionRange,
  isCellInFillPreview,
  isCellInCutPreview,
  getFillPreviewEdges,
  getCutPreviewEdges,
  buildSelectionSnapshot,
  getSelectionEdges,
  getColumnIndex,
  getAriaColIndex,
  getAriaRowIndex,
  getHeaderTabIndex,
  getCellTabIndex,
  getCellDomId,
  columnWidthDomMap,
  supportsCssZoom,
  zoom,
  editCommand,
  isColumnEditable,
  getValidationError,
  onCellEdit,
  onCellSelect,
  focusNextCell,
  handleCellEditingChange,
  onCellDragStart,
  onCellDragEnter,
  onCellComponentFocus,
  startFillDrag,
  autoFillDown,
  isFillDragging: () => isFillDragging.value,
  hasCustomRenderer,
  tableSlots,
  getStickyTopOffset,
  isSearchMatchCell,
  isActiveSearchMatchCell,
  SELECTION_COLUMN_KEY,
  totalRowCountDisplay,
  loadingState,
  rowHeaderClass,
  onRowIndexClick,
  hasCutPreview: () => hasCutPreview(),
  getActiveSelectionRange: () => {
    const area = getActiveSelectionArea()
    return area
      ? {
          startRow: area.startRow,
          endRow: area.endRow,
          startCol: area.startCol,
          endCol: area.endCol,
        }
      : null
  },
  canMoveActiveSelection: () => canMoveActiveSelection(),
  moveActiveSelectionTo: (rowStart: number, colStart: number) => moveActiveSelectionTo(rowStart, colStart),
  hoverOverlayVisible,
  hoverOverlayStyle,
  pinnedLeftEntries: computed(() => pinnedLeftEntries.value),
  visibleScrollableEntries: computed(() => visibleScrollableEntries.value),
  pinnedRightEntries: computed(() => pinnedRightEntries.value),
  leftPaddingDom,
  rightPaddingDom,
  viewportWidthDom,
  buildRowViewModel: options => buildRowViewModel(options),
  renderCellSlot: (slotName, slotProps) => renderTableSlotNodes(slotName, slotProps),
  resolveColumnSurface: (region, columnKey) => resolveColumnSurface(region, columnKey),
  getRegionSurfaceWidth: region => columnSurfaces.value.byRegion[region]?.totalWidth ?? 0,
}

provide(UiTableBodyContextKey, bodyBindings)

const summaryBindings: UiTableSummaryBindings = {
  headerRenderableEntries,
  summaryRowClass,
  summaryCellClass,
  summaryLabelCellClass,
  summaryCellStyle,
  summaryRowAriaIndex,
  summaryRowData,
  tableSlots,
  columnBindings: bodyColumnBindings,
  getColumnBinding: getBodyColumnBinding,
  selectionColumnKey: SELECTION_COLUMN_KEY,
  resolveColumnSurface: (region, columnKey) => resolveColumnSurface(region, columnKey),
}

provide(UiTableSummaryContextKey, summaryBindings)

const tableApi = useUiTableApi({
  navigation: {
    scrollToRow,
    scrollToColumn,
    isRowVisible,
    focusCell,
  },
  selection: {
    setSelection,
    clearSelection,
    getSelectionSnapshot,
    getSelectedCells,
    selectionMetrics,
    selectAllRows,
    clearRowSelection: rowSelection.clearSelection,
    toggleRowSelection: rowSelection.toggleRow,
    selectedRows: rowSelection.selectedRows,
  },
  clipboard: {
    copySelectionToClipboardWithFlash,
    copySelectionToClipboard,
    pasteClipboardDataWithFlash,
    pasteClipboardData,
  },
  data: {
    getCellValue,
    setCellValue,
    exportCSV,
    importCSV,
  },
  paging: {
    requestPage,
    requestNextPage,
    resetLazyPaging,
  },
  history: {
    undo,
    redo,
  },
  filter: {
    onApplyFilter,
    onCancelFilter,
    onSortColumn,
    onResetFilter,
    resetAllFilters: handleResetAllFilters,
    openAdvancedFilterModal,
    handleAdvancedModalApply,
    handleAdvancedModalClear,
    handleAdvancedModalCancel,
    handleAdvancedFilterApply,
    handleAdvancedFilterClear,
    setMultiSortState,
    getFilterStateSnapshot,
    setFilterStateSnapshot,
  },
  columns: {
    handleColumnHeaderClick,
    resetColumnVisibility,
    openVisibilityPanel,
    closeVisibilityPanel,
  },
  grouping: {
    onGroupColumn,
    toggleGroupRow,
    getFilteredRowEntries,
    getFilteredRows,
  },
})

const legacyExpose = createLegacyExpose(tableApi)

const tableExpose: UiTableExposeBindings = {
  ...legacyExpose,
  ...tableApi,
}

setTableExpose(tableExpose)

provide(UiTableExposeContextKey, tableExpose)

// Check if a column header should render highlighted state
function isColumnHeaderHighlighted(_colIndex: number) {
  return false
}

// Close column menu and restore focus to the grid
function onColumnMenuClose(columnKey: string) {
  internalOnColumnMenuClose(columnKey)
  focusContainer()
}

// Persist updated column width and notify subscribers
function onColumnResize(key: string, newWidth: number) {
  const index = localColumns.value.findIndex(column => column.key === key)
  if (index === -1) return
  const nextColumns = [...localColumns.value]
  const target = nextColumns[index]
  nextColumns[index] = { ...target, width: newWidth, userResized: true }
  localColumns.value = nextColumns
  autoColumnResize.markManualResize()
  autoColumnResize.syncManualState(nextColumns)
  settingsAdapter.value.setColumnWidth(tableId.value, key, newWidth)
  const payload = { key, width: newWidth }
  emit("column-resize", payload)
  fireEvent("columnResize", payload)
  nextTick(() => requestTableRecalc("column-resize", { refresh: false, viewport: true }))
}

useTableRecalcWatcher({
  resolvedRows,
  processedRows,
  columnFilters,
  filtersState,
  totalRowCount,
  visibleColumns,
  summaryRow: summaryRowData,
  validationErrors,
  ensureSortedOrder,
  measureRowHeight,
  requestTableRecalc,
})

onBeforeUnmount(() => {
  cancelScrollRaf()
  // closeActiveMenu() // Commented out to prevent resetting the filter manually
})

function handleResetAllFilters() {
  if (!hasActiveFiltersOrGroups.value) return
  clearSuspendedFilterRows()
  if (hasActiveFilters.value) {
    resetAllFilters()
  }
  handleAdvancedModalCancel()
  if (groupState.value.length) {
    groupState.value = []
  }
  if (Object.keys(groupExpansion.value).length) {
    groupExpansion.value = {}
  }
  emit("filters-reset")
  fireEvent("filtersReset")
  nextTick(() => {
    requestTableRecalc("filters-reset", { overlay: true })
    focusContainer()
  })
}

// Toggle sort or column selection depending on modifier keys
function handleColumnHeaderClick(column: UiTableColumn, colIndex: number, event: MouseEvent | KeyboardEvent) {
  const isSortable = !column.isSystem && column.sortable !== false
  const shouldSelectColumn = !isSortable || event.metaKey || event.ctrlKey
  if (shouldSelectColumn) {
    onColumnHeaderClick(colIndex)
    return
  }
  toggleColumnSort(column.key, event.shiftKey)
  focusContainer()
}

defineExpose(tableExpose)
export type {
  CellEditEvent,
  UiTableColumn,
  UiTableConfig,
  UiTableEventHandlers,
  UiTableStyleConfig,
  UiTableStyleSection,
  UiTableHeaderStyle,
  UiTableBodyStyle,
  UiTableGroupStyle,
  UiTableSummaryStyle,
  UiTableStateStyle,
  UiTableLazyLoadContext,
  UiTableLazyLoadReason,
  UiTableFilterSnapshot,
  UiTableSortState,
  UiTableFilterClause,
  UiTableAdvancedFilter,
  UiTableFilterOptionLoader,
  UiTableSelectionSnapshot,
  UiTableSelectionSnapshotRange,
  UiTableRowClickEvent,
} from "../../core/types"

</script>
