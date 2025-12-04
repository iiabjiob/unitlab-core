import { computed, type ComputedRef, type Ref } from "vue"
import type { CSSProperties } from "vue"
import type { RowPoolItem } from "./useTableViewport"

interface UseTableGridLayoutParams {
  totalContentHeightDom: ComputedRef<number>
  rowHeightDom: ComputedRef<number>
  stickyBottomOffsets: Ref<Map<number, number>>
  startIndex: Ref<number>
  rowWindowStart?: Ref<number> | ComputedRef<number>
}

export function useTableGridLayout({
  totalContentHeightDom,
  rowHeightDom,
  stickyBottomOffsets,
  startIndex,
  rowWindowStart: _rowWindowStart,
}: UseTableGridLayoutParams) {
  const headerRowStickyStyle = computed<CSSProperties>(() => ({
    position: "sticky",
    top: "0px",
    zIndex: 45,
  }))

  const virtualContainerStyle = computed<CSSProperties>(() => ({
    position: "relative",
    height: `${totalContentHeightDom.value}px`,
  }))

  const rowLayerStyle = (pooled: RowPoolItem): CSSProperties => {
    const resolvedIndex = pooled.rowIndex >= 0 ? pooled.rowIndex : startIndex.value + pooled.poolIndex
    const translateIndex = resolvedIndex >= 0 ? resolvedIndex : 0
    const translateY = translateIndex * rowHeightDom.value
    const style: CSSProperties = {
      position: "absolute",
      top: "0",
      left: "0",
      transform: `translate3d(0, ${translateY}px, 0)`,
      height: `${rowHeightDom.value}px`,
      width: "100%",
    }
    if (!pooled.entry) {
      style.visibility = "hidden"
      style.pointerEvents = "none"
      return style
    }
    const bottomOffset = stickyBottomOffsets.value.get(pooled.entry.originalIndex)
    if (bottomOffset != null) {
      style.position = "sticky"
      style.top = "auto"
      style.bottom = `${bottomOffset}px`
      style.transform = "none"
      style.zIndex = 8
    }
    if (pooled.entry.stickyTop && pooled.displayIndex === 0) {
      style.top = `var(--ui-table-header-height, 40px)`
    }
    return style
  }

  return {
    virtualContainerStyle,
    headerRowStickyStyle,
    rowLayerStyle,
  }
}
