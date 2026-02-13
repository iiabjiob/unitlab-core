<template>
  <div
    class="ui-affino-grid"
    :class="{ 'is-row-fixed': isRowHeightFixed }"
    :style="gridStyle"
  >
    <div class="ui-affino-grid__layout">
      <div class="ui-affino-grid__content-shell">
        <div class="ui-affino-grid__index-column">
          <div class="ui-affino-grid__index-header" :style="indexHeaderStyle">#</div>
          <div v-if="showFilterRow" class="ui-affino-grid__index-filter" :style="indexFilterStyle"></div>

          <div ref="indexViewportRef" class="ui-affino-grid__index-viewport">
            <div class="ui-affino-grid__index-canvas">
              <template v-if="hasRenderableData">
                <div
                  v-if="topSpacerPx > 0"
                  class="ui-affino-grid__spacer-row ui-affino-grid__spacer-row--index"
                  :style="{ height: `${topSpacerPx}px` }"
                ></div>

                <div
                  v-for="(rowNode, localIndex) in visibleRowNodes"
                  :key="`idx-${String(rowNode.rowId)}`"
                  class="ui-affino-grid__index-row"
                  :class="{ 'is-even': localIndex % 2 === 1 }"
                >
                  {{ resolveNodeDisplayIndex(rowNode, localIndex) + 1 }}
                </div>

                <div
                  v-if="bottomSpacerPx > 0"
                  class="ui-affino-grid__spacer-row ui-affino-grid__spacer-row--index"
                  :style="{ height: `${bottomSpacerPx}px` }"
                ></div>
              </template>
            </div>
          </div>
        </div>

        <div v-if="leftPinnedColumns.length > 0" class="ui-affino-grid__pinned-column ui-affino-grid__pinned-column--left">
          <div ref="leftPinnedHeaderRowRef" class="ui-affino-grid__row ui-affino-grid__row--header ui-affino-grid__row--pinned" :style="pinnedHeaderRowStyle">
            <div
              v-for="entry in leftPinnedColumns"
              :key="`head-left-${entry.key}`"
              class="ui-affino-grid__cell ui-affino-grid__cell--header"
              :style="columnStyle(entry.width)"
              v-bind="grid.bindings.headerCell(entry.key)"
            >
              <div class="ui-affino-grid__header-content">
                <span class="ui-affino-grid__header-label">{{ entry.label }}</span>
                <span v-if="sortDirection(entry.key)" class="ui-affino-grid__sort-indicator" :class="`is-${sortDirection(entry.key)}`">
                  {{ sortDirection(entry.key) === "asc" ? "▲" : "▼" }}
                </span>
              </div>
              <span
                v-if="enableColumnResize && grid.bindings.columnResizeHandle"
                class="ui-affino-grid__resize-handle"
                v-bind="grid.bindings.columnResizeHandle(entry.key)"
              ></span>
            </div>
          </div>

          <div
            v-if="showFilterRow"
            ref="leftPinnedFilterRowRef"
            class="ui-affino-grid__row ui-affino-grid__row--filter ui-affino-grid__row--pinned"
            :style="pinnedFilterRowStyle"
          >
            <div
              v-for="entry in leftPinnedColumns"
              :key="`filter-left-${entry.key}`"
              class="ui-affino-grid__cell ui-affino-grid__cell--filter"
              :style="columnStyle(entry.width)"
            >
              <input
                v-model="columnFilters[entry.key]"
                type="text"
                class="ui-affino-grid__filter-input"
                placeholder="Filter"
                @input="applyFilters"
              />
            </div>
          </div>

          <div ref="leftPinnedViewportRef" class="ui-affino-grid__pinned-viewport">
            <div class="ui-affino-grid__pinned-canvas">
              <template v-if="hasRenderableData">
                <div
                  v-if="topSpacerPx > 0"
                  class="ui-affino-grid__spacer-row ui-affino-grid__spacer-row--pinned"
                  :style="{ height: `${topSpacerPx}px` }"
                ></div>

                <div
                  v-for="(rowNode, localIndex) in visibleRowNodes"
                  :key="`left-${String(rowNode.rowId)}`"
                  class="ui-affino-grid__row ui-affino-grid__row--data ui-affino-grid__row--pinned"
                  :class="{ 'is-even': localIndex % 2 === 1 }"
                  v-bind="grid.bindings.rowSelection(rowData(rowNode.data), resolveNodeDisplayIndex(rowNode, localIndex))"
                  @click="emit('row-click', { row: rowData(rowNode.data), rowIndex: resolveNodeDisplayIndex(rowNode, localIndex) })"
                >
                  <div
                    v-for="entry in leftPinnedColumns"
                    :key="`left-${rowNode.rowId}-${entry.key}`"
                    class="ui-affino-grid__cell"
                    :style="columnStyle(entry.width)"
                    v-bind="grid.bindings.dataCell({
                      row: rowData(rowNode.data),
                      rowIndex: resolveNodeDisplayIndex(rowNode, localIndex),
                      columnKey: entry.key,
                      editable: false,
                      value: resolveValue(rowData(rowNode.data), entry.key),
                    })"
                  >
                    <slot
                      name="cell"
                      :column="entry.column"
                      :row="rowNode.data"
                      :rowIndex="resolveNodeDisplayIndex(rowNode, localIndex)"
                      :value="resolveValue(rowData(rowNode.data), entry.key)"
                    >
                      <span class="ui-affino-grid__value">{{ formatValue(resolveValue(rowData(rowNode.data), entry.key)) }}</span>
                    </slot>
                  </div>
                </div>

                <div
                  v-if="bottomSpacerPx > 0"
                  class="ui-affino-grid__spacer-row ui-affino-grid__spacer-row--pinned"
                  :style="{ height: `${bottomSpacerPx}px` }"
                ></div>
              </template>
            </div>
          </div>
        </div>

        <div
          ref="mainViewportRef"
          class="ui-affino-grid__main-viewport"
          @scroll.passive="handleMainScroll"
        >
          <div class="ui-affino-grid__main-canvas">
            <div class="ui-affino-grid__canvas">
              <div ref="headerRowRef" class="ui-affino-grid__row ui-affino-grid__row--header">
                <div
                  v-if="leftSpacerPx > 0"
                  class="ui-affino-grid__spacer"
                  :style="{ width: `${leftSpacerPx}px`, minWidth: `${leftSpacerPx}px` }"
                ></div>
                <div
                  v-for="entry in visibleColumns"
                  :key="`head-${entry.key}`"
                  class="ui-affino-grid__cell ui-affino-grid__cell--header"
                  :style="columnStyle(entry.width)"
                  v-bind="grid.bindings.headerCell(entry.key)"
                >
                  <div class="ui-affino-grid__header-content">
                    <span class="ui-affino-grid__header-label">{{ entry.label }}</span>
                    <span v-if="sortDirection(entry.key)" class="ui-affino-grid__sort-indicator" :class="`is-${sortDirection(entry.key)}`">
                      {{ sortDirection(entry.key) === "asc" ? "▲" : "▼" }}
                    </span>
                  </div>
                  <span
                    v-if="enableColumnResize && grid.bindings.columnResizeHandle"
                    class="ui-affino-grid__resize-handle"
                    v-bind="grid.bindings.columnResizeHandle(entry.key)"
                  ></span>
                </div>
                <div
                  v-if="rightSpacerPx > 0"
                  class="ui-affino-grid__spacer"
                  :style="{ width: `${rightSpacerPx}px`, minWidth: `${rightSpacerPx}px` }"
                ></div>
              </div>

              <div v-if="showFilterRow" ref="filterRowRef" class="ui-affino-grid__row ui-affino-grid__row--filter">
                <div
                  v-if="leftSpacerPx > 0"
                  class="ui-affino-grid__spacer"
                  :style="{ width: `${leftSpacerPx}px`, minWidth: `${leftSpacerPx}px` }"
                ></div>
                <div
                  v-for="entry in visibleColumns"
                  :key="`filter-${entry.key}`"
                  class="ui-affino-grid__cell ui-affino-grid__cell--filter"
                  :style="columnStyle(entry.width)"
                >
                  <input
                    v-model="columnFilters[entry.key]"
                    type="text"
                    class="ui-affino-grid__filter-input"
                    placeholder="Filter"
                    @input="applyFilters"
                  />
                </div>
                <div
                  v-if="rightSpacerPx > 0"
                  class="ui-affino-grid__spacer"
                  :style="{ width: `${rightSpacerPx}px`, minWidth: `${rightSpacerPx}px` }"
                ></div>
              </div>
            </div>

            <div
              ref="viewportRef"
              class="ui-affino-grid__viewport"
              @scroll.passive="handleBodyScroll"
            >
              <div class="ui-affino-grid__canvas">
                <template v-if="hasRenderableData">
                  <div v-if="topSpacerPx > 0" class="ui-affino-grid__spacer-row" :style="{ height: `${topSpacerPx}px` }"></div>

                  <div
                    v-for="(rowNode, localIndex) in visibleRowNodes"
                    :key="String(rowNode.rowId)"
                    class="ui-affino-grid__row ui-affino-grid__row--data"
                    :class="{ 'is-even': localIndex % 2 === 1 }"
                    v-bind="grid.bindings.rowSelection(rowData(rowNode.data), resolveNodeDisplayIndex(rowNode, localIndex))"
                    @click="emit('row-click', { row: rowData(rowNode.data), rowIndex: resolveNodeDisplayIndex(rowNode, localIndex) })"
                  >
                    <div
                      v-if="leftSpacerPx > 0"
                      class="ui-affino-grid__spacer"
                      :style="{ width: `${leftSpacerPx}px`, minWidth: `${leftSpacerPx}px` }"
                    ></div>

                    <div
                      v-for="entry in visibleColumns"
                      :key="`${rowNode.rowId}-${entry.key}`"
                      class="ui-affino-grid__cell"
                      :style="columnStyle(entry.width)"
                      v-bind="grid.bindings.dataCell({
                        row: rowData(rowNode.data),
                        rowIndex: resolveNodeDisplayIndex(rowNode, localIndex),
                        columnKey: entry.key,
                        editable: false,
                        value: resolveValue(rowData(rowNode.data), entry.key),
                      })"
                    >
                      <slot
                        name="cell"
                        :column="entry.column"
                        :row="rowNode.data"
                        :rowIndex="resolveNodeDisplayIndex(rowNode, localIndex)"
                        :value="resolveValue(rowData(rowNode.data), entry.key)"
                      >
                        <span class="ui-affino-grid__value">{{ formatValue(resolveValue(rowData(rowNode.data), entry.key)) }}</span>
                      </slot>
                    </div>

                    <div
                      v-if="rightSpacerPx > 0"
                      class="ui-affino-grid__spacer"
                      :style="{ width: `${rightSpacerPx}px`, minWidth: `${rightSpacerPx}px` }"
                    ></div>
                  </div>

                  <div v-if="bottomSpacerPx > 0" class="ui-affino-grid__spacer-row" :style="{ height: `${bottomSpacerPx}px` }"></div>
                </template>

                <div v-else class="ui-affino-grid__empty">
                  {{ emptyText }}
                </div>
              </div>
            </div>
          </div>
        </div>

        <div v-if="rightPinnedColumns.length > 0" class="ui-affino-grid__pinned-column ui-affino-grid__pinned-column--right">
          <div ref="rightPinnedHeaderRowRef" class="ui-affino-grid__row ui-affino-grid__row--header ui-affino-grid__row--pinned" :style="pinnedHeaderRowStyle">
            <div
              v-for="entry in rightPinnedColumns"
              :key="`head-right-${entry.key}`"
              class="ui-affino-grid__cell ui-affino-grid__cell--header"
              :style="columnStyle(entry.width)"
              v-bind="grid.bindings.headerCell(entry.key)"
            >
              <div class="ui-affino-grid__header-content">
                <span class="ui-affino-grid__header-label">{{ entry.label }}</span>
                <span v-if="sortDirection(entry.key)" class="ui-affino-grid__sort-indicator" :class="`is-${sortDirection(entry.key)}`">
                  {{ sortDirection(entry.key) === "asc" ? "▲" : "▼" }}
                </span>
              </div>
              <span
                v-if="enableColumnResize && grid.bindings.columnResizeHandle"
                class="ui-affino-grid__resize-handle"
                v-bind="grid.bindings.columnResizeHandle(entry.key)"
              ></span>
            </div>
          </div>

          <div
            v-if="showFilterRow"
            ref="rightPinnedFilterRowRef"
            class="ui-affino-grid__row ui-affino-grid__row--filter ui-affino-grid__row--pinned"
            :style="pinnedFilterRowStyle"
          >
            <div
              v-for="entry in rightPinnedColumns"
              :key="`filter-right-${entry.key}`"
              class="ui-affino-grid__cell ui-affino-grid__cell--filter"
              :style="columnStyle(entry.width)"
            >
              <input
                v-model="columnFilters[entry.key]"
                type="text"
                class="ui-affino-grid__filter-input"
                placeholder="Filter"
                @input="applyFilters"
              />
            </div>
          </div>

          <div ref="rightPinnedViewportRef" class="ui-affino-grid__pinned-viewport">
            <div class="ui-affino-grid__pinned-canvas">
              <template v-if="hasRenderableData">
                <div
                  v-if="topSpacerPx > 0"
                  class="ui-affino-grid__spacer-row ui-affino-grid__spacer-row--pinned"
                  :style="{ height: `${topSpacerPx}px` }"
                ></div>

                <div
                  v-for="(rowNode, localIndex) in visibleRowNodes"
                  :key="`right-${String(rowNode.rowId)}`"
                  class="ui-affino-grid__row ui-affino-grid__row--data ui-affino-grid__row--pinned"
                  :class="{ 'is-even': localIndex % 2 === 1 }"
                  v-bind="grid.bindings.rowSelection(rowData(rowNode.data), resolveNodeDisplayIndex(rowNode, localIndex))"
                  @click="emit('row-click', { row: rowData(rowNode.data), rowIndex: resolveNodeDisplayIndex(rowNode, localIndex) })"
                >
                  <div
                    v-for="entry in rightPinnedColumns"
                    :key="`right-${rowNode.rowId}-${entry.key}`"
                    class="ui-affino-grid__cell"
                    :style="columnStyle(entry.width)"
                    v-bind="grid.bindings.dataCell({
                      row: rowData(rowNode.data),
                      rowIndex: resolveNodeDisplayIndex(rowNode, localIndex),
                      columnKey: entry.key,
                      editable: false,
                      value: resolveValue(rowData(rowNode.data), entry.key),
                    })"
                  >
                    <slot
                      name="cell"
                      :column="entry.column"
                      :row="rowNode.data"
                      :rowIndex="resolveNodeDisplayIndex(rowNode, localIndex)"
                      :value="resolveValue(rowData(rowNode.data), entry.key)"
                    >
                      <span class="ui-affino-grid__value">{{ formatValue(resolveValue(rowData(rowNode.data), entry.key)) }}</span>
                    </slot>
                  </div>
                </div>

                <div
                  v-if="bottomSpacerPx > 0"
                  class="ui-affino-grid__spacer-row ui-affino-grid__spacer-row--pinned"
                  :style="{ height: `${bottomSpacerPx}px` }"
                ></div>
              </template>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, reactive, ref, watch } from "vue"
