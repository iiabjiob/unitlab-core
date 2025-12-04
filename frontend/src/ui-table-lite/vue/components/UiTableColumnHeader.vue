<template>
  <UiColumnHeader
    v-bind="forwardedAttrs"
    :header-class-list="headerCellClassList"
    :header-style="headerCellStyleBinding"
    :column-key="col.key"
    :title="col.label"
    :display-text="displayText"
    :secondary-label="col.label ?? null"
    :header-text-class="headerTextClass"
    :header-justify-class="headerJustifyClass"
    :grouped="grouped"
    :group-order="groupOrder"
    :sort-direction="sortDirection"
    :sort-priority-label="sortPriorityLabel"
    :is-menu-disabled="isMenuDisabled"
    :filter-active="filterActive"
    :filter-button-class="filterButtonClass"
    :is-menu-open="isMenuOpen"
    :show-secondary="showSecondary"
    :menu-slot-bindings="menuSlotBindings"
    :popover-panel-style="popoverPanelStyle"
    :show-left-placeholder="showLeftPlaceholder"
    :left-padding-style="leftPaddingStyle"
    :show-right-placeholder="showRightPlaceholder"
    :right-padding-style="rightPaddingStyle"
    :tab-index="computedTabIndex"
    :aria-sort="ariaSort"
    :aria-col-index="ariaColIndex"
    aria-rowindex="1"
    :menu-button-ref-setter="setMenuButtonElement"
    :popover-panel-ref-setter="setPopoverPanelElement"
    :header-cell-ref-setter="setHeaderCellElement"
    :show-resize-handle="col.resizable"
    :resize-handle-style="resizeHandleStyle"
    @header-click="onHeaderClick"
    @header-keydown="onHeaderKeydown"
    @menu-toggle="toggleMenu"
    @resize-start="startResize"
    @resize-auto="autoResize"
  >
    <template #menu="slotProps">
      <slot name="menu" v-bind="slotProps" />
    </template>
  </UiColumnHeader>
</template>

<script setup lang="ts">
import { computed, useAttrs } from "vue"
import type { UiTableColumn } from "../../core/types"
import UiColumnHeader from "./UiColumnHeader.vue"
import { useColumnHeaderInteractions } from "../composables/useColumnHeaderInteractions"

const headerCellStyle = computed(() => {
  const styles: Record<string, string> = { top: `${topOffset.value}px` }
  const side = stickySide.value
  const stickyZIndex = props.col.isSystem ? "36" : "21"

  if (side === "left") {
    styles.position = "sticky"
    styles.left = stickyLeftOffset.value != null ? `${stickyLeftOffset.value}px` : "0px"
    styles.zIndex = stickyZIndex
  } else if (side === "right") {
    styles.position = "sticky"
    styles.right = stickyRightOffset.value != null ? `${stickyRightOffset.value}px` : "0px"
    styles.zIndex = stickyZIndex
  } else if (props.sticky) {
    styles.position = "sticky"
    styles.left = props.stickyLeftOffset != null ? `${props.stickyLeftOffset}px` : ""
    styles.zIndex = stickyZIndex
  }

  const isPinned = side === "left" || side === "right"
  if (isPinned) {
    const pinnedBackground =
      side === "left"
        ? "var(--ui-table-pinned-left-bg, var(--ui-table-pinned-bg, var(--ui-table-header-background-color)))"
        : "var(--ui-table-pinned-right-bg, var(--ui-table-pinned-bg, var(--ui-table-header-background-color)))"
    const dividerWidth =
      side === "left"
        ? "var(--ui-table-pinned-left-divider-width, var(--ui-table-pinned-left-border-width, 1px))"
        : "var(--ui-table-pinned-right-divider-width, var(--ui-table-pinned-right-border-width, 1px))"
    const dividerColor =
      side === "left"
        ? "var(--ui-table-pinned-left-divider-color, var(--ui-table-pinned-left-border-color, var(--ui-table-cell-border-color)))"
        : "var(--ui-table-pinned-right-divider-color, var(--ui-table-pinned-right-border-color, var(--ui-table-cell-border-color)))"

    styles.backgroundColor = pinnedBackground
    styles.backgroundImage =
      side === "left"
        ? `linear-gradient(to left, transparent calc(100% - ${dividerWidth}), ${dividerColor} calc(100% - ${dividerWidth}))`
        : `linear-gradient(to right, transparent calc(100% - ${dividerWidth}), ${dividerColor} calc(100% - ${dividerWidth}))`
    styles.backgroundRepeat = "no-repeat"
    styles.backgroundSize = "100% 100%"
  }

  if (visualWidth.value != null) {
    const width = Math.max(0, visualWidth.value)
    styles.width = `${width}px`
    styles.minWidth = `${width}px`
    return styles
  }

  if (props.col.maxWidth) {
    return styles
  }

  if (props.col.width != null) {
    styles.width = `${props.col.width * (props.zoomScale ?? 1)}px`
  }
  if (props.col.minWidth != null) {
    styles.minWidth = `${props.col.minWidth * (props.zoomScale ?? 1)}px`
  }

  return styles
})

