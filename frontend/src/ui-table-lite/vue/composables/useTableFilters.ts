import { computed, nextTick, reactive, ref, shallowRef, watch } from "vue"
import type { Ref } from "vue"
import {
  serializeFilterValue,
  formatFilterLabel,
  toNumberValue,
  toDateValue,
} from "../../core/utils/validators"
import type { UiTableColumn } from "../../core/types"
import type {
  FilterCondition,
  FilterConditionClause,
  FilterMenuState,
  FilterStateSnapshot,
} from "../../core/types/filters"

export type {
  FilterCondition,
  FilterConditionClause,
  FilterMenuState,
  FilterStateSnapshot,
} from "../../core/types/filters"

// Manages set-based and advanced filter state for UiTable columns.

export interface FilterOption {
  key: string
  label: string
  raw: unknown
  count?: number
}

type FilterClause = FilterConditionClause

interface VisibleRow {
  row: any
  originalIndex: number
}

interface UseTableFiltersOptions {
  rows: () => any[]
  localColumns: Ref<UiTableColumn[]>
  emitFilterChange: (filters: Record<string, string[]>) => void
  getSuspendedRows?: () => Map<string, Set<number>> | null | undefined
}

function getColumnByKey(columns: UiTableColumn[], key: string) {
  return columns.find(col => col.key === key)
}

/**
 * Provides filter-menu state, option generation, and predicate application for UiTable.
 */
