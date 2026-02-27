<template>
  <div class="ui-table-light" :class="[tableThemeClass, { 'ui-table-light--resizing': isResizing }]">
    <div class="ui-table-light__container" :style="containerStyle">
      <table class="ui-table-light__table">
        <thead>
          <tr>
            <th
              v-for="column in columns"
              :key="column.key"
              class="ui-table-light__cell ui-table-light__cell--header"
              :class="headerClasses(column)"
              :style="columnStyle(column)"
              scope="col"
              @click="handleSort(column)"
            >
              <div class="ui-table-light__header-content">
                <span>{{ column.label }}</span>
                <span v-if="isColumnSorted(column)" class="ui-table-light__sort-indicator" :class="`is-${sortState?.direction}`">
                  <svg viewBox="0 0 12 12" aria-hidden="true">
                    <path d="M6 2l3 4H3z" />
                  </svg>
                </span>
              </div>
              <span
                v-if="canResize(column)"
                class="ui-table-light__resize-handle"
                role="separator"
                aria-orientation="horizontal"
                @mousedown.stop.prevent="startResize($event, column)"
              ></span>
            </th>
          </tr>
          <tr v-if="showFilterRow" class="ui-table-light__filter-row">
            <th
              v-for="column in columns"
              :key="`${column.key}-filter`"
              class="ui-table-light__cell ui-table-light__cell--filter"
              :style="columnStyle(column)"
            >
              <input
                v-if="canFilter(column)"
                v-model="filters[column.key]"
                type="text"
                autocomplete="off"
                :name="`ui-table-light-filter-${column.key}`"
                class="ui-table-light__filter-input"
                placeholder="Filter"
              />
            </th>
          </tr>
        </thead>
        <tbody v-if="processedRows.length">
          <tr
            v-for="(row, rowIndex) in processedRows"
            :key="resolveRowKey(row, rowIndex)"
            class="ui-table-light__row"
            @click="handleRowClick(row, rowIndex)"
          >
            <td
              v-for="column in columns"
              :key="column.key"
              class="ui-table-light__cell"
              :class="alignClass(column.align)"
            >
              <slot name="cell" :column="column" :row="row" :rowIndex="rowIndex" :value="resolveValue(row, column.key)">
                {{ formatValue(resolveValue(row, column.key)) }}
              </slot>
            </td>
          </tr>
        </tbody>
        <tbody v-else>
          <tr>
            <td class="ui-table-light__empty" :colspan="columns.length || 1">
              <slot name="empty">No data available</slot>
            </td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from "vue"
import { storeToRefs } from "pinia"

import { useThemeStore } from "@/stores/themeStore"

type ColumnAlign = "left" | "center" | "right"
type SortDirection = "asc" | "desc"

type TableColumn = {
  key: string
  label: string
  width?: string | number
  align?: ColumnAlign
  sortable?: boolean
  filterable?: boolean
  resizable?: boolean
  minWidth?: number
  maxWidth?: number
}

type TableRow = Record<string, unknown>

const props = withDefaults(
  defineProps<{
    columns: TableColumn[]
    rows: TableRow[]
    maxHeight?: string | number
    rowKey?: (row: TableRow, rowIndex: number) => string | number
    enableColumnResize?: boolean
    enableSorting?: boolean
    enableFiltering?: boolean
    defaultSortKey?: string | null
    defaultSortDirection?: SortDirection
  }>(),
  {
    columns: () => [],
    rows: () => [],
    maxHeight: undefined,
    rowKey: undefined,
    enableColumnResize: false,
    enableSorting: false,
    enableFiltering: false,
    defaultSortKey: null,
    defaultSortDirection: "asc",
  },
)

const emit = defineEmits<{ (e: "row-click", payload: { row: TableRow; rowIndex: number }): void }>()

const themeStore = useThemeStore()
const { currentTheme } = storeToRefs(themeStore)

const tableThemeClass = computed(() => (currentTheme.value === "dark" ? "ui-table-light--dark" : null))

const containerStyle = computed(() => {
  if (!props.maxHeight) return undefined
  const value = typeof props.maxHeight === "number" ? `${props.maxHeight}px` : props.maxHeight
  return { maxHeight: value }
})

