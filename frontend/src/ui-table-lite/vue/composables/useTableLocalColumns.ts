import { watch, type ComputedRef, type Ref } from "vue"

import type { NormalizedTableProps } from "../../core/config/tableConfig"
import type { UiTableColumn } from "../../core/types"
import type { VisibilitySnapshot } from "./useColumnVisibility"

interface UseTableLocalColumnsOptions {
  normalizedProps: ComputedRef<NormalizedTableProps>
  selectionColumnVisible: ComputedRef<boolean>
  localColumns: Ref<UiTableColumn[]>
  ensureSystemColumns: (
    columns: UiTableColumn[],
    includeRowIndex: boolean,
    includeSelection: boolean,
  ) => UiTableColumn[]
  getSavedColumnWidth: (columnKey: string) => number | undefined
  loadColumnStateFromStorage: () => VisibilitySnapshot[] | null
  applyStoredColumnState: (snapshot: VisibilitySnapshot[]) => void
  updateVisibilityMapFromColumns: (columns: UiTableColumn[]) => VisibilitySnapshot[]
  persistColumnState: (snapshot?: VisibilitySnapshot[]) => void
  visibilityHydrated: Ref<boolean>
  autoColumnResizeReset: (columns?: UiTableColumn[]) => void
  scheduleAutoColumnResize: () => void
  applyStoredPinState: () => void
  reorderPinnedColumns: () => void
}

export function useTableLocalColumns(options: UseTableLocalColumnsOptions) {
  watch(
    [
      () => options.normalizedProps.value.columns,
      () => options.normalizedProps.value.showRowIndexColumn,
      () => options.normalizedProps.value.selection.enabled,
      () => options.normalizedProps.value.selection.showSelectionColumn,
    ],
    ([newColumns, includeRowIndex]) => {
      const incoming = (Array.isArray(newColumns) ? newColumns : []) as UiTableColumn[]
      const normalized = options
        .ensureSystemColumns(
          incoming,
          includeRowIndex !== false,
          options.selectionColumnVisible.value,
        )
        .map((column: UiTableColumn) => {
          const savedWidth = options.getSavedColumnWidth(column.key)
          const resolvedWidth = savedWidth ?? column.width
          const userResized = column.userResized === true || typeof savedWidth === "number"
          return {
            ...column,
            width: resolvedWidth,
            userResized,
            visible: column.visible !== false,
          }
        })

      options.localColumns.value = normalized
      const stored = options.loadColumnStateFromStorage()
      if (stored) {
        options.applyStoredColumnState(stored)
      } else {
        const snapshot = options.updateVisibilityMapFromColumns(options.localColumns.value)
        options.persistColumnState(snapshot)
      }

      options.applyStoredPinState()
      options.reorderPinnedColumns()
      options.visibilityHydrated.value = true
      options.autoColumnResizeReset(options.localColumns.value)
      options.scheduleAutoColumnResize()
    },
    { immediate: true },
  )
}
