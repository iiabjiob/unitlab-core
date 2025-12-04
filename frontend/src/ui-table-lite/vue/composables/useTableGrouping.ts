import { computed, ref, shallowRef, watch, type ComputedRef } from "vue"
import type { UiTableColumn, VisibleRow } from "../../core/types"
import type { RowData, RowKey } from "@/composables/useSelectableRows"
import type { UiTableSettingsAdapter } from "@/ui-table/core/tableSettingsAdapter"

export interface GroupRowData {
  __group: true
  key: string
  columnKey: string
  value: any
  level: number
  size: number
  [key: string]: unknown
}

interface UseTableGroupingOptions {
  sortedRows: ComputedRef<VisibleRow[]>
  settingsAdapter: ComputedRef<UiTableSettingsAdapter>
  tableId: ComputedRef<string>
  isPlaceholderRow?: (row: unknown) => boolean
}

interface GroupNode {
  key: string
  columnKey: string
  value: any
  level: number
  rows: VisibleRow[]
  children: GroupNode[]
  expanded: boolean
}

export const GROUP_INDENT_BASE = 12
export const GROUP_INDENT_STEP = 16

export function useTableGrouping(options: UseTableGroupingOptions) {
  const groupState = ref<{ key: string; expanded: boolean }[]>([])
  const groupExpansion = ref<Record<string, boolean>>({})
  const groupHydrated = ref(false)

  const placeholderPredicate = options.isPlaceholderRow ?? null

  const effectiveSortedRows = computed(() => {
    const rows = options.sortedRows.value
    if (!placeholderPredicate) {
      return rows
    }
    if (!rows.length) {
      return rows
    }
    return rows.filter(entry => !placeholderPredicate(entry.row))
  })

  const groupedColumns = computed(() => groupState.value.map(group => group.key))

  const groupedColumnSet = computed(() => {
    const set = new Set<string>()
    groupState.value.forEach(entry => set.add(entry.key))
    return set
  })

  const groupOrderMap = computed(() => {
    const map = new Map<string, number>()
    groupState.value.forEach((entry, index) => {
      map.set(entry.key, index + 1)
    })
    return map
  })

  const refreshCallback = shallowRef<(() => void) | null>(null)

  function scheduleRefresh() {
    refreshCallback.value?.()
  }

  function setGroupRefreshCallback(callback: (() => void) | null) {
    refreshCallback.value = callback
  }

  function getGroupExpanded(key: string): boolean {
    const record = groupExpansion.value
    if (Object.prototype.hasOwnProperty.call(record, key)) {
      return record[key]
    }
    return true
  }

  function setGroupExpanded(key: string, expanded: boolean) {
    groupExpansion.value = {
      ...groupExpansion.value,
      [key]: expanded,
    }
    if (groupState.value.some(item => item.key === key)) {
      groupState.value = groupState.value.map(item =>
        item.key === key ? { ...item, expanded } : item,
      )
    }
  }

  function groupRowsByColumns(rows: VisibleRow[], columns: string[]): GroupNode[] {
    if (!columns.length) return []

    const buildLevel = (data: VisibleRow[], level: number): GroupNode[] => {
      const columnKey = columns[level]
      const groupsMap = new Map<any, VisibleRow[]>()
      for (const row of data) {
        const raw = row.row?.[columnKey]
        const value = raw ?? "(blank)"
        if (!groupsMap.has(value)) {
          groupsMap.set(value, [])
        }
        groupsMap.get(value)!.push(row)
      }

      return Array.from(groupsMap.entries()).map(([value, subset]) => {
        const key = `${columnKey}:${String(value)}`
        const children = level < columns.length - 1 ? buildLevel(subset, level + 1) : []
        return {
          key,
          columnKey,
          value,
          level,
          rows: subset,
          children,
          expanded: getGroupExpanded(key),
        }
      })
    }

    return buildLevel(rows, 0)
  }

  function flattenGroupedTree(groups: GroupNode[]): VisibleRow[] {
    const output: VisibleRow[] = []

    const pushGroupRow = (node: GroupNode) => {
      const payload: GroupRowData = {
        __group: true,
        key: node.key,
        columnKey: node.columnKey,
        value: node.value,
        level: node.level,
        size: node.rows.length,
      }

      output.push({
        row: payload,
        rowId: `__group:${node.key}`,
        originalIndex: -1,
        displayIndex: 0,
      })
    }

    const pushDataRow = (entry: VisibleRow) => {
      output.push({
        ...entry,
        displayIndex: 0,
      })
    }

    const traverse = (nodes: GroupNode[]) => {
      for (const node of nodes) {
        pushGroupRow(node)
        if (!node.expanded) {
          continue
        }
        if (node.children.length) {
          traverse(node.children)
        } else {
          node.rows.forEach(pushDataRow)
        }
      }
    }

    traverse(groups)

    return output.map((entry, index) => ({
      ...entry,
      displayIndex: index,
    }))
  }

  const groupedTree = computed(() =>
    groupedColumns.value.length ? groupRowsByColumns(effectiveSortedRows.value, groupedColumns.value) : [],
  )

  const groupRowMembership = computed(() => {
    const map = new Map<string, RowData[]>()
    const collect = (nodes: GroupNode[]) => {
      for (const node of nodes) {
        const rows: RowData[] = []
        for (const entry of node.rows) {
          const candidate = entry?.row as RowData | undefined
          if (!candidate) {
            continue
          }
          if ((candidate as Record<string, unknown>).__group) {
            continue
          }
          rows.push(candidate)
        }
        map.set(node.key, rows)
        if (node.children.length) {
          collect(node.children)
        }
      }
    }

    if (groupedTree.value.length) {
      collect(groupedTree.value)
    }

    return map
  })

  const flattenedGroupedRows = computed(() =>
    groupedColumns.value.length ? flattenGroupedTree(groupedTree.value) : [],
  )

  const processedRows = computed(() =>
    groupedColumns.value.length ? flattenedGroupedRows.value : effectiveSortedRows.value,
  )

  const processedRowIndexMap = computed(() => {
    const map = new Map<RowKey, number>()
    processedRows.value.forEach((entry, index) => {
      if (entry?.rowId !== undefined && entry?.rowId !== null) {
        map.set(entry.rowId, index)
      }
    })
    return map
  })

  function findRowIndexByRowId(rowId: RowKey | null | undefined): number | null {
    if (rowId === null || rowId === undefined) {
      return null
    }
    const index = processedRowIndexMap.value.get(rowId)
    return typeof index === "number" ? index : null
  }

  function getRowIdForDisplayIndex(index: number): RowKey | null {
    const entry = processedRows.value[index]
    return entry?.rowId ?? null
  }

  function findGroupByKey(nodes: GroupNode[], key: string): GroupNode | null {
    for (const node of nodes) {
      if (node.key === key) return node
      const child = findGroupByKey(node.children, key)
      if (child) return child
    }
    return null
  }

  function toggleGroupRow(key: string) {
    if (!groupedColumns.value.length) {
      return
    }
    const node = findGroupByKey(groupedTree.value, key)
    if (!node) {
      return
    }
    const next = !getGroupExpanded(key)
    setGroupExpanded(key, next)
    scheduleRefresh()
  }

  function isGroupExpanded(key: string): boolean {
    return getGroupExpanded(key)
  }

  function onGroupColumn(column: UiTableColumn) {
    if (!column || column.isSystem) {
      return
    }
    const existingIndex = groupState.value.findIndex(item => item.key === column.key)
    if (existingIndex >= 0) {
      groupState.value = groupState.value.filter(item => item.key !== column.key)
    } else {
      groupState.value = [...groupState.value, { key: column.key, expanded: true }]
    }
    groupExpansion.value = {}
    scheduleRefresh()
  }

  function isGroupRowEntry(entry: VisibleRow | null | undefined): entry is VisibleRow & { row: GroupRowData } {
    return Boolean(entry?.row && (entry.row as Record<string, unknown>).__group)
  }

  watch(
    groupState,
    () => {
      scheduleRefresh()
    },
    { deep: true },
  )

  watch(groupedColumns, () => {
    if (!groupHydrated.value) return
    groupExpansion.value = {}
    scheduleRefresh()
  })

  watch(
    [groupState, groupExpansion],
    ([state, expansion]) => {
      if (!groupHydrated.value) return
      const columns = state.map(item => item.key)
      options.settingsAdapter.value.setGroupState(options.tableId.value, columns, { ...expansion })
    },
    { deep: true },
  )

  return {
    groupState,
    groupExpansion,
    groupHydrated,
    groupedColumns,
    groupedColumnSet,
    groupOrderMap,
    groupRowMembership,
    processedRowIndexMap,
    processedRows,
    getGroupExpanded,
    setGroupExpanded,
    findRowIndexByRowId,
    getRowIdForDisplayIndex,
    toggleGroupRow,
    isGroupExpanded,
    onGroupColumn,
    isGroupRowEntry,
    setGroupRefreshCallback,
  }
}
