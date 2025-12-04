import { computed } from "vue"
import type { ComputedRef, Ref } from "vue"
import type { HeaderRenderableEntry } from "../../core/types/internal"
import { buildColumnGroupRenderRows, type ColumnGroupRenderRows, type ColumnGroup } from "../../core/columns/columnGroup"
import type { UiTableColumn } from "../../core/types"
import type { ColumnMetric } from "./useTableViewport"

export interface UseTableHeaderLayoutOptions {
  rootColumnGroups: ComputedRef<ColumnGroup[]>
  ungroupedColumns: ComputedRef<UiTableColumn[]>
  pinnedLeftEntries: Ref<ColumnMetric[]>
  pinnedRightEntries: Ref<ColumnMetric[]>
  visibleScrollableEntries: Ref<ColumnMetric[]>
  leftPadding: Ref<number>
  rightPadding: Ref<number>
}

export interface UseTableHeaderLayoutResult {
  headerRenderableEntries: ComputedRef<HeaderRenderableEntry[]>
  headerPinnedLeftEntries: ComputedRef<HeaderRenderableEntry[]>
  headerMainEntries: ComputedRef<HeaderRenderableEntry[]>
  headerPinnedRightEntries: ComputedRef<HeaderRenderableEntry[]>
  columnTrackStartMap: ComputedRef<Map<string, number>>
  pinnedLeftTrackStartMap: ComputedRef<Map<string, number>>
  mainTrackStartMap: ComputedRef<Map<string, number>>
  pinnedRightTrackStartMap: ComputedRef<Map<string, number>>
  headerGroupRows: ComputedRef<ColumnGroupRenderRows>
  headerPinnedLeftGroupRows: ComputedRef<ColumnGroupRenderRows>
  headerMainGroupRows: ComputedRef<ColumnGroupRenderRows>
  headerPinnedRightGroupRows: ComputedRef<ColumnGroupRenderRows>
  groupedColumnKeys: ComputedRef<Set<string> | null>
  hasColumnGroups: ComputedRef<boolean>
  visibleColumnEntries: ComputedRef<HeaderRenderableEntry[]>
}

export function useTableHeaderLayout(options: UseTableHeaderLayoutOptions): UseTableHeaderLayoutResult {
  const pinnedLeftHeaderEntries = computed<HeaderRenderableEntry[]>(() =>
    options.pinnedLeftEntries.value.map(metric => ({ metric, showLeftFiller: false, showRightFiller: false })),
  )

  const mainHeaderEntries = computed<HeaderRenderableEntry[]>(() => {
    const entries: HeaderRenderableEntry[] = []
    const includeLeftFiller = options.leftPadding.value > 0
    const includeRightFiller = options.rightPadding.value > 0
    const scrollable = options.visibleScrollableEntries.value

    scrollable.forEach((metric, index) => {
      entries.push({
        metric,
        showLeftFiller: includeLeftFiller && index === 0,
        showRightFiller: includeRightFiller && index === scrollable.length - 1,
      })
    })

    return entries
  })

  const pinnedRightHeaderEntries = computed<HeaderRenderableEntry[]>(() =>
    options.pinnedRightEntries.value.map(metric => ({ metric, showLeftFiller: false, showRightFiller: false })),
  )

  const headerRenderableEntries = computed<HeaderRenderableEntry[]>(() => [
    ...pinnedLeftHeaderEntries.value,
    ...mainHeaderEntries.value,
    ...pinnedRightHeaderEntries.value,
  ])

  const buildTrackStartMap = (entries: HeaderRenderableEntry[]): Map<string, number> => {
    const map = new Map<string, number>()
    let trackIndex = 1

    entries.forEach(entry => {
      if (entry.showLeftFiller) {
        trackIndex += 1
      }

      map.set(entry.metric.column.key, trackIndex)
      trackIndex += 1

      if (entry.showRightFiller) {
        trackIndex += 1
      }
    })

    return map
  }

  const columnTrackStartMap = computed(() => buildTrackStartMap(headerRenderableEntries.value))
  const pinnedLeftTrackStartMap = computed(() => buildTrackStartMap(pinnedLeftHeaderEntries.value))
  const mainTrackStartMap = computed(() => buildTrackStartMap(mainHeaderEntries.value))
  const pinnedRightTrackStartMap = computed(() => buildTrackStartMap(pinnedRightHeaderEntries.value))

  const buildGroupRows = (trackMap: Map<string, number>): ColumnGroupRenderRows => {
    if (!options.rootColumnGroups.value.length || !trackMap.size) return []
    return buildColumnGroupRenderRows(options.rootColumnGroups.value, trackMap)
  }

  const headerGroupRows = computed<ColumnGroupRenderRows>(() => buildGroupRows(columnTrackStartMap.value))
  const headerPinnedLeftGroupRows = computed<ColumnGroupRenderRows>(() => buildGroupRows(pinnedLeftTrackStartMap.value))
  const headerMainGroupRows = computed<ColumnGroupRenderRows>(() => buildGroupRows(mainTrackStartMap.value))
  const headerPinnedRightGroupRows = computed<ColumnGroupRenderRows>(() => buildGroupRows(pinnedRightTrackStartMap.value))

  const groupedColumnKeys = computed<Set<string> | null>(() => {
    if (!options.rootColumnGroups.value.length) return null
    const keys = new Set<string>()
    options.rootColumnGroups.value.forEach(group => {
      group.getDisplayedLeafColumns().forEach(column => {
        keys.add(column.key)
      })
    })
    options.ungroupedColumns.value.forEach(column => keys.add(column.key))
    return keys
  })

  const hasColumnGroups = computed(() => headerGroupRows.value.length > 0 && (groupedColumnKeys.value?.size ?? 0) > 0)

  const visibleColumnEntries = computed(() => headerRenderableEntries.value)

  return {
    headerRenderableEntries,
    headerPinnedLeftEntries: pinnedLeftHeaderEntries,
    headerMainEntries: mainHeaderEntries,
    headerPinnedRightEntries: pinnedRightHeaderEntries,
    columnTrackStartMap,
    pinnedLeftTrackStartMap,
    mainTrackStartMap,
    pinnedRightTrackStartMap,
    headerGroupRows,
    headerPinnedLeftGroupRows,
    headerMainGroupRows,
    headerPinnedRightGroupRows,
    groupedColumnKeys,
    hasColumnGroups,
    visibleColumnEntries,
  }
}
