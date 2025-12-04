import { computed } from "vue"
import { ALIGN_CLASS_MAP, normalizeAlign } from "./cellUtils.js"
import type { AlignOption, CellProps } from "./cellUtils.js"
import type { ComputedRef, Ref } from "vue"

interface CellStyleContext {
  editing: Ref<boolean>
  isRowSelected: ComputedRef<boolean>
  isColumnSelected: ComputedRef<boolean>
  isRangeSelected: ComputedRef<boolean>
  validationError: ComputedRef<string | null>
  isSearchMatch: ComputedRef<boolean>
  isActiveSearchMatch: ComputedRef<boolean>
  isSystemColumn: ComputedRef<boolean>
  isSelectionColumn: ComputedRef<boolean>
}

export function useCellStyle(props: CellProps, context: CellStyleContext) {
  const columnAlign = computed<AlignOption>(() => normalizeAlign(props.col.align, "left"))
  const columnTextAlignClass = computed(() => ALIGN_CLASS_MAP[columnAlign.value])
  const stickySide = computed<"left" | "right" | null>(() => props.stickySide ?? (props.sticky ? "left" : null))
  const validationShadowColor = "var(--ui-table-selection-validation-shadow, rgba(248, 113, 113, 0.75))"
  const searchShadowColor = "var(--ui-table-selection-search-match-shadow, rgba(250, 204, 21, 0.7))"
  const activeSearchShadowColor = "var(--ui-table-selection-search-match-active-shadow, rgba(37, 99, 235, 0.85))"

  const inactiveErrorShadow = computed(() => {
    if (!context.validationError.value) return ""
    if (context.editing.value) return ""
    return `inset 0 0 0 1px ${validationShadowColor}`
  })

  const searchShadow = computed(() => {
    if (!context.isSearchMatch.value && !context.isActiveSearchMatch.value) return ""
    if (context.isActiveSearchMatch.value) {
      return `inset 0 0 0 2px ${activeSearchShadowColor}`
    }
    return `inset 0 0 0 2px ${searchShadowColor}`
  })

  const combinedShadow = computed(() => {
    const parts = [] as string[]
    if (inactiveErrorShadow.value) parts.push(inactiveErrorShadow.value)
    if (searchShadow.value) parts.push(searchShadow.value)
    return parts.join(", ")
  })

  const hasPriorityBackground = computed(() => {
    if (context.editing.value) return true
    if (context.isRowSelected.value || context.isColumnSelected.value || context.isRangeSelected.value) return true
    if (context.validationError.value) return true
    if (context.isActiveSearchMatch.value || context.isSearchMatch.value) return true
    return false
  })

  const pinnedBackground = computed(() => {
    if (!stickySide.value) return ""
    if (hasPriorityBackground.value) return ""
    if (stickySide.value === "left") {
      return "var(--ui-table-pinned-left-bg, var(--ui-table-pinned-bg, var(--ui-table-row-background-color)))"
    }
    if (stickySide.value === "right") {
      return "var(--ui-table-pinned-right-bg, var(--ui-table-pinned-bg, var(--ui-table-row-background-color)))"
    }
    return ""
  })

  const pinnedDividerGradient = computed(() => {
    if (!stickySide.value) return ""
    if (stickySide.value === "left") {
      return "linear-gradient(to left, transparent calc(100% - var(--ui-table-pinned-left-divider-width, var(--ui-table-pinned-left-border-width, 1px))), var(--ui-table-pinned-left-divider-color, var(--ui-table-pinned-left-border-color, var(--ui-table-cell-border-color))) calc(100% - var(--ui-table-pinned-left-divider-width, var(--ui-table-pinned-left-border-width, 1px))))"
    }
    if (stickySide.value === "right") {
      return "linear-gradient(to right, transparent calc(100% - var(--ui-table-pinned-right-divider-width, var(--ui-table-pinned-right-border-width, 1px))), var(--ui-table-pinned-right-divider-color, var(--ui-table-pinned-right-border-color, var(--ui-table-cell-border-color))) calc(100% - var(--ui-table-pinned-right-divider-width, var(--ui-table-pinned-right-border-width, 1px))))"
    }
    return ""
  })

  const highlightActive = computed(() => {
    if (context.isSystemColumn.value || context.editing.value) return false
    const hasSelection =
      context.isRowSelected.value ||
      context.isColumnSelected.value ||
      context.isRangeSelected.value
    if (hasSelection) return true
    const hasSearch = context.isActiveSearchMatch.value || context.isSearchMatch.value
    if (hasSearch) return true
    if (context.validationError.value) return true
    return false
  })

  const cellStyle = computed(() => {
    const styles: Record<string, string> = {}
    let stickyApplied = false

    if (stickySide.value === "left") {
      styles.position = "sticky"
      styles.left = props.stickyLeftOffset != null ? `${props.stickyLeftOffset}px` : "0px"
      styles.zIndex = "10"
      stickyApplied = true
    } else if (stickySide.value === "right") {
      styles.position = "sticky"
      styles.right = props.stickyRightOffset != null ? `${props.stickyRightOffset}px` : "0px"
      styles.zIndex = "10"
      stickyApplied = true
    }

    if (props.stickyTop) {
      styles.position = "sticky"
      styles.top = props.stickyTopOffset != null ? `${props.stickyTopOffset}px` : "0px"
      styles.zIndex = stickyApplied ? "12" : "11"
      stickyApplied = true
    }

    if (context.isSelectionColumn.value) {
      const current = Number(styles.zIndex ?? 0)
      styles.zIndex = String(current > 0 ? Math.max(current, 40) : 40)
    }

    if (combinedShadow.value) {
      styles.boxShadow = styles.boxShadow ? `${styles.boxShadow}, ${combinedShadow.value}` : combinedShadow.value
    }

    if (pinnedBackground.value) {
      styles.backgroundColor = pinnedBackground.value
    }

    if (pinnedDividerGradient.value) {
      styles.backgroundImage = pinnedDividerGradient.value
      styles.backgroundRepeat = "no-repeat"
      styles.backgroundSize = "100% 100%"
    }

    return styles
  })

  return {
    columnTextAlignClass,
    cellStyle,
    combinedShadow,
    highlightActive,
    stickySide,
  }
}
