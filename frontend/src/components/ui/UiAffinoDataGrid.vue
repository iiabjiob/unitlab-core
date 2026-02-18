<template>
  <div
    ref="gridRootRef"
    class="ui-affino-grid"
    :class="{ 'is-row-fixed': isRowHeightFixed }"
    :style="gridStyle"
  >
    <div class="ui-affino-grid__layout">
      <div v-if="props.showControls" class="ui-affino-grid__toolbar">
        <div class="ui-affino-grid__toolbar-filters">
          <span class="ui-affino-grid__toolbar-title">Filters</span>
          <span v-if="activeFilters.length === 0" class="ui-affino-grid__toolbar-empty">No active filters</span>
          <span
            v-for="filter in activeFilters"
            :key="`filter-chip-${filter.key}`"
            class="ui-affino-grid__filter-chip"
            :title="`${filter.label}: ${filter.value}`"
          >
            {{ filter.label }}: {{ filter.value }}
          </span>
          <button
            type="button"
            class="ui-affino-grid__toolbar-button"
            :disabled="activeFilters.length === 0"
            @click="resetAllFilters"
          >
            Reset all
          </button>
          <span v-if="selectAllInProgress" class="ui-affino-grid__toolbar-stat ui-affino-grid__toolbar-stat--busy">
            Selecting…
          </span>
          <span class="ui-affino-grid__toolbar-stat">Filtered: {{ filteredRowsCount }}</span>
          <span class="ui-affino-grid__toolbar-stat">Selected: {{ selectedRowsCount }}</span>
        </div>
        <div class="ui-affino-grid__toolbar-actions">
          <button
            :ref="columnPanelFloating.triggerRef"
            type="button"
            class="ui-affino-grid__toolbar-button"
            v-bind="columnPanelTriggerProps"
          >
            {{ isColumnPanelOpen ? "Hide columns" : "Columns" }}
          </button>
        </div>
      </div>

      <Teleport
        v-if="props.showControls && columnPanelTeleportTarget"
        :to="columnPanelTeleportTarget"
      >
        <div
          v-show="isColumnPanelOpen"
          :ref="columnPanelFloating.contentRef"
          class="ui-affino-grid__column-panel ui-affino-grid__column-panel--floating"
          :style="columnPanelContentStyle"
          v-bind="columnPanelContentProps"
        >
          <div class="ui-affino-grid__column-panel-title">Column visibility and order</div>
          <div
            v-for="entry in columnManagerColumns"
            :key="`column-panel-${entry.key}`"
            class="ui-affino-grid__column-panel-row"
          >
            <label class="ui-affino-grid__column-toggle">
              <input
                type="checkbox"
                :checked="entry.visible"
                @change="event => handleColumnVisibilityChange(entry.key, event)"
              />
              <span>{{ entry.label }}</span>
            </label>
            <div class="ui-affino-grid__column-order-actions">
              <button
                type="button"
                class="ui-affino-grid__column-order-button"
                :disabled="!canMoveColumn(entry.key, -1)"
                @click="moveColumn(entry.key, -1)"
              >
                ↑
              </button>
              <button
                type="button"
                class="ui-affino-grid__column-order-button"
                :disabled="!canMoveColumn(entry.key, 1)"
                @click="moveColumn(entry.key, 1)"
              >
                ↓
              </button>
            </div>
          </div>
        </div>
      </Teleport>

      <div class="ui-affino-grid__content-shell">
        <div class="ui-affino-grid__index-column">
          <div class="ui-affino-grid__index-header" :style="indexHeaderStyle">#</div>
          <div v-if="showFilterRow" class="ui-affino-grid__index-filter" :style="indexFilterStyle"></div>

          <div ref="indexViewportRef" class="ui-affino-grid__index-viewport" @wheel="handleLinkedViewportWheel">
            <div ref="indexCanvasRef" class="ui-affino-grid__index-canvas">
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
                  :class="{ 'is-even': isEvenDisplayRow(rowNode, localIndex), 'is-hovered': isRowHovered(rowNode) }"
                  @mouseenter="setHoveredRow(rowNode)"
                  @mouseleave="clearHoveredRow(rowNode)"
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

        <div class="ui-affino-grid__select-column">
          <div class="ui-affino-grid__select-header" :style="indexHeaderStyle">
            <input
              ref="selectHeaderCheckboxRef"
              type="checkbox"
              class="ui-affino-grid__row-select-checkbox"
              :checked="allVisibleRowsSelected"
              :disabled="visibleRowSelectionKeys.length === 0"
              aria-label="Select all visible rows"
              @click.stop
              @change="handleSelectAllVisibleChange"
            />
          </div>
          <div v-if="showFilterRow" class="ui-affino-grid__select-filter" :style="indexFilterStyle"></div>

          <div ref="selectionViewportRef" class="ui-affino-grid__select-viewport" @wheel="handleLinkedViewportWheel">
            <div ref="selectionCanvasRef" class="ui-affino-grid__select-canvas">
              <template v-if="hasRenderableData">
                <div
                  v-if="topSpacerPx > 0"
                  class="ui-affino-grid__spacer-row ui-affino-grid__spacer-row--index"
                  :style="{ height: `${topSpacerPx}px` }"
                ></div>

                <div
                  v-for="(rowNode, localIndex) in visibleRowNodes"
                  :key="`sel-${String(rowNode.rowId)}`"
                  class="ui-affino-grid__select-row"
                  :class="{ 'is-even': isEvenDisplayRow(rowNode, localIndex), 'is-hovered': isRowHovered(rowNode) }"
                  @mouseenter="setHoveredRow(rowNode)"
                  @mouseleave="clearHoveredRow(rowNode)"
                >
                  <input
                    type="checkbox"
                    class="ui-affino-grid__row-select-checkbox"
                    :checked="isRowSelected(rowNode, localIndex)"
                    :aria-label="`Select row ${resolveNodeDisplayIndex(rowNode, localIndex) + 1}`"
                    @click.stop="rememberCheckboxSelectionGesture"
                    @keydown="rememberCheckboxSelectionGesture"
                    @change="event => handleRowSelectionChange(rowNode, localIndex, event)"
                  />
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
              @click.capture="handleHeaderCellClickCapture"
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
                @pointerdown.stop="armResizeClickGuard"
                @mousedown.stop="armResizeClickGuard"
                @click.stop.prevent="armResizeClickGuard"
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
                v-if="isColumnFilterable(entry.column)"
                v-model="columnFilters[entry.key]"
                type="text"
                class="ui-affino-grid__filter-input"
                placeholder="Filter"
                 @input="applyFiltersDebounced"
              />
            </div>
          </div>

          <div ref="leftPinnedViewportRef" class="ui-affino-grid__pinned-viewport" @wheel="handleLinkedViewportWheel">
            <div ref="leftPinnedCanvasRef" class="ui-affino-grid__pinned-canvas">
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
                  :class="{ 'is-even': isEvenDisplayRow(rowNode, localIndex), 'is-hovered': isRowHovered(rowNode) }"
                  @mouseenter="setHoveredRow(rowNode)"
                  @mouseleave="clearHoveredRow(rowNode)"
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
                  @click.capture="handleHeaderCellClickCapture"
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
                    @pointerdown.stop="armResizeClickGuard"
                    @mousedown.stop="armResizeClickGuard"
                    @click.stop.prevent="armResizeClickGuard"
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
                    v-if="isColumnFilterable(entry.column)"
                    v-model="columnFilters[entry.key]"
                    type="text"
                    class="ui-affino-grid__filter-input"
                    placeholder="Filter"
                     @input="applyFiltersDebounced"
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
              @wheel="handleBodyViewportWheel"
              @scroll.passive="handleBodyScroll"
            >
              <div class="ui-affino-grid__canvas">
                <template v-if="hasRenderableData">
                  <div v-if="topSpacerPx > 0" class="ui-affino-grid__spacer-row" :style="{ height: `${topSpacerPx}px` }"></div>

                  <div
                    v-for="(rowNode, localIndex) in visibleRowNodes"
                    :key="String(rowNode.rowId)"
                    class="ui-affino-grid__row ui-affino-grid__row--data"
                    :class="{ 'is-even': isEvenDisplayRow(rowNode, localIndex), 'is-hovered': isRowHovered(rowNode) }"
                    @mouseenter="setHoveredRow(rowNode)"
                    @mouseleave="clearHoveredRow(rowNode)"
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
              @click.capture="handleHeaderCellClickCapture"
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
                @pointerdown.stop="armResizeClickGuard"
                @mousedown.stop="armResizeClickGuard"
                @click.stop.prevent="armResizeClickGuard"
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
                v-if="isColumnFilterable(entry.column)"
                v-model="columnFilters[entry.key]"
                type="text"
                class="ui-affino-grid__filter-input"
                placeholder="Filter"
                @input="applyFiltersDebounced"
              />
            </div>
          </div>

          <div ref="rightPinnedViewportRef" class="ui-affino-grid__pinned-viewport" @wheel="handleLinkedViewportWheel">
            <div ref="rightPinnedCanvasRef" class="ui-affino-grid__pinned-canvas">
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
                  :class="{ 'is-even': isEvenDisplayRow(rowNode, localIndex), 'is-hovered': isRowHovered(rowNode) }"
                  @mouseenter="setHoveredRow(rowNode)"
                  @mouseleave="clearHoveredRow(rowNode)"
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

        <div
          v-show="showRightScrollbar"
          ref="rightScrollbarRef"
          class="ui-affino-grid__right-scrollbar"
          @scroll.passive="handleRightScrollbarScroll"
        >
          <div
            class="ui-affino-grid__right-scrollbar-content"
            :style="{ height: `${bodyScrollableHeightPx}px` }"
          ></div>
        </div>
      </div>

      <div
        v-show="showBottomScrollbar"
        ref="bottomScrollbarRef"
        class="ui-affino-grid__bottom-scrollbar"
        @scroll.passive="handleBottomScrollbarScroll"
      >
        <div
          class="ui-affino-grid__bottom-scrollbar-content"
          :style="{ width: `${centerScrollableWidthPx}px` }"
        ></div>
      </div>
    </div>

    <div v-if="showInitialLoadingOverlay" class="ui-affino-grid__loading-overlay" aria-live="polite" aria-busy="true">
      <div class="ui-affino-grid__loading-chip">
        <span class="ui-affino-grid__loading-spinner" aria-hidden="true"></span>
        <span>Loading table…</span>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, reactive, ref, watch } from "vue"
