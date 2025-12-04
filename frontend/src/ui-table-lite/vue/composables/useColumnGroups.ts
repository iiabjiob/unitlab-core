import { computed, ref, type ComputedRef } from "vue"
import type { UiTableColumn } from "../../core/types"
import { buildColumnTree, type ColumnGroup } from "../../core/columns/columnGroup"
import type { UiTableColumnGroupDef } from "../../core/types/column"

export interface UseColumnGroupsOptions {
  columns: () => UiTableColumn[]
  groupDefs: () => UiTableColumnGroupDef[]
}

export interface UseColumnGroupsResult {
  rootGroups: ComputedRef<ColumnGroup[]>
  ungroupedColumns: ComputedRef<UiTableColumn[]>
  displayedLeafColumns: ComputedRef<UiTableColumn[]>
}

export function useColumnGroups(options: UseColumnGroupsOptions): UseColumnGroupsResult {
  const createExpandedSignal = (initial: boolean) => ref(initial)

  const tree = computed(() => {
    const columnDefs = options.columns()
    const groupDefs = options.groupDefs()
    if (!groupDefs.length) {
      return {
        roots: [] as ColumnGroup[],
        unusedColumns: columnDefs,
      }
    }
    return buildColumnTree(columnDefs, groupDefs, {
      createSignal: createExpandedSignal,
    })
  })

  const rootGroups = computed(() => tree.value.roots)

  const ungroupedColumns = computed(() => tree.value.unusedColumns)

  const displayedLeafColumns = computed(() => {
    if (!rootGroups.value.length) {
      return options.columns()
    }
    const leaves: UiTableColumn[] = []
    for (const group of rootGroups.value) {
      leaves.push(...group.getDisplayedLeafColumns())
    }
    return leaves
  })

  return {
    rootGroups,
    ungroupedColumns,
    displayedLeafColumns,
  }
}
