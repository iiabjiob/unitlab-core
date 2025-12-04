import { computed } from "vue"
import type { ComputedRef } from "vue"
import type { RowPoolItem } from "./useTableViewport"
import type { VisibleRow } from "../../core/types"
import type { FindReplaceStore } from "./useTableFindReplaceBridge"

interface UseTableSearchHighlightsOptions {
  findReplace: FindReplaceStore
  pooledRows: ComputedRef<RowPoolItem[]>
  isGroupRowEntry: (entry: VisibleRow | null | undefined) => boolean
}

function makeSearchKey(rowIndex: number, columnKey: string) {
  return `${rowIndex}:${columnKey}`
}

export function useTableSearchHighlights(options: UseTableSearchHighlightsOptions) {
  const searchHighlightMap = computed(() => {
    if (!options.findReplace.matches.length) {
      return new Map<string, { active: boolean }>()
    }

    const visibleRows = new Set<number>()
    for (const pooled of options.pooledRows.value) {
      const entry = pooled.entry
      if (!entry || options.isGroupRowEntry(entry)) continue
      const index = entry.displayIndex ?? pooled.displayIndex
      if (typeof index === "number" && index >= 0) {
        visibleRows.add(index)
      }
    }

    const map = new Map<string, { active: boolean }>()
    const activeKey = options.findReplace.activeMatchKey
    for (const match of options.findReplace.matches) {
      if (!visibleRows.has(match.rowIndex)) continue
      const key = makeSearchKey(match.rowIndex, match.columnKey)
      map.set(key, { active: activeKey === key })
    }

    return map
  })

  function isSearchMatchCell(rowIndex: number | null | undefined, columnKey: string) {
    if (rowIndex == null || rowIndex < 0) return false
    return searchHighlightMap.value.has(makeSearchKey(rowIndex, columnKey))
  }

  function isActiveSearchMatchCell(rowIndex: number | null | undefined, columnKey: string) {
    if (rowIndex == null || rowIndex < 0) return false
    return searchHighlightMap.value.get(makeSearchKey(rowIndex, columnKey))?.active ?? false
  }

  return {
    searchHighlightMap,
    isSearchMatchCell,
    isActiveSearchMatchCell,
  }
}
