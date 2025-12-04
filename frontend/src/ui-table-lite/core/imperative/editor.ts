import type { CellSlot } from "./types.js"

export interface CellEditorHandlers {
  commit: (value: string) => void
  cancel: () => void
  navigate?: (payload: { type: "tab" | "enter"; shift: boolean }) => void
}

export class CellEditorOverlay {
  private input: HTMLInputElement | null = null
  private host: CellSlot | null = null
  private previousPosition: string | null = null
  private commitCallback: ((value: string) => void) | null = null
  private cancelCallback: (() => void) | null = null
  private navigateCallback: ((payload: { type: "tab" | "enter"; shift: boolean }) => void) | null = null

  open(cell: CellSlot, initialValue: string, handlers: CellEditorHandlers) {
    this.close()
    this.host = cell
    this.commitCallback = handlers.commit
    this.cancelCallback = handlers.cancel
    this.navigateCallback = handlers.navigate ?? null

    const input = document.createElement("input")
    input.type = "text"
    input.value = initialValue ?? ""
    input.className = "ui-table-cell__editor ui-table-cell__editor-overlay"
    const rowIdentifier = cell.wrapper.dataset.rowIndex ?? "0"
    const columnKey = cell.wrapper.dataset.colKey ?? ""
    input.name = `cell-${rowIdentifier}-${columnKey}`
    input.id = `cell-${rowIdentifier}-${columnKey}-editor`
    input.style.position = "absolute"
    input.style.inset = "0"
    input.style.width = "100%"
    input.style.height = "100%"
    input.style.zIndex = "100"
    input.style.boxSizing = "border-box"
    input.style.fontSize = "inherit"
    input.style.fontFamily = "inherit"
    input.style.background = "transparent"
    input.style.color = "inherit"

    this.previousPosition = cell.wrapper.style.position || null
    if (cell.wrapper.style.position !== "relative") {
      cell.wrapper.style.position = "relative"
    }
    cell.wrapper.appendChild(input)

    input.addEventListener("keydown", this.handleKeydown)
    input.addEventListener("blur", this.handleBlur)
    requestAnimationFrame(() => {
      input.focus()
      input.select()
    })

    this.input = input
  }

  close() {
    if (this.input) {
      this.input.removeEventListener("keydown", this.handleKeydown)
      this.input.removeEventListener("blur", this.handleBlur)
      const parent = this.input.parentElement
      if (parent) parent.removeChild(this.input)
    }
    if (this.host) {
      this.host.wrapper.style.position = this.previousPosition ?? ""
    }
    this.input = null
    this.host = null
    this.previousPosition = null
    this.commitCallback = null
    this.cancelCallback = null
    this.navigateCallback = null
  }

  isActive(cell: CellSlot | null) {
    return this.host === cell && this.input !== null
  }

  getActiveCell() {
    return this.host
  }

  commitSilently() {
    this.commit()
  }

  private handleKeydown = (event: KeyboardEvent) => {
    if (event.key === "Enter") {
      event.preventDefault()
      this.commit({ type: "enter", shift: event.shiftKey })
      return
    }
    if (event.key === "Escape") {
      event.preventDefault()
      this.cancel()
      return
    }
    if (event.key === "Tab") {
      event.preventDefault()
      this.commit({ type: "tab", shift: event.shiftKey })
    }
  }

  private handleBlur = () => {
    this.commit()
  }

  private commit(action?: { type: "tab" | "enter"; shift: boolean }) {
    if (!this.input) return
    const value = this.input.value
    const navigate = action && this.navigateCallback ? this.navigateCallback : null
    this.commitCallback?.(value)
    this.close()
    if (navigate && action) {
      navigate(action)
    }
  }

  private cancel() {
    this.cancelCallback?.()
    this.close()
  }
}