import type {
  DataGridColumnModelSnapshot,
  DataGridColumnModel,
  DataGridColumnStateSnapshot,
  DataGridColumnSnapshot,
  DataGridCoreServiceContext,
  DataGridRowModel,
  DataGridSettingsAdapter,
  DataGridSortState,
} from "@affino/datagrid-core"
import { createDataGridSettingsAdapter, useAffinoDataGrid, useDataGridSettingsStore } from "@affino/datagrid-vue"
import { useFloatingPopover, usePopoverController } from "@affino/popover-vue"
import {
  useDataGridColumnLayoutOrchestration,
  useDataGridViewportScrollLifecycle,
  useDataGridRowSelectionInputHandlers,
} from "@affino/datagrid-vue/advanced"
import {
  useDataGridInitialViewportRecovery,
  useDataGridLinkedPaneScrollSync,
  useDataGridManagedWheelScroll,
  useDataGridResizeClickGuard,
  useDataGridRowSelectionModel,
  setsEqual,
} from "@affino/datagrid-orchestration"

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

type PersistedFilterSnapshot = ReturnType<DataGridSettingsAdapter["getFilterSnapshot"]>

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
  showControls?: boolean
  tableId?: string
  persistState?: boolean
  datasetKey?: string
  selectedRowKeys?: readonly string[]
}>(), {
  rowHeight: 34,
  overscanRows: 8,
  overscanColumns: 2,
  enableSorting: true,
  enableFiltering: true,
  enableColumnResize: true,
  emptyText: "No data",
  rowKey: undefined,
  showControls: false,
  tableId: undefined,
  persistState: true,
  datasetKey: "",
  selectedRowKeys: undefined,
})

const emit = defineEmits<{
  (e: "row-click", payload: { row: GridRow; rowIndex: number }): void
  (e: "selection-change", payload: { rowKeys: string[] }): void
}>()

const gridRootRef = ref<HTMLElement | null>(null)
const mainViewportRef = ref<HTMLElement | null>(null)
const bottomScrollbarRef = ref<HTMLElement | null>(null)
const rightScrollbarRef = ref<HTMLElement | null>(null)
const viewportRef = ref<HTMLElement | null>(null)
const indexViewportRef = ref<HTMLElement | null>(null)
const selectionViewportRef = ref<HTMLElement | null>(null)
const indexCanvasRef = ref<HTMLElement | null>(null)
const selectionCanvasRef = ref<HTMLElement | null>(null)
const headerRowRef = ref<HTMLElement | null>(null)
const filterRowRef = ref<HTMLElement | null>(null)
const leftPinnedViewportRef = ref<HTMLElement | null>(null)
const rightPinnedViewportRef = ref<HTMLElement | null>(null)
const leftPinnedCanvasRef = ref<HTMLElement | null>(null)
const rightPinnedCanvasRef = ref<HTMLElement | null>(null)
const leftPinnedHeaderRowRef = ref<HTMLElement | null>(null)
const rightPinnedHeaderRowRef = ref<HTMLElement | null>(null)
const leftPinnedFilterRowRef = ref<HTMLElement | null>(null)
const rightPinnedFilterRowRef = ref<HTMLElement | null>(null)
const PINNED_INDEX_COLUMN_WIDTH = 64
const PINNED_SELECTION_COLUMN_WIDTH = 42

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
const observedBodyScrollHeight = ref(0)
const observedBodyClientHeight = ref(0)
const rowModelRevision = ref(0)
const measuredHeaderHeight = ref<number | null>(null)
const measuredFilterHeight = ref<number | null>(null)
const hoveredRowId = ref<string | null>(null)
const selectHeaderCheckboxRef = ref<HTMLInputElement | null>(null)
const checkboxSelectionAnchorIndex = ref<number | null>(null)
const lastCheckboxGestureShift = ref(false)
const localSelectedRowKeySet = ref<Set<string>>(new Set())
const selectAllInProgress = ref(false)

let viewportRowModel: ViewportRowModelBridge | null = null
let viewportColumnModel: ViewportColumnModelBridge | null = null
let explicitRowRange: WindowRange | null = null
let lastAppliedRowRange: WindowRange | null = null
let cachedColumnWindowSource: readonly DataGridColumnSnapshot[] | null = null
let cachedColumnWindowPrefix: number[] = []
let lastAppliedFilterSignature: string | null = null
let lastVirtualWindowSnapshot: VirtualWindowSnapshot | null = null
let settingsPersistTimer: ReturnType<typeof setTimeout> | null = null
let filterDebounceTimer: ReturnType<typeof setTimeout> | null = null
let restoringSettings = false
const SETTINGS_PERSIST_DELAY_MS = 120
const FILTER_APPLY_DEBOUNCE_MS = 250
const dataGridSettingsAdapter = createDataGridSettingsAdapter(useDataGridSettingsStore())
const columnPanelPopover = usePopoverController({
  role: "dialog",
  closeOnEscape: true,
  closeOnInteractOutside: true,
})
const columnPanelFloating = useFloatingPopover(columnPanelPopover, {
  strategy: "fixed",
  placement: "bottom",
  align: "end",
  gutter: 8,
  viewportPadding: 8,
  zIndex: 1200,
})
const isColumnPanelOpen = computed(() => columnPanelPopover.state.value.open)
const columnPanelTeleportTarget = computed(() => columnPanelFloating.teleportTarget.value)
const columnPanelContentStyle = computed(() => columnPanelFloating.contentStyle.value)
const columnPanelTriggerProps = computed(() => columnPanelPopover.getTriggerProps({
  type: "button",
  role: "dialog",
}))
const columnPanelContentProps = computed(() => columnPanelPopover.getContentProps({
  role: "dialog",
  tabIndex: -1,
}))

const linkedPaneScrollSync = useDataGridLinkedPaneScrollSync({
  resolveSourceScrollTop: () => viewportRef.value?.scrollTop ?? 0,
  mode: "direct-transform",
  resolvePaneElements: () => [
    indexCanvasRef.value,
    selectionCanvasRef.value,
    leftPinnedCanvasRef.value,
    rightPinnedCanvasRef.value,
  ],
})

const managedWheelScroll = useDataGridManagedWheelScroll({
  resolveWheelMode: () => "managed",
  resolveWheelAxisLockMode: () => "dominant",
  resolvePreventDefaultWhenHandled: () => true,
  resolveBodyViewport: () => viewportRef.value,
  resolveMainViewport: () => {
    const mainViewport = mainViewportRef.value
    if (!mainViewport) {
      return null
    }
    return {
      scrollLeft: mainViewport.scrollLeft,
      scrollWidth: mainViewport.scrollWidth,
      clientWidth: mainViewport.clientWidth,
    }
  },
  setHandledScrollTop: (nextTop) => {
    const bodyViewport = viewportRef.value
    if (!bodyViewport) {
      return
    }
    bodyViewport.scrollTop = nextTop
    lastHandledScrollTop = nextTop
    syncLinkedScroll(nextTop)
    scheduleLinkedScrollSyncLoop()
    updateObservedViewportSize()
    scheduleViewportSync()
  },
  setHandledScrollLeft: (nextLeft) => {
    const mainViewport = mainViewportRef.value
    if (!mainViewport) {
      return
    }
    mainViewport.scrollLeft = nextLeft
    lastHandledScrollLeft = nextLeft
    updateObservedViewportSize()
    scheduleViewportSync()
  },
})

const mainViewportScrollLifecycle = useDataGridViewportScrollLifecycle({
  isContextMenuVisible: () => false,
  closeContextMenu: () => {},
  resolveScrollTop: () => 0,
  resolveScrollLeft: () => lastHandledScrollLeft,
  setScrollTop: () => {},
  setScrollLeft: (nextLeft) => {
    updateObservedViewportSize()
    if (nextLeft === lastHandledScrollLeft) {
      return
    }
    lastHandledScrollLeft = nextLeft
    scheduleViewportSync()
  },
  hasInlineEditor: () => false,
  commitInlineEdit: () => {},
})

