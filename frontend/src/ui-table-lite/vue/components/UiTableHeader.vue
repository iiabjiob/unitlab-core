<template>
  <div
    v-if="shouldRender"
    :ref="setHeaderRootRef"
    :class="rootClass"
    role="rowgroup"
    :style="rowGroupStyle"
  >
    <div class="ui-table__row-layer ui-table__row-layer--header">
      <div
        class="ui-table__row-surface ui-table__row-surface--header"
        :class="headerRowClass"
        :style="rowSurfaceStyle"
        role="row"
      >
        <template v-for="cell in systemCells" :key="cell.key">
          <div
            v-if="cell.type === 'selection'"
            :class="['ui-table__selection-header flex items-center justify-center', headerSelectionCellClass]"
            :style="cell.style"
            role="columnheader"
            aria-label="Select rows"
          >
            <label class="flex items-center justify-center p-0">
              <input
                :ref="setHeaderSelectionCheckboxRef"
                type="checkbox"
                class="h-4 w-4 cursor-pointer rounded border-neutral-300 text-blue-600 transition-colors focus:ring-2 focus:ring-blue-500 dark:border-neutral-600 dark:bg-neutral-800 dark:text-blue-400 dark:focus:ring-blue-400"
                :checked="rowSelection.allSelected.value"
                :disabled="!selectableRowCount"
                :name="headerSelectionName"
                @change="handleHeaderSelectionChange"
                @click.stop
              >
            </label>
          </div>
          <div v-else class="ui-table__header-cell-surface" :style="cell.style" role="columnheader">
            <UiTableColumnHeader
              :style="getColumnBinding(cell.columnKey).style"
              v-bind="getColumnBinding(cell.columnKey).props"
              data-testid="ui-table-row-index-header"
              @resize="onColumnResize"
              @dblclick="() => handleAutoResizeBinding(getColumnBinding(cell.columnKey))"
              @select="event => handleColumnHeaderSelect(getColumnBinding(cell.columnKey), event)"
              @hide="hideColumn"
            />
          </div>
        </template>

        <DraggableList
          class="ui-table__header-draggable"
          axis="horizontal"
          wrapper-tag="div"
          item-tag="div"
          :items="draggableEntries"
          :item-key="entry => entry.metric.column.key"
          :item-style="draggableItemStyle"
          :item-draggable="headerItemDraggable"
          @update:items="handleHeaderReorder"
        >
          <template #default="{ item: entry }">
            <div class="ui-table__header-cell-surface" role="columnheader">
              <UiTableColumnHeader
                :style="getColumnBinding(entry.metric.column.key).style"
                v-bind="getColumnBinding(entry.metric.column.key).props"
                @resize="onColumnResize"
                @select="event => handleColumnHeaderSelect(getColumnBinding(entry.metric.column.key), event)"
                @menu-open="closeFn => onColumnMenuOpen(entry.metric.column.key, closeFn)"
                @menu-close="() => onColumnMenuClose(entry.metric.column.key)"
                @dblclick="() => handleAutoResizeBinding(getColumnBinding(entry.metric.column.key))"
                @hide="hideColumn"
              >
                <template #menu="{ close }">
                  <FilterPopover
                    :col="getColumnBinding(entry.metric.column.key).column"
                    :close="close"
                    :options="activeMenuOptions"
                    :selected-keys="effectiveMenuSelectedKeys"
                    :search="filterMenuState.search"
                    :is-select-all-checked="isSelectAllChecked"
                    :is-select-all-indeterminate="isSelectAllIndeterminate"
                    :sort-direction="getColumnBinding(entry.metric.column.key).props.sortDirection"
                    :register-search-input="setFilterMenuSearchRef"
                    :filter-condition="getAdvancedFilter(entry.metric.column.key)"
                    :pin-state="resolveColumnPinState(getColumnBinding(entry.metric.column.key).column)"
                    :load-options="loadFilterOptions"
                    :search-debounce="filterSearchDebounce"
                    :group-active="groupedColumns.includes(entry.metric.column.key)"
                    @update:search="(value: string) => (filterMenuState.search = value)"
                    @toggle-option="toggleFilterOption"
                    @toggle-select-all="toggleSelectAll"
                    @apply="onApplyFilter"
                    @cancel="onCancelFilter"
                    @sort="onSortColumn"
                    @reset="onResetFilter"
                    @group="() => onGroupColumn(getColumnBinding(entry.metric.column.key).column)"
                    @pin="position => handleColumnPin(getColumnBinding(entry.metric.column.key).column, position)"
                    @open-advanced="() => openAdvancedFilterModal(entry.metric.column.key)"
                  />
                </template>
              </UiTableColumnHeader>
            </div>
          </template>
        </DraggableList>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, inject, ref, watchEffect, type ComponentPublicInstance, type CSSProperties } from "vue"