import type {
  DataGridColumnModel,
  DataGridColumnSnapshot,
  DataGridCoreServiceContext,
  DataGridRowModel,
} from "@affino/datagrid-core"
import { useAffinoDataGrid } from "@affino/datagrid-vue"
import {
  useDataGridColumnLayoutOrchestration,
} from "@affino/datagrid-vue/advanced"

type GridRow = Record<string, unknown>

type GridColumn = {
  key: string
  label?: string
  width?: number
  minWidth?: number
  maxWidth?: number
  visible?: boolean
  pin?: "left" | "right" | "none"
  meta?: Record<string, unknown>
}

type ResolvedColumn = {
  key: string
  label: string
  width: number
  pin?: "left" | "right" | "none"
  column: GridColumn
}

type WindowRange = {
  start: number
  end: number
}

type RowHeightMode = "fixed" | "auto"

type ViewportMetricsSnapshot = {
  scrollTop: number
  scrollLeft: number
  viewportHeight: number
  viewportWidth: number
  rowHeight: number
  overscanRows: number
  overscanColumns: number
}

type ViewportRowModelBridge = Pick<DataGridRowModel<GridRow>, "setViewportRange" | "getRowCount">
type ViewportColumnModelBridge = Pick<DataGridColumnModel, "getSnapshot">
type VirtualWindowSnapshot = {
  rowStart: number
  rowEnd: number
  rowTotal: number
  colStart: number
  colEnd: number
  colTotal: number
  overscan: {
    top: number
    bottom: number
    left: number
    right: number
  }
}

