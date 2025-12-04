import type { TableSettingsStore } from "./tableSettingsStore"
import type { UiTableSettingsAdapter } from "../core/tableSettingsAdapter"
import type { UiTableSortState } from "../core/types/sort"
import type { SortState } from "./composables/useTableSorting"

function mapSortStateToLegacy(state: UiTableSortState[]): SortState[] {
  const result: SortState[] = []
  state.forEach(entry => {
    if (entry.direction !== "asc" && entry.direction !== "desc") return
    const key = entry.key ?? entry.field
    if (!key) return
    result.push({ key, direction: entry.direction })
  })
  return result
}

function mapSortStateFromLegacy(state: SortState[] | undefined): UiTableSortState[] | undefined {
  if (!state) return undefined
  return state.map(entry => ({ key: entry.key, field: entry.key, direction: entry.direction }))
}

export function createPiniaTableSettingsAdapter(store: TableSettingsStore): UiTableSettingsAdapter {
  return {
    setColumnWidth(tableId, columnKey, width) {
      store.setColumnWidth(tableId, columnKey, width)
    },

    getColumnWidth(tableId, columnKey) {
      return store.getColumnWidth(tableId, columnKey)
    },

    setSortState(tableId, state) {
      store.setSortState(tableId, mapSortStateToLegacy(state))
    },

    getSortState(tableId) {
      return mapSortStateFromLegacy(store.getSortState(tableId))
    },

    setFilterSnapshot(tableId, snapshot) {
      store.setFilterSnapshot(tableId, snapshot)
    },

    getFilterSnapshot(tableId) {
      return store.getFilterSnapshot(tableId)
    },

    setPinState(tableId, columnKey, position) {
      store.setPinState(tableId, columnKey, position)
    },

    getPinState(tableId) {
      return store.getPinState(tableId)
    },

    setGroupState(tableId, columns, expansion) {
      store.setGroupState(tableId, columns, expansion)
    },

    getGroupState(tableId) {
      return store.getGroupState(tableId)
    },

    clearTable(tableId) {
      store.clearTable(tableId)
    },
  }
}