import DraggableList from "@/components/ui/DraggableList.vue"
import FilterPopover from "./FilterPopover.vue"
import UiTableColumnHeader from "./UiTableColumnHeader.vue"
import { UiTableHeaderContextKey, type UiTableRowRegion } from "../context"
import type { HeaderRenderableEntry } from "../../core/types/internal"
import type { UiTableColumn } from "../../core/types"

const props = defineProps<{
  section?: "pinned-left" | "main" | "pinned-right"
}>()

const header = inject(UiTableHeaderContextKey)
if (!header) {
  throw new Error("UiTableHeader requires UiTableHeaderContext")
}

const {
  headerRef,
  headerRowStickyStyle,
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
  getColumnIndex,
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
  isColumnSticky,
  getStickySide,
  getStickyLeftOffset,
  getStickyRightOffset,
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
} = header

const resolvedSection = computed(() => props.section ?? "main")
const isSplitLayout = computed(() => splitPinnedLayoutEnabled.value)
const rowRegion = computed<UiTableRowRegion>(() => {
  if (!isSplitLayout.value) {
    return "center"
  }
  if (resolvedSection.value === "pinned-left") {
    return "pinned-left"
  }
  if (resolvedSection.value === "pinned-right") {
    return "pinned-right"
  }
  return "center"
})

const activeEntries = computed(() => {
  if (!isSplitLayout.value) {
    return headerRenderableEntries.value
  }
  if (resolvedSection.value === "pinned-left") {
    return headerPinnedLeftEntries.value
  }
  if (resolvedSection.value === "pinned-right") {
    return headerPinnedRightEntries.value
  }
  return headerMainEntries.value
})

const shouldRender = computed(() => activeEntries.value.length > 0)
const rootClass = computed(() => ["ui-table__header-row", `ui-table__header-row--${resolvedSection.value}`])
const rowGroupStyle = computed(() => headerRowStickyStyle.value)
const rowSurfaceStyle = computed<CSSProperties>(() => ({
  position: "relative",
  width: "100%",
  height: "100%",
}))

const surfaceMap = computed(() => {
  const map = new Map<string, { left: number; width: number }>()
  const region = rowRegion.value
  for (const entry of activeEntries.value) {
    const surface = resolveColumnSurface(region, entry.metric.column.key)
    if (surface) {
      map.set(entry.metric.column.key, surface)
    }
  }
  return map
})

function entrySurfaceStyle(entry: HeaderRenderableEntry): CSSProperties {
  const surface = surfaceMap.value.get(entry.metric.column.key)
  if (!surface) {
    return { display: "none" }
  }
  return {
    position: "absolute",
    top: "0",
    bottom: "0",
    left: `${surface.left}px`,
    width: `${surface.width}px`,
    height: "100%",
  }
}

const systemEntries = computed(() => activeEntries.value.filter(entry => entry.metric.column.isSystem))
const draggableEntries = computed(() => activeEntries.value.filter(entry => !entry.metric.column.isSystem))

type SystemCellDescriptor =
  | {
      key: string
      type: "selection"
      style: Array<CSSProperties | undefined>
    }
  | {
      key: string
      type: "header"
      columnKey: string
      style: Array<CSSProperties | undefined>
    }

const systemCells = computed(() => {
  const region = rowRegion.value
  return systemEntries.value
    .map<SystemCellDescriptor | null>(entry => {
      const surface = resolveColumnSurface(region, entry.metric.column.key)
      if (!surface) {
        return null
      }
      const baseStyle: CSSProperties = {
        position: "absolute",
        top: "0",
        bottom: "0",
        left: `${surface.left}px`,
        width: `${surface.width}px`,
        display: "flex",
        alignItems: "stretch",
        height: "100%",
      }

      if (entry.metric.column.key === SELECTION_COLUMN_KEY) {
        return {
          key: `system-selection-${entry.metric.column.key}`,
          type: "selection" as const,
          style: [baseStyle, systemColumnStyle(entry.metric.column)],
        }
      }

      return {
        key: `system-${entry.metric.column.key}`,
        type: "header" as const,
        columnKey: entry.metric.column.key,
        style: [baseStyle, systemColumnStyle(entry.metric.column)],
      }
    })
    .filter((value): value is SystemCellDescriptor => value !== null)
})