const props = withDefaults(defineProps<{
  rows: GridRow[]
  columns: GridColumn[]
  rowHeight?: number
  overscanRows?: number
  overscanColumns?: number
  enableSorting?: boolean
  enableFiltering?: boolean
  enableColumnResize?: boolean
  emptyText?: string
  rowKey?: (row: GridRow, rowIndex: number) => string
}>(), {
  rowHeight: 34,
  overscanRows: 8,
  overscanColumns: 2,
  enableSorting: true,
  enableFiltering: true,
  enableColumnResize: true,
  emptyText: "No data",
  rowKey: undefined,
})

const emit = defineEmits<{ (e: "row-click", payload: { row: GridRow; rowIndex: number }): void }>()

const mainViewportRef = ref<HTMLElement | null>(null)
const viewportRef = ref<HTMLElement | null>(null)
const indexViewportRef = ref<HTMLElement | null>(null)
const headerRowRef = ref<HTMLElement | null>(null)
const filterRowRef = ref<HTMLElement | null>(null)
const leftPinnedViewportRef = ref<HTMLElement | null>(null)
const rightPinnedViewportRef = ref<HTMLElement | null>(null)
const leftPinnedHeaderRowRef = ref<HTMLElement | null>(null)
const rightPinnedHeaderRowRef = ref<HTMLElement | null>(null)
const leftPinnedFilterRowRef = ref<HTMLElement | null>(null)
const rightPinnedFilterRowRef = ref<HTMLElement | null>(null)
const PINNED_INDEX_COLUMN_WIDTH = 64

const columnFilters = reactive<Record<string, string>>({})
const baseRowHeight = ref(Math.max(1, props.rowHeight))
const rowHeightMode = ref<RowHeightMode>("fixed")
const measuredAutoRowHeight = ref<number | null>(null)
const viewportMetrics = reactive<ViewportMetricsSnapshot>({
  scrollTop: 0,
  scrollLeft: 0,
  viewportHeight: 0,
  viewportWidth: 0,
  rowHeight: baseRowHeight.value,
  overscanRows: Math.max(0, Math.trunc(props.overscanRows)),
  overscanColumns: Math.max(0, Math.trunc(props.overscanColumns)),
})
const observedViewportWidth = ref<number | null>(null)
const observedViewportHeight = ref<number | null>(null)
const rowModelRevision = ref(0)
const measuredHeaderHeight = ref<number | null>(null)
const measuredFilterHeight = ref<number | null>(null)

let viewportRowModel: ViewportRowModelBridge | null = null
let viewportColumnModel: ViewportColumnModelBridge | null = null
let explicitRowRange: WindowRange | null = null
let lastAppliedRowRange: WindowRange | null = null
let cachedColumnWindowSource: readonly DataGridColumnSnapshot[] | null = null
let cachedColumnWindowPrefix: number[] = []
let lastAppliedFilterSignature: string | null = null
let lastVirtualWindowSnapshot: VirtualWindowSnapshot | null = null

const INDEX_COLUMN_KEYS = new Set<string>([
  "__snapshotindex__",
  "__snapshot_index__",
  "__rowindex__",
  "__row_index__",
  "__index__",
  "rownum",
  "row_number",
])

function isIndexLikeColumn(column: GridColumn): boolean {
  const normalizedKey = String(column.key ?? "")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9_]/g, "")
  if (INDEX_COLUMN_KEYS.has(normalizedKey)) {
    return true
  }
  const normalizedLabel = String(column.label ?? "").trim()
  return normalizedLabel === "#" || normalizedLabel === "№"
}

const coreColumns = computed(() => props.columns.filter(column => !isIndexLikeColumn(column)))

function resolveRowTotalFromModel(): number {
  return Math.max(0, viewportRowModel?.getRowCount() ?? 0)
}

function isSameRange(left: WindowRange | null, right: WindowRange | null): boolean {
  if (!left && !right) return true
  if (!left || !right) return false
  return left.start === right.start && left.end === right.end
}

function normalizeRange(range: WindowRange, total: number): WindowRange {
  if (total <= 0) {
    return { start: 0, end: 0 }
  }
  const start = Math.max(0, Math.min(total - 1, Math.trunc(range.start)))
  const end = Math.max(start, Math.min(total - 1, Math.trunc(range.end)))
  return { start, end }
}

function computeRowRangeFromMetrics(totalRows: number): WindowRange {
  if (totalRows <= 0) {
    return { start: 0, end: 0 }
  }

  const rowHeight = Math.max(1, viewportMetrics.rowHeight)
  const overscan = Math.max(0, viewportMetrics.overscanRows)
  const viewportBody = Math.max(rowHeight, viewportMetrics.viewportHeight || rowHeight)
  const estimatedVisible = Math.max(1, Math.ceil(viewportBody / rowHeight))
  const baseStart = Math.floor(viewportMetrics.scrollTop / rowHeight)
  const start = Math.max(0, Math.min(totalRows - 1, baseStart - overscan))
  const end = Math.max(start, Math.min(totalRows - 1, start + estimatedVisible + overscan * 2 - 1))
  return { start, end }
}

function resolveColumnWidth(snapshot: DataGridColumnSnapshot): number {
  const fromSnapshot = snapshot.width
  if (Number.isFinite(fromSnapshot) && (fromSnapshot as number) > 0) {
    return Math.max(1, Math.trunc(fromSnapshot as number))
  }
  const fromColumn = snapshot.column.width
  if (Number.isFinite(fromColumn) && (fromColumn as number) > 0) {
    return Math.max(1, Math.trunc(fromColumn as number))
  }
  const fromMin = snapshot.column.minWidth
  if (Number.isFinite(fromMin) && (fromMin as number) > 0) {
    return Math.max(1, Math.trunc(fromMin as number))
  }
  return 180
}

function ensureColumnWindowPrefix(columns: readonly DataGridColumnSnapshot[]): readonly number[] {
  if (cachedColumnWindowSource === columns) {
    return cachedColumnWindowPrefix
  }

  const nextPrefix: number[] = new Array(columns.length)
  let cursor = 0
  for (let index = 0; index < columns.length; index += 1) {
    const column = columns[index]
    if (!column) {
      nextPrefix[index] = cursor
      continue
    }
    cursor += resolveColumnWidth(column)
    nextPrefix[index] = cursor
  }

  cachedColumnWindowSource = columns
  cachedColumnWindowPrefix = nextPrefix
  return nextPrefix
}

function findFirstPrefixGreaterThan(prefix: readonly number[], threshold: number): number {
  if (prefix.length === 0) return -1
  let left = 0
  let right = prefix.length - 1
  let answer = -1
  while (left <= right) {
    const middle = (left + right) >> 1
    const value = prefix[middle] ?? 0
    if (value > threshold) {
      answer = middle
      right = middle - 1
    } else {
      left = middle + 1
    }
  }
  return answer
}

function findFirstPrefixGreaterOrEqual(prefix: readonly number[], threshold: number): number {
  if (prefix.length === 0) return -1
  let left = 0
  let right = prefix.length - 1
  let answer = -1
  while (left <= right) {
    const middle = (left + right) >> 1
    const value = prefix[middle] ?? 0
    if (value >= threshold) {
      answer = middle
      right = middle - 1
    } else {
      left = middle + 1
    }
  }
  return answer
}

function computeColumnRange(columns: readonly DataGridColumnSnapshot[]): WindowRange {
  if (!columns.length) {
    return { start: 0, end: -1 }
  }

  const viewport = Math.max(1, viewportMetrics.viewportWidth)
  const overscan = Math.max(0, viewportMetrics.overscanColumns)
  const startEdge = Math.max(0, viewportMetrics.scrollLeft)
  const endEdge = startEdge + viewport
  const prefix = ensureColumnWindowPrefix(columns)
  const firstVisibleRaw = findFirstPrefixGreaterThan(prefix, startEdge)
  const lastVisibleRaw = findFirstPrefixGreaterOrEqual(prefix, endEdge)
  const firstVisible = firstVisibleRaw >= 0 ? firstVisibleRaw : columns.length - 1
  const lastVisible = lastVisibleRaw >= 0 ? lastVisibleRaw : columns.length - 1

  return {
    start: Math.max(0, firstVisible - overscan),
    end: Math.min(columns.length - 1, lastVisible + overscan),
  }
}