defineOptions({ inheritAttrs: false })

type ColumnSortDirection = "asc" | "desc"

type AlignOption = "left" | "center" | "right"

const JUSTIFY_CLASS_MAP: Record<AlignOption, string> = {
  left: "justify-start",
  center: "justify-center",
  right: "justify-end",
}

const TEXT_ALIGN_CLASS_MAP: Record<AlignOption, string> = {
  left: "text-left",
  center: "text-center",
  right: "text-right",
}

function normalizeAlign(value: string | null | undefined, fallback: AlignOption): AlignOption {
  if (typeof value === "string") {
    const lowered = value.toLowerCase()
    if (lowered === "left" || lowered === "center" || lowered === "right") {
      return lowered as AlignOption
    }
  }
  return fallback
}


const props = defineProps<{
  col: UiTableColumn
  displayLabel?: string
  topOffset?: number
  sortDirection?: ColumnSortDirection | null
  filterActive?: boolean
  menuOpen?: boolean
  zoomScale?: number
  layoutWidth?: number | null
  visualWidth?: number | null
  sortPriority?: number | null
  showLeftFiller?: boolean
  leftPadding?: number
  showRightFiller?: boolean
  rightPadding?: number
  tabIndex?: number
  ariaColIndex?: number | string
  tableContainer?: HTMLElement | null
  stickyLeftOffset?: number
  sticky?: boolean
  stickySide?: "left" | "right" | null
  stickyRightOffset?: number
  disableMenu?: boolean
  baseClass?: string
  grouped?: boolean
  groupOrder?: number | null
}>()
const emit = defineEmits(["resize", "select", "menu-open", "menu-close", "hide"])

