import { defineStore } from "pinia"
import type { SortState } from "./composables/useTableSorting"
import type { FilterStateSnapshot } from "../core/types/filters"

const createMemoryStorage = (): Storage => {
  const data = new Map<string, string>()

  return {
    getItem(key) {
      return data.has(key) ? data.get(key)! : null
    },
    setItem(key, value) {
      data.set(key, value)
    },
    removeItem(key) {
      data.delete(key)
    },
    clear() {
      data.clear()
    },
    key(index) {
      return Array.from(data.keys())[index] ?? null
    },
    get length() {
      return data.size
    },
  } as Storage
}

const tableSettingsStorage: Storage =
  // Use browser storage when available, otherwise fall back to an in-memory shim (e.g., during SSR)
  typeof window !== "undefined" && window.localStorage ? window.localStorage : createMemoryStorage()

export const useTableSettingsStore = defineStore("tableSettingsStore", {
  state: () => ({
  // structure: { [tableId]: { [colKey]: width } }
    columnWidths: {} as Record<string, Record<string, number>>,
    sortStates: {} as Record<string, SortState[]>,
    filterSnapshots: {} as Record<string, FilterStateSnapshot>,
    pinStates: {} as Record<string, Record<string, "left" | "right" | "none" >>,
    groupStates: {} as Record<string, { columns: string[]; expansion: Record<string, boolean> }>,
  }),

  actions: {
    setColumnWidth(tableId: string, colKey: string, width: number) {
      if (!this.columnWidths[tableId]) {
        this.columnWidths[tableId] = {}
      }
      this.columnWidths[tableId][colKey] = width
    },

    getColumnWidth(tableId: string, colKey: string): number | undefined {
      return this.columnWidths[tableId]?.[colKey]
    },

    setSortState(tableId: string, state: SortState[]) {
      if (!state.length) {
        delete this.sortStates[tableId]
        return
      }
      this.sortStates[tableId] = state.map(entry => ({ ...entry }))
    },

    getSortState(tableId: string): SortState[] | undefined {
      const stored = this.sortStates[tableId]
      return stored ? stored.map(entry => ({ ...entry })) : undefined
    },

    setFilterSnapshot(tableId: string, snapshot: FilterStateSnapshot | null) {
      const isEmpty =
        !snapshot ||
        (Object.keys(snapshot.columnFilters ?? {}).length === 0 &&
          Object.keys(snapshot.advancedFilters ?? {}).length === 0)
      if (isEmpty) {
        delete this.filterSnapshots[tableId]
        return
      }
      this.filterSnapshots[tableId] = JSON.parse(JSON.stringify(snapshot))
    },

    getFilterSnapshot(tableId: string): FilterStateSnapshot | null {
      const stored = this.filterSnapshots[tableId]
      if (!stored) return null
      return JSON.parse(JSON.stringify(stored))
    },

    setPinState(tableId: string, columnKey: string, position: "left" | "right" | "none") {
      if (position === "none") {
        if (this.pinStates[tableId]) {
          delete this.pinStates[tableId][columnKey]
          if (Object.keys(this.pinStates[tableId]).length === 0) {
            delete this.pinStates[tableId]
          }
        }
        return
      }

      if (!this.pinStates[tableId]) {
        this.pinStates[tableId] = {}
      }
      this.pinStates[tableId][columnKey] = position
    },

    getPinState(tableId: string): Record<string, "left" | "right" | "none"> | null {
      const stored = this.pinStates[tableId]
      if (!stored) return null
      return { ...stored }
    },

    setGroupState(tableId: string, columns: string[], expansion: Record<string, boolean> = {}) {
      const uniqueColumns = Array.from(new Set(columns.filter(column => typeof column === "string" && column.length > 0)))
      if (!uniqueColumns.length) {
        delete this.groupStates[tableId]
        return
      }
      const sanitizedExpansion: Record<string, boolean> = {}
      Object.entries(expansion ?? {}).forEach(([key, value]) => {
        if (typeof value === "boolean") {
          sanitizedExpansion[key] = value
        }
      })
      this.groupStates[tableId] = {
        columns: uniqueColumns,
        expansion: sanitizedExpansion,
      }
    },

    getGroupState(tableId: string): { columns: string[]; expansion: Record<string, boolean> } | null {
      const stored = this.groupStates[tableId]
      if (!stored) return null
      return {
        columns: [...stored.columns],
        expansion: { ...stored.expansion },
      }
    },

    clearTable(tableId: string) {
      delete this.columnWidths[tableId]
      delete this.sortStates[tableId]
      delete this.filterSnapshots[tableId]
      delete this.pinStates[tableId]
      delete this.groupStates[tableId]
    },
  },

  // auto-persist via pinia-plugin-persistedstate
  persist: {
    key: "unitlab-table-settings",
    storage: tableSettingsStorage,
  },
})

export type TableSettingsStore = ReturnType<typeof useTableSettingsStore>