function resolveRowRange(rowTotal: number): WindowRange {
  if (explicitRowRange) {
    return normalizeRange(explicitRowRange, rowTotal)
  }
  return computeRowRangeFromMetrics(rowTotal)
}

function applyRowRangeToModel(): WindowRange {
  const rowTotal = resolveRowTotalFromModel()
  const range = resolveRowRange(rowTotal)
  if (!isSameRange(lastAppliedRowRange, range)) {
    viewportRowModel?.setViewportRange(range)
    lastAppliedRowRange = { start: range.start, end: range.end }
  }
  return range
}

function isSameVirtualWindowSnapshot(left: VirtualWindowSnapshot | null, right: VirtualWindowSnapshot): boolean {
  if (!left) return false
  return (
    left.rowStart === right.rowStart &&
    left.rowEnd === right.rowEnd &&
    left.rowTotal === right.rowTotal &&
    left.colStart === right.colStart &&
    left.colEnd === right.colEnd &&
    left.colTotal === right.colTotal &&
    left.overscan.top === right.overscan.top &&
    left.overscan.bottom === right.overscan.bottom &&
    left.overscan.left === right.overscan.left &&
    left.overscan.right === right.overscan.right
  )
}

const viewportService = {
  name: "viewport" as const,
  init(context: DataGridCoreServiceContext) {
    const rowService = context.getService("rowModel") as { model?: ViewportRowModelBridge | null }
    const columnService = context.getService("columnModel") as { model?: ViewportColumnModelBridge | null }
    viewportRowModel = rowService.model ?? null
    viewportColumnModel = columnService.model ?? null
    lastAppliedRowRange = null
    cachedColumnWindowSource = null
    cachedColumnWindowPrefix = []
    lastVirtualWindowSnapshot = null
  },
  setViewportRange(range: WindowRange) {
    explicitRowRange = normalizeRange(range, resolveRowTotalFromModel())
    if (!isSameRange(lastAppliedRowRange, explicitRowRange)) {
      viewportRowModel?.setViewportRange(explicitRowRange)
      lastAppliedRowRange = { start: explicitRowRange.start, end: explicitRowRange.end }
    }
  },
  setViewportMetrics(next: ViewportMetricsSnapshot): WindowRange | null {
    const normalizedTop = Math.max(0, Number.isFinite(next.scrollTop) ? next.scrollTop : 0)
    const normalizedLeft = Math.max(0, Number.isFinite(next.scrollLeft) ? next.scrollLeft : 0)
    const normalizedHeight = Math.max(0, Number.isFinite(next.viewportHeight) ? next.viewportHeight : 0)
    const normalizedWidth = Math.max(0, Number.isFinite(next.viewportWidth) ? next.viewportWidth : 0)
    const normalizedRowHeight = Math.max(1, Number.isFinite(next.rowHeight) ? next.rowHeight : props.rowHeight)
    const normalizedOverscanRows = Math.max(
      0,
      Math.trunc(Number.isFinite(next.overscanRows) ? next.overscanRows : props.overscanRows),
    )
    const normalizedOverscanColumns = Math.max(
      0,
      Math.trunc(Number.isFinite(next.overscanColumns) ? next.overscanColumns : props.overscanColumns),
    )

    const changed = (
      viewportMetrics.scrollTop !== normalizedTop ||
      viewportMetrics.scrollLeft !== normalizedLeft ||
      viewportMetrics.viewportHeight !== normalizedHeight ||
      viewportMetrics.viewportWidth !== normalizedWidth ||
      viewportMetrics.rowHeight !== normalizedRowHeight ||
      viewportMetrics.overscanRows !== normalizedOverscanRows ||
      viewportMetrics.overscanColumns !== normalizedOverscanColumns
    )
    if (!changed) {
      return null
    }

    viewportMetrics.scrollTop = normalizedTop
    viewportMetrics.scrollLeft = normalizedLeft
    viewportMetrics.viewportHeight = normalizedHeight
    viewportMetrics.viewportWidth = normalizedWidth
    viewportMetrics.rowHeight = normalizedRowHeight
    viewportMetrics.overscanRows = normalizedOverscanRows
    viewportMetrics.overscanColumns = normalizedOverscanColumns
    explicitRowRange = null
    return applyRowRangeToModel()
  },
  setRowHeightMode(mode: RowHeightMode) {
    const normalized: RowHeightMode = mode === "auto" ? "auto" : "fixed"
    if (rowHeightMode.value === normalized) {
      return
    }
    rowHeightMode.value = normalized
    if (normalized === "fixed") {
      measuredAutoRowHeight.value = null
    } else {
      scheduleAutoRowHeightMeasure()
    }
    scheduleViewportSync()
  },
  setBaseRowHeight(height: number) {
    const normalized = Math.max(1, Math.trunc(Number.isFinite(height) ? height : baseRowHeight.value))
    if (baseRowHeight.value === normalized) {
      return
    }
    baseRowHeight.value = normalized
    if (rowHeightMode.value === "auto") {
      scheduleAutoRowHeightMeasure()
    }
    scheduleViewportSync()
  },
  measureRowHeight() {
    scheduleAutoRowHeightMeasure()
  },
  getVirtualWindow() {
    const rowTotal = resolveRowTotalFromModel()
    const rowRange = resolveRowRange(rowTotal)
    const columns = viewportColumnModel?.getSnapshot().visibleColumns ?? []
    const colTotal = columns.length
    const colRange = computeColumnRange(columns)
    const safeColStart = colTotal <= 0 ? 0 : Math.max(0, Math.min(colTotal - 1, Math.trunc(colRange.start)))
    const safeColEnd = colTotal <= 0
      ? 0
      : Math.max(safeColStart, Math.min(colTotal - 1, Math.trunc(colRange.end)))

    const nextSnapshot: VirtualWindowSnapshot = {
      rowStart: rowRange.start,
      rowEnd: rowRange.end,
      rowTotal,
      colStart: safeColStart,
      colEnd: safeColEnd,
      colTotal,
      overscan: {
        top: viewportMetrics.overscanRows,
        bottom: viewportMetrics.overscanRows,
        left: viewportMetrics.overscanColumns,
        right: viewportMetrics.overscanColumns,
      },
    }
    if (isSameVirtualWindowSnapshot(lastVirtualWindowSnapshot, nextSnapshot)) {
      return lastVirtualWindowSnapshot as VirtualWindowSnapshot
    }
    lastVirtualWindowSnapshot = nextSnapshot
    return nextSnapshot
  },
}

const grid = useAffinoDataGrid<GridRow>({
  rows: computed(() => props.rows),
  columns: coreColumns,
  services: {
    viewport: viewportService,
  },
  features: {
    selection: {
      enabled: true,
      resolveRowKey: (row, index) => {
        if (props.rowKey) {
          return props.rowKey(row, index)
        }
        const candidate = row.rowId ?? row.id ?? row.key
        if (candidate !== undefined && candidate !== null && String(candidate).trim()) {
          return String(candidate)
        }
        return `row-${index}`
      },
    },
    clipboard: false,
    editing: false,
    filtering: {
      enabled: props.enableFiltering,
      initialFilterModel: null,
    },
    keyboardNavigation: {
      enabled: true,
    },
    rowHeight: {
      enabled: true,
      mode: "fixed",
      base: baseRowHeight.value,
    },
    interactions: false,
    headerFilters: false,
    feedback: false,
    statusBar: false,
    tree: false,
    summary: false,
    visibility: false,
  },
})

const unsubscribeRowModel = grid.rowModel.subscribe((snapshot) => {
  const revision = Number(snapshot?.revision)
  rowModelRevision.value = Number.isFinite(revision)
    ? Math.max(0, Math.trunc(revision))
    : rowModelRevision.value + 1
})

function resolveMeasuredHeightStyle(height: number | null): Record<string, string> {
  if (!height || height <= 0) {
    return {}
  }
  const px = `${height}px`
  return {
    height: px,
    minHeight: px,
    maxHeight: px,
  }
}