export function useTableFilters({ rows, localColumns, emitFilterChange, getSuspendedRows }: UseTableFiltersOptions) {
  const columnFilters = ref<Record<string, string[]>>({})
  const filtersState = ref<Record<string, FilterCondition>>({})
  const filterMenuState = reactive<FilterMenuState>({
    columnKey: null,
    search: "",
    selectedKeys: [],
  })
  const activeMenuClose = ref<(() => void) | null>(null)
  const filterMenuSearchRef = ref<HTMLInputElement | null>(null)
  const selectionInitialized = ref(false)
  const defaultAllSelected = ref(true)

  /**
   * Resolves static or dynamic filter options declared on the column.
   */
  function getColumnOptions(column: UiTableColumn | undefined, row: any) {
    if (!column?.options) return []
    try {
      const resolved = typeof column.options === "function" ? column.options(row) ?? [] : column.options ?? []
      return Array.isArray(resolved) ? resolved : []
    } catch {
      return []
    }
  }

  /**
   * Finds the human-friendly label for a raw option value if one is available.
   */
  function resolveOptionLabel(column: UiTableColumn | undefined, row: any, raw: unknown) {
    if (!column) return null
    const options = getColumnOptions(column, row)
    if (!options.length) return null
    const rawKey = serializeFilterValue(raw)
    const match = options.find(option => serializeFilterValue(option.value) === rawKey)
    return match?.label ?? null
  }

  const sourceEntries = computed<VisibleRow[]>(() => rows().map((row, originalIndex) => ({ row, originalIndex })))

  const columnOptionsCache = shallowRef(new Map<string, FilterOption[]>())
  const activeMenuOptionsRef = shallowRef<FilterOption[]>([])

  const activeMenuOptions = computed<FilterOption[]>(() => activeMenuOptionsRef.value)
  const activeMenuSelectedSet = computed(() => {
    if (defaultAllSelected.value) {
      return new Set(activeMenuOptionsRef.value.map(option => option.key))
    }
    return new Set(filterMenuState.selectedKeys)
  })
  const visibleMenuOptions = computed(() => activeMenuOptionsRef.value)

  const isSelectAllChecked = computed(() => {
    const options = visibleMenuOptions.value
    if (!options.length) return false
    if (defaultAllSelected.value) return true
    return options.every(option => activeMenuSelectedSet.value.has(option.key))
  })

  const isSelectAllIndeterminate = computed(() => {
    const options = visibleMenuOptions.value
    if (!options.length) return false
    if (defaultAllSelected.value) return false
    let selectedCount = 0
    for (const option of options) {
      if (activeMenuSelectedSet.value.has(option.key)) {
        selectedCount += 1
      }
    }
    return selectedCount > 0 && selectedCount < options.length
  })

  function collectColumnOptions(columnKey: string): FilterOption[] {
    const column = getColumnByKey(localColumns.value, columnKey)
    if (!column || column.isSystem) return []
    const entries = applyFilters(sourceEntries.value, columnKey)
    const seen = new Map<string, FilterOption>()
    for (const entry of entries) {
      const raw = entry.row?.[columnKey]
      const key = serializeFilterValue(raw)
      if (seen.has(key)) continue
      const labelFromOptions = resolveOptionLabel(column, entry.row, raw)
      seen.set(key, {
        key,
        raw,
        label: labelFromOptions ?? formatFilterLabel(raw),
      })
    }
    const options = Array.from(seen.values())
    options.sort((a, b) => a.label.localeCompare(b.label, undefined, { sensitivity: "base" }))
    return options
  }

  function getAllOptionsForColumn(columnKey: string): FilterOption[] {
    const cache = columnOptionsCache.value
    if (!cache.has(columnKey)) {
      cache.set(columnKey, collectColumnOptions(columnKey))
    }
    return cache.get(columnKey) ?? []
  }

  function clearCachedOptions(columnKey?: string | null) {
    if (columnKey) {
      columnOptionsCache.value.delete(columnKey)
      if (filterMenuState.columnKey === columnKey) {
        activeMenuOptionsRef.value = []
      }
      return
    }
    columnOptionsCache.value = new Map<string, FilterOption[]>()
    activeMenuOptionsRef.value = []
  }

  function initializeSelection(columnKey: string, options: FilterOption[]) {
    const applied = columnFilters.value[columnKey]
    if (applied && applied.length) {
      const available = new Set(options.map(option => option.key))
      const normalized = applied.filter(key => available.has(key))
      filterMenuState.selectedKeys = normalized.length ? [...normalized] : [...applied]
      defaultAllSelected.value = false
    } else {
      filterMenuState.selectedKeys = []
      defaultAllSelected.value = true
    }
  }

  async function loadActiveMenuOptions(search: string, overrideOptions?: FilterOption[]): Promise<FilterOption[]> {
    const columnKey = filterMenuState.columnKey
    filterMenuState.search = search
    if (!columnKey) {
      activeMenuOptionsRef.value = []
      return []
    }
    await nextTick()
    const allOptions = overrideOptions ?? getAllOptionsForColumn(columnKey)
    if (overrideOptions) {
      columnOptionsCache.value.set(columnKey, overrideOptions)
    }
    if (!allOptions.length) {
      activeMenuOptionsRef.value = []
      return []
    }
    if (!selectionInitialized.value) {
      initializeSelection(columnKey, allOptions)
      selectionInitialized.value = true
    }
    const normalized = search.trim().toLowerCase()
    const nextOptions = normalized
      ? allOptions.filter(option => option.label.toLowerCase().includes(normalized))
      : allOptions.slice()
    activeMenuOptionsRef.value = nextOptions
    return nextOptions
  }

  watch(sourceEntries, () => {
    clearCachedOptions()
    selectionInitialized.value = false
  })

  watch(
    () => [columnFilters.value, filtersState.value],
    () => {
      clearCachedOptions(filterMenuState.columnKey)
      selectionInitialized.value = false
    },
    { deep: true }
  )

  /**
   * Returns true when the column uses checkbox-driven set filters.
   */
  function isSetFilterColumn(columnKey: string | null): boolean {
    if (!columnKey) return false
    const column = localColumns.value.find(col => col.key === columnKey)
    if (column?.isSystem) return false
    const type = column?.filterType ?? "set"
    return type === "set"
  }

  function isRowSuspended(
    columnKey: string,
    entry: VisibleRow,
    suspendedMap?: Map<string, Set<number>> | null | undefined,
  ) {
    const suspended = suspendedMap ?? getSuspendedRows?.()
    if (!suspended?.size) return false
    const originalIndex = entry.originalIndex
    if (typeof originalIndex !== "number") return false
    const set = suspended.get(columnKey)
    return Boolean(set?.has(originalIndex))
  }

  /**
   * Applies both set-filter and advanced-filter predicates to visible rows.
   */
  function applyFilters<T extends VisibleRow>(entries: T[], skipColumnKey?: string): T[] {
    const suspended = getSuspendedRows?.()
    const setFiltered = entries.filter(entry => {
      for (const key of Object.keys(columnFilters.value)) {
        if (skipColumnKey && key === skipColumnKey) continue
        const allowed = columnFilters.value[key]
        if (!allowed) continue
        const valueKey = serializeFilterValue(entry.row?.[key])
        if (!allowed.includes(valueKey)) {
          if (isRowSuspended(key, entry, suspended)) {
            continue
          }
          return false
        }
      }
      return true
    })
    return applyAdvancedFilters(setFiltered, skipColumnKey, suspended)
  }

  /**
   * Notifies listeners about the latest set-filter selections.
   */
  function emitFilters() {
    const payload: Record<string, string[]> = {}
    for (const [key, value] of Object.entries(columnFilters.value)) {
      payload[key] = Array.isArray(value) ? [...value] : []
    }
    emitFilterChange(payload)
  }

  /**
   * Invokes the stored close handler without clearing columnKey directly.
   */
  function closeActiveMenu() {
    const closeFn = activeMenuClose.value
    activeMenuClose.value = null
    closeFn?.()
    // Do not clear filterMenuState.columnKey here so the caller can manage it.
  }

  /**
   * Opens a filter menu for the specified column and stores its close handler.
   */
  function onColumnMenuOpen(columnKey: string, closeFn: () => void) {
    if (filterMenuState.columnKey && filterMenuState.columnKey !== columnKey) {
      activeMenuClose.value?.()
    }
    activeMenuClose.value = closeFn
    filterMenuState.columnKey = columnKey
    // Load options immediately when menu opens
    loadActiveMenuOptions("")
  }

  /**
   * Clears menu state when the given column finishes closing its popover.
   */
  function onColumnMenuClose(columnKey: string) {
    if (filterMenuState.columnKey === columnKey) {
      filterMenuState.columnKey = null // Reset filter state only here
    }
    activeMenuClose.value = null
  }

  /**
   * Commits the pending menu selections into columnFilters and emits an update.
   */
  function confirmFilterSelection() {
    if (!filterMenuState.columnKey) return
    const columnKey = filterMenuState.columnKey
    const allOptions = getAllOptionsForColumn(columnKey)
    const visibleOptions = visibleMenuOptions.value
    const allKeys = allOptions.map(option => option.key)
    const availableSet = new Set(allKeys)

    const searchActive = Boolean(filterMenuState.search.trim())
    let uniqueSelected: string[]

    if (defaultAllSelected.value) {
      // Preserve explicit selections even when the default-all flag is still true.
      uniqueSelected = filterMenuState.selectedKeys.length
        ? Array.from(new Set(filterMenuState.selectedKeys))
        : [...allKeys]
    } else {
      uniqueSelected = Array.from(new Set(filterMenuState.selectedKeys))
    }

    const visibleKeys = visibleOptions.map(option => option.key)
    if (searchActive && (defaultAllSelected.value || uniqueSelected.length === allKeys.length)) {
      uniqueSelected = visibleKeys
    }

    const normalizedSelection = uniqueSelected.filter(key => availableSet.has(key))
    if (normalizedSelection.length) {
      uniqueSelected = normalizedSelection
    }

    const nextFilters = { ...columnFilters.value }
    const selectingAllOptions =
      !searchActive &&
      uniqueSelected.length === allKeys.length &&
      defaultAllSelected.value &&
      filterMenuState.selectedKeys.length === 0
    if (selectingAllOptions) {
      delete nextFilters[columnKey]
      defaultAllSelected.value = true
      filterMenuState.selectedKeys = []
    } else {
      nextFilters[columnKey] = uniqueSelected
      defaultAllSelected.value = false
      filterMenuState.selectedKeys = [...uniqueSelected]
    }
    columnFilters.value = nextFilters
    emitFilters()
  }

  /**
   * No-op placeholder; the popover itself restores selection state on cancel.
   */
  function cancelFilterSelection() {
    // State cleanup handled when the popover reports it closed.
  }

  /**
   * Removes stored filters for the provided column key and emits the change.
   */
  function clearFilterForColumn(columnKey: string) {
    if (columnFilters.value[columnKey] !== undefined) {
      const next = { ...columnFilters.value }
      delete next[columnKey]
      columnFilters.value = next
      if (filterMenuState.columnKey === columnKey) {
        defaultAllSelected.value = true
        filterMenuState.selectedKeys = []
      }
      emitFilters()
    }
  }

  /**
   * Selects or clears every option in the visible set-filter list.
   */
  function toggleSelectAll(checked: boolean) {
    if (!filterMenuState.columnKey) return
    if (!isSetFilterColumn(filterMenuState.columnKey)) return
    if (checked) {
      defaultAllSelected.value = true
      filterMenuState.selectedKeys = []
      return
    }
    defaultAllSelected.value = false
    filterMenuState.selectedKeys = []
  }

  /**
   * Toggles a single option key within the current set-filter selection.
   */
  function toggleFilterOption(optionKey: string) {
    if (!isSetFilterColumn(filterMenuState.columnKey)) return
    const columnKey = filterMenuState.columnKey
    if (!columnKey) return
    const allOptions = getAllOptionsForColumn(columnKey)
    const fullSet = new Set(allOptions.map(option => option.key))
    const workingSet = defaultAllSelected.value ? new Set(fullSet) : new Set(filterMenuState.selectedKeys)

    if (workingSet.has(optionKey)) {
      workingSet.delete(optionKey)
    } else {
      workingSet.add(optionKey)
    }

    if (workingSet.size === fullSet.size && fullSet.size > 0) {
      defaultAllSelected.value = true
      filterMenuState.selectedKeys = []
    } else {
      defaultAllSelected.value = false
      filterMenuState.selectedKeys = Array.from(workingSet)
    }
  }

  /**
   * Updates the selection when the "select all" checkbox toggles.
   */
  function onSelectAllChange(event: Event) {
    const target = event.target as HTMLInputElement | null
    toggleSelectAll(Boolean(target?.checked))
  }

  /**
   * Stores a reference to the menu search input for focus management.
   */
  function setFilterMenuSearchRef(element: Element | { $el?: Element | null } | null) {
    if (!element) {
      filterMenuSearchRef.value = null
      return
    }

    if (element instanceof HTMLInputElement) {
      filterMenuSearchRef.value = element
      return
    }

    if (element instanceof Element) {
      const input = element.querySelector("input")
      filterMenuSearchRef.value = (input as HTMLInputElement) ?? null
      return
    }

    const instanceEl = element.$el as Element | undefined
    if (!instanceEl) {
      filterMenuSearchRef.value = null
      return
    }
    if (instanceEl instanceof HTMLInputElement) {
      filterMenuSearchRef.value = instanceEl
      return
    }
    const nested = instanceEl.querySelector("input")
    filterMenuSearchRef.value = (nested as HTMLInputElement) ?? null
  }

  /**
   * Evaluates a text clause against the candidate value.
   */
  function evaluateTextClause(clause: FilterClause | null, candidate: any): boolean {
    if (!clause || !clause.operator) return true
    const candidateValue = candidate == null ? "" : String(candidate).toLowerCase()
    const target = clause.value == null ? "" : String(clause.value).toLowerCase()
    if (!target && clause.operator !== "equals") return true
    switch (clause.operator) {
      case "contains":
        return candidateValue.includes(target)
      case "startsWith":
        return candidateValue.startsWith(target)
      case "endsWith":
        return candidateValue.endsWith(target)
      case "equals":
        return candidateValue === target
      default:
        return true
    }
  }

  /**
   * Evaluates a numeric clause, coercing values via toNumberValue.
   */
  function evaluateNumericClause(clause: FilterClause | null, candidate: any): boolean {
    if (!clause || !clause.operator) return true
    const candidateValue = toNumberValue(candidate)
    if (candidateValue === null) return false
    const primary = toNumberValue(clause.value)
    const secondary = toNumberValue(clause.value2)
    switch (clause.operator) {
      case "equals":
        return primary === null ? true : candidateValue === primary
      case ">":
        return primary === null ? true : candidateValue > primary
      case "<":
        return primary === null ? true : candidateValue < primary
      case ">=":
        return primary === null ? true : candidateValue >= primary
      case "<=":
        return primary === null ? true : candidateValue <= primary
      case "between":
        if (primary === null && secondary === null) return true
        if (primary === null) return candidateValue <= (secondary ?? candidateValue)
        if (secondary === null) return candidateValue >= primary
        return candidateValue >= primary && candidateValue <= secondary
      default:
        return true
    }
  }

  /**
   * Evaluates a date clause, handling range comparisons.
   */
  function evaluateDateClause(clause: FilterClause | null, candidate: any): boolean {
    if (!clause || !clause.operator) return true
    const candidateValue = toDateValue(candidate)
    if (candidateValue === null) return false
    const primary = toDateValue(clause.value)
    const secondary = toDateValue(clause.value2)
    switch (clause.operator) {
      case "equals":
        return primary === null ? true : candidateValue === primary
      case ">":
        return primary === null ? true : candidateValue > primary
      case "<":
        return primary === null ? true : candidateValue < primary
      case ">=":
        return primary === null ? true : candidateValue >= primary
      case "<=":
        return primary === null ? true : candidateValue <= primary
      case "between":
        if (primary === null && secondary === null) return true
        if (primary === null) return candidateValue <= (secondary ?? candidateValue)
        if (secondary === null) return candidateValue >= primary
        return candidateValue >= primary && candidateValue <= secondary
      default:
        return true
    }
  }

  /**
   * Applies the appropriate evaluator for an advanced filter condition.
   */
  function evaluateAdvancedCondition(condition: FilterCondition | undefined, value: any): boolean {
    if (!condition || !condition.clauses?.length) return true

    let accumulated: boolean | null = null

    for (const clause of condition.clauses) {
      let clauseResult = true
      switch (condition.type) {
        case "text":
          clauseResult = evaluateTextClause(clause, value)
          break
        case "number":
          clauseResult = evaluateNumericClause(clause, value)
          break
        case "date":
          clauseResult = evaluateDateClause(clause, value)
          break
        default:
          clauseResult = true
      }

      if (accumulated === null) {
        accumulated = clauseResult
        continue
      }

      const join = clause.join ?? "and"
      accumulated = join === "or" ? accumulated || clauseResult : accumulated && clauseResult
    }

    return accumulated ?? true
  }

  function resolveAdvancedCandidateValue(
    condition: FilterCondition | undefined,
    column: UiTableColumn | undefined,
    row: any,
    key: string
  ) {
    if (!condition) return row?.[key]
    const rawValue = row?.[key]
    if (condition.type === "text" && column) {
      const label = resolveOptionLabel(column, row, rawValue)
      if (label !== null && label !== undefined) {
        return label
      }
    }
    return rawValue
  }

  /**
   * Applies advanced filter conditions defined per column to the dataset.
   */
  function applyAdvancedFilters<T extends VisibleRow>(
    entries: T[],
    skipColumnKey?: string,
    suspendedRows?: Map<string, Set<number>> | null | undefined,
  ): T[] {
    const suspended = suspendedRows ?? getSuspendedRows?.()
    const keys = Object.keys(filtersState.value)
    if (!keys.length) return entries
    return entries.filter(entry => {
      for (const key of keys) {
        if (skipColumnKey && key === skipColumnKey) continue
        if (isRowSuspended(key, entry, suspended)) {
          continue
        }
        const condition = filtersState.value[key]
        const column = getColumnByKey(localColumns.value, key)
        const candidate = resolveAdvancedCandidateValue(condition, column, entry.row, key)
        if (!evaluateAdvancedCondition(condition, candidate)) {
          return false
        }
      }
      return true
    })
  }

  function setAdvancedFilter(columnKey: string, condition: FilterCondition | null) {
    const next = { ...filtersState.value }
    if (!condition || !condition.clauses.length) {
      delete next[columnKey]
    } else {
      next[columnKey] = {
        type: condition.type,
        clauses: condition.clauses.map(clause => ({ ...clause })),
      }
    }
    filtersState.value = next
    emitFilters()
  }

  function getAdvancedFilter(columnKey: string): FilterCondition | null {
    const existing = filtersState.value[columnKey]
    if (!existing) return null
    return {
      type: existing.type,
      clauses: existing.clauses ? existing.clauses.map(clause => ({ ...clause })) : [],
    }
  }

  function clearAdvancedFilter(columnKey: string) {
    if (!filtersState.value[columnKey]) return
    const next = { ...filtersState.value }
    delete next[columnKey]
    filtersState.value = next
    emitFilters()
  }

  function resetAllFilters() {
    closeActiveMenu()
    columnFilters.value = {}
    filtersState.value = {}
    filterMenuState.columnKey = null
    filterMenuState.selectedKeys = []
    filterMenuState.search = ""
    activeMenuOptionsRef.value = []
    filterMenuSearchRef.value = null
    selectionInitialized.value = false
    defaultAllSelected.value = true
    clearCachedOptions()
    emitFilters()
  }

  function isFilterActiveForColumn(columnKey: string) {
    const column = localColumns.value.find(col => col.key === columnKey)
    if (column?.isSystem) return false
    if (filtersState.value[columnKey]?.clauses?.length) return true
    const applied = columnFilters.value[columnKey]
    if (!applied) return false
    const total = getAllOptionsForColumn(columnKey).length
    if (total === 0) return applied.length > 0
    return applied.length !== total
  }

  watch(
    () => filterMenuState.columnKey,
    columnKey => {
      activeMenuOptionsRef.value = []
      selectionInitialized.value = false
      if (columnKey && isSetFilterColumn(columnKey)) {
        const applied = columnFilters.value[columnKey]
        if (applied && applied.length) {
          filterMenuState.selectedKeys = [...applied]
          defaultAllSelected.value = false
        } else {
          filterMenuState.selectedKeys = []
          defaultAllSelected.value = true
        }
        filterMenuState.search = ""
        nextTick(() => filterMenuSearchRef.value?.focus())
      } else {
        filterMenuState.search = ""
        filterMenuState.selectedKeys = []
        defaultAllSelected.value = true
        filterMenuSearchRef.value = null
      }
    }
  )

  function cloneColumnFiltersState(source: Record<string, string[]>): Record<string, string[]> {
    const next: Record<string, string[]> = {}
    for (const [key, values] of Object.entries(source ?? {})) {
      if (!Array.isArray(values) || !values.length) continue
      next[key] = values.map(value => String(value))
    }
    return next
  }

  function cloneAdvancedFiltersState(source: Record<string, FilterCondition>): Record<string, FilterCondition> {
    const next: Record<string, FilterCondition> = {}
    for (const [key, condition] of Object.entries(source ?? {})) {
      if (!condition) continue
      const clauses = Array.isArray(condition.clauses)
        ? condition.clauses.map(clause => ({ ...clause }))
        : []
      if (!clauses.length) {
        continue
      }
      next[key] = {
        type: condition.type,
        clauses,
      }
    }
    return next
  }

  function getFilterStateSnapshot(): FilterStateSnapshot {
    return {
      columnFilters: cloneColumnFiltersState(columnFilters.value),
      advancedFilters: cloneAdvancedFiltersState(filtersState.value),
    }
  }

  function setFilterStateSnapshot(snapshot: FilterStateSnapshot | null | undefined) {
    if (!snapshot) {
      resetAllFilters()
      return
    }

    const nextColumnFilters = cloneColumnFiltersState(snapshot.columnFilters ?? {})
    const nextAdvancedFilters = cloneAdvancedFiltersState(snapshot.advancedFilters ?? {})

    columnFilters.value = nextColumnFilters
    filtersState.value = nextAdvancedFilters
    filterMenuState.columnKey = null
    filterMenuState.selectedKeys = []
    filterMenuState.search = ""
    selectionInitialized.value = false
    defaultAllSelected.value = Object.keys(nextColumnFilters).length === 0
    clearCachedOptions()
    emitFilters()
  }

  return {
    columnFilters,
    filtersState,
    filterMenuState,
    activeMenuOptions,
    activeMenuSelectedSet,
    isSelectAllChecked,
    isSelectAllIndeterminate,
    isFilterActiveForColumn,
    onColumnMenuOpen,
    onColumnMenuClose,
    confirmFilterSelection,
    cancelFilterSelection,
    clearFilterForColumn,
    toggleSelectAll,
    toggleFilterOption,
    onSelectAllChange,
    setFilterMenuSearchRef,
    closeActiveMenu,
    loadActiveMenuOptions,
    applyFilters,
    applyAdvancedFilters,
    setAdvancedFilter,
    getAdvancedFilter,
    clearAdvancedFilter,
    resetAllFilters,
    getFilterStateSnapshot,
    setFilterStateSnapshot,
  }
}