const bodyViewportScrollLifecycle = useDataGridViewportScrollLifecycle({
  isContextMenuVisible: () => false,
  closeContextMenu: () => {},
  resolveScrollTop: () => lastHandledScrollTop,
  resolveScrollLeft: () => 0,
  setScrollTop: (nextTop) => {
    updateObservedViewportSize()
    if (nextTop === lastHandledScrollTop) {
      return
    }
    lastHandledScrollTop = nextTop
    syncLinkedScroll(nextTop)
    scheduleLinkedScrollSyncLoop()
    if (rowHeightMode.value === "auto") {
      scheduleAutoRowHeightMeasure()
    }
    scheduleViewportSync()
  },
  setScrollLeft: () => {},
  hasInlineEditor: () => false,
  commitInlineEdit: () => {},
})

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
    const previousAppliedRange = lastAppliedRowRange
      ? { start: lastAppliedRowRange.start, end: lastAppliedRowRange.end }
      : null

    if (changed) {
      viewportMetrics.scrollTop = normalizedTop
      viewportMetrics.scrollLeft = normalizedLeft
      viewportMetrics.viewportHeight = normalizedHeight
      viewportMetrics.viewportWidth = normalizedWidth
      viewportMetrics.rowHeight = normalizedRowHeight
      viewportMetrics.overscanRows = normalizedOverscanRows
      viewportMetrics.overscanColumns = normalizedOverscanColumns
      explicitRowRange = null
    }

    const range = applyRowRangeToModel()
    const rangeChanged = !isSameRange(previousAppliedRange, range)
    if (!changed && !rangeChanged) {
      return null
    }
    return range
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
    visibility: true,
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
  "--ui-affino-select-width": `${PINNED_SELECTION_COLUMN_WIDTH}px`,
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

const visibleRowSelectionKeys = computed(() => {
  const total = totalRows.value
  if (total <= 0) {
    return [] as string[]
  }
  const allVisibleRows = grid.rowModel.getRowsInRange({ start: 0, end: total - 1 })
  return allVisibleRows.map((rowNode, index) => resolveSelectionKeyFromNode(rowNode, index))
})

const selectedRowKeySet = computed(() => localSelectedRowKeySet.value)

const selectedRowsCount = computed(() => selectedRowKeySet.value.size)
const filteredRowsCount = computed(() => totalRows.value)

const selectedVisibleRowsCount = computed(() => (
  visibleRowSelectionKeys.value.reduce((count, rowKey) => (
    count + (selectedRowKeySet.value.has(rowKey) ? 1 : 0)
  ), 0)
))

const allVisibleRowsSelected = computed(() => (
  visibleRowSelectionKeys.value.length > 0 && selectedVisibleRowsCount.value === visibleRowSelectionKeys.value.length
))

const hasPartialVisibleSelection = computed(() => (
  selectedVisibleRowsCount.value > 0 && !allVisibleRowsSelected.value
))

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

const persistedTableId = computed(() => {
  if (!props.persistState) {
    return null
  }
  const value = String(props.tableId ?? "").trim()
  return value.length > 0 ? value : null
})
const persistedDatasetKey = computed(() => {
  const value = String(props.datasetKey ?? "").trim()
  return value.length > 0 ? value : "__default__"
})
const persistedHydrationSignature = computed(() => (
  `${persistedTableId.value ?? ""}|${persistedDatasetKey.value}|${coreColumns.value.map(column => column.key).join("|")}`
))

const columnLabelByKey = computed(() => {
  const entries = coreColumns.value.map(column => [column.key, column.label ?? column.key] as const)
  return new Map(entries)
})

const activeFilters = computed(() => (
  Object.entries(columnFilters)
    .map(([key, rawValue]) => ({
      key,
      label: columnLabelByKey.value.get(key) ?? key,
      value: rawValue.trim(),
    }))
    .filter(item => item.value.length > 0)
    .sort((left, right) => left.label.localeCompare(right.label))
))

const columnManagerColumns = computed(() => (
  grid.columnState.snapshot.value.columns.map(column => ({
    key: column.key,
    label: column.column.label ?? column.key,
    visible: column.visible,
  }))
))

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

function isColumnFilterable(column: GridColumn): boolean {
  return column.meta?.filterable !== false
}

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

const centerScrollableWidthPx = computed(() => {
  const prefix = centerPrefix.value
  return Math.max(0, prefix[prefix.length - 1] ?? 0)
})

const showBottomScrollbar = computed(() => {
  const viewportWidth = Math.max(
    0,
    observedViewportWidth.value
      ?? mainViewportRef.value?.clientWidth
      ?? 0,
  )
  return centerScrollableWidthPx.value > viewportWidth + 1
})

const bodyScrollableHeightPx = computed(() => Math.max(0, observedBodyScrollHeight.value))

const showRightScrollbar = computed(() => (
  observedBodyScrollHeight.value > observedBodyClientHeight.value + 1
))

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

function resolveBootstrapRowRange(total: number): WindowRange {
  if (total <= 0) {
    return { start: 0, end: -1 }
  }

  const bodyViewport = viewportRef.value
  const mainViewport = mainViewportRef.value
  const measuredHeight = Math.max(
    0,
    bodyViewport?.clientHeight ?? 0,
    mainViewport?.clientHeight ?? 0,
    observedViewportHeight.value ?? 0,
  )
  const rowHeight = Math.max(1, rowHeightPx.value)
  const visibleCountFromHeight = measuredHeight > 0
    ? Math.max(1, Math.ceil(measuredHeight / rowHeight))
    : 16
  const overscan = Math.max(0, viewportMetrics.overscanRows)
  const end = Math.min(total - 1, visibleCountFromHeight + overscan * 2 - 1)
  return { start: 0, end }
}

function forceBootstrapViewportRange() {
  const total = totalRows.value
  if (total <= 1) {
    return
  }
  const bootstrapRange = resolveBootstrapRowRange(total)
  if (bootstrapRange.end < bootstrapRange.start) {
    return
  }
  viewportService.setViewportRange(bootstrapRange)
  grid.syncRowsInRange(bootstrapRange)
}

const visibleRowNodes = computed(() => {
  void props.rows
  void rowModelRevision.value
  const total = totalRows.value
  if (total === 0) return []

  const { start, end } = visibleRowRange.value
  if (end >= start) {
    const nodes = grid.rowModel.getRowsInRange({ start, end })
    if (nodes.length > 1 || total <= 1) {
      return nodes
    }

    const bootstrapRange = resolveBootstrapRowRange(total)
    if (bootstrapRange.end >= bootstrapRange.start) {
      const bootstrapNodes = grid.rowModel.getRowsInRange(bootstrapRange)
      if (bootstrapNodes.length > nodes.length) {
        return bootstrapNodes
      }
    }
    return nodes
  }

  const bootstrapRange = resolveBootstrapRowRange(total)
  if (bootstrapRange.end < bootstrapRange.start) {
    return []
  }
  return grid.rowModel.getRowsInRange(bootstrapRange)
})

function resolveNodeDisplayIndex(rowNode: unknown, localIndex: number): number {
  const fromNode = Number((rowNode as { displayIndex?: number })?.displayIndex)
  if (Number.isFinite(fromNode) && fromNode >= 0) {
    return Math.trunc(fromNode)
  }
  return Math.max(0, visibleRowRange.value.start + Math.max(0, Math.trunc(localIndex)))
}

function isEvenDisplayRow(rowNode: unknown, localIndex: number): boolean {
  return resolveNodeDisplayIndex(rowNode, localIndex) % 2 === 1
}

function resolveSelectionKeyFromNode(rowNode: unknown, fallbackIndex: number): string {
  const row = rowData((rowNode as { data?: unknown })?.data)
  const fromNode = Number((rowNode as { displayIndex?: number })?.displayIndex)
  const displayIndex = Number.isFinite(fromNode) && fromNode >= 0
    ? Math.trunc(fromNode)
    : Math.max(0, Math.trunc(fallbackIndex))
  return grid.bindings.getRowKey(row, displayIndex)
}

function resolveRowSelectionKey(rowNode: unknown, localIndex: number): string {
  return resolveSelectionKeyFromNode(rowNode, resolveNodeDisplayIndex(rowNode, localIndex))
}

function normalizeExternalSelectedRowKeys(value: readonly string[] | undefined): Set<string> {
  if (!Array.isArray(value) || value.length === 0) {
    return new Set<string>()
  }
  return new Set(
    value
      .map(item => String(item ?? "").trim())
      .filter(item => item.length > 0),
  )
}

function areStringSetsEqual(left: Set<string>, right: Set<string>): boolean {
  return setsEqual(left, right)
}

function isRowSelected(rowNode: unknown, localIndex: number): boolean {
  const rowKey = resolveRowSelectionKey(rowNode, localIndex)
  return selectedRowKeySet.value.has(rowKey)
}

function resolveAllFilteredRowNodes(): unknown[] {
  const total = totalRows.value
  if (total <= 0) {
    return []
  }
  return [...grid.rowModel.getRowsInRange({ start: 0, end: total - 1 })]
}

function setSelectionInFilteredRange(startIndex: number, endIndex: number, selected: boolean) {
  rowSelectionModel.setAnchorIndex(startIndex)
  rowSelectionModel.applyShiftRange(endIndex, selected)
  checkboxSelectionAnchorIndex.value = rowSelectionModel.getAnchorIndex()
}

function rememberCheckboxSelectionGesture(event: Event) {
  lastCheckboxGestureShift.value = Boolean((event as MouseEvent).shiftKey || (event as KeyboardEvent).shiftKey)
}

