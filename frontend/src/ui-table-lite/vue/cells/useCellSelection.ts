import type { CellEmitFn, CellProps } from "./cellUtils"

export function useCellSelection(props: CellProps, emit: CellEmitFn) {
  function onCellMouseDown(event: MouseEvent) {
    const target = event.target as HTMLElement | null
    const interactive = target?.closest("input,select,textarea,[contenteditable='true'],button")
    if (interactive) {
      return
    }

    const shouldFocusTable = true
    emit("select", {
      rowIndex: props.rowIndex,
      key: props.col.key,
      colIndex: props.colIndex,
      focus: shouldFocusTable,
      event,
    })

    const isPrimaryButton = event.button === 0
    const hasModifier = event.shiftKey || event.metaKey || event.ctrlKey || event.altKey
    if (isPrimaryButton && !hasModifier) {
      emit("drag-start", {
        rowIndex: props.rowIndex,
        colIndex: props.colIndex,
        event,
      })
    }
  }

  function onCellMouseEnter(event: MouseEvent) {
    if ((event.buttons & 1) === 0) return
    emit("drag-enter", {
      rowIndex: props.rowIndex,
      colIndex: props.colIndex,
      event,
    })
  }

  function handleFocus(event: FocusEvent) {
    emit("cell-focus", {
      rowIndex: props.rowIndex,
      colIndex: props.colIndex,
      columnKey: props.col.key,
      event,
    })
  }

  return {
    onCellMouseDown,
    onCellMouseEnter,
    handleFocus,
  }
}