const showFilterRow = computed(() => props.enableFiltering)
const indexHeaderStyle = computed<Record<string, string>>(() => resolveMeasuredHeightStyle(measuredHeaderHeight.value))
const indexFilterStyle = computed<Record<string, string>>(() => resolveMeasuredHeightStyle(measuredFilterHeight.value))
const pinnedHeaderRowStyle = computed<Record<string, string>>(() => resolveMeasuredHeightStyle(measuredHeaderHeight.value))
const pinnedFilterRowStyle = computed<Record<string, string>>(() => resolveMeasuredHeightStyle(measuredFilterHeight.value))
const rowHeightPx = computed(() => {
  if (rowHeightMode.value === "auto") {
    return Math.max(baseRowHeight.value, measuredAutoRowHeight.value ?? baseRowHeight.value)
  }
  return baseRowHeight.value
})
const isRowHeightFixed = computed(() => rowHeightMode.value === "fixed")
const gridStyle = computed<Record<string, string>>(() => ({
  "--ui-affino-row-height": `${rowHeightPx.value}px`,
  "--ui-affino-index-width": `${PINNED_INDEX_COLUMN_WIDTH}px`,
  "--ui-affino-left-width": `${leftPinnedWidthPx.value}px`,
  "--ui-affino-right-width": `${rightPinnedWidthPx.value}px`,
}))

const totalRows = computed(() => {
  void rowModelRevision.value
  const window = grid.virtualWindow.value
  if (window && Number.isFinite(window.rowTotal)) {
    return Math.max(0, Math.trunc(window.rowTotal))
  }
  return Math.max(0, grid.rowModel.getRowCount())
})

const resolvedColumns = computed<readonly ResolvedColumn[]>(() => {
  const snapshot = grid.columnState.snapshot.value
  return snapshot.visibleColumns.map((column) => ({
    key: column.key,
    label: column.column.label ?? column.key,
    width: Math.max(column.column.minWidth ?? 80, column.width ?? column.column.width ?? 180),
    pin: column.column.pin as "left" | "right" | "none" | undefined,
    column: column.column as GridColumn,
  }))
})

const visibleRowRange = computed<WindowRange>(() => {
  const window = grid.virtualWindow.value
  const total = totalRows.value
  if (!window || total <= 0) {
    return { start: 0, end: -1 }
  }
  const start = Math.max(0, Math.min(total - 1, Math.trunc(window.rowStart)))
  const end = Math.max(start, Math.min(total - 1, Math.trunc(window.rowEnd)))
  return { start, end }
})

const columnLayout = useDataGridColumnLayoutOrchestration({
  columns: resolvedColumns,
  resolveColumnWidth: column => column.width,
  virtualWindow: grid.virtualWindow,
})

const orderedColumns = computed(() => columnLayout.orderedColumns.value)
function normalizePin(pin: GridColumn["pin"] | ResolvedColumn["pin"]): "left" | "right" | "none" {
  if (pin === "left" || pin === "right") {
    return pin
  }
  return "none"
}

function resolveResolvedColumnWidth(column: ResolvedColumn): number {
  return Math.max(1, Math.trunc(Number.isFinite(column.width) ? column.width : 180))
}

const leftPinnedColumns = computed(() => (
  orderedColumns.value.filter(column => normalizePin(column.pin) === "left")
))

const rightPinnedColumns = computed(() => (
  orderedColumns.value.filter(column => normalizePin(column.pin) === "right")
))

const centerColumns = computed(() => (
  orderedColumns.value.filter(column => normalizePin(column.pin) === "none")
))

const leftPinnedWidthPx = computed(() => (
  leftPinnedColumns.value.reduce((sum, column) => sum + resolveResolvedColumnWidth(column), 0)
))

const rightPinnedWidthPx = computed(() => (
  rightPinnedColumns.value.reduce((sum, column) => sum + resolveResolvedColumnWidth(column), 0)
))

const centerPrefix = computed<readonly number[]>(() => {
  const columns = centerColumns.value
  const prefix: number[] = new Array(columns.length)
  let cursor = 0
  for (let index = 0; index < columns.length; index += 1) {
    cursor += resolveResolvedColumnWidth(columns[index] as ResolvedColumn)
    prefix[index] = cursor
  }
  return prefix
})

const centerVisibleColumnWindow = computed<WindowRange>(() => {
  const columns = centerColumns.value
  const total = columns.length
  if (total === 0) {
    return { start: 0, end: -1 }
  }

  const mainViewport = mainViewportRef.value
  const viewportWidth = Math.max(
    1,
    observedViewportWidth.value
      ?? (mainViewport ? Math.max(0, mainViewport.clientWidth) : 0)
      ?? 0,
  )
  const scrollLeft = Math.max(0, viewportMetrics.scrollLeft)
  const endEdge = scrollLeft + viewportWidth
  const overscan = Math.max(0, viewportMetrics.overscanColumns)
  const prefix = centerPrefix.value

  const firstVisibleRaw = findFirstPrefixGreaterThan(prefix, scrollLeft)
  const lastVisibleRaw = findFirstPrefixGreaterOrEqual(prefix, endEdge)
  const firstVisible = firstVisibleRaw >= 0 ? firstVisibleRaw : total - 1
  const lastVisible = lastVisibleRaw >= 0 ? lastVisibleRaw : total - 1

  return {
    start: Math.max(0, firstVisible - overscan),
    end: Math.min(total - 1, lastVisible + overscan),
  }
})

const leftSpacerPx = computed(() => {
  const { start } = centerVisibleColumnWindow.value
  if (start <= 0) return 0
  const prefix = centerPrefix.value
  return Math.max(0, prefix[start - 1] ?? 0)
})

const rightSpacerPx = computed(() => {
  const { end } = centerVisibleColumnWindow.value
  const prefix = centerPrefix.value
  const totalWidth = prefix[prefix.length - 1] ?? 0
  if (end < 0 || end >= prefix.length - 1) return 0
  return Math.max(0, totalWidth - (prefix[end] ?? 0))
})

const visibleColumns = computed(() => {
  const { start, end } = centerVisibleColumnWindow.value
  if (end < start) return []
  return centerColumns.value.slice(start, end + 1)
})

const allRenderableColumnsCount = computed(() => (
  leftPinnedColumns.value.length + centerColumns.value.length + rightPinnedColumns.value.length
))

const hasRenderableData = computed(() => totalRows.value > 0 && allRenderableColumnsCount.value > 0)

const renderedColumnsSignature = computed(() => (
  orderedColumns.value
    .map(column => `${column.key}:${normalizePin(column.pin)}:${resolveResolvedColumnWidth(column)}`)
    .join("|")
))

const visibleRowNodes = computed(() => {
  void props.rows
  void rowModelRevision.value
  const { start, end } = visibleRowRange.value
  if (end < start || totalRows.value === 0) return []
  return grid.rowModel.getRowsInRange({ start, end })
})

function resolveNodeDisplayIndex(rowNode: unknown, localIndex: number): number {
  const fromNode = Number((rowNode as { displayIndex?: number })?.displayIndex)
  if (Number.isFinite(fromNode) && fromNode >= 0) {
    return Math.trunc(fromNode)
  }
  return Math.max(0, visibleRowRange.value.start + Math.max(0, Math.trunc(localIndex)))
}

const renderedDisplayRange = computed<WindowRange>(() => {
  const rows = visibleRowNodes.value
  if (!rows.length) {
    return { start: -1, end: -1 }
  }
  const first = resolveNodeDisplayIndex(rows[0], 0)
  const last = resolveNodeDisplayIndex(rows[rows.length - 1], rows.length - 1)
  return {
    start: Math.max(0, Math.min(first, last)),
    end: Math.max(first, last),
  }
})

const renderedRowsCount = computed(() => visibleRowNodes.value.length)

const topSpacerPx = computed(() => {
  const { start } = renderedDisplayRange.value
  if (start < 0 || totalRows.value <= 0) return 0
  return Math.max(0, start * rowHeightPx.value)
})

const bottomSpacerPx = computed(() => {
  const total = totalRows.value
  if (total <= 0) return 0
  const totalHeight = Math.max(0, total * rowHeightPx.value)
  const renderedHeight = Math.max(0, renderedRowsCount.value * rowHeightPx.value)
  return Math.max(0, totalHeight - topSpacerPx.value - renderedHeight)
})

let syncFrame: number | null = null
let autoRowHeightMeasureFrame: number | null = null
let onWindowResize: (() => void) | null = null
let viewportResizeObserver: ResizeObserver | null = null
let lastHandledScrollTop = Number.NaN
let lastHandledScrollLeft = Number.NaN