function handleRowSelectionChange(rowNode: unknown, localIndex: number, event: Event) {
  const rowKey = resolveRowSelectionKey(rowNode, localIndex)
  const target = event.target as HTMLInputElement | null
  const shouldSelect = Boolean(target?.checked)
  const currentIndex = resolveNodeDisplayIndex(rowNode, localIndex)
  const shiftPressed = lastCheckboxGestureShift.value
    || Boolean((event as MouseEvent).shiftKey || (event as KeyboardEvent).shiftKey)
  lastCheckboxGestureShift.value = false
  const anchorIndex = checkboxSelectionAnchorIndex.value

  if (shiftPressed && anchorIndex !== null) {
    setSelectionInFilteredRange(anchorIndex, currentIndex, shouldSelect)
    return
  }

  rowSelectionModel.toggleRowAtFilteredIndex(currentIndex, shouldSelect, { shiftKey: shiftPressed })
  checkboxSelectionAnchorIndex.value = rowSelectionModel.getAnchorIndex()
}

async function applySelectAllVisibleSelection(shouldSelect: boolean) {
  checkboxSelectionAnchorIndex.value = null
  lastCheckboxGestureShift.value = false
  if (shouldSelect) {
    selectAllInProgress.value = true
    try {
      await nextTick()
      await new Promise<void>((resolve) => {
        if (typeof requestAnimationFrame === "function") {
          requestAnimationFrame(() => resolve())
          return
        }
        setTimeout(() => resolve(), 0)
      })
      rowSelectionModel.toggleSelectAllFiltered(true)
      await nextTick()
    } finally {
      selectAllInProgress.value = false
    }
    return
  }
  rowSelectionModel.toggleSelectAllFiltered(false)
  selectAllInProgress.value = false
}

function handleSelectAllVisibleChange(event: Event) {
  rowSelectionInputHandlers.onSelectAllChange(event)
}

function pruneSelectionToCurrentRows() {
  if (selectedRowKeySet.value.size === 0) {
    return
  }
  rowSelectionModel.reconcileWithRows(props.rows)
}

const rowSelectionModel = useDataGridRowSelectionModel<string>({
  resolveFilteredRows: () => visibleRowSelectionKeys.value,
  resolveRowId: (rowId: string) => rowId,
  resolveAllRows: () => props.rows.map((row, index) => grid.bindings.getRowKey(rowData(row), index)),
  initialSelection: localSelectedRowKeySet.value,
  onSelectionChange: (nextSelection: ReadonlySet<string>) => {
    localSelectedRowKeySet.value = new Set(nextSelection)
  },
})

const rowSelectionInputHandlers = useDataGridRowSelectionInputHandlers({
  toggleSelectAllVisible: (checked) => {
    void applySelectAllVisibleSelection(checked)
  },
  toggleRowSelection: (rowId, checked) => {
    rowSelectionModel.toggleRowById(rowId, checked)
    checkboxSelectionAnchorIndex.value = rowSelectionModel.getAnchorIndex()
  },
})

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
const tableReadyOnce = ref(false)

const showInitialLoadingOverlay = computed(() => (
  !tableReadyOnce.value && totalRows.value > 1 && renderedRowsCount.value <= 1
))

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
let hoverClearTimer: ReturnType<typeof setTimeout> | null = null
let lastHandledScrollTop = Number.NaN
let lastHandledScrollLeft = Number.NaN
const MAX_INITIAL_VIEWPORT_RECOVERY_ATTEMPTS = 12

const resizeClickGuard = useDataGridResizeClickGuard({
  guardDurationMs: 140,
})

function armResizeClickGuard() {
  resizeClickGuard.armResizeGuard()
  schedulePersistTableSettings()
}

function handleHeaderCellClickCapture(event: MouseEvent) {
  resizeClickGuard.onHeaderClickCapture(event)
}

function resolveRowHoverKey(rowNode: unknown): string | null {
  const candidate = (rowNode as { rowId?: unknown })?.rowId
  if (candidate === null || candidate === undefined) {
    return null
  }
  return String(candidate)
}

function setHoveredRow(rowNode: unknown) {
  const key = resolveRowHoverKey(rowNode)
  if (!key) return
  if (hoverClearTimer !== null) {
    clearTimeout(hoverClearTimer)
    hoverClearTimer = null
  }
  hoveredRowId.value = key
}

function clearHoveredRow(rowNode: unknown) {
  const key = resolveRowHoverKey(rowNode)
  if (!key || hoveredRowId.value !== key) {
    return
  }
  if (hoverClearTimer !== null) {
    clearTimeout(hoverClearTimer)
  }
  hoverClearTimer = setTimeout(() => {
    if (hoveredRowId.value === key) {
      hoveredRowId.value = null
    }
    hoverClearTimer = null
  }, 0)
}

function isRowHovered(rowNode: unknown): boolean {
  const key = resolveRowHoverKey(rowNode)
  return !!key && hoveredRowId.value === key
}

function updateObservedViewportSize() {
  const mainViewport = mainViewportRef.value
  const bodyViewport = viewportRef.value
  const bottomScrollbar = bottomScrollbarRef.value
  const rightScrollbar = rightScrollbarRef.value

  if (mainViewport) {
    const width = Math.max(0, mainViewport.clientWidth)
    observedViewportWidth.value = width > 0 ? width : null
  }
  if (bodyViewport) {
    const height = Math.max(0, bodyViewport.clientHeight)
    observedViewportHeight.value = height > 0 ? height : null
    observedBodyClientHeight.value = height
    observedBodyScrollHeight.value = Math.max(0, bodyViewport.scrollHeight)
  }
  if (mainViewport && bottomScrollbar && bottomScrollbar.scrollLeft !== mainViewport.scrollLeft) {
    bottomScrollbar.scrollLeft = mainViewport.scrollLeft
  }
  if (bodyViewport && rightScrollbar && rightScrollbar.scrollTop !== bodyViewport.scrollTop) {
    rightScrollbar.scrollTop = bodyViewport.scrollTop
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
  linkedPaneScrollSync.syncNow(scrollTop)
}

function cancelLinkedScrollSyncLoop() {
  linkedPaneScrollSync.cancelSyncLoop()
}

function scheduleLinkedScrollSyncLoop() {
  linkedPaneScrollSync.scheduleSyncLoop()
}

function handleLinkedViewportWheel(event: WheelEvent) {
  managedWheelScroll.onBodyViewportWheel(event)
}

function handleBodyViewportWheel(event: WheelEvent) {
  managedWheelScroll.onBodyViewportWheel(event)
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
  scheduleLinkedScrollSyncLoop()
  const liveBodyHeight = Math.max(0, bodyViewport.clientHeight)
  const liveMainHeight = Math.max(0, mainViewport.clientHeight)
  const liveShellHeight = Math.max(0, mainViewport.parentElement?.clientHeight ?? 0)
  const liveHeight = Math.max(liveBodyHeight, liveMainHeight, liveShellHeight)
  const liveWidth = Math.max(0, mainViewport.clientWidth)
  const viewportHeight = Math.max(
    0,
    observedViewportHeight.value ?? 0,
    liveHeight,
  )
  const viewportWidth = Math.max(
    0,
    observedViewportWidth.value ?? 0,
    liveWidth,
  )
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

function cancelInitialViewportRecovery() {
  initialViewportRecovery.cancelRecovery()
}

const initialViewportRecovery = useDataGridInitialViewportRecovery({
  maxAttempts: MAX_INITIAL_VIEWPORT_RECOVERY_ATTEMPTS,
  resolveShouldRecover: () => {
    const total = totalRows.value
    const range = visibleRowRange.value
    const rendered = range.end >= range.start ? (range.end - range.start + 1) : 0
    return total > 1 && rendered <= 1
  },
  runRecoveryStep: () => {
    const bodyViewport = viewportRef.value
    const measuredHeight = bodyViewport ? Math.max(0, bodyViewport.clientHeight) : 0
    const _likelyMeasured = measuredHeight >= Math.max(rowHeightPx.value * 2, 48)

    updateObservedViewportSize()
    syncViewportMetrics()
    void _likelyMeasured
  },
})

function scheduleInitialViewportRecovery(reset = false) {
  initialViewportRecovery.scheduleRecovery(reset)
}

onMounted(() => {
  gridRootRef.value?.style.setProperty("--ui-affino-linked-scroll-top", "0px")
  const bodyViewport = viewportRef.value
  const mainViewport = mainViewportRef.value
  if (bodyViewport || mainViewport) {
    updateObservedViewportSize()
    updateMeasuredHeaderHeights()
    lastHandledScrollTop = bodyViewport?.scrollTop ?? 0
    lastHandledScrollLeft = mainViewport?.scrollLeft ?? 0
    syncLinkedScroll(lastHandledScrollTop)
    scheduleLinkedScrollSyncLoop()
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
    if (selectionViewportRef.value) {
      viewportResizeObserver.observe(selectionViewportRef.value)
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
  restorePersistedTableSettings()
  applyFilters()
  if (rowHeightMode.value === "auto") {
    scheduleAutoRowHeightMeasure()
  }
  scheduleViewportSync()
  scheduleInitialViewportRecovery(true)
  requestAnimationFrame(() => {
    updateObservedViewportSize()
    updateMeasuredHeaderHeights()
    scheduleViewportSync()
    scheduleInitialViewportRecovery(false)
  })
})

onBeforeUnmount(() => {
  managedWheelScroll.reset()
  linkedPaneScrollSync.reset()
  resizeClickGuard.dispose()
  gridRootRef.value?.style.removeProperty("--ui-affino-linked-scroll-top")
  indexCanvasRef.value?.style.removeProperty("transform")
  selectionCanvasRef.value?.style.removeProperty("transform")
  leftPinnedCanvasRef.value?.style.removeProperty("transform")
  rightPinnedCanvasRef.value?.style.removeProperty("transform")
  columnPanelPopover.dispose()
  persistTableSettingsNow()
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
  cancelLinkedScrollSyncLoop()
  if (autoRowHeightMeasureFrame !== null) {
    cancelAnimationFrame(autoRowHeightMeasureFrame)
    autoRowHeightMeasureFrame = null
  }
  cancelInitialViewportRecovery()
  if (settingsPersistTimer !== null) {
    clearTimeout(settingsPersistTimer)
    settingsPersistTimer = null
  }
    if (filterDebounceTimer !== null) {
      clearTimeout(filterDebounceTimer)
      filterDebounceTimer = null
    }
  if (hoverClearTimer !== null) {
    clearTimeout(hoverClearTimer)
    hoverClearTimer = null
  }
  hoveredRowId.value = null
})

watch(
  persistedHydrationSignature,
  () => {
    restorePersistedTableSettings()
  },
  { immediate: true },
)

watch(
  () => grid.columnState.snapshot.value,
  () => {
    schedulePersistTableSettings()
  },
  { deep: true },
)

watch(
  () => grid.sortState.value,
  () => {
    schedulePersistTableSettings()
  },
  { deep: true },
)

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
      restorePersistedTableSettings()
      scheduleViewportSync()
    })
  },
)

