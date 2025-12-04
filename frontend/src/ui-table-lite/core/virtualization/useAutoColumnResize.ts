import type { UiTableColumn } from "../types/column"
import {
  createAutoColumnResizer,
  type AutoColumnResizeContext,
} from "./autoColumnResize"

export interface AutoColumnResizeOptions {
  minWidth?: number
  onApplied?: (payload: { columns: string[]; shareWidth: number; viewportWidth: number }) => void
}

export function useAutoColumnResize(options: AutoColumnResizeOptions = {}) {
  const resizer = createAutoColumnResizer<UiTableColumn>({
    minWidth: options.minWidth,
  })

  function reset(columns?: UiTableColumn[]) {
    resizer.reset(columns)
  }

  function invalidate() {
    resizer.invalidate()
  }

  function markManualResize() {
    resizer.markManualResize()
  }

  function syncManualState(columns: UiTableColumn[]) {
    resizer.syncManualState(columns)
  }

  function applyAutoColumnWidths(
    context: AutoColumnResizeContext<UiTableColumn>,
  ): Map<string, number> | null {
    const result = resizer.apply(context)
    if (!result) {
      return null
    }

    options.onApplied?.({
      columns: result.appliedColumns,
      shareWidth: result.shareWidth,
      viewportWidth: result.viewportWidth,
    })

    return result.updates
  }

  return {
    applyAutoColumnWidths,
    reset,
    invalidate,
    markManualResize,
    syncManualState,
  }
}