const filters = ref<Record<string, string>>({})
const columnWidths = ref<Record<string, number>>({})
const resizeState = ref<{ columnKey: string; startX: number; startWidth: number; min: number; max?: number } | null>(null)

const initialSortState = computed(() => {
  if (props.defaultSortKey) {
    return { key: props.defaultSortKey, direction: props.defaultSortDirection as SortDirection }
  }
  return null
})

const sortState = ref<{ key: string; direction: SortDirection } | null>(initialSortState.value)

const processedRows = computed(() => {
  const rowsCopy = props.rows.slice()
  const filtered = applyFilters(rowsCopy)
  return applySorting(filtered)
})

const showFilterRow = computed(() => props.enableFiltering && props.columns.some(column => canFilter(column)))
const isResizing = computed(() => resizeState.value !== null)

watch(
  () => props.columns,
  (columns) => {
    syncFilterKeys(columns)
    syncColumnWidths(columns)
  },
  { immediate: true, deep: true },
)

watch(
  () => props.defaultSortKey,
  () => {
    sortState.value = initialSortState.value
  },
)

function resolveRowKey(row: TableRow, rowIndex: number) {
  if (props.rowKey) {
    return props.rowKey(row, rowIndex)
  }
  const candidate = (row as Record<string, unknown>).id ?? (row as Record<string, unknown>).key
  if (typeof candidate === "string" || typeof candidate === "number") {
    return candidate
  }
  return `row-${rowIndex}`
}

function resolveValue(row: TableRow, key: string) {
  return (row as Record<string, unknown>)[key]
}

function handleRowClick(row: TableRow, rowIndex: number) {
  emit("row-click", { row, rowIndex })
}

function formatValue(value: unknown): string {
  if (value === null || value === undefined) return ""
  if (typeof value === "string") return value
  if (typeof value === "number" || typeof value === "boolean") return String(value)
  if (value instanceof Date) return value.toISOString()
  if (typeof value === "object") {
    try {
      return JSON.stringify(value)
    } catch (err) {
      return "[object]"
    }
  }
  return String(value)
}

function columnStyle(column: TableColumn) {
  const width = columnWidths.value[column.key]
  if (width) {
    return { width: `${width}px`, minWidth: `${width}px` }
  }
  if (!column.width) return undefined
  const normalized = typeof column.width === "number" ? `${column.width}px` : column.width
  return { width: normalized }
}

function alignClass(align: ColumnAlign | undefined) {
  if (align === "center") return "ui-table-light__cell--center"
  if (align === "right") return "ui-table-light__cell--right"
  return "ui-table-light__cell--left"
}

function headerClasses(column: TableColumn) {
  return [
    alignClass(column.align),
    {
      "ui-table-light__cell--sortable": canSort(column),
      "ui-table-light__cell--sorted": isColumnSorted(column),
    },
  ]
}

function canResize(column: TableColumn) {
  return props.enableColumnResize && (column.resizable ?? true)
}

function canSort(column: TableColumn) {
  return props.enableSorting && (column.sortable ?? true)
}

function canFilter(column: TableColumn) {
  return props.enableFiltering && (column.filterable ?? true)
}

function handleSort(column: TableColumn) {
  if (!canSort(column)) return
  if (!sortState.value || sortState.value.key !== column.key) {
    sortState.value = { key: column.key, direction: "asc" }
    return
  }
  if (sortState.value.direction === "asc") {
    sortState.value = { key: column.key, direction: "desc" }
    return
  }
  sortState.value = null
}

function isColumnSorted(column: TableColumn) {
  return !!sortState.value && sortState.value.key === column.key
}

function applyFilters(rows: TableRow[]) {
  if (!props.enableFiltering) return rows
  const activeFilters = Object.entries(filters.value).filter(([, value]) => value?.trim())
  if (!activeFilters.length) return rows
  const lowerFilters = activeFilters.map(([key, value]) => [key, value.toLowerCase()] as const)
  return rows.filter(row =>
    lowerFilters.every(([key, needle]) => {
      const haystack = formatValue(resolveValue(row, key)).toLowerCase()
      return haystack.includes(needle)
    }),
  )
}