watch(
  () => props.rows.length,
  (next, prev) => {
    if (next > 0 && next !== prev) {
      tableReadyOnce.value = false
    }
    pruneSelectionToCurrentRows()
    void nextTick(() => {
      updateObservedViewportSize()
      scheduleViewportSync()
      scheduleInitialViewportRecovery(true)
    })
  },
  { immediate: true },
)

watch(
  () => props.datasetKey,
  () => {
    rowSelectionModel.clearSelection()
    checkboxSelectionAnchorIndex.value = null
    lastCheckboxGestureShift.value = false
  },
)

watch(
  () => props.selectedRowKeys,
  (externalRowKeys) => {
    if (!Array.isArray(externalRowKeys)) {
      return
    }
    const normalized = normalizeExternalSelectedRowKeys(externalRowKeys)
    if (areStringSetsEqual(localSelectedRowKeySet.value, normalized)) {
      return
    }
    rowSelectionModel.replaceSelection(normalized)
    if (normalized.size === 0) {
      rowSelectionModel.setAnchorIndex(null)
      checkboxSelectionAnchorIndex.value = null
      lastCheckboxGestureShift.value = false
    }
  },
  { immediate: true },
)

watch(
  rowModelRevision,
  () => {
    const total = totalRows.value
    const range = visibleRowRange.value
    const rendered = range.end >= range.start ? (range.end - range.start + 1) : 0
    if (total > 1 && rendered <= 1) {
      forceBootstrapViewportRange()
      scheduleViewportSync()
      scheduleInitialViewportRecovery(false)
    }
  },
)

watch(
  () => [totalRows.value, renderedRowsCount.value] as const,
  ([total, rendered]) => {
    if (total <= 1 || rendered > 1) {
      tableReadyOnce.value = true
    }
  },
  { immediate: true },
)

