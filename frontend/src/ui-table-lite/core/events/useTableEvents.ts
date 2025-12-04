import type { CellEditEvent } from "../types"

export interface Signal<T> {
  value: T
}

interface NextCellPayload {
  rowIndex: number
  key: string
  colIndex?: number
  shift?: boolean
  handled?: boolean
  direction?: "up" | "down"
}

interface UseTableEventsOptions {
  isEditingCell: Signal<boolean>
  selection: {
    moveSelection: (rowDelta: number, colDelta: number, options?: { extend?: boolean }) => void
    moveByTab: (forward: boolean) => boolean
    moveByPage: (direction: number, options?: { extend?: boolean }) => boolean
    triggerEditForSelection: () => { rowIndex: number; columnKey: string } | null
    clearSelectionValues: () => boolean
    selectCell: (rowIndex: number, columnKey: string, focus?: boolean, options?: { colIndex?: number }) => void
    scheduleOverlayUpdate: () => void
    goToRowEdge: (edge: "start" | "end", options?: { extend?: boolean }) => boolean
    goToColumnEdge: (edge: "start" | "end", options?: { extend?: boolean }) => boolean
    goToGridEdge: (edge: "start" | "end", options?: { extend?: boolean }) => boolean
  }
  clipboard: {
    copySelectionToClipboard: () => Promise<void> | void
    cutSelectionToClipboard?: () => Promise<boolean> | boolean
    pasteClipboardData: () => Promise<void> | void
    copySelectionToClipboardWithFlash?: () => Promise<void> | void
    cutSelectionToClipboardWithFlash?: () => Promise<boolean> | boolean
    pasteClipboardDataWithFlash?: () => Promise<void> | void
    cancelCutPreview?: () => void
  }
  history: {
    undo: () => CellEditEvent[]
    redo: () => CellEditEvent[]
  }
  requestEdit: (rowIndex: number, columnKey: string) => void
}

export function useTableEvents({
  isEditingCell,
  selection,
  clipboard,
  history,
  requestEdit,
}: UseTableEventsOptions) {
  async function handleKeydown(event: KeyboardEvent) {
    if (isEditingCell.value) return

    if (event.key === "Escape") {
      if (clipboard.cancelCutPreview) {
        event.preventDefault()
        clipboard.cancelCutPreview()
        selection.scheduleOverlayUpdate()
      }
      return
    }

    if (event.ctrlKey || event.metaKey) {
      const key = event.key.toLowerCase()
      if (!isEditingCell.value) {
        if (key === "z") {
          event.preventDefault()
          if (event.shiftKey) {
            history.redo()
          } else {
            history.undo()
          }
          selection.scheduleOverlayUpdate()
          return
        }
        if (key === "y") {
          event.preventDefault()
          history.redo()
          selection.scheduleOverlayUpdate()
          return
        }
      }
      if (key === "c") {
        event.preventDefault()
        if (clipboard.copySelectionToClipboardWithFlash) {
          await clipboard.copySelectionToClipboardWithFlash()
        } else {
          await clipboard.copySelectionToClipboard()
        }
        return
      }
      if (key === "x") {
        event.preventDefault()
        if (clipboard.cutSelectionToClipboardWithFlash) {
          await clipboard.cutSelectionToClipboardWithFlash()
        } else if (clipboard.cutSelectionToClipboard) {
          await clipboard.cutSelectionToClipboard()
        }
        return
      }
      if (key === "v") {
        event.preventDefault()
        if (clipboard.pasteClipboardDataWithFlash) {
          await clipboard.pasteClipboardDataWithFlash()
        } else {
          await clipboard.pasteClipboardData()
        }
        return
      }
    }

    if (event.key === "Delete" || event.key === "Backspace") {
      event.preventDefault()
      selection.clearSelectionValues()
      return
    }

    switch (event.key) {
      case "ArrowUp":
        event.preventDefault()
        if (event.ctrlKey || event.metaKey) {
          selection.goToColumnEdge("start", { extend: event.shiftKey })
        } else {
          selection.moveSelection(-1, 0, { extend: event.shiftKey })
        }
        break
      case "ArrowDown":
        event.preventDefault()
        if (event.ctrlKey || event.metaKey) {
          selection.goToColumnEdge("end", { extend: event.shiftKey })
        } else {
          selection.moveSelection(1, 0, { extend: event.shiftKey })
        }
        break
      case "ArrowLeft":
        event.preventDefault()
        if (event.ctrlKey || event.metaKey) {
          selection.goToRowEdge("start", { extend: event.shiftKey })
        } else {
          selection.moveSelection(0, -1, { extend: event.shiftKey })
        }
        break
      case "ArrowRight":
        event.preventDefault()
        if (event.ctrlKey || event.metaKey) {
          selection.goToRowEdge("end", { extend: event.shiftKey })
        } else {
          selection.moveSelection(0, 1, { extend: event.shiftKey })
        }
        break
      case "Tab": {
        const moved = selection.moveByTab(!event.shiftKey)
        if (moved) {
          event.preventDefault()
        }
        break
      }
      case "Home":
        event.preventDefault()
        if (event.ctrlKey || event.metaKey) {
          selection.goToGridEdge("start", { extend: event.shiftKey })
        } else {
          selection.goToRowEdge("start", { extend: event.shiftKey })
        }
        break
      case "End":
        event.preventDefault()
        if (event.ctrlKey || event.metaKey) {
          selection.goToGridEdge("end", { extend: event.shiftKey })
        } else {
          selection.goToRowEdge("end", { extend: event.shiftKey })
        }
        break
      case "PageUp": {
        const moved = selection.moveByPage(-1, { extend: event.shiftKey })
        if (moved) {
          event.preventDefault()
        }
        break
      }
      case "PageDown": {
        const moved = selection.moveByPage(1, { extend: event.shiftKey })
        if (moved) {
          event.preventDefault()
        }
        break
      }
      case "Enter":
        event.preventDefault()
        {
          const target = selection.triggerEditForSelection()
          if (target) {
            requestEdit(target.rowIndex, target.columnKey)
          }
        }
        break
    }
  }

  function focusNextCell(payload: NextCellPayload) {
    selection.selectCell(payload.rowIndex, payload.key, false, { colIndex: payload.colIndex })
    let handled = false
    if (payload.direction === "up") {
      selection.moveSelection(-1, 0)
      handled = true
    } else if (payload.direction === "down") {
      selection.moveSelection(1, 0)
      handled = true
    } else {
      handled = selection.moveByTab(!(payload.shift ?? false))
    }
    payload.handled = handled
  }

  return {
    handleKeydown,
    focusNextCell,
  }
}
