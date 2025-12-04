import { computed, type ComputedRef } from "vue"
import type { CSSProperties } from "vue"
import type { UiTableColumn, VisibleRow } from "../../core/types"
import type { RowPoolItem } from "./useTableViewport"

interface UseTableStickyColumnsOptions {
  visibleColumns: ComputedRef<UiTableColumn[]>
  pinnedLeftEntries: unknown
  pinnedRightEntries: unknown
  columnWidthDomMap: ComputedRef<Map<string, number>>
  columnWidthMap: unknown
  toDomUnits: (value: number) => number
  processedRows: ComputedRef<VisibleRow[]>
  rowHeightDom: ComputedRef<number>
  pooledRows: ComputedRef<RowPoolItem[]>
  viewportHeight: unknown
  scrollTop: unknown
  selectionColumnKey: string
  splitPinnedLayoutEnabled?: unknown
}

export interface UseTableStickyColumnsResult {
  pinnedLeftKeys: ComputedRef<Set<string>>
  pinnedRightKeys: ComputedRef<Set<string>>
  stickyLeftOffsets: ComputedRef<Map<string, number>>
  stickyRightOffsets: ComputedRef<Map<string, number>>
  stickyBottomOffsets: ComputedRef<Map<number, number>>
  isColumnLeftSticky: (column: UiTableColumn) => boolean
  isColumnRightSticky: (column: UiTableColumn) => boolean
  isColumnSticky: (column: UiTableColumn) => boolean
  getStickySide: (column: UiTableColumn) => "left" | "right" | null
  getStickyLeftOffset: (column: UiTableColumn) => number | undefined
  getStickyRightOffset: (column: UiTableColumn) => number | undefined
  columnStickyStyle: (column: UiTableColumn, baseZIndex?: number) => CSSProperties
  systemColumnStyle: (column: UiTableColumn) => CSSProperties
  summaryCellStyle: (column: UiTableColumn) => CSSProperties
  getStickyTopOffset: (row: VisibleRow) => number | undefined
}

const EMPTY_STRING_MAP = new Map<string, number>()
const EMPTY_NUMBER_MAP = new Map<number, number>()
const EMPTY_SET = new Set<string>()

export function useTableStickyColumns(_: UseTableStickyColumnsOptions): UseTableStickyColumnsResult {
  const pinnedLeftKeys = computed(() => EMPTY_SET)
  const pinnedRightKeys = computed(() => EMPTY_SET)
  const stickyLeftOffsets = computed(() => EMPTY_STRING_MAP)
  const stickyRightOffsets = computed(() => EMPTY_STRING_MAP)
  const stickyBottomOffsets = computed(() => EMPTY_NUMBER_MAP)

  const isColumnLeftSticky = () => false
  const isColumnRightSticky = () => false
  const isColumnSticky = () => false
  const getStickySide = () => null
  const getStickyLeftOffset = () => undefined
  const getStickyRightOffset = () => undefined
  const columnStickyStyle = (): CSSProperties => ({})
  const systemColumnStyle = (): CSSProperties => ({})
  const summaryCellStyle = (): CSSProperties => ({})
  const getStickyTopOffset = () => undefined

  return {
    pinnedLeftKeys,
    pinnedRightKeys,
    stickyLeftOffsets,
    stickyRightOffsets,
    stickyBottomOffsets,
    isColumnLeftSticky,
    isColumnRightSticky,
    isColumnSticky,
    getStickySide,
    getStickyLeftOffset,
    getStickyRightOffset,
    columnStickyStyle,
    systemColumnStyle,
    summaryCellStyle,
    getStickyTopOffset,
  }
}