watch(
  renderedColumnsSignature,
  () => {
    void nextTick(() => {
      updateMeasuredHeaderHeights()
      if (viewportResizeObserver && leftPinnedViewportRef.value) {
        viewportResizeObserver.observe(leftPinnedViewportRef.value)
      }
      if (viewportResizeObserver && selectionViewportRef.value) {
        viewportResizeObserver.observe(selectionViewportRef.value)
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

watch(
  () => props.showControls,
  (showControls) => {
    if (showControls) {
      return
    }
    columnPanelPopover.close("programmatic")
  },
)

watch(
  selectedRowKeySet,
  (rowKeys) => {
    emit("selection-change", { rowKeys: Array.from(rowKeys) })
  },
  { immediate: true },
)

watch(
  hasPartialVisibleSelection,
  (partial) => {
    if (!selectHeaderCheckboxRef.value) {
      return
    }
    selectHeaderCheckboxRef.value.indeterminate = partial
  },
  { immediate: true },
)

watch(
  () => visibleRowSelectionKeys.value.length,
  (length) => {
    if (length <= 0) {
      rowSelectionModel.setAnchorIndex(null)
      checkboxSelectionAnchorIndex.value = null
      return
    }
    if (checkboxSelectionAnchorIndex.value !== null && checkboxSelectionAnchorIndex.value >= length) {
      rowSelectionModel.setAnchorIndex(null)
      checkboxSelectionAnchorIndex.value = null
    }
  },
  { immediate: true },
)

watch(
  [
    () => leftPinnedColumns.value.length,
    () => rightPinnedColumns.value.length,
  ],
  () => {
    const currentTop = viewportRef.value?.scrollTop ?? 0
    linkedPaneScrollSync.reset()
    syncLinkedScroll(currentTop)
  },
  { flush: "post" },
)

function handleMainScroll(event: Event) {
  const mainViewport = event.target as HTMLElement | null
  const bottomScrollbar = bottomScrollbarRef.value
  if (mainViewport && bottomScrollbar && bottomScrollbar.scrollLeft !== mainViewport.scrollLeft) {
    bottomScrollbar.scrollLeft = mainViewport.scrollLeft
  }
  mainViewportScrollLifecycle.onViewportScroll(event)
}

function handleBottomScrollbarScroll(event: Event) {
  const bottomScrollbar = event.target as HTMLElement | null
  const mainViewport = mainViewportRef.value
  if (!bottomScrollbar || !mainViewport) {
    return
  }
  if (mainViewport.scrollLeft !== bottomScrollbar.scrollLeft) {
    mainViewport.scrollLeft = bottomScrollbar.scrollLeft
  }
}

function handleRightScrollbarScroll(event: Event) {
  const rightScrollbar = event.target as HTMLElement | null
  const bodyViewport = viewportRef.value
  if (!rightScrollbar || !bodyViewport) {
    return
  }
  if (bodyViewport.scrollTop !== rightScrollbar.scrollTop) {
    bodyViewport.scrollTop = rightScrollbar.scrollTop
  }
}

function handleBodyScroll(event: Event) {
  const bodyViewport = event.target as HTMLElement | null
  const rightScrollbar = rightScrollbarRef.value
  if (bodyViewport && rightScrollbar && rightScrollbar.scrollTop !== bodyViewport.scrollTop) {
    rightScrollbar.scrollTop = bodyViewport.scrollTop
  }
  bodyViewportScrollLifecycle.onViewportScroll(event)
}

function handleColumnVisibilityChange(columnKey: string, event: Event) {
  const target = event.target as HTMLInputElement | null
  const nextVisible = Boolean(target?.checked)
  grid.columnState.setVisibility(columnKey, nextVisible)
  persistTableSettingsNow()
}

function canMoveColumn(columnKey: string, direction: -1 | 1): boolean {
  const order = grid.columnState.snapshot.value.order
  const index = order.indexOf(columnKey)
  if (index < 0) return false
  const targetIndex = index + direction
  return targetIndex >= 0 && targetIndex < order.length
}

function moveColumn(columnKey: string, direction: -1 | 1) {
  const order = [...grid.columnState.snapshot.value.order]
  const fromIndex = order.indexOf(columnKey)
  if (fromIndex < 0) return
  const toIndex = fromIndex + direction
  if (toIndex < 0 || toIndex >= order.length) return
  const [column] = order.splice(fromIndex, 1)
  if (!column) return
  order.splice(toIndex, 0, column)
  grid.columnState.setOrder(order)
  persistTableSettingsNow()
}

function resetAllFilters() {
  Object.keys(columnFilters).forEach((key) => {
    columnFilters[key] = ""
  })
  applyFilters()
}

function captureColumnStateSnapshot(snapshot: DataGridColumnModelSnapshot): DataGridColumnStateSnapshot {
  const visibility: Record<string, boolean> = {}
  const widths: Record<string, number> = {}
  const pinning: Record<string, "left" | "right" | "none"> = {}
  snapshot.columns.forEach((column) => {
    visibility[column.key] = column.visible
    pinning[column.key] = column.pin
    if (typeof column.width === "number" && Number.isFinite(column.width)) {
      widths[column.key] = Math.max(0, Math.trunc(column.width))
    }
  })
  return {
    order: [...snapshot.order],
    visibility,
    widths,
    pinning,
  }
}

function applyPersistedColumnState(state: DataGridColumnStateSnapshot) {
  const currentOrder = grid.columnState.snapshot.value.order
  if (state.order.length > 0) {
    const knownKeys = new Set(currentOrder)
    const nextOrder = state.order.filter(columnKey => knownKeys.has(columnKey))
    const nextOrderSet = new Set(nextOrder)
    currentOrder.forEach((columnKey) => {
      if (!nextOrderSet.has(columnKey)) {
        nextOrder.push(columnKey)
        nextOrderSet.add(columnKey)
      }
    })
    if (nextOrder.length > 0) {
      grid.columnState.setOrder(nextOrder)
    }
  }
  Object.entries(state.visibility).forEach(([columnKey, visible]) => {
    grid.columnState.setVisibility(columnKey, Boolean(visible))
  })
  Object.entries(state.widths).forEach(([columnKey, width]) => {
    if (!Number.isFinite(width)) return
    grid.columnState.setWidth(columnKey, Math.max(0, Math.trunc(width)))
  })
  Object.entries(state.pinning).forEach(([columnKey, pin]) => {
    const normalizedPin = pin === "left" || pin === "right" ? pin : "none"
    grid.columnState.setPin(columnKey, normalizedPin)
  })
}

function normalizePersistedSortState(state: DataGridSortState[] | undefined): DataGridSortState[] {
  if (!Array.isArray(state)) {
    return []
  }
  return state
    .map((item): DataGridSortState => {
      const direction: DataGridSortState["direction"] = item?.direction === "desc" ? "desc" : "asc"
      return {
        key: String(item?.key ?? "").trim(),
        direction,
      }
    })
    .filter(item => item.key.length > 0)
}

function buildFilterSnapshotFromInputs(): PersistedFilterSnapshot {
  const columnFiltersSnapshot: Record<string, string[]> = {}
  Object.entries(columnFilters).forEach(([key, value]) => {
    const trimmed = value.trim()
    if (!trimmed.length) return
    columnFiltersSnapshot[key] = [trimmed]
  })
  if (!Object.keys(columnFiltersSnapshot).length) {
    return null
  }
  return {
    columnFilters: columnFiltersSnapshot,
    advancedFilters: {},
  }
}

function applyPersistedFilterSnapshot(snapshot: PersistedFilterSnapshot) {
  syncFilterKeys()
  Object.keys(columnFilters).forEach((key) => {
    columnFilters[key] = ""
  })
  if (!snapshot) {
    return
  }
  Object.entries(snapshot.columnFilters ?? {}).forEach(([key, values]) => {
    if (!(key in columnFilters) || !Array.isArray(values) || values.length === 0) {
      return
    }
    const first = values.find(value => String(value ?? "").trim().length > 0)
    if (first === undefined || first === null) {
      return
    }
    columnFilters[key] = String(first)
  })
}

function persistTableSettingsNow() {
  const tableId = persistedTableId.value
  if (!tableId || restoringSettings) {
    return
  }
  const columnState = captureColumnStateSnapshot(grid.columnState.snapshot.value)
  dataGridSettingsAdapter.setColumnState(
    tableId,
    columnState,
  )
  writePersistedColumnWidths(tableId, persistedDatasetKey.value, columnState.widths)
  dataGridSettingsAdapter.setSortState(
    tableId,
    grid.sortState.value.map(item => ({
      key: item.key,
      field: item.key,
      direction: item.direction,
    })),
  )
  dataGridSettingsAdapter.setFilterSnapshot(
    tableId,
    buildFilterSnapshotFromInputs(),
  )
}

function resolveDatasetStorageKey(tableId: string): string {
  return `affino-datagrid-dataset::${tableId}`
}

function resolveColumnWidthsStorageKey(tableId: string, datasetKey: string): string {
  return `affino-datagrid-widths::${tableId}::${datasetKey}`
}

function readPersistedColumnWidths(tableId: string, datasetKey: string): Record<string, number> | null {
  if (typeof window === "undefined") {
    return null
  }
  try {
    const raw = window.localStorage.getItem(resolveColumnWidthsStorageKey(tableId, datasetKey))
    if (!raw) return null
    const parsed = JSON.parse(raw) as Record<string, unknown>
    if (!parsed || typeof parsed !== "object") {
      return null
    }
    const normalized: Record<string, number> = {}
    Object.entries(parsed).forEach(([key, value]) => {
      const width = Number(value)
      if (!Number.isFinite(width) || width <= 0) return
      normalized[key] = Math.max(1, Math.trunc(width))
    })
    return Object.keys(normalized).length ? normalized : null
  } catch {
    return null
  }
}

function writePersistedColumnWidths(tableId: string, datasetKey: string, widths: Record<string, number>) {
  if (typeof window === "undefined") {
    return
  }
  try {
    window.localStorage.setItem(
      resolveColumnWidthsStorageKey(tableId, datasetKey),
      JSON.stringify(widths),
    )
  } catch {
    // Ignore storage write failures and keep runtime functional.
  }
}

function applyPersistedColumnWidths(widths: Record<string, number> | null) {
  if (!widths) {
    return
  }
  Object.entries(widths).forEach(([columnKey, width]) => {
    if (!Number.isFinite(width)) return
    grid.columnState.setWidth(columnKey, Math.max(1, Math.trunc(width)))
  })
}

function readPersistedDatasetKey(tableId: string): string | null {
  if (typeof window === "undefined") {
    return null
  }
  try {
    return window.localStorage.getItem(resolveDatasetStorageKey(tableId))
  } catch {
    return null
  }
}

function writePersistedDatasetKey(tableId: string, datasetKey: string) {
  if (typeof window === "undefined") {
    return
  }
  try {
    window.localStorage.setItem(resolveDatasetStorageKey(tableId), datasetKey)
  } catch {
    // Ignore storage write failures and keep runtime functional.
  }
}

function ensurePersistedDatasetScope() {
  const tableId = persistedTableId.value
  if (!tableId) {
    return
  }
  const currentDatasetKey = persistedDatasetKey.value
  if (currentDatasetKey === "__default__") {
    return
  }
  const storedDatasetKey = readPersistedDatasetKey(tableId)
  if (storedDatasetKey !== null && storedDatasetKey !== currentDatasetKey) {
    dataGridSettingsAdapter.clearTable(tableId)
    if (settingsPersistTimer !== null) {
      clearTimeout(settingsPersistTimer)
      settingsPersistTimer = null
    }
  }
  writePersistedDatasetKey(tableId, currentDatasetKey)
}

function schedulePersistTableSettings() {
  if (!persistedTableId.value || restoringSettings) {
    return
  }
  if (settingsPersistTimer !== null) {
    clearTimeout(settingsPersistTimer)
  }
  settingsPersistTimer = setTimeout(() => {
    settingsPersistTimer = null
    persistTableSettingsNow()
  }, SETTINGS_PERSIST_DELAY_MS)
}

function restorePersistedTableSettings() {
  const tableId = persistedTableId.value
  if (!tableId) {
    return
  }
  ensurePersistedDatasetScope()
  restoringSettings = true
  try {
    const persistedColumnState = dataGridSettingsAdapter.getColumnState(tableId)
    if (persistedColumnState) {
      applyPersistedColumnState(persistedColumnState)
    }
    applyPersistedColumnWidths(readPersistedColumnWidths(tableId, persistedDatasetKey.value))

    const persistedSortState = normalizePersistedSortState(dataGridSettingsAdapter.getSortState(tableId))
    if (persistedSortState.length > 0 && props.enableSorting) {
      grid.setSortState(persistedSortState)
    }

    const persistedFilterSnapshot = dataGridSettingsAdapter.getFilterSnapshot(tableId)
    applyPersistedFilterSnapshot(persistedFilterSnapshot)
  } finally {
    restoringSettings = false
  }
  applyFilters()
}

function syncFilterKeys() {
  const allowedKeys = new Set(
    coreColumns.value
      .filter(column => isColumnFilterable(column))
      .map(column => column.key),
  )
  coreColumns.value
    .filter(column => isColumnFilterable(column))
    .forEach((column) => {
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

function refreshViewportAfterFilterMutation() {
  explicitRowRange = null
  lastAppliedRowRange = null
  void nextTick(() => {
    updateObservedViewportSize()
    scheduleViewportSync()
    scheduleInitialViewportRecovery(true)
  })
}

function applyFilters() {
  if (!props.enableFiltering || !grid.features.filtering.enabled.value) {
    if (lastAppliedFilterSignature === "__disabled__") {
      return
    }
    lastAppliedFilterSignature = "__disabled__"
    grid.features.filtering.clear()
    refreshViewportAfterFilterMutation()
    schedulePersistTableSettings()
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
    refreshViewportAfterFilterMutation()
    schedulePersistTableSettings()
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
  refreshViewportAfterFilterMutation()
  schedulePersistTableSettings()
}

  function applyFiltersDebounced() {
    if (filterDebounceTimer !== null) {
      clearTimeout(filterDebounceTimer)
    }
    filterDebounceTimer = setTimeout(() => {
      filterDebounceTimer = null
      applyFilters()
    }, FILTER_APPLY_DEBOUNCE_MS)
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
  position: relative;
  width: 100%;
  height: 100%;
  min-height: 0;
  min-width: 0;
}

.ui-affino-grid__loading-overlay {
  position: absolute;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  pointer-events: none;
  z-index: 4;
  background: linear-gradient(to bottom, rgba(255, 255, 255, 0.4), rgba(255, 255, 255, 0.15));
}

.dark .ui-affino-grid__loading-overlay {
  background: linear-gradient(to bottom, rgba(23, 23, 23, 0.52), rgba(23, 23, 23, 0.22));
}

.ui-affino-grid__loading-chip {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
  padding: 0.35rem 0.65rem;
  border-radius: 0.5rem;
  border: 1px solid rgba(148, 163, 184, 0.35);
  background: rgba(248, 250, 252, 0.92);
  color: #334155;
  font-size: 0.75rem;
  font-weight: 600;
}

.dark .ui-affino-grid__loading-chip {
  border-color: var(--ui-affino-dark-border);
  background: rgba(38, 38, 38, 0.88);
  color: var(--ui-affino-dark-text-strong);
}

.ui-affino-grid__loading-spinner {
  width: 0.78rem;
  height: 0.78rem;
  border-radius: 9999px;
  border: 2px solid rgba(100, 116, 139, 0.28);
  border-top-color: rgba(51, 65, 85, 0.9);
  animation: ui-affino-grid-spin 0.8s linear infinite;
}

.dark .ui-affino-grid__loading-spinner {
  border-color: rgba(163, 163, 163, 0.3);
  border-top-color: rgba(229, 229, 229, 0.9);
}

@keyframes ui-affino-grid-spin {
  to {
    transform: rotate(360deg);
  }
}

.dark .ui-affino-grid {
  --ui-affino-dark-bg-main: rgba(23, 23, 23, 0.94);
  --ui-affino-dark-bg-pinned: rgba(20, 20, 20, 0.96);
  --ui-affino-dark-bg-surface: #171717;
  --ui-affino-dark-bg-input: #111111;
  --ui-affino-dark-bg-even: rgba(38, 38, 38, 0.5);
  --ui-affino-dark-bg-hover: rgba(64, 64, 64, 0.52);
  --ui-affino-dark-border: rgba(115, 115, 115, 0.32);
  --ui-affino-dark-text: #e5e5e5;
  --ui-affino-dark-text-muted: #a3a3a3;
  --ui-affino-dark-text-strong: #d4d4d4;
}

.ui-affino-grid__layout {
  width: 100%;
  height: 100%;
  min-height: 0;
  min-width: 0;
  display: flex;
  flex-direction: column;
}

.ui-affino-grid__toolbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.75rem;
  padding: 0.45rem 0.55rem;
  border-bottom: 1px solid rgba(148, 163, 184, 0.25);
  background: rgba(248, 250, 252, 0.86);
}

.dark .ui-affino-grid__toolbar {
  border-bottom-color: var(--ui-affino-dark-border);
  background: rgba(23, 23, 23, 0.9);
}

.ui-affino-grid__toolbar-filters {
  min-width: 0;
  display: flex;
  align-items: center;
  gap: 0.4rem;
  flex-wrap: wrap;
}

.ui-affino-grid__toolbar-title {
  font-size: 0.68rem;
  font-weight: 700;
  letter-spacing: 0.06em;
  text-transform: uppercase;
  color: #475569;
}

.dark .ui-affino-grid__toolbar-title {
  color: var(--ui-affino-dark-text-strong);
}

.ui-affino-grid__toolbar-empty {
  font-size: 0.75rem;
  color: #64748b;
}

.dark .ui-affino-grid__toolbar-empty {
  color: var(--ui-affino-dark-text-muted);
}

.ui-affino-grid__filter-chip {
  display: inline-flex;
  align-items: center;
  max-width: 20rem;
  border: 1px solid rgba(148, 163, 184, 0.4);
  border-radius: 9999px;
  padding: 0.15rem 0.5rem;
  font-size: 0.72rem;
  line-height: 1.2;
  color: #334155;
  background: rgba(255, 255, 255, 0.92);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.dark .ui-affino-grid__filter-chip {
  border-color: var(--ui-affino-dark-border);
  color: var(--ui-affino-dark-text);
  background: rgba(17, 17, 17, 0.9);
}

.ui-affino-grid__toolbar-actions {
  display: inline-flex;
  align-items: center;
  gap: 0.4rem;
}

.ui-affino-grid__toolbar-stat {
  font-size: 0.72rem;
  font-weight: 600;
  color: #475569;
  white-space: nowrap;
}

.ui-affino-grid__toolbar-stat--busy {
  color: #0369a1;
}

.dark .ui-affino-grid__toolbar-stat {
  color: var(--ui-affino-dark-text-muted);
}

.dark .ui-affino-grid__toolbar-stat--busy {
  color: #7dd3fc;
}

.ui-affino-grid__toolbar-button,
.ui-affino-grid__column-order-button {
  border: 1px solid rgba(148, 163, 184, 0.45);
  border-radius: 0.45rem;
  background: #fff;
  color: #334155;
  font-size: 0.72rem;
  line-height: 1;
  font-weight: 600;
  padding: 0.3rem 0.5rem;
}

.ui-affino-grid__toolbar-button:disabled,
.ui-affino-grid__column-order-button:disabled {
  opacity: 0.5;
  cursor: default;
}

.dark .ui-affino-grid__toolbar-button,
.dark .ui-affino-grid__column-order-button {
  border-color: var(--ui-affino-dark-border);
  color: var(--ui-affino-dark-text-strong);
  background: var(--ui-affino-dark-bg-input);
}

.ui-affino-grid__column-panel {
  display: flex;
  flex-direction: column;
  gap: 0.35rem;
  padding: 0.55rem;
  border: 1px solid rgba(148, 163, 184, 0.35);
  border-radius: 0.55rem;
  background: #f8fafc;
  width: min(22rem, calc(100vw - 1rem));
  max-height: 18rem;
  overflow: auto;
}

.ui-affino-grid__column-panel--floating {
  box-shadow: 0 12px 28px rgba(15, 23, 42, 0.18);
}

.dark .ui-affino-grid__column-panel {
  border-color: var(--ui-affino-dark-border);
  background: #171717;
}

.dark .ui-affino-grid__column-panel--floating {
  box-shadow: 0 14px 28px rgba(2, 6, 23, 0.5);
}

.ui-affino-grid__column-panel-title {
  font-size: 0.7rem;
  font-weight: 700;
  letter-spacing: 0.04em;
  text-transform: uppercase;
  color: #475569;
}

.dark .ui-affino-grid__column-panel-title {
  color: var(--ui-affino-dark-text-strong);
}

.ui-affino-grid__column-panel-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.5rem;
}

.ui-affino-grid__column-toggle {
  min-width: 0;
  display: inline-flex;
  align-items: center;
  gap: 0.45rem;
  font-size: 0.76rem;
  color: #334155;
}

.dark .ui-affino-grid__column-toggle {
  color: var(--ui-affino-dark-text);
}

.ui-affino-grid__column-order-actions {
  display: inline-flex;
  align-items: center;
  gap: 0.2rem;
}

.ui-affino-grid__column-order-button {
  min-width: 1.65rem;
  padding: 0.25rem 0.35rem;
}

.ui-affino-grid__content-shell {
  flex: 1;
  width: 100%;
  height: 0;
  min-height: 0;
  min-width: 0;
  display: grid;
  position: relative;
  grid-template-columns:
    var(--ui-affino-index-width, 64px)
    var(--ui-affino-select-width, 42px)
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
  color: var(--ui-affino-dark-text-strong);
  border-right-color: var(--ui-affino-dark-border);
  border-bottom-color: var(--ui-affino-dark-border);
}

.ui-affino-grid__index-filter {
  border-right: 1px solid rgba(148, 163, 184, 0.2);
  border-bottom: 1px solid rgba(148, 163, 184, 0.2);
  background: #f8fafc;
  min-height: 29px;
  box-sizing: border-box;
}

.dark .ui-affino-grid__index-filter {
  border-right-color: var(--ui-affino-dark-border);
  border-bottom-color: var(--ui-affino-dark-border);
  background: var(--ui-affino-dark-bg-surface);
}

.ui-affino-grid__main-viewport {
  grid-column: 4;
  min-height: 0;
  min-width: 0;
  overflow-x: hidden;
  overflow-y: hidden;
  -webkit-overflow-scrolling: touch;
  background: rgba(255, 255, 255, 0.95);
}

.dark .ui-affino-grid__main-viewport {
  background: var(--ui-affino-dark-bg-main);
}

.ui-affino-grid__bottom-scrollbar {
  width: 100%;
  min-height: 16px;
  overflow-x: scroll;
  overflow-y: hidden;
  scrollbar-gutter: stable both-edges;
  scrollbar-width: auto;
  scrollbar-color: rgba(100, 116, 139, 0.7) rgba(226, 232, 240, 0.7);
  background: rgba(248, 250, 252, 0.75);
  border-top: 1px solid rgba(148, 163, 184, 0.24);
}

.dark .ui-affino-grid__bottom-scrollbar {
  scrollbar-color: rgba(163, 163, 163, 0.72) rgba(38, 38, 38, 0.72);
  background: rgba(23, 23, 23, 0.86);
  border-top-color: var(--ui-affino-dark-border);
}

.ui-affino-grid__bottom-scrollbar::-webkit-scrollbar {
  height: 14px;
}

.ui-affino-grid__bottom-scrollbar::-webkit-scrollbar-track {
  background: rgba(226, 232, 240, 0.72);
}

.ui-affino-grid__bottom-scrollbar::-webkit-scrollbar-thumb {
  background: rgba(100, 116, 139, 0.72);
  border-radius: 9999px;
  border: 2px solid rgba(226, 232, 240, 0.72);
}

.dark .ui-affino-grid__bottom-scrollbar::-webkit-scrollbar-track {
  background: rgba(38, 38, 38, 0.72);
}

.dark .ui-affino-grid__bottom-scrollbar::-webkit-scrollbar-thumb {
  background: rgba(163, 163, 163, 0.75);
  border-color: rgba(38, 38, 38, 0.72);
}

.ui-affino-grid__bottom-scrollbar-content {
  height: 1px;
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
  background: var(--ui-affino-dark-bg-pinned);
}

.ui-affino-grid__pinned-column--left {
  grid-column: 3;
  box-shadow: inset -2px 0 0 rgba(148, 163, 184, 0.28);
}

.ui-affino-grid__pinned-column--right {
  grid-column: 5;
  box-shadow: inset 2px 0 0 rgba(148, 163, 184, 0.28);
}

.dark .ui-affino-grid__pinned-column--left {
  box-shadow: inset -2px 0 0 var(--ui-affino-dark-border);
}

.dark .ui-affino-grid__pinned-column--right {
  box-shadow: inset 2px 0 0 var(--ui-affino-dark-border);
}

.ui-affino-grid__pinned-viewport {
  flex: 1;
  min-height: 0;
  overflow-y: hidden;
  overflow-x: hidden;
  position: relative;
}

.ui-affino-grid__pinned-canvas {
  width: 100%;
  min-width: 100%;
  transform: translate3d(0, var(--ui-affino-linked-scroll-top, 0px), 0);
  will-change: transform;
}

.ui-affino-grid__index-viewport {
  flex: 1;
  min-height: 0;
  overflow-y: hidden;
  overflow-x: hidden;
  position: relative;
  border-right: 1px solid rgba(148, 163, 184, 0.2);
  background: rgba(255, 255, 255, 0.95);
}

.ui-affino-grid__select-column {
  grid-column: 2;
  min-height: 0;
  min-width: 0;
  display: flex;
  flex-direction: column;
}

.ui-affino-grid__select-header {
  display: flex;
  align-items: center;
  justify-content: center;
  border-right: 1px solid rgba(148, 163, 184, 0.2);
  border-bottom: 1px solid rgba(148, 163, 184, 0.2);
  background: #f8fafc;
  box-sizing: border-box;
}

.dark .ui-affino-grid__select-header {
  border-right-color: var(--ui-affino-dark-border);
  border-bottom-color: var(--ui-affino-dark-border);
  background: var(--ui-affino-dark-bg-surface);
}

.ui-affino-grid__select-filter {
  border-right: 1px solid rgba(148, 163, 184, 0.2);
  border-bottom: 1px solid rgba(148, 163, 184, 0.2);
  background: #f8fafc;
  min-height: 29px;
  box-sizing: border-box;
}

.dark .ui-affino-grid__select-filter {
  border-right-color: var(--ui-affino-dark-border);
  border-bottom-color: var(--ui-affino-dark-border);
  background: var(--ui-affino-dark-bg-surface);
}

.ui-affino-grid__select-viewport {
  flex: 1;
  min-height: 0;
  overflow-y: hidden;
  overflow-x: hidden;
  position: relative;
  border-right: 1px solid rgba(148, 163, 184, 0.2);
  background: rgba(255, 255, 255, 0.95);
}

.dark .ui-affino-grid__select-viewport {
  border-right-color: var(--ui-affino-dark-border);
  background: var(--ui-affino-dark-bg-main);
}

.ui-affino-grid__select-canvas {
  width: 100%;
  min-width: 100%;
  transform: translate3d(0, var(--ui-affino-linked-scroll-top, 0px), 0);
  will-change: transform;
}

.ui-affino-grid__select-row {
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0 0.2rem;
  border-bottom: 1px solid rgba(148, 163, 184, 0.2);
  box-sizing: border-box;
}

.dark .ui-affino-grid__select-row {
  border-bottom-color: var(--ui-affino-dark-border);
}

.ui-affino-grid__select-row.is-even {
  background: rgba(241, 245, 249, 0.45);
}

.dark .ui-affino-grid__select-row.is-even {
  background: var(--ui-affino-dark-bg-even);
}

.ui-affino-grid__select-row.is-hovered {
  background: rgba(226, 232, 240, 0.55);
}

.dark .ui-affino-grid__select-row.is-hovered {
  background: var(--ui-affino-dark-bg-hover);
}

.ui-affino-grid__row-select-checkbox {
  width: 0.9rem;
  height: 0.9rem;
  cursor: default;
}

.ui-affino-grid.is-row-fixed .ui-affino-grid__select-row {
  height: var(--ui-affino-row-height, 34px);
  min-height: var(--ui-affino-row-height, 34px);
  max-height: var(--ui-affino-row-height, 34px);
}

.ui-affino-grid__index-viewport::-webkit-scrollbar,
.ui-affino-grid__select-viewport::-webkit-scrollbar,
.ui-affino-grid__pinned-viewport::-webkit-scrollbar {
  width: 0;
  height: 0;
}

.dark .ui-affino-grid__index-viewport {
  border-right-color: var(--ui-affino-dark-border);
  background: var(--ui-affino-dark-bg-main);
}

.ui-affino-grid__index-canvas {
  width: 100%;
  min-width: 100%;
  transform: translate3d(0, var(--ui-affino-linked-scroll-top, 0px), 0);
  will-change: transform;
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
  color: var(--ui-affino-dark-text-muted);
  border-bottom-color: var(--ui-affino-dark-border);
}

.ui-affino-grid__index-row.is-even {
  background: rgba(241, 245, 249, 0.45);
}

.dark .ui-affino-grid__index-row.is-even {
  background: var(--ui-affino-dark-bg-even);
}

.ui-affino-grid__index-row.is-hovered {
  background: rgba(226, 232, 240, 0.55);
}

.dark .ui-affino-grid__index-row.is-hovered {
  background: var(--ui-affino-dark-bg-hover);
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
  scrollbar-width: none;
  -webkit-overflow-scrolling: touch;
  background: rgba(255, 255, 255, 0.95);
}

.ui-affino-grid__viewport::-webkit-scrollbar {
  width: 0;
  height: 0;
}

.dark .ui-affino-grid__viewport {
  background: var(--ui-affino-dark-bg-main);
}

.ui-affino-grid__right-scrollbar {
  position: absolute;
  top: 0;
  right: 0;
  bottom: 0;
  width: 16px;
  overflow-y: scroll;
  overflow-x: hidden;
  scrollbar-gutter: stable;
  scrollbar-width: auto;
  scrollbar-color: rgba(100, 116, 139, 0.75) rgba(226, 232, 240, 0.8);
  background: rgba(248, 250, 252, 0.8);
  border-left: 1px solid rgba(148, 163, 184, 0.24);
  z-index: 8;
}

.ui-affino-grid__right-scrollbar::-webkit-scrollbar {
  width: 14px;
}

.ui-affino-grid__right-scrollbar::-webkit-scrollbar-track {
  background: rgba(226, 232, 240, 0.8);
}

.ui-affino-grid__right-scrollbar::-webkit-scrollbar-thumb {
  background: rgba(100, 116, 139, 0.75);
  border-radius: 9999px;
  border: 2px solid rgba(226, 232, 240, 0.8);
}

.ui-affino-grid__right-scrollbar-content {
  width: 1px;
}

.dark .ui-affino-grid__right-scrollbar {
  scrollbar-color: rgba(163, 163, 163, 0.75) rgba(38, 38, 38, 0.78);
  background: rgba(23, 23, 23, 0.9);
  border-left-color: var(--ui-affino-dark-border);
}

.dark .ui-affino-grid__right-scrollbar::-webkit-scrollbar-track {
  background: rgba(38, 38, 38, 0.78);
}

.dark .ui-affino-grid__right-scrollbar::-webkit-scrollbar-thumb {
  background: rgba(163, 163, 163, 0.75);
  border-color: rgba(38, 38, 38, 0.78);
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
  border-bottom-color: var(--ui-affino-dark-border);
  border-right-color: var(--ui-affino-dark-border);
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
  color: var(--ui-affino-dark-text);
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
  background: var(--ui-affino-dark-bg-surface);
}

.dark .ui-affino-grid__cell--header {
  color: var(--ui-affino-dark-text-strong);
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
  color: var(--ui-affino-dark-text-strong);
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
  background: var(--ui-affino-dark-bg-surface);
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
  border-color: var(--ui-affino-dark-border);
  background: var(--ui-affino-dark-bg-input);
  color: var(--ui-affino-dark-text-strong);
}

.ui-affino-grid__row--data.is-even .ui-affino-grid__cell {
  background: rgba(241, 245, 249, 0.45);
}

.dark .ui-affino-grid__row--data.is-even .ui-affino-grid__cell {
  background: var(--ui-affino-dark-bg-even);
}

.ui-affino-grid__row--data:hover .ui-affino-grid__cell {
  background: rgba(226, 232, 240, 0.55);
}

.dark .ui-affino-grid__row--data:hover .ui-affino-grid__cell {
  background: var(--ui-affino-dark-bg-hover);
}

.ui-affino-grid__row--data.is-hovered .ui-affino-grid__cell,
.ui-affino-grid__row--data.is-hovered .ui-affino-grid__spacer {
  background: rgba(226, 232, 240, 0.55);
}

.dark .ui-affino-grid__row--data.is-hovered .ui-affino-grid__cell,
.dark .ui-affino-grid__row--data.is-hovered .ui-affino-grid__spacer {
  background: var(--ui-affino-dark-bg-hover);
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
  color: var(--ui-affino-dark-text-muted);
}
</style>