const displayText = computed(() => props.displayLabel ?? props.col.label ?? "")
const sortDirection = computed<ColumnSortDirection | null>(() => props.sortDirection ?? null)
const filterActive = computed(() => props.filterActive ?? false)
const zoomScale = computed(() => props.zoomScale ?? 1)
const visualWidth = computed(() => props.visualWidth ?? null)
const sortPriority = computed(() => props.sortPriority ?? null)
const grouped = computed(() => props.grouped === true)
const groupOrder = computed(() => (typeof props.groupOrder === "number" ? props.groupOrder : null))
const resizeHandleStyle = computed(() => ({
  cursor: "col-resize",
  width: "12px",
  right: "-6px",
  left: "auto",
  pointerEvents: "auto" as const,
}))
const stickySide = computed<"left" | "right" | null>(() => props.stickySide ?? (props.sticky ? "left" : null))
const stickyLeftOffset = computed(() => props.stickyLeftOffset ?? 0)
const stickyRightOffset = computed(() => props.stickyRightOffset ?? 0)
const isMenuDisabled = computed(() => Boolean(props.disableMenu))
const showSecondary = computed(() => {
  const text = displayText.value?.trim()
  const label = props.col.label?.trim()
  return Boolean(label && text && label !== text)
})
const topOffset = computed(() => props.topOffset ?? 0)
const filterButtonClass = computed(() => {
  if (isMenuDisabled.value) return ""
  if (sortDirection.value || filterActive.value) {
    return "ui-table__icon-button--highlight"
  }
  return ""
})
const headerAlign = computed<AlignOption>(() => normalizeAlign(props.col.headerAlign, normalizeAlign(props.col.align, "center")))
const headerTextClass = computed(() => TEXT_ALIGN_CLASS_MAP[headerAlign.value])
const headerJustifyClass = computed(() => JUSTIFY_CLASS_MAP[headerAlign.value])
const baseHeaderClass = computed(() => props.baseClass ?? "")
const sortPriorityLabel = computed(() => (sortPriority.value != null ? String(sortPriority.value) : null))
const computedTabIndex = computed(() => props.tabIndex ?? -1)
const ariaColIndex = computed(() => props.ariaColIndex ?? undefined)
const ariaSort = computed(() => {
  if (sortDirection.value === "asc") return "ascending"
  if (sortDirection.value === "desc") return "descending"
  return "none"
})

const leftPaddingValue = computed(() => Math.max(0, props.leftPadding ?? 0))
const rightPaddingValue = computed(() => Math.max(0, props.rightPadding ?? 0))
const showLeftPlaceholder = computed(() => Boolean(props.showLeftFiller && leftPaddingValue.value > 0))
const showRightPlaceholder = computed(() => Boolean(props.showRightFiller && rightPaddingValue.value > 0))
const leftPaddingStyle = computed(() => `${leftPaddingValue.value}px`)
const rightPaddingStyle = computed(() => `${rightPaddingValue.value}px`)

const attrs = useAttrs()

const forwardedAttrs = computed(() => {
  const result: Record<string, unknown> = {}
  for (const [key, value] of Object.entries(attrs)) {
    if (key === "class" || key === "style") {
      continue
    }
    result[key] = value
  }
  return result
})

const attrStyle = computed(() => attrs.style)

// TODO: Styles headless from tailwind classes
const headerCellClassList = computed(() => {
  const classes: unknown[] = [
    "ui-table-header-cell relative overflow-visible h-full select-none text-start cursor-pointer",
    baseHeaderClass.value,
  ]

  const externalClass = attrs.class
  if (externalClass) {
    classes.push(externalClass)
  }
  return classes
})

const headerCellStyleBinding = computed(() => {
  const externalStyle = attrStyle.value
  if (!externalStyle) {
    return headerCellStyle.value
  }
  if (Array.isArray(externalStyle)) {
    return [headerCellStyle.value, ...externalStyle]
  }
  return [headerCellStyle.value, externalStyle]
})

const {
  isMenuOpen,
  popoverPanelStyle,
  setMenuButtonElement,
  setPopoverPanelElement,
  setHeaderCellElement,
  toggleMenu,
  onHeaderClick,
  onHeaderKeydown,
  startResize,
  autoResize,
  menuSlotBindings,
} = useColumnHeaderInteractions({
  column: computed(() => props.col),
  isMenuDisabled,
  sortDirection,
  zoomScale,
  tableContainer: () => props.tableContainer ?? null,
  externalMenuOpen: computed(() => props.menuOpen ?? false),
  emitResize: (columnKey, width) => emit("resize", columnKey, width),
  emitSelect: event => emit("select", event),
  emitMenuOpen: close => emit("menu-open", close),
  emitMenuClose: () => emit("menu-close"),
  hideColumn: columnKey => emit("hide", columnKey),
})
</script>

<style scoped>
th {
  transition: width 0.1s ease-out;
  overflow: visible;
  text-overflow: ellipsis;
  white-space: nowrap;
}

:global(body.ui-table-resize-active),
:global(body.ui-table-resize-active *) {
  cursor: col-resize !important;
}

</style>