function updateObservedViewportSize() {
  const mainViewport = mainViewportRef.value
  const bodyViewport = viewportRef.value

  if (mainViewport) {
    const width = Math.max(0, mainViewport.clientWidth)
    observedViewportWidth.value = width > 0 ? width : null
  }
  if (bodyViewport) {
    const height = Math.max(0, bodyViewport.clientHeight)
    observedViewportHeight.value = height > 0 ? height : null
  }
}

function updateMeasuredHeaderHeights() {
  const measureHeight = (element: HTMLElement | null): number => (
    element ? Math.max(0, Math.round(element.getBoundingClientRect().height)) : 0
  )

  const headerHeight = Math.max(
    measureHeight(headerRowRef.value),
    measureHeight(leftPinnedHeaderRowRef.value),
    measureHeight(rightPinnedHeaderRowRef.value),
  )
  measuredHeaderHeight.value = headerHeight > 0 ? headerHeight : null

  const filterHeight = Math.max(
    measureHeight(filterRowRef.value),
    measureHeight(leftPinnedFilterRowRef.value),
    measureHeight(rightPinnedFilterRowRef.value),
  )
  measuredFilterHeight.value = filterHeight > 0 ? filterHeight : null
}

function syncLinkedScroll(scrollTop: number) {
  const indexViewport = indexViewportRef.value
  if (indexViewport && indexViewport.scrollTop !== scrollTop) {
    indexViewport.scrollTop = scrollTop
  }
  const leftPinnedViewport = leftPinnedViewportRef.value
  if (leftPinnedViewport && leftPinnedViewport.scrollTop !== scrollTop) {
    leftPinnedViewport.scrollTop = scrollTop
  }
  const rightPinnedViewport = rightPinnedViewportRef.value
  if (rightPinnedViewport && rightPinnedViewport.scrollTop !== scrollTop) {
    rightPinnedViewport.scrollTop = scrollTop
  }
}

function measureVisibleAutoRowHeight(): number | null {
  const viewport = viewportRef.value
  if (!viewport) return null
  const rows = viewport.querySelectorAll<HTMLDivElement>(".ui-affino-grid__row--data")
  if (!rows.length) return null
  let maxHeight = 0
  rows.forEach((row) => {
    maxHeight = Math.max(maxHeight, row.getBoundingClientRect().height)
  })
  if (!Number.isFinite(maxHeight) || maxHeight <= 0) {
    return null
  }
  return Math.max(1, Math.round(maxHeight))
}

function applyMeasuredAutoRowHeight(value: number | null) {
  const next = value && value > 0 ? value : null
  if (measuredAutoRowHeight.value === next) {
    return
  }
  measuredAutoRowHeight.value = next
  scheduleViewportSync()
}

function scheduleAutoRowHeightMeasure() {
  if (rowHeightMode.value !== "auto") return
  if (autoRowHeightMeasureFrame !== null) return
  autoRowHeightMeasureFrame = requestAnimationFrame(() => {
    autoRowHeightMeasureFrame = null
    applyMeasuredAutoRowHeight(measureVisibleAutoRowHeight())
  })
}

function syncViewportMetrics() {
  const bodyViewport = viewportRef.value
  const mainViewport = mainViewportRef.value
  if (!bodyViewport || !mainViewport) return
  syncLinkedScroll(bodyViewport.scrollTop)
  const liveHeight = Math.max(0, bodyViewport.clientHeight)
  const liveWidth = Math.max(0, mainViewport.clientWidth)
  const viewportHeight = (observedViewportHeight.value && observedViewportHeight.value > 0)
    ? observedViewportHeight.value
    : liveHeight
  const viewportWidth = (observedViewportWidth.value && observedViewportWidth.value > 0)
    ? observedViewportWidth.value
    : liveWidth
  const range = viewportService.setViewportMetrics({
    scrollTop: bodyViewport.scrollTop,
    scrollLeft: mainViewport.scrollLeft,
    viewportHeight,
    viewportWidth,
    rowHeight: rowHeightPx.value,
    overscanRows: props.overscanRows,
    overscanColumns: props.overscanColumns,
  })
  if (range) {
    // Runtime virtualWindow recompute is tied to row/column model ticks.
    // Force a lightweight sync so horizontal-only scroll updates column window.
    grid.syncRowsInRange(range)
  }
}

function scheduleViewportSync() {
  if (syncFrame !== null) {
    return
  }
  syncFrame = requestAnimationFrame(() => {
    syncFrame = null
    syncViewportMetrics()
  })
}

onMounted(() => {
  const bodyViewport = viewportRef.value
  const mainViewport = mainViewportRef.value
  if (bodyViewport || mainViewport) {
    updateObservedViewportSize()
    updateMeasuredHeaderHeights()
    lastHandledScrollTop = bodyViewport?.scrollTop ?? 0
    lastHandledScrollLeft = mainViewport?.scrollLeft ?? 0
    syncLinkedScroll(lastHandledScrollTop)
  }
  if (typeof window !== "undefined") {
    onWindowResize = () => {
      const currentBody = viewportRef.value
      const currentMain = mainViewportRef.value
      if (!currentBody && !currentMain) {
        return
      }
      updateObservedViewportSize()
      if (rowHeightMode.value === "auto") {
        scheduleAutoRowHeightMeasure()
      }
      scheduleViewportSync()
    }
    window.addEventListener("resize", onWindowResize, { passive: true })
  }

  if (typeof ResizeObserver !== "undefined" && (bodyViewport || mainViewport)) {
    viewportResizeObserver = new ResizeObserver(() => {
      updateObservedViewportSize()
      updateMeasuredHeaderHeights()
      if (rowHeightMode.value === "auto") {
        scheduleAutoRowHeightMeasure()
      }
      scheduleViewportSync()
    })
    if (bodyViewport) {
      viewportResizeObserver.observe(bodyViewport)
    }
    if (mainViewport && mainViewport !== bodyViewport) {
      viewportResizeObserver.observe(mainViewport)
    }
    if (leftPinnedViewportRef.value) {
      viewportResizeObserver.observe(leftPinnedViewportRef.value)
    }
    if (rightPinnedViewportRef.value) {
      viewportResizeObserver.observe(rightPinnedViewportRef.value)
    }
    if (headerRowRef.value) {
      viewportResizeObserver.observe(headerRowRef.value)
    }
    if (filterRowRef.value) {
      viewportResizeObserver.observe(filterRowRef.value)
    }
    if (leftPinnedHeaderRowRef.value) {
      viewportResizeObserver.observe(leftPinnedHeaderRowRef.value)
    }
    if (rightPinnedHeaderRowRef.value) {
      viewportResizeObserver.observe(rightPinnedHeaderRowRef.value)
    }
    if (leftPinnedFilterRowRef.value) {
      viewportResizeObserver.observe(leftPinnedFilterRowRef.value)
    }
    if (rightPinnedFilterRowRef.value) {
      viewportResizeObserver.observe(rightPinnedFilterRowRef.value)
    }
  }

  syncFilterKeys()
  applyFilters()
  if (rowHeightMode.value === "auto") {
    scheduleAutoRowHeightMeasure()
  }
  scheduleViewportSync()
  requestAnimationFrame(() => {
    updateObservedViewportSize()
    updateMeasuredHeaderHeights()
    scheduleViewportSync()
  })
})

onBeforeUnmount(() => {
  unsubscribeRowModel()
  if (onWindowResize && typeof window !== "undefined") {
    window.removeEventListener("resize", onWindowResize)
  }
  onWindowResize = null
  viewportResizeObserver?.disconnect()
  viewportResizeObserver = null
  observedViewportWidth.value = null
  observedViewportHeight.value = null
  lastHandledScrollTop = Number.NaN
  lastHandledScrollLeft = Number.NaN
  if (syncFrame !== null) {
    cancelAnimationFrame(syncFrame)
    syncFrame = null
  }
  if (autoRowHeightMeasureFrame !== null) {
    cancelAnimationFrame(autoRowHeightMeasureFrame)
    autoRowHeightMeasureFrame = null
  }
})

