import { computed } from "vue"
import type { ComputedRef, Ref } from "vue"
import { BASE_ROW_HEIGHT } from "../../core/utils/constants"
import { useVerticalViewport } from "./useVerticalViewport"
import type { VisibleRow } from "../../core/types"

type MaybeComputedRef<T> = Ref<T> | ComputedRef<T>

export interface UseVirtualListOptions<T> {
  containerRef: Ref<HTMLDivElement | null>
  items: MaybeComputedRef<T[]>
  rowHeight?: number
  overscan?: number
}

export interface VirtualListItem<T> {
  item: T
  index: number
  top: number
}

export function useVirtualList<T>({ containerRef, items, rowHeight = 28, overscan }: UseVirtualListOptions<T>) {
  const normalizedRowHeight = computed(() => Math.max(rowHeight, 1))

  const zoom = computed(() => {
    const base = BASE_ROW_HEIGHT || 1
    return Math.max(normalizedRowHeight.value / base, 0.01)
  })

  const processedRows = computed<VisibleRow<T>[]>(() =>
    items.value.map((item, index) => ({
      row: item,
      rowId: typeof (item as any)?.rowId !== "undefined" ? (item as any).rowId : index,
      originalIndex: index,
    }))
  )

  const viewport = useVerticalViewport({
    containerRef,
    processedRows,
    zoom,
    rowHeightMode: "fixed",
    overscan,
  })

  const itemHeight = computed(() => viewport.effectiveRowHeight.value)

  const visibleItems = computed<VirtualListItem<T>[]>(() => {
    const rows = viewport.visibleRows.value
    const start = viewport.startIndex.value
    const height = itemHeight.value
    const list: VirtualListItem<T>[] = []

    for (let offset = 0; offset < rows.length; offset += 1) {
      const entry = rows[offset]
      if (!entry) continue
      const index = entry.displayIndex ?? start + offset
      list.push({
        item: entry.row as T,
        index,
        top: index * height,
      })
    }

    return list
  })

  function handleScroll(event: Event) {
    viewport.handleVerticalScroll(event)
  }

  return {
    items: visibleItems,
    itemHeight,
    totalHeight: viewport.totalContentHeight,
    startIndex: viewport.startIndex,
    endIndex: viewport.endIndex,
    virtualizationEnabled: viewport.virtualizationEnabled,
    handleScroll,
    updateMetrics: viewport.updateViewportMetrics,
  }
}
