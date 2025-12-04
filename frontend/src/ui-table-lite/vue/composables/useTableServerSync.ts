import { watch, watchEffect, type ComputedRef, type Ref, type ShallowRef } from "vue"
import type { UiTableAdvancedFilter, UiTableFilterSnapshot, UiTableLazyLoadReason, UiTableSortState } from "../../core/types"
import type { UiTableSortState as AdapterSortState } from "../../core/types/sort"
import type { UiTableSettingsAdapter } from "@/ui-table/core/tableSettingsAdapter"
import type { FilterCondition, FilterStateSnapshot } from "./useTableFilters"
import type { SortState } from "./useTableSorting"

type ColumnFiltersState = Record<string, unknown>
type FiltersStateRecord = Record<string, FilterCondition | undefined>

type ServerFilterResolver = () => UiTableFilterSnapshot | null

type ServerSortResolver = () => UiTableSortState[]

interface UseTableServerSyncOptions {
  tableId: ComputedRef<string>
  serverSideFiltering: ComputedRef<boolean>
  serverSideSorting: ComputedRef<boolean>
  columnFilters: Ref<ColumnFiltersState>
  filtersState: Ref<FiltersStateRecord>
  filterHydrated: Ref<boolean>
  sortHydrated: Ref<boolean>
  multiSortState: Ref<SortState[]>
  getFilterStateSnapshot: () => FilterStateSnapshot
  settingsAdapter: ComputedRef<UiTableSettingsAdapter>
  scheduleServerReload: (reason: UiTableLazyLoadReason) => void
  lastServerFilterFingerprint: Ref<string | null>
  lastServerSortFingerprint: Ref<string | null>
  serverFilterResolver: ShallowRef<ServerFilterResolver>
  serverSortResolver: ShallowRef<ServerSortResolver>
  cloneSortStateForServer: (source?: SortState[]) => UiTableSortState[]
}

function mapSortStateToAdapter(state: SortState[]): AdapterSortState[] {
  return state
    .map(entry => ({ key: entry.key, field: entry.key, direction: entry.direction }))
    .filter(entry => Boolean(entry.key))
}

function convertClauseForServer(clause: {
  operator: string
  value: any
  value2?: any
  join?: "and" | "or"
}) {
  return {
    operator: clause.operator,
    value: clause.value,
    value2: clause.value2,
    join: clause.join,
  }
}

function convertAdvancedFilterForServer(condition: FilterCondition): UiTableAdvancedFilter {
  return {
    type: condition.type,
    clauses: (condition.clauses ?? []).map(clause => convertClauseForServer(clause)),
  }
}

export function useTableServerSync(options: UseTableServerSyncOptions) {
  function filterSnapshotForServer(source: FilterStateSnapshot | null): UiTableFilterSnapshot | null {
    if (!options.serverSideFiltering.value || !source) {
      return null
    }
    const columnFilters: Record<string, string[]> = {}
    Object.entries(source.columnFilters ?? {}).forEach(([key, values]) => {
      if (!Array.isArray(values) || !values.length) {
        return
      }
      columnFilters[key] = values.map(value => String(value))
    })

    const advancedFilters: Record<string, UiTableAdvancedFilter> = {}
    Object.entries(source.advancedFilters ?? {}).forEach(([key, condition]) => {
      if (!condition || !condition.clauses?.length) {
        return
      }
      advancedFilters[key] = convertAdvancedFilterForServer(condition)
    })

    return {
      columnFilters,
      advancedFilters,
    }
  }

  function computeSortFingerprint(sorts: UiTableSortState[]): string {
    if (!sorts.length) return ""
    return JSON.stringify(sorts.map(sort => ({ key: sort.key, direction: sort.direction })))
  }

  function computeFilterFingerprint(snapshot: UiTableFilterSnapshot | null): string {
    if (!snapshot) return ""
    const normalizedColumns = Object.entries(snapshot.columnFilters)
      .sort(([a], [b]) => a.localeCompare(b))
      .map(([key, values]) => ({ key, values: [...values].sort() }))
    const normalizedAdvanced = Object.entries(snapshot.advancedFilters)
      .sort(([a], [b]) => a.localeCompare(b))
      .map(([key, condition]) => ({
        key,
        type: condition.type,
        clauses: condition.clauses.map(clause => ({
          operator: clause.operator,
          value: clause.value,
          value2: clause.value2,
          join: clause.join ?? "and",
        })),
      }))
    return JSON.stringify({ columnFilters: normalizedColumns, advancedFilters: normalizedAdvanced })
  }

  function snapshotHasActiveFilters(snapshot: UiTableFilterSnapshot | null): boolean {
    if (!snapshot) return false
    const { columnFilters = {}, advancedFilters = {} } = snapshot
    for (const values of Object.values(columnFilters)) {
      if (Array.isArray(values) && values.length > 0) {
        return true
      }
    }
    return Object.values(advancedFilters).some(filter => Array.isArray(filter?.clauses) && filter.clauses.length > 0)
  }

  watch(
    [() => options.columnFilters.value, () => options.filtersState.value],
    () => {
      if (!options.filterHydrated.value) return
      const snapshot = options.getFilterStateSnapshot()
      options.settingsAdapter.value.setFilterSnapshot(options.tableId.value, snapshot)
      if (options.serverSideFiltering.value) {
        const serverSnapshot = filterSnapshotForServer(snapshot)
        const fingerprint = computeFilterFingerprint(serverSnapshot)
        if (fingerprint !== options.lastServerFilterFingerprint.value) {
          options.lastServerFilterFingerprint.value = fingerprint
          options.scheduleServerReload("filter-change")
        }
      }
    },
    { deep: true },
  )

  watch(
    () => options.multiSortState.value,
    (next: SortState[]) => {
      if (!options.sortHydrated.value) return
      options.settingsAdapter.value.setSortState(options.tableId.value, mapSortStateToAdapter(next))
      if (options.serverSideSorting.value) {
        const serverSorts = options.cloneSortStateForServer(next)
        const fingerprint = computeSortFingerprint(serverSorts)
        if (fingerprint !== options.lastServerSortFingerprint.value) {
          options.lastServerSortFingerprint.value = fingerprint
          options.scheduleServerReload("sort-change")
        }
      }
    },
    { deep: true },
  )

  watchEffect(() => {
    if (!options.serverSideFiltering.value) {
      options.serverFilterResolver.value = () => null
      return
    }
    const snapshot = filterSnapshotForServer(options.getFilterStateSnapshot())
    options.serverFilterResolver.value = () => snapshot
  })

  watchEffect(() => {
    if (!options.serverSideSorting.value) {
      options.serverSortResolver.value = () => []
      return
    }
    const sorts = options.cloneSortStateForServer(options.multiSortState.value)
    options.serverSortResolver.value = () => sorts
  })

  return {
    filterSnapshotForServer,
    snapshotHasActiveFilters,
    computeSortFingerprint,
    computeFilterFingerprint,
  }
}