watch(
  () => props.columns,
  () => {
    syncFilterKeys()
    applyFilters()
    void nextTick(() => {
      updateMeasuredHeaderHeights()
      if (viewportResizeObserver && headerRowRef.value) {
        viewportResizeObserver.observe(headerRowRef.value)
      }
      if (viewportResizeObserver && filterRowRef.value) {
        viewportResizeObserver.observe(filterRowRef.value)
      }
      if (viewportResizeObserver && leftPinnedHeaderRowRef.value) {
        viewportResizeObserver.observe(leftPinnedHeaderRowRef.value)
      }
      if (viewportResizeObserver && rightPinnedHeaderRowRef.value) {
        viewportResizeObserver.observe(rightPinnedHeaderRowRef.value)
      }
      if (viewportResizeObserver && leftPinnedFilterRowRef.value) {
        viewportResizeObserver.observe(leftPinnedFilterRowRef.value)
      }
      if (viewportResizeObserver && rightPinnedFilterRowRef.value) {
        viewportResizeObserver.observe(rightPinnedFilterRowRef.value)
      }
      scheduleViewportSync()
    })
  },
)

watch(
  renderedColumnsSignature,
  () => {
    void nextTick(() => {
      updateMeasuredHeaderHeights()
      if (viewportResizeObserver && leftPinnedViewportRef.value) {
        viewportResizeObserver.observe(leftPinnedViewportRef.value)
      }
      if (viewportResizeObserver && rightPinnedViewportRef.value) {
        viewportResizeObserver.observe(rightPinnedViewportRef.value)
      }
      if (viewportResizeObserver && headerRowRef.value) {
        viewportResizeObserver.observe(headerRowRef.value)
      }
      if (viewportResizeObserver && filterRowRef.value) {
        viewportResizeObserver.observe(filterRowRef.value)
      }
      if (viewportResizeObserver && leftPinnedHeaderRowRef.value) {
        viewportResizeObserver.observe(leftPinnedHeaderRowRef.value)
      }
      if (viewportResizeObserver && rightPinnedHeaderRowRef.value) {
        viewportResizeObserver.observe(rightPinnedHeaderRowRef.value)
      }
      if (viewportResizeObserver && leftPinnedFilterRowRef.value) {
        viewportResizeObserver.observe(leftPinnedFilterRowRef.value)
      }
      if (viewportResizeObserver && rightPinnedFilterRowRef.value) {
        viewportResizeObserver.observe(rightPinnedFilterRowRef.value)
      }
      scheduleViewportSync()
    })
  },
)

watch(
  () => [props.overscanRows, props.overscanColumns],
  () => {
    scheduleViewportSync()
  },
)

watch(
  () => props.rowHeight,
  (next) => {
    baseRowHeight.value = Math.max(1, next)
    if (rowHeightMode.value === "auto") {
      scheduleAutoRowHeightMeasure()
    }
    scheduleViewportSync()
  },
)

watch(
  () => props.enableFiltering,
  () => {
    if (!props.enableFiltering) {
      grid.features.filtering.clear()
    } else {
      applyFilters()
    }
    void nextTick(() => {
      updateMeasuredHeaderHeights()
      if (viewportResizeObserver && filterRowRef.value) {
        viewportResizeObserver.observe(filterRowRef.value)
      }
      if (viewportResizeObserver && leftPinnedFilterRowRef.value) {
        viewportResizeObserver.observe(leftPinnedFilterRowRef.value)
      }
      if (viewportResizeObserver && rightPinnedFilterRowRef.value) {
        viewportResizeObserver.observe(rightPinnedFilterRowRef.value)
      }
      scheduleViewportSync()
    })
  },
)

function handleMainScroll(event: Event) {
  const target = event.currentTarget as HTMLElement | null
  if (!target) {
    return
  }
  updateObservedViewportSize()
  const nextLeft = target.scrollLeft
  if (nextLeft === lastHandledScrollLeft) {
    return
  }
  lastHandledScrollLeft = nextLeft
  scheduleViewportSync()
}

function handleBodyScroll(event: Event) {
  const target = event.currentTarget as HTMLElement | null
  if (!target) {
    return
  }
  updateObservedViewportSize()
  const nextTop = target.scrollTop
  if (nextTop === lastHandledScrollTop) {
    return
  }
  lastHandledScrollTop = nextTop
  syncLinkedScroll(nextTop)
  if (rowHeightMode.value === "auto") {
    scheduleAutoRowHeightMeasure()
  }
  scheduleViewportSync()
}

function syncFilterKeys() {
  const allowedKeys = new Set(coreColumns.value.map(column => column.key))
  coreColumns.value.forEach((column) => {
    if (!(column.key in columnFilters)) {
      columnFilters[column.key] = ""
    }
  })
  Object.keys(columnFilters).forEach((key) => {
    if (!allowedKeys.has(key)) {
      delete columnFilters[key]
    }
  })
}

function applyFilters() {
  if (!props.enableFiltering || !grid.features.filtering.enabled.value) {
    if (lastAppliedFilterSignature === "__disabled__") {
      return
    }
    lastAppliedFilterSignature = "__disabled__"
    grid.features.filtering.clear()
    return
  }

  const active = Object.entries(columnFilters)
    .map(([key, value]) => [key, value.trim()] as const)
    .filter(([, value]) => value.length > 0)
    .sort(([left], [right]) => left.localeCompare(right))

  const signature = active.length
    ? active.map(([key, value]) => `${key}:${value}`).join("|")
    : "__empty__"
  if (signature === lastAppliedFilterSignature) {
    return
  }
  lastAppliedFilterSignature = signature

  if (!active.length) {
    grid.features.filtering.clear()
    return
  }

  let first = true
  active.forEach(([key, value]) => {
    grid.features.filtering.helpers.setText(key, {
      value,
      operator: "contains",
      mergeMode: first ? "replace" : "merge-and",
    })
    first = false
  })
}

function sortDirection(columnKey: string): "asc" | "desc" | null {
  if (!props.enableSorting) return null
  const entry = grid.sortState.value.find(item => item.key === columnKey)
  return entry?.direction ?? null
}

function resolveValue(row: GridRow, key: string): unknown {
  return row[key]
}

function rowData(value: unknown): GridRow {
  if (value && typeof value === "object") {
    return value as GridRow
  }
  return {}
}

function formatValue(value: unknown): string {
  if (value === null || value === undefined) return ""
  if (typeof value === "string") return value
  if (typeof value === "number" || typeof value === "boolean") return String(value)
  if (value instanceof Date) return value.toISOString()
  if (typeof value === "object") {
    try {
      return JSON.stringify(value)
    } catch {
      return "[object]"
    }
  }
  return String(value)
}

function columnStyle(width: number) {
  return {
    width: `${width}px`,
    minWidth: `${width}px`,
    maxWidth: `${width}px`,
  }
}
</script>

<style scoped>
.ui-affino-grid {
  width: 100%;
  height: 100%;
  min-height: 0;
  min-width: 0;
}

.ui-affino-grid__layout {
  width: 100%;
  height: 100%;
  min-height: 0;
  min-width: 0;
  display: flex;
  flex-direction: column;
}

.ui-affino-grid__content-shell {
  width: 100%;
  height: 100%;
  min-height: 0;
  min-width: 0;
  display: grid;
  grid-template-columns:
    var(--ui-affino-index-width, 64px)
    var(--ui-affino-left-width, 0px)
    minmax(0, 1fr)
    var(--ui-affino-right-width, 0px);
}

.ui-affino-grid__index-column {
  grid-column: 1;
  min-height: 0;
  min-width: 0;
  display: flex;
  flex-direction: column;
}

.ui-affino-grid__index-header {
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0 0.45rem;
  font-size: 0.68rem;
  text-transform: uppercase;
  letter-spacing: 0.07em;
  font-weight: 700;
  color: #475569;
  border-right: 1px solid rgba(148, 163, 184, 0.2);
  border-bottom: 1px solid rgba(148, 163, 184, 0.2);
  box-sizing: border-box;
  user-select: none;
}

.dark .ui-affino-grid__index-header {
  color: #cbd5e1;
  border-right-color: rgba(148, 163, 184, 0.22);
  border-bottom-color: rgba(148, 163, 184, 0.22);
}

.ui-affino-grid__index-filter {
  border-right: 1px solid rgba(148, 163, 184, 0.2);
  border-bottom: 1px solid rgba(148, 163, 184, 0.2);
  background: #f8fafc;
  min-height: 29px;
  box-sizing: border-box;
}

