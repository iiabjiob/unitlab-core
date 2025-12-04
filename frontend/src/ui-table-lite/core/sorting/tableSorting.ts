import { createComparator } from "../utils/validators"
import type { UiTableColumn } from "../types"

export type SortDirection = "asc" | "desc"

export interface SortState {
  key: string
  direction: SortDirection
}

export interface TableSortingOptions<TRow extends Record<string, unknown> = Record<string, unknown>> {
  getRows: () => TRow[]
  getColumnByKey: (key: string) => UiTableColumn | undefined
  onPrimarySortChange?: (state: SortState | null) => void
}

export interface TableSorting {
  getSortState(): SortState | null
  getMultiSortState(): SortState[]
  getSortedOrder(): number[] | null
  setMultiSortState(next: SortState[]): void
  applySort(columnKey: string, direction: SortDirection): void
  toggleColumnSort(columnKey: string, additive: boolean): void
  clearSortForColumn(columnKey: string): void
  getSortDirectionForColumn(columnKey: string): SortDirection | null
  getSortPriorityForColumn(columnKey: string): number | null
  ensureSortedOrder(): void
  recomputeSortedOrder(): void
  applySorting<T extends { originalIndex: number }>(entries: T[]): T[]
  applyMultiSort<T extends Record<string, unknown>>(data: T[]): T[]
  notifyDatasetChanged(): void
}

function cloneSortState(state: SortState[]): SortState[] {
  return state.map(entry => ({ ...entry }))
}

export function createTableSorting<TRow extends Record<string, unknown> = Record<string, unknown>>(
  options: TableSortingOptions<TRow>,
): TableSorting {
  let multiSortState: SortState[] = []
  let sortedOrder: number[] | null = null

  function getSortState(): SortState | null {
    return multiSortState[0] ?? null
  }

  function getMultiSortState(): SortState[] {
    return cloneSortState(multiSortState)
  }

  function getSortedOrder(): number[] | null {
    return sortedOrder ? [...sortedOrder] : null
  }

  function getColumnByKey(key: string): UiTableColumn | undefined {
    return options.getColumnByKey(key)
  }

  function ensureSortedOrder(): void {
    if (!sortedOrder) return
    const dataset = options.getRows()
    const rowCount = dataset.length
    const seen = new Set<number>()
    const ordered: number[] = []

    sortedOrder.forEach(index => {
      if (index >= 0 && index < rowCount && !seen.has(index)) {
        ordered.push(index)
        seen.add(index)
      }
    })

    for (let index = 0; index < rowCount; index += 1) {
      if (!seen.has(index)) {
        ordered.push(index)
      }
    }

    sortedOrder = ordered
  }

  function recomputeSortedOrder(): void {
    if (!multiSortState.length) {
      sortedOrder = null
      return
    }

    const dataset = options.getRows()
    const comparators = multiSortState.map(sort => ({
      ...sort,
      comparator: createComparator(getColumnByKey(sort.key)),
    }))

    sortedOrder = dataset
      .map((_, index) => index)
      .sort((aIndex, bIndex) => {
        const aRow = dataset[aIndex]
        const bRow = dataset[bIndex]

        for (const sort of comparators) {
          const valueA = (aRow as Record<string, unknown> | undefined)?.[sort.key]
          const valueB = (bRow as Record<string, unknown> | undefined)?.[sort.key]
          const result = sort.comparator(valueA, valueB)
          if (result !== 0) {
            const multiplier = sort.direction === "asc" ? 1 : -1
            return result * multiplier
          }
        }

        return aIndex - bIndex
      })

    ensureSortedOrder()
  }

  function setMultiSortState(next: SortState[]): void {
    multiSortState = cloneSortState(next)
    if (!multiSortState.length) {
      sortedOrder = null
      options.onPrimarySortChange?.(null)
      return
    }
    recomputeSortedOrder()
    options.onPrimarySortChange?.({ ...multiSortState[0] })
  }

  function applySort(columnKey: string, direction: SortDirection): void {
    setMultiSortState([{ key: columnKey, direction }])
  }

  function getNextDirection(current: SortDirection | null): SortDirection | null {
    if (!current) return "asc"
    if (current === "asc") return "desc"
    return null
  }

  function toggleColumnSort(columnKey: string, additive: boolean): void {
    const currentIndex = multiSortState.findIndex(entry => entry.key === columnKey)

    if (!additive) {
      const current = currentIndex !== -1 ? multiSortState[currentIndex] : null
      const nextDirection = getNextDirection(current?.direction ?? null)
      if (!nextDirection) {
        setMultiSortState([])
        return
      }
      setMultiSortState([{ key: columnKey, direction: nextDirection }])
      return
    }

    const next = cloneSortState(multiSortState)
    if (currentIndex === -1) {
      next.push({ key: columnKey, direction: "asc" })
      setMultiSortState(next)
      return
    }

    const current = next[currentIndex]
    const nextDirection = getNextDirection(current.direction)
    if (!nextDirection) {
      next.splice(currentIndex, 1)
      setMultiSortState(next)
      return
    }
    next[currentIndex] = { key: columnKey, direction: nextDirection }
    setMultiSortState(next)
  }

  function clearSortForColumn(columnKey: string): void {
    const next = multiSortState.filter(entry => entry.key !== columnKey)
    setMultiSortState(next)
  }

  function getSortDirectionForColumn(columnKey: string): SortDirection | null {
    const entry = multiSortState.find(sort => sort.key === columnKey)
    return entry?.direction ?? null
  }

  function getSortPriorityForColumn(columnKey: string): number | null {
    const index = multiSortState.findIndex(entry => entry.key === columnKey)
    return index === -1 ? null : index + 1
  }

  function applySorting<T extends { originalIndex: number }>(entries: T[]): T[] {
    if (!sortedOrder || !multiSortState.length) return entries

    const orderMap = new Map<number, number>()
    sortedOrder.forEach((idx, position) => {
      if (!orderMap.has(idx)) {
        orderMap.set(idx, position)
      }
    })
    const maxPosition = sortedOrder.length

    return [...entries].sort((a, b) => {
      const posA = orderMap.has(a.originalIndex)
        ? orderMap.get(a.originalIndex)!
        : maxPosition + a.originalIndex
      const posB = orderMap.has(b.originalIndex)
        ? orderMap.get(b.originalIndex)!
        : maxPosition + b.originalIndex

      if (posA === posB) {
        return a.originalIndex - b.originalIndex
      }
      return posA - posB
    })
  }

  function applyMultiSort<T extends Record<string, unknown>>(data: T[]): T[] {
    if (!multiSortState.length) return [...data]

    const comparators = multiSortState.map(sort => ({
      ...sort,
      comparator: createComparator(getColumnByKey(sort.key)),
    }))

    return [...data].sort((a, b) => {
      for (const sort of comparators) {
        const valueA = (a as Record<string, unknown> | undefined)?.[sort.key]
        const valueB = (b as Record<string, unknown> | undefined)?.[sort.key]
        const result = sort.comparator(valueA, valueB)
        if (result !== 0) {
          const multiplier = sort.direction === "asc" ? 1 : -1
          return result * multiplier
        }
      }
      return 0
    })
  }

  function notifyDatasetChanged(): void {
    ensureSortedOrder()
  }

  return {
    getSortState,
    getMultiSortState,
    getSortedOrder,
    setMultiSortState,
    applySort,
    toggleColumnSort,
    clearSortForColumn,
    getSortDirectionForColumn,
    getSortPriorityForColumn,
    ensureSortedOrder,
    recomputeSortedOrder,
    applySorting,
    applyMultiSort,
    notifyDatasetChanged,
  }
}
