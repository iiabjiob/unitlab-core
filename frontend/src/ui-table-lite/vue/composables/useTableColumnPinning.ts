import type { UiTableColumn } from "../../core/types"
import type { ColumnPinPosition } from "../context"

export interface UseTableColumnPinningResult {
  resolveColumnPinState: (column: UiTableColumn) => ColumnPinPosition
  applyStoredPinState: () => void
  reorderPinnedColumns: () => void
  setColumnPin: (columnKey: string, position: ColumnPinPosition) => void
}

export function useTableColumnPinning(): UseTableColumnPinningResult {
  const resolveColumnPinState = () => "none" as ColumnPinPosition

  const applyStoredPinState = () => {
    // no-op in lite build
  }

  const reorderPinnedColumns = () => {
    // no-op in lite build
  }

  const setColumnPin = () => {
    // no-op in lite build
  }

  return {
    resolveColumnPinState,
    applyStoredPinState,
    reorderPinnedColumns,
    setColumnPin,
  }
}