const headerItemDraggable = (entry: HeaderRenderableEntry) => !entry.metric.column.isSystem

const draggableItemStyle = (entry: HeaderRenderableEntry): Record<string, string | number> => {
  const base = entrySurfaceStyle(entry)
  return { ...base, display: "block", height: "100%" } as Record<string, string | number>
}

const headerColumnWidthStyle = (entry: HeaderRenderableEntry): CSSProperties => {
  const column = entry.metric.column
  const measured =
    column.width ?? columnWidthMap.value.get(column.key) ?? entry.metric.width ?? column.minWidth
  const width = typeof measured === "number" && Number.isFinite(measured) ? measured : undefined
  if (typeof width === "number" && Number.isFinite(width) && width > 0) {
    return { width: `${Math.round(width)}px` }
  }
  return {} as CSSProperties
}

interface ColumnHeaderBinding {
  column: UiTableColumn
  columnIndex: number
  style: Array<CSSProperties | undefined>
  props: {
    col: UiTableColumn
    baseClass: string | undefined
    sortDirection: "asc" | "desc" | null
    sortPriority: number | null
    filterActive: boolean
    menuOpen: boolean
    zoomScale: number
    layoutWidth: number | null
    visualWidth: number
    tabIndex: number
    ariaColIndex: number
    tableContainer: HTMLDivElement | null
    sticky: boolean
    stickySide: "left" | "right" | null
    stickyLeftOffset?: number
    stickyRightOffset?: number
    disableMenu: boolean
    grouped: boolean
    groupOrder: number | null
  }
}

const columnHeaderBindings = computed(() => {
  const bindings = new Map<string, ColumnHeaderBinding>()
  for (const entry of activeEntries.value) {
    const column = entry.metric.column
    const columnIndex = getColumnIndex(column.key)
    const layoutWidth = columnWidthMap.value.get(column.key) ?? entry.metric.width ?? null
    const visualWidth = columnWidthDomMap.value.get(column.key) ?? 0
    bindings.set(column.key, {
      column,
      columnIndex,
      style: [headerColumnWidthStyle(entry)],
      props: {
        col: column,
        baseClass: headerCellClass(column),
        sortDirection: column.isSystem ? null : getSortDirectionForColumn(column.key),
        sortPriority: column.isSystem ? null : getSortPriorityForColumn(column.key),
        filterActive: column.isSystem ? false : isFilterActiveForColumn(column.key),
        menuOpen: !column.isSystem && filterMenuState.columnKey === column.key,
        zoomScale: zoom.value,
        layoutWidth,
        visualWidth,
        tabIndex: getHeaderTabIndex(columnIndex),
        ariaColIndex: getAriaColIndex(columnIndex),
        tableContainer: containerRef.value,
        sticky: isColumnSticky(column),
        stickySide: getStickySide(column),
        stickyLeftOffset: getStickyLeftOffset(column),
        stickyRightOffset: getStickyRightOffset(column),
        disableMenu: Boolean(column.isSystem),
        grouped: !column.isSystem && groupedColumnSet.value.has(column.key),
        groupOrder: !column.isSystem ? groupOrderMap.value.get(column.key) ?? null : null,
      },
    })
  }
  return bindings
})

function getColumnBinding(columnKey: string): ColumnHeaderBinding {
  const binding = columnHeaderBindings.value.get(columnKey)
  if (!binding) {
    throw new Error(`Missing column header binding for key: ${columnKey}`)
  }
  return binding
}

function handleColumnHeaderSelect(binding: ColumnHeaderBinding, event: MouseEvent | KeyboardEvent) {
  handleColumnHeaderClick(binding.column, binding.columnIndex, event)
}

function handleAutoResizeBinding(binding: ColumnHeaderBinding) {
  autoResizeColumn(binding.column)
}

const localRootRef = ref<HTMLElement | null>(null)

watchEffect(() => {
  if (!isSplitLayout.value || resolvedSection.value === "main") {
    headerRef.value = localRootRef.value
  } else if (headerRef.value === localRootRef.value) {
    headerRef.value = null
  }
})

function setHeaderRootRef(element: Element | ComponentPublicInstance | null) {
  localRootRef.value = element instanceof HTMLElement ? element : null
}
</script>

<style scoped>
.ui-table__header-cell-surface {
  display: flex;
  align-items: stretch;
  height: 100%;
  width: 100%;
}

.ui-table__header-draggable :deep(.draggable-list) {
  display: contents;
}

.ui-table__header-draggable :deep(.draggable-list__item) {
  height: 100%;
}
</style>
