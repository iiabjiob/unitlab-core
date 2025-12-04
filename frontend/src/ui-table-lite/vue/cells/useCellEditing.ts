import { computed, nextTick, ref, watch } from "vue"
import { resolveColumnOptions } from "./cellUtils.js"
import type {
  CellEditEventPayload,
  CellEmitFn,
  CellProps,
  ColumnOption,
  NextCellPayload,
  SelectComponentLike,
} from "./cellUtils.js"

export function useCellEditing(props: CellProps, emit: CellEmitFn) {
  const editing = ref(false)
  const editValue = ref<unknown>("")
  const inputRef = ref<HTMLInputElement | null>(null)
  const selectComponent = ref<SelectComponentLike | null>(null)

  const columnOptions = computed<ColumnOption[]>(() => resolveColumnOptions(props.col.options, props.row))

  const sourceRowIndex = computed(() => props.originalRowIndex ?? props.rowIndex)
  const isSelected = computed(() => Boolean(props.isSelected))
  const isSelectionAnchorCell = computed(() => Boolean(props.isSelectionAnchor))
  const isRowSelected = computed(() => Boolean(props.isRowSelected))
  const isColumnSelected = computed(() => Boolean(props.isColumnSelected))
  const isRangeSelected = computed(() => Boolean(props.isRangeSelected))
  const isEditable = computed(() => props.editable !== false)
  const validationError = computed(() => props.validationError ?? null)
  const isSearchMatch = computed(() => Boolean(props.searchMatch))
  const isActiveSearchMatch = computed(() => Boolean(props.activeSearchMatch))
  const isSystemColumn = computed(() => Boolean(props.col.isSystem))
  const isSelectionColumn = computed(() => props.col.key === "__select__")

  const selectLabel = computed(() => {
    if (props.col.editor !== "select") {
      const raw = props.row[props.col.key]
      return raw == null ? "" : String(raw)
    }
    const options = columnOptions.value
    const value = props.row[props.col.key]
    const match = options.find(option => String(option.value) === String(value))
    if (match) return match.label
    if (value == null) return ""
    return String(value)
  })

  const tabIndexAttr = computed(() => props.tabIndex ?? -1)
  const ariaRowIndexAttr = computed(() => props.ariaRowIndex ?? undefined)
  const ariaColIndexAttr = computed(() => props.ariaColIndex ?? undefined)
  const cellIdAttr = computed(() => props.cellId ?? undefined)

  function getOptions(): ColumnOption[] {
    return columnOptions.value
  }

  function emitEditEvent(): void {
    const payload: CellEditEventPayload = {
      rowIndex: sourceRowIndex.value,
      originalRowIndex: sourceRowIndex.value,
      displayRowIndex: props.rowIndex,
      row: props.row,
      key: props.col.key,
      value: editValue.value,
    }
    emit("edit", payload)
  }

  function confirmEdit() {
    const current = props.row[props.col.key]
    if (editValue.value !== current) {
      emitEditEvent()
    }
    editing.value = false
  }

  function cancelEdit() {
    editing.value = false
    editValue.value = props.row[props.col.key]
  }

  function startTextEdit() {
    editing.value = true
    editValue.value = props.row[props.col.key]
    nextTick(() => {
      inputRef.value?.focus()
      inputRef.value?.select?.()
    })
  }

  function startSelectEdit() {
    if (editing.value || props.col.editor !== "select") return
    if (!isEditable.value) return
    editing.value = true
    editValue.value = props.row[props.col.key]
    nextTick(() => {
      selectComponent.value?.focus?.()
      selectComponent.value?.open?.()
    })
  }

  function startEdit() {
    if (editing.value) return
    if (!isEditable.value) return
    const editorType = props.col.editor ?? "text"
    if (editorType === "text" || editorType === "number") {
      startTextEdit()
      return
    }
    if (editorType === "select") {
      startSelectEdit()
    }
  }

  function cancelSelectEdit() {
    if (!editing.value) return
    editing.value = false
    editValue.value = props.row[props.col.key]
  }

  function onSelectChange(value: string | null) {
    editValue.value = value
    confirmEdit()
  }

  function onSelectBlur() {
    if (!editing.value) return
    confirmEdit()
  }

  function onInputBlur() {
    confirmEdit()
  }

  function onTabKey(event: KeyboardEvent) {
    confirmEdit()
    const payload: NextCellPayload = {
      rowIndex: props.rowIndex,
      key: props.col.key,
      colIndex: props.colIndex,
      shift: event.shiftKey,
      handled: false,
    }
    emit("next-cell", payload)
    if (payload.handled) {
      event.preventDefault()
    }
  }

  function onEnterKey(event: KeyboardEvent) {
    confirmEdit()
    const payload: NextCellPayload = {
      rowIndex: props.rowIndex,
      key: props.col.key,
      colIndex: props.colIndex,
      shift: event.shiftKey,
      handled: false,
      direction: event.shiftKey ? "up" : "down",
    }
    emit("next-cell", payload)
  }

  watch(
    () => props.row[props.col.key],
    value => {
      editValue.value = value
    },
    { immediate: true },
  )

  watch(editing, value => {
    emit("editing-change", value)
  })

  watch(
    () => props.editCommand,
    command => {
      if (!command) return
      if (command.rowIndex === props.rowIndex && command.key === props.col.key) {
        startEdit()
      }
    },
  )

  watch(
    () => props.isSelected,
    selected => {
      if (!selected && editing.value) {
        confirmEdit()
      }
    },
  )

  return {
    editing,
    editValue,
    inputRef,
    selectComponent,
    sourceRowIndex,
    isSelected,
    isSelectionAnchorCell,
    isRowSelected,
    isColumnSelected,
    isRangeSelected,
    isEditable,
    validationError,
    isSearchMatch,
    isActiveSearchMatch,
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
    confirmEdit,
    cancelEdit,
    onSelectChange,
    onSelectBlur,
    onTabKey,
    onEnterKey,
    onInputBlur,
  }
}
