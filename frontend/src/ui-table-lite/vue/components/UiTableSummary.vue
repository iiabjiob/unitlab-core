<template>
  <div
    :class="['ui-table__summary-layer sticky bottom-0 z-10', summaryRowClass]"
    role="row"
    :aria-rowindex="summaryRowAriaIndex"
  >
    <div class="ui-table__summary-surface ui-table__summary-grid" :style="rowSurfaceStyle">
      <template v-for="cell in summaryCells" :key="cell.key">
        <div
          v-if="cell.kind === 'system'"
          class="ui-table__row-index ui-table__sticky-divider ui-table__summary-index"
          :class="summaryLabelCellClass"
          :style="cell.style"
          role="rowheader"
          :aria-rowindex="summaryRowAriaIndex"
          tabindex="-1"
        >
          <component
            v-for="(node, index) in renderSummarySlot('summary-label', {
              column: cell.column,
              value: summaryRowData?.[cell.column.key],
            })"
            :key="`summary-label-${index}`"
            :is="node"
          />
          <span v-if="!hasSummaryLabelContent && cell.column.key !== selectionColumnKey">
            Summary
          </span>
        </div>
        <div
          v-else
          class="ui-table__summary-cell"
          :class="summaryCellClass"
          role="gridcell"
          :style="cell.style"
          :aria-rowindex="summaryRowAriaIndex"
          :aria-colindex="getColumnBinding(cell.column.key).ariaColIndex"
        >
          <template v-if="hasColumnSummaryContent(cell.column.key)">
            <component
              v-for="(node, index) in renderSummarySlot(`summary-${cell.column.key}`, {
                column: cell.column,
                value: summaryRowData?.[cell.column.key],
              })"
              :key="`summary-slot-${cell.column.key}-${index}`"
              :is="node"
            />
          </template>
          <template v-else>
            <component
              v-for="(node, index) in renderSummarySlot('summary', {
                column: cell.column,
                value: summaryRowData?.[cell.column.key],
              })"
              :key="`summary-default-${cell.column.key}-${index}`"
              :is="node"
            />
            <span v-if="!hasDefaultSummaryContent">
              {{ summaryRowData?.[cell.column.key] ?? '' }}
            </span>
          </template>
        </div>
      </template>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, inject } from "vue"
import { UiTableSummaryContextKey } from "../context"
import type { UiTableColumn } from "../../core/types"
import type { CSSProperties } from "vue"

interface SummaryCellDescriptor {
  key: string
  kind: "system" | "data"
  column: UiTableColumn
  style: Array<CSSProperties | undefined>
}

const summary = inject(UiTableSummaryContextKey)
if (!summary) {
  throw new Error("UiTableSummary requires UiTableSummaryContext")
}

const {
  headerRenderableEntries,
  summaryRowClass,
  summaryCellClass,
  summaryLabelCellClass,
  summaryCellStyle,
  summaryRowAriaIndex,
  summaryRowData,
  tableSlots,
  getColumnBinding,
  selectionColumnKey,
  resolveColumnSurface,
} = summary

const summaryLabelSlot = computed(() => tableSlots["summary-label"])
const defaultSummarySlot = computed(() => tableSlots.summary)

const hasSummaryLabelContent = computed(() => Boolean(summaryLabelSlot.value))
const hasDefaultSummaryContent = computed(() => Boolean(defaultSummarySlot.value))

const rowSurfaceStyle = computed<CSSProperties>(() => ({
  position: "relative",
  width: "100%",
  height: "100%",
}))

const summaryCells = computed<SummaryCellDescriptor[]>(() => {
  const cells: SummaryCellDescriptor[] = []
  headerRenderableEntries.value.forEach(entry => {
    const surface = resolveColumnSurface("center", entry.metric.column.key)
    if (!surface) {
      return
    }
    const baseStyle: CSSProperties = {
      position: "absolute",
      top: "0",
      bottom: "0",
      left: `${surface.left}px`,
      width: `${surface.width}px`,
      height: "100%",
    }

    if (entry.metric.column.isSystem) {
      cells.push({
        key: `summary-system-${entry.metric.column.key}`,
        kind: "system",
        column: entry.metric.column,
        style: [baseStyle, summaryCellStyle(entry.metric.column)],
      })
    } else {
      cells.push({
        key: `summary-data-${entry.metric.column.key}`,
        kind: "data",
        column: entry.metric.column,
        style: [baseStyle, summaryCellStyle(entry.metric.column)],
      })
    }
  })
  return cells
})

function hasColumnSummaryContent(columnKey: string) {
  return Boolean(tableSlots[`summary-${columnKey}`])
}

function renderSummarySlot(slotName: string, slotProps: Record<string, unknown>) {
  const slot = tableSlots[slotName]
  if (!slot) return []
  return slot(slotProps) ?? []
}
</script>

<style scoped>
.ui-table__summary-surface {
  position: relative;
  min-height: 100%;
}
</style>