function applySorting(rows: TableRow[]) {
  if (!sortState.value || !props.enableSorting) return rows
  const { key, direction } = sortState.value
  return rows.slice().sort((a, b) => {
    const first = resolveValue(a, key)
    const second = resolveValue(b, key)
    const comparison = compareValues(first, second)
    return direction === "asc" ? comparison : -comparison
  })
}

function compareValues(a: unknown, b: unknown) {
  if (a === b) return 0
  if (a === null || a === undefined) return -1
  if (b === null || b === undefined) return 1

  const aNumber = typeof a === "number" ? a : Number(a)
  const bNumber = typeof b === "number" ? b : Number(b)
  if (!Number.isNaN(aNumber) && !Number.isNaN(bNumber)) {
    return aNumber < bNumber ? -1 : 1
  }

  const aDate = a instanceof Date ? a.getTime() : Date.parse(String(a))
  const bDate = b instanceof Date ? b.getTime() : Date.parse(String(b))
  if (!Number.isNaN(aDate) && !Number.isNaN(bDate)) {
    return aDate < bDate ? -1 : 1
  }

  const aString = formatValue(a).toLowerCase()
  const bString = formatValue(b).toLowerCase()
  if (aString === bString) return 0
  return aString < bString ? -1 : 1
}

function syncFilterKeys(columns: TableColumn[]) {
  const next: Record<string, string> = {}
  columns.forEach(column => {
    next[column.key] = filters.value[column.key] ?? ""
  })
  filters.value = next
}

function syncColumnWidths(columns: TableColumn[]) {
  const next: Record<string, number> = {}
  columns.forEach(column => {
    const parsed = parseWidth(column.width)
    if (parsed) {
      next[column.key] = parsed
    } else if (columnWidths.value[column.key]) {
      next[column.key] = columnWidths.value[column.key]
    }
  })
  columnWidths.value = next
}

function parseWidth(width: string | number | undefined) {
  if (typeof width === "number") return width
  if (typeof width === "string" && width.endsWith("px")) {
    const value = Number.parseFloat(width)
    return Number.isFinite(value) ? value : undefined
  }
  return undefined
}

function startResize(event: MouseEvent, column: TableColumn) {
  if (!canResize(column)) return
  const initialWidth = columnWidths.value[column.key] ?? parseWidth(column.width) ?? column.minWidth ?? 120
  const minWidth = column.minWidth ?? 10
  const maxWidth = column.maxWidth
  resizeState.value = {
    columnKey: column.key,
    startX: event.clientX,
    startWidth: initialWidth,
    min: minWidth,
    max: maxWidth,
  }
  document.addEventListener("mousemove", handleResizeMouseMove)
  document.addEventListener("mouseup", handleResizeMouseUp)
}

function handleResizeMouseMove(event: MouseEvent) {
  if (!resizeState.value) return
  const delta = event.clientX - resizeState.value.startX
  let nextWidth = resizeState.value.startWidth + delta
  nextWidth = Math.max(resizeState.value.min, nextWidth)
  if (resizeState.value.max) {
    nextWidth = Math.min(resizeState.value.max, nextWidth)
  }
  columnWidths.value = {
    ...columnWidths.value,
    [resizeState.value.columnKey]: Math.round(nextWidth),
  }
}

function handleResizeMouseUp() {
  cleanupResizeListeners()
}

function cleanupResizeListeners() {
  resizeState.value = null
  document.removeEventListener("mousemove", handleResizeMouseMove)
  document.removeEventListener("mouseup", handleResizeMouseUp)
}

onBeforeUnmount(() => {
  cleanupResizeListeners()
})
</script>

<style scoped>
.ui-table-light {
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  color-scheme: light;
  --ui-table-surface: #ffffff;
  --ui-table-text: #0f172a;
  --ui-table-border: rgba(226, 232, 240, 0.9);
  --ui-table-header-bg: #f8fafc;
  --ui-table-header-text: #475569;
  --ui-table-accent: #0f172a;
  --ui-table-row-alt: rgba(248, 250, 252, 0.7);
  --ui-table-row-hover: rgba(226, 232, 240, 0.5);
  --ui-table-filter-bg: #ffffff;
  --ui-table-filter-border: rgba(148, 163, 184, 0.6);
  --ui-table-filter-text: #0f172a;
  --ui-table-empty: #94a3b8;
  --ui-table-shadow: 0 12px 30px rgba(15, 23, 42, 0.08);
}

