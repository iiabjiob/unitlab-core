<template>
  <div
    :class="wrapperClasses"
    :id="cellIdAttr || undefined"
    role="gridcell"
    :tabindex="tabIndexAttr"
    :aria-selected="isSelectedAttr"
    :aria-readonly="ariaReadonlyAttr"
    :aria-rowindex="ariaRowIndexAttr"
    :aria-colindex="ariaColIndexAttr"
    :style="cellStyle"
    :data-row-index="rowIndex"
    :data-col-key="col.key"
    :data-col-index="colIndex"
    :data-editor-type="col.editor || undefined"
    :data-highlighted="highlightActive ? 'true' : null"
    :title="validationError || undefined"
    :aria-invalid="validationError ? 'true' : undefined"
    @mousedown="onCellMouseDown"
    @dblclick="startEdit"
    @mouseenter="onCellMouseEnter"
    @focus="handleFocus"
  >
    <div v-if="isSelectEditor" class="ui-table-cell__select">
      <UiSelect
        v-if="editing"
        ref="selectComponent"
        v-model="selectEditValue"
        :name="`cell-${rowIndex}-${col.key}`"
        :placeholder="col.placeholder"
        :class="['ui-table-cell__select-editor text-sm cursor-pointer px-1 py-0.5 flex items-center', columnTextAlignClass]"
        @update:modelValue="onSelectChange"
        @blur="onSelectBlur"
        @keydown.esc.stop.prevent="cancelSelectEdit"
      >
        <option v-for="option in selectOptions" :key="String(option.value)" :value="option.value">
          {{ option.label }}
        </option>
      </UiSelect>
      <template v-else>
        <div :class="['ui-table-cell__value px-1 py-0.5 pr-6', columnTextAlignClass]" data-auto-resize-target>
          <slot name="display" v-bind="slotProps">
            <span class="ui-table-cell__text">{{ selectLabel }}</span>
          </slot>
        </div>
        <button
          v-if="isSelected && isEditable"
          type="button"
          class="ui-table__cell-select-trigger"
          @mousedown.prevent
          @click.stop="startSelectEdit"
          aria-label="Open options"
        >
          <svg class="w-3 h-3" fill="none" stroke="currentColor" stroke-width="2" viewBox="0 0 24 24">
            <path stroke-linecap="round" stroke-linejoin="round" d="M6 9l6 6 6-6" />
          </svg>
        </button>
      </template>
    </div>
    <template v-else-if="editing && isTextEditor">
      <input
        ref="inputRef"
        v-model="textEditValue"
        :name="`cell-${rowIndex}-${col.key}`"
        :class="['ui-table-cell__editor px-1 py-0.5 text-sm outline-none focus:outline-none', columnTextAlignClass]"
        :type="col.editor === 'number' ? 'number' : 'text'"
        autocomplete="off"
        spellcheck="false"
        inputmode="text"
        @keydown.enter.prevent="onEnterKey"
        @keydown.tab="onTabKey"
        @keydown.esc.prevent="cancelEdit"
        @blur="onInputBlur"
      />
    </template>
    <div v-else :class="['ui-table-cell__value px-1 py-0.5', columnTextAlignClass]" data-auto-resize-target>
      <slot name="display" v-bind="slotProps">
        <span class="ui-table-cell__text">{{ displayValue }}</span>
      </slot>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, useSlots } from "vue"
import UiSelect from "@/components/ui/UiSelect.vue"
import {
  useCellEditing,
  useCellSelection,
  useCellStyle,
  type CellEmitFn,
  type CellProps,
  type ColumnOption,
} from "../cells"

const props = withDefaults(defineProps<CellProps>(), {
  isSelected: false,
  isSelectionAnchor: false,
  isRowSelected: false,
  isColumnSelected: false,
  editCommand: null,
  isRangeSelected: false,
  zoomScale: 1,
  editable: true,
  validationError: null,
  tabIndex: -1,
  ariaRowIndex: undefined,
  ariaColIndex: undefined,
  cellId: undefined,
  visualWidth: null,
  stickySide: null,
  stickyRightOffset: undefined,
  searchMatch: false,
  activeSearchMatch: false,
})

const emit = defineEmits<CellEmitFn>()
const slots = useSlots()

const editingContext = useCellEditing(props, emit)
const selectionHandlers = useCellSelection(props, emit)
const styleContext = useCellStyle(props, {
  editing: editingContext.editing,
  isRowSelected: editingContext.isRowSelected,
  isColumnSelected: editingContext.isColumnSelected,
  isRangeSelected: editingContext.isRangeSelected,
  validationError: editingContext.validationError,
  isSearchMatch: editingContext.isSearchMatch,
  isActiveSearchMatch: editingContext.isActiveSearchMatch,
  isSystemColumn: editingContext.isSystemColumn,
  isSelectionColumn: editingContext.isSelectionColumn,
})

const {
  editing,
  editValue,
  inputRef,
  selectComponent,
  isSelected,
  isEditable,
  isSystemColumn,
  isSelectionColumn,
  selectLabel,
  tabIndexAttr,
  ariaRowIndexAttr,
  ariaColIndexAttr,
  cellIdAttr,
  getOptions,
  startEdit,
  startSelectEdit,
  cancelSelectEdit,
  cancelEdit,
  onSelectChange,
  onSelectBlur,
  onTabKey,
  onEnterKey,
  onInputBlur,
} = editingContext

const { onCellMouseDown, onCellMouseEnter, handleFocus } = selectionHandlers
const { columnTextAlignClass, cellStyle, highlightActive } = styleContext

const isSelectEditor = computed(() => props.col.editor === "select")
const isTextEditor = computed(() => {
  const editor = props.col.editor ?? "text"
  return editor === "text" || editor === "number"
})

const selectOptions = computed<ColumnOption[]>(() => getOptions())

const selectEditValue = computed<string | number | null>({
  get: () => {
    const value = editValue.value
    if (value == null || typeof value === "string" || typeof value === "number") {
      return value as string | number | null
    }
    return String(value)
  },
  set: newValue => {
    editValue.value = newValue
  },
})

const textEditValue = computed<string>({
  get: () => {
    const value = editValue.value
    return value == null ? "" : String(value)
  },
  set: newValue => {
    editValue.value = newValue
  },
})

const displayValue = computed(() => {
  const value = props.row?.[props.col.key]
  return value == null ? "" : String(value)
})

const wrapperClasses = computed(() => [
  "ui-table-cell",
  "ui-table-cell--hover-target",
  "relative",
  columnTextAlignClass.value,
  {
    "ui-table-cell--system": isSystemColumn.value,
    "ui-table-cell--selection": isSelectionColumn.value,
    "ui-table-cell--editing": editing.value,
    "ui-table-cell--highlighted": highlightActive.value,
    "select-none": !editing.value,
    "cursor-text": isEditable.value && !isSystemColumn.value,
    "cursor-default": !isEditable.value || isSystemColumn.value,
  },
])

const isSelectedAttr = computed(() => (isSelected.value ? "true" : "false"))
const ariaReadonlyAttr = computed(() => (isEditable.value ? undefined : "true"))

const slotProps = computed(() => ({
  value: props.row?.[props.col.key],
  row: props.row,
  column: props.col,
  rowIndex: props.rowIndex,
  colIndex: props.colIndex,
  slots,
}))

</script>

<style scoped>
td {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
select {
  appearance: none;
  cursor: pointer;
  padding-right: 1rem;
}
</style>