.dark .ui-affino-grid__index-filter {
  border-right-color: rgba(148, 163, 184, 0.22);
  border-bottom-color: rgba(148, 163, 184, 0.22);
  background: #111827;
}

.ui-affino-grid__main-viewport {
  grid-column: 3;
  min-height: 0;
  min-width: 0;
  overflow-x: auto;
  overflow-y: hidden;
  scrollbar-gutter: stable both-edges;
  -webkit-overflow-scrolling: touch;
  background: rgba(255, 255, 255, 0.95);
}

.dark .ui-affino-grid__main-viewport {
  background: rgba(10, 12, 18, 0.92);
}

.ui-affino-grid__main-canvas {
  width: max-content;
  min-width: 100%;
  height: 100%;
  min-height: 0;
  display: flex;
  flex-direction: column;
}

.ui-affino-grid__pinned-column {
  min-height: 0;
  min-width: 0;
  display: flex;
  flex-direction: column;
  background: rgba(255, 255, 255, 0.95);
}

.dark .ui-affino-grid__pinned-column {
  background: rgba(10, 12, 18, 0.92);
}

.ui-affino-grid__pinned-column--left {
  grid-column: 2;
  border-right: 1px solid rgba(148, 163, 184, 0.2);
}

.ui-affino-grid__pinned-column--right {
  grid-column: 4;
  border-left: 1px solid rgba(148, 163, 184, 0.2);
}

.dark .ui-affino-grid__pinned-column--left {
  border-right-color: rgba(148, 163, 184, 0.22);
}

.dark .ui-affino-grid__pinned-column--right {
  border-left-color: rgba(148, 163, 184, 0.22);
}

.ui-affino-grid__pinned-viewport {
  flex: 1;
  min-height: 0;
  overflow: hidden;
}

.ui-affino-grid__pinned-canvas {
  width: 100%;
  min-width: 100%;
}

.ui-affino-grid__index-viewport {
  flex: 1;
  min-height: 0;
  overflow: hidden;
  border-right: 1px solid rgba(148, 163, 184, 0.2);
  background: rgba(255, 255, 255, 0.95);
}

.dark .ui-affino-grid__index-viewport {
  border-right-color: rgba(148, 163, 184, 0.22);
  background: rgba(10, 12, 18, 0.92);
}

.ui-affino-grid__index-canvas {
  width: 100%;
  min-width: 100%;
}

.ui-affino-grid__index-row {
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0 0.4rem;
  font-size: 0.72rem;
  font-variant-numeric: tabular-nums;
  color: #64748b;
  border-bottom: 1px solid rgba(148, 163, 184, 0.2);
  box-sizing: border-box;
  white-space: nowrap;
  overflow: hidden;
}

.dark .ui-affino-grid__index-row {
  color: #94a3b8;
  border-bottom-color: rgba(148, 163, 184, 0.22);
}

.ui-affino-grid__index-row.is-even {
  background: rgba(241, 245, 249, 0.45);
}

.dark .ui-affino-grid__index-row.is-even {
  background: rgba(15, 23, 42, 0.45);
}

.ui-affino-grid.is-row-fixed .ui-affino-grid__index-row {
  height: var(--ui-affino-row-height, 34px);
  min-height: var(--ui-affino-row-height, 34px);
  max-height: var(--ui-affino-row-height, 34px);
}

.ui-affino-grid__viewport {
  flex: 1;
  width: 100%;
  min-width: 0;
  min-height: 0;
  overflow-y: auto;
  overflow-x: hidden;
  -webkit-overflow-scrolling: touch;
  background: rgba(255, 255, 255, 0.95);
}

.dark .ui-affino-grid__viewport {
  background: rgba(10, 12, 18, 0.92);
}

.ui-affino-grid__canvas {
  width: max-content;
  min-width: 100%;
}

.ui-affino-grid__row {
  display: flex;
  width: max-content;
  min-width: 100%;
}

.ui-affino-grid__row--pinned {
  width: 100%;
  min-width: 100%;
}

.ui-affino-grid__cell,
.ui-affino-grid__spacer {
  flex: 0 0 auto;
  border-bottom: 1px solid rgba(148, 163, 184, 0.2);
  border-right: 1px solid rgba(148, 163, 184, 0.2);
  background: transparent;
  box-sizing: border-box;
}

.dark .ui-affino-grid__cell,
.dark .ui-affino-grid__spacer {
  border-bottom-color: rgba(148, 163, 184, 0.22);
  border-right-color: rgba(148, 163, 184, 0.22);
}

.ui-affino-grid__cell {
  padding: 0.4rem 0.55rem;
  font-size: 0.8rem;
  color: #0f172a;
}

.ui-affino-grid.is-row-fixed .ui-affino-grid__row--data {
  height: var(--ui-affino-row-height, 34px);
}

.ui-affino-grid.is-row-fixed .ui-affino-grid__row--data > .ui-affino-grid__cell,
.ui-affino-grid.is-row-fixed .ui-affino-grid__row--data > .ui-affino-grid__spacer {
  height: var(--ui-affino-row-height, 34px);
  min-height: var(--ui-affino-row-height, 34px);
  max-height: var(--ui-affino-row-height, 34px);
  overflow: hidden;
  white-space: nowrap;
  vertical-align: middle;
}

.dark .ui-affino-grid__cell {
  color: #f1f5f9;
}

.ui-affino-grid__cell--header {
  position: relative;
  background: #f8fafc;
}

.ui-affino-grid__row--header .ui-affino-grid__spacer {
  background: #f8fafc;
}

.ui-affino-grid__cell--header {
  font-size: 0.68rem;
  text-transform: uppercase;
  letter-spacing: 0.07em;
  font-weight: 700;
  color: #475569;
  user-select: none;
}

.dark .ui-affino-grid__row--header .ui-affino-grid__cell,
.dark .ui-affino-grid__row--header .ui-affino-grid__spacer {
  background: #111827;
}

.dark .ui-affino-grid__cell--header {
  color: #cbd5e1;
}

.ui-affino-grid__header-content {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.4rem;
  min-width: 0;
}

.ui-affino-grid__header-label {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.ui-affino-grid__sort-indicator {
  font-size: 0.62rem;
  color: #334155;
}

.dark .ui-affino-grid__sort-indicator {
  color: #e2e8f0;
}

.ui-affino-grid__resize-handle {
  position: absolute;
  top: 0;
  right: 0;
  width: 6px;
  height: 100%;
  cursor: col-resize;
}

.ui-affino-grid__row--filter .ui-affino-grid__cell,
.ui-affino-grid__row--filter .ui-affino-grid__spacer {
  background: #f8fafc;
}

.dark .ui-affino-grid__row--filter .ui-affino-grid__cell,
.dark .ui-affino-grid__row--filter .ui-affino-grid__spacer {
  background: #111827;
}

.ui-affino-grid__filter-input {
  width: 100%;
  border: 1px solid rgba(148, 163, 184, 0.45);
  border-radius: 0.5rem;
  padding: 0.2rem 0.4rem;
  font-size: 0.72rem;
  background: #fff;
  color: #0f172a;
}

.dark .ui-affino-grid__filter-input {
  border-color: rgba(148, 163, 184, 0.35);
  background: #0b1220;
  color: #e2e8f0;
}

.ui-affino-grid__row--data.is-even .ui-affino-grid__cell {
  background: rgba(241, 245, 249, 0.45);
}

.dark .ui-affino-grid__row--data.is-even .ui-affino-grid__cell {
  background: rgba(15, 23, 42, 0.45);
}

.ui-affino-grid__row--data:hover .ui-affino-grid__cell {
  background: rgba(226, 232, 240, 0.55);
}

.dark .ui-affino-grid__row--data:hover .ui-affino-grid__cell {
  background: rgba(30, 41, 59, 0.6);
}

.ui-affino-grid__value {
  display: inline-block;
  max-width: 100%;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.ui-affino-grid__spacer-row {
  border: 0;
  padding: 0;
  width: max-content;
  min-width: 100%;
}

.ui-affino-grid__spacer-row--index {
  width: 100%;
  min-width: 100%;
}

.ui-affino-grid__spacer-row--pinned {
  width: 100%;
  min-width: 100%;
}

.ui-affino-grid__empty {
  width: max-content;
  min-width: 100%;
  text-align: center;
  padding: 1rem;
  color: #64748b;
}

.dark .ui-affino-grid__empty {
  color: #94a3b8;
}
</style>
