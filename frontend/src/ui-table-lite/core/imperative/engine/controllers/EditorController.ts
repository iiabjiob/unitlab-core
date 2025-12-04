import { CellEditorOverlay, type CellEditorHandlers } from "../../editor.js"
import type { CellSlot } from "../../types.js"

/**
 * Manages the editable overlay lifecycle for imperative cells.
 */
export class EditorController {
  private readonly overlay: CellEditorOverlay

  constructor() {
    this.overlay = new CellEditorOverlay()
  }

  open(cell: CellSlot, initialValue: string, handlers: CellEditorHandlers) {
    this.overlay.open(cell, initialValue, handlers)
  }

  close() {
    this.overlay.close()
  }

  isActive(cell: CellSlot | null) {
    return this.overlay.isActive(cell)
  }

  commitSilently() {
    this.overlay.commitSilently()
  }
}
