<template>
  <div class="ui-table-light">
    <div class="ui-table-light__container" :style="containerStyle">
      <table class="ui-table-light__table">
        <thead>
          <tr>
            <th
              v-for="column in columns"
              :key="column.key"
              class="ui-table-light__cell ui-table-light__cell--header"
              :style="columnStyle(column)"
              :class="alignClass(column.align)"
              scope="col"
            >
              {{ column.label }}
            </th>
          </tr>
        </thead>
        <tbody v-if="rows.length">
          <tr
            v-for="(row, rowIndex) in rows"
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
import { computed } from "vue"

type ColumnAlign = "left" | "center" | "right"

type TableColumn = {
  key: string
  label: string
  width?: string | number
  align?: ColumnAlign
}

type TableRow = Record<string, unknown>

const props = withDefaults(
  defineProps<{
    columns: TableColumn[]
    rows: TableRow[]
    maxHeight?: string | number
    rowKey?: (row: TableRow, rowIndex: number) => string | number
  }>(),
  {
    columns: () => [],
    rows: () => [],
    maxHeight: undefined,
    rowKey: undefined,
  },
)

const emit = defineEmits<{ (e: "row-click", payload: { row: TableRow; rowIndex: number }): void }>()

const containerStyle = computed(() => {
  if (!props.maxHeight) return undefined
  const value = typeof props.maxHeight === "number" ? `${props.maxHeight}px` : props.maxHeight
  return { maxHeight: value }
})

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
  return JSON.stringify(value)
}

function columnStyle(column: TableColumn) {
  if (!column.width) return undefined
  const width = typeof column.width === "number" ? `${column.width}px` : column.width
  return { width }
}

function alignClass(align: ColumnAlign | undefined) {
  if (align === "center") return "ui-table-light__cell--center"
  if (align === "right") return "ui-table-light__cell--right"
  return "ui-table-light__cell--left"
}
</script>

<style scoped>
.ui-table-light {
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
}

:global(.dark) .ui-table-light {
  background-color: transparent;
}

.ui-table-light__container {
  flex: 1;
  min-height: 0;
  overflow: auto;
}

.ui-table-light__table {
  width: 100%;
  border-collapse: separate;
  border-spacing: 0;
  font-size: 0.875rem;
}

.ui-table-light__cell {
  padding: 0.5rem 0.75rem;
  border-bottom: 1px solid rgba(226, 232, 240, 0.8);
  color: #0f172a;
  vertical-align: top;
}

:global(.dark) .ui-table-light__cell {
  color: #e2e8f0;
  border-color: rgba(51, 65, 85, 0.7);
}

.ui-table-light__cell--header {
  position: sticky;
  top: 0;
  z-index: 1;
  background-color: #f8fafc;
  text-align: left;
  font-size: 0.75rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  color: #475569;
}

:global(.dark) .ui-table-light__cell--header {
  background-color: #111827;
  color: #cbd5f5;
}

.ui-table-light__row:nth-child(even) .ui-table-light__cell {
  background-color: rgba(248, 250, 252, 0.7);
}

:global(.dark) .ui-table-light__row:nth-child(even) .ui-table-light__cell {
  background-color: rgba(15, 23, 42, 0.6);
}

.ui-table-light__row:hover .ui-table-light__cell {
  background-color: rgba(226, 232, 240, 0.5);
}

:global(.dark) .ui-table-light__row:hover .ui-table-light__cell {
  background-color: rgba(30, 41, 59, 0.8);
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
  color: #94a3b8;
}

:global(.dark) .ui-table-light__empty {
  color: #64748b;
}
</style>