.ui-table-light--dark {
  color-scheme: dark;
  background-color: transparent;
  --ui-table-surface: #06080f;
  --ui-table-text: #f2f4f7;
  --ui-table-border: rgba(148, 163, 184, 0.4);
  --ui-table-header-bg: rgba(10, 12, 18, 0.95);
  --ui-table-header-text: #e2e8f0;
  --ui-table-accent: #f4f4f5;
  --ui-table-row-alt: rgba(17, 20, 30, 0.65);
  --ui-table-row-hover: rgba(148, 163, 184, 0.18);
  --ui-table-filter-bg: rgba(5, 7, 12, 0.9);
  --ui-table-filter-border: rgba(148, 163, 184, 0.4);
  --ui-table-filter-text: #f8fafc;
  --ui-table-empty: #a0aec0;
  --ui-table-shadow: 0 18px 45px rgba(2, 6, 12, 0.45);
}

.ui-table-light__container {
  flex: 1;
  min-height: 0;
  overflow: auto;
  background-color: var(--ui-table-surface);
  border-radius: 1rem;
  border: 1px solid var(--ui-table-border);
  box-shadow: var(--ui-table-shadow);
}

.ui-table-light__table {
  width: max-content;
  min-width: 100%;
  border-collapse: separate;
  border-spacing: 0;
  font-size: 0.875rem;
}

.ui-table-light__cell {
  padding: 0.5rem 0.75rem;
  border-bottom: 1px solid var(--ui-table-border);
  color: var(--ui-table-text);
  vertical-align: top;
}

.ui-table-light__cell--header {
  position: sticky;
  top: 0;
  z-index: 2;
  background-color: var(--ui-table-header-bg);
  text-align: left;
  font-size: 0.75rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  color: var(--ui-table-header-text);
  user-select: none;
  cursor: default;
  position: sticky;
}

.ui-table-light__cell--sortable {
  cursor: default;
}

.ui-table-light__cell--sorted {
  color: var(--ui-table-accent);
}

.ui-table-light__header-content {
  display: flex;
  align-items: center;
  gap: 0.25rem;
}

.ui-table-light__sort-indicator {
  display: inline-flex;
  width: 0.75rem;
  height: 0.75rem;
}

.ui-table-light__sort-indicator svg {
  fill: currentColor;
  width: 100%;
  height: 100%;
  transform-origin: center;
}

.ui-table-light__sort-indicator.is-desc svg {
  transform: rotate(180deg);
}

.ui-table-light__resize-handle {
  position: absolute;
  top: 0;
  right: 0;
  width: 6px;
  height: 100%;
  cursor: col-resize;
  background-color: transparent;
}

.ui-table-light--resizing .ui-table-light__resize-handle {
  background-color: rgba(148, 163, 184, 0.3);
}

.ui-table-light__filter-row {
  position: sticky;
  top: 2.625rem;
  z-index: 1;
  background-color: var(--ui-table-header-bg);
}

.ui-table-light__cell--filter {
  padding: 0.25rem 0.5rem 0.5rem;
}

.ui-table-light__filter-input {
  width: 100%;
  border: 1px solid var(--ui-table-filter-border);
  border-radius: 0.5rem;
  padding: 0.2rem 0.4rem;
  font-size: 0.75rem;
  background-color: var(--ui-table-filter-bg);
  color: var(--ui-table-filter-text);
}

.ui-table-light__row:nth-child(even) .ui-table-light__cell {
  background-color: var(--ui-table-row-alt);
}

.ui-table-light__row:hover .ui-table-light__cell {
  background-color: var(--ui-table-row-hover);
}

.ui-table-light__cell--left {
  text-align: left;
}

.ui-table-light__cell--center {
  text-align: center;
}

.ui-table-light__cell--right {
  text-align: right;
}

.ui-table-light__empty {
  padding: 1.5rem;
  text-align: center;
  font-size: 0.875rem;
  color: var(--ui-table-empty);
}
</style>
