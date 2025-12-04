import { computed, type ComputedRef, type Ref } from "vue"
import type { UiTableColumn } from "../../core/types"
import type { HeaderRenderableEntry } from "../../core/types/internal"
import type { UiTableColumnBinding } from "../context"

interface UseTableColumnBindingsOptions {
  headerRenderableEntries: ComputedRef<HeaderRenderableEntry[]>
  visibleColumnEntries: ComputedRef<HeaderRenderableEntry[]>
  columnTrackStartMap: ComputedRef<Map<string, number>>
  localColumns: Ref<UiTableColumn[]>
  columnWidthDomMap: ComputedRef<Map<string, number>>
  bodyCellClass: (column: UiTableColumn) => string | undefined
  isColumnSticky: (column: UiTableColumn) => boolean
  getStickySide: (column: UiTableColumn) => "left" | "right" | null
  getStickyLeftOffset: (column: UiTableColumn) => number | undefined
  getStickyRightOffset: (column: UiTableColumn) => number | undefined
  getAriaColIndex: (columnIndex: number) => number
}

export function useTableColumnBindings({
  headerRenderableEntries,
  visibleColumnEntries,
  columnTrackStartMap,
  localColumns,
  columnWidthDomMap,
  bodyCellClass,
  isColumnSticky,
  getStickySide,
  getStickyLeftOffset,
  getStickyRightOffset,
  getAriaColIndex,
}: UseTableColumnBindingsOptions) {
  const visibleColumnIndexMap = computed(() => {
    const map = new Map<string, number>()
    visibleColumnEntries.value.forEach(entry => {
      const columnKey = entry.metric.column.key
      const columnIndex = entry.metric.index
      if (Number.isInteger(columnIndex)) {
        map.set(columnKey, columnIndex)
      }
    })
    return map
  })

  const getColumnIndex = (columnKey: string): number => {
    const visibleIndex = visibleColumnIndexMap.value.get(columnKey)
    if (visibleIndex != null) {
      return visibleIndex
    }
    const fallbackIndex = localColumns.value.findIndex(column => column.key === columnKey)
    if (fallbackIndex !== -1) {
      return fallbackIndex
    }
    const trackEntry = columnTrackStartMap.value.get(columnKey)
    if (trackEntry != null) {
      return trackEntry - 1
    }
    return 0
  }

  const columnIndexToKey = computed(() => {
    const map = new Map<number, string>()
    headerRenderableEntries.value.forEach(entry => {
      const columnKey = entry.metric.column.key
      const columnIndex = getColumnIndex(columnKey)
      if (Number.isInteger(columnIndex)) {
        map.set(columnIndex, columnKey)
      }
    })
    return map
  })

  const bodyColumnBindings = computed<Map<string, UiTableColumnBinding>>(() => {
    const bindings = new Map<string, UiTableColumnBinding>()
    for (const entry of headerRenderableEntries.value) {
      const column = entry.metric.column
      const columnIndex = getColumnIndex(column.key)
      bindings.set(column.key, {
        column,
        columnIndex,
        visualWidth: columnWidthDomMap.value.get(column.key) ?? 0,
        cellClass: bodyCellClass(column),
        sticky: isColumnSticky(column),
        stickySide: getStickySide(column),
        stickyLeftOffset: getStickyLeftOffset(column),
        stickyRightOffset: getStickyRightOffset(column),
        ariaColIndex: getAriaColIndex(columnIndex),
      })
    }
    return bindings
  })

  const getBodyColumnBinding = (columnKey: string): UiTableColumnBinding => {
    const binding = bodyColumnBindings.value.get(columnKey)
    if (!binding) {
      throw new Error(`Missing body column binding for key: ${columnKey}`)
    }
    return binding
  }

  return {
    getColumnIndex,
    columnIndexToKey,
    bodyColumnBindings,
    getBodyColumnBinding,
  }
}
