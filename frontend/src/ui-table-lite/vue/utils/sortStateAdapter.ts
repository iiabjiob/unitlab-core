import type { UiTableSortState } from "../../core/types/sort"
import type { SortState } from "../composables/useTableSorting"

export function toAdapterSortState(state: SortState[]): UiTableSortState[] {
  return state
    .map(entry => ({
      key: entry.key,
      field: entry.key,
      direction: entry.direction,
    }))
    .filter(entry => Boolean(entry.key))
}

export function fromAdapterSortState(state: UiTableSortState[] | null | undefined): SortState[] {
  if (!state || !state.length) {
    return []
  }
  const result: SortState[] = []
  state.forEach(entry => {
    if (entry.direction !== "asc" && entry.direction !== "desc") return
    const key = entry.key ?? entry.field
    if (!key) return
    result.push({ key, direction: entry.direction })
  })
  return result
}
