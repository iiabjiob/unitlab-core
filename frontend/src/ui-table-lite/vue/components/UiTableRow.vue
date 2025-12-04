<template>
  <template v-if="rowView.kind === 'group'">
    <div :class="rowView.classList" role="row">
      <span
        :class="rowView.cellClassList"
        role="gridcell"
        :aria-colspan="rowView.ariaColCount"
        :style="rowView.cellStyle"
        tabindex="0"
        @click.stop="toggleGroupRow(rowView.rowKey)"
        @keydown.enter.prevent.stop="toggleGroupRow(rowView.rowKey)"
        @keydown.space.prevent.stop="toggleGroupRow(rowView.rowKey)"
      >
        <span :class="rowView.caretClassList" aria-hidden="true"></span>
        <span class="ui-table__group-label">{{ rowView.label }}</span>
        <span class="ui-table__group-count">({{ rowView.size }})</span>
      </span>
    </div>
  </template>
  <template v-else-if="rowView.kind === 'data'">
    <div
      class="ui-table__row-surface"
      :class="rowView.classList"
      :style="rowView.style"
      role="row"
      :aria-rowindex="rowView.ariaRowIndex ?? undefined"
      :data-region="rowRegion"
    >
      <template v-for="cell in rowView.cells" :key="cell.key">
        <div
          v-if="cell.kind === 'selection'"
          :class="cell.classList"
          :style="cell.style"
          role="gridcell"
          :aria-colindex="cell.binding.ariaColIndex"
          :aria-rowindex="rowView.ariaRowIndex ?? undefined"
        >
          <input
            type="checkbox"
            class="h-4 w-4 cursor-pointer rounded border-neutral-300 text-blue-600 transition-colors focus:ring-2 focus:ring-blue-500 dark:border-neutral-600 dark:bg-neutral-800 dark:text-blue-400 dark:focus:ring-blue-400"
            :checked="cell.selectionState?.checked"
            :name="rowSelectionName"
            @change="() => handleRowCheckboxToggle(cell.selectionState?.rowData ?? null)"
            @click.stop
            @mousedown.stop
          >
        </div>
        <div
          v-else-if="cell.kind === 'index'"
          :class="cell.classList"
          :style="cell.style"
          role="rowheader"
          :aria-rowindex="rowView.ariaRowIndex ?? undefined"
          :data-row-index="rowView.displayIndex"
          :data-col-key="cell.columnKey"
          tabindex="-1"
          @mousedown.stop="onRowIndexClick(rowView.displayIndex, $event)"
        >
          {{ cell.indexDisplay }}
        </div>
        <UiTableCell
          v-else
          :style="cell.style"
          v-bind="resolveDataCellProps(cell)"
          :class="cell.classList"
          @edit="onCellEdit"
          @next-cell="focusNextCell"
          @select="onCellSelect"
          @editing-change="value => handleCellEditingChange(value, cell.columnKey, pooled.entry?.originalIndex ?? null)"
          @drag-start="onCellDragStart"
          @drag-enter="onCellDragEnter"
          @cell-focus="onCellComponentFocus"
        >
          <template v-if="cell.hasCustomRenderer" #display="slotProps">
            <component
              v-for="(node, index) in renderCellSlot(`cell-${cell.columnKey}`, slotProps)"
              :key="`cell-slot-${cell.columnKey}-${rowView.displayIndex}-${index}`"
              :is="node"
            />
          </template>
        </UiTableCell>
      </template>
    </div>
  </template>
</template>

<script setup lang="ts">
import { computed, inject, toRef } from "vue"
import UiTableCell from "./UiTableCell.vue"
import { UiTableBodyContextKey, type UiTableRowViewModel, type UiTableRowRegion, type UiTableRowCellDescriptor } from "../context"
import type { RowPoolItem } from "../composables/useTableViewport"
import type { HeaderRenderableEntry } from "../../core/types/internal"
import type { CellProps } from "../cells/cellUtils"

const body = inject(UiTableBodyContextKey)
if (!body) {
  throw new Error("UiTableRow requires UiTableBodyContext")
}

const props = defineProps<{
  pooled: RowPoolItem
  entries?: HeaderRenderableEntry[]
  region?: UiTableRowRegion
}>()

const pooled = toRef(props, "pooled")

const {
  headerRenderableEntries,
  toggleGroupRow,
  rowSelectionName,
  handleRowCheckboxToggle,
  onCellEdit,
  onCellSelect,
  focusNextCell,
  handleCellEditingChange,
  onCellDragStart,
  onCellDragEnter,
  onCellComponentFocus,
  onRowIndexClick,
  buildRowViewModel,
  renderCellSlot,
} = body

const rowEntries = computed(() => props.entries ?? headerRenderableEntries.value)
const rowRegion = computed<UiTableRowRegion>(() => props.region ?? "center")

const rowView = computed<UiTableRowViewModel>(() =>
  buildRowViewModel({
    pooled: pooled.value,
    entries: rowEntries.value,
    region: rowRegion.value,
  }),
)

const resolveDataCellProps = (cell: UiTableRowCellDescriptor): CellProps => {
  if (cell.kind !== "data") {
    return {} as CellProps
  }
  return cell.cellProps as unknown as CellProps
}
</script>
