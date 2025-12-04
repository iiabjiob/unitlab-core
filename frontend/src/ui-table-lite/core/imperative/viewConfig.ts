import type { ClassLike } from "./bindings.js"
import { mergeClasses } from "./dom.js"

export interface ImperativeBodyAlignmentClassMap {
  left?: ClassLike
  center?: ClassLike
  right?: ClassLike
}

export interface ImperativeBodyStateClassMap {
  activeCell?: ClassLike
  rowSelected?: ClassLike
  columnSelected?: ClassLike
  rangeSelected?: ClassLike
  fillPreview?: ClassLike
  searchMatch?: ClassLike
  searchMatchActive?: ClassLike
  validationError?: ClassLike
  editing?: ClassLike
  readOnly?: ClassLike
  anchorCell?: ClassLike
  cursorCell?: ClassLike
}

export interface ImperativeBodyClassMap {
  rowLayer?: ClassLike
  rowGrid?: ClassLike
  groupRow?: ClassLike
  groupCell?: ClassLike
  groupCaret?: ClassLike
  groupLabel?: ClassLike
  groupCount?: ClassLike
  selectionCell?: ClassLike
  selectionCheckbox?: ClassLike
  indexCell?: ClassLike
  indexText?: ClassLike
  dataCell?: ClassLike
  dataCellText?: ClassLike
  customRendererHost?: ClassLike
  moveOverlay?: ClassLike
  align?: ImperativeBodyAlignmentClassMap
  state?: ImperativeBodyStateClassMap
}

export interface ImperativeBodyViewConfig {
  classMap?: ImperativeBodyClassMap
}

export interface ResolvedImperativeBodyClassMap {
  rowLayer: string
  rowGrid: string
  groupRow: string
  groupCell: string
  groupCaret: string
  groupLabel: string
  groupCount: string
  selectionCell: string
  selectionCheckbox: string
  indexCell: string
  indexText: string
  dataCell: string
  dataCellText: string
  customRendererHost: string
  moveOverlay: string
  align: Required<ImperativeBodyAlignmentClassMap>
  state: Required<ImperativeBodyStateClassMap>
}

export const DEFAULT_IMPERATIVE_BODY_CLASS_MAP: ResolvedImperativeBodyClassMap = {
  rowLayer: "ui-table__row-layer virtual-layer transform-gpu will-change-transform contain-strict backface-hidden",
  rowGrid: "ui-table__row-surface",
  groupRow: "ui-table__group-row",
  groupCell: "ui-table__group-cell",
  groupCaret: "ui-table__group-caret",
  groupLabel: "ui-table__group-label",
  groupCount: "ui-table__group-count",
  selectionCell: "ui-table__selection-cell ui-table__sticky-divider flex items-center justify-center w-full h-full",
  selectionCheckbox:
    "ui-table__selection-checkbox h-4 w-4 cursor-pointer rounded border-neutral-300 text-blue-600 transition-colors focus:ring-2 focus:ring-blue-500 dark:border-neutral-600 dark:bg-neutral-800 dark:text-blue-400 dark:focus:ring-blue-400",
  indexCell: "ui-table__row-index ui-table__sticky-divider",
  indexText: "ui-table__row-index-text",
  dataCell: "ui-table-cell ui-table-cell--hover-target relative ui-table-cell__value",
  dataCellText: "ui-table-cell__text",
  customRendererHost: "ui-table-cell__custom",
  moveOverlay: "ui-table__move-overlay",
  align: {
    left: "",
    center: "justify-center text-center",
    right: "justify-end text-right",
  },
  state: {
    activeCell: "ui-table-state__active-cell",
    rowSelected: "ui-table-state__row-selected",
    columnSelected: "ui-table-state__column-selected",
    rangeSelected: "ui-table-state__range-selected",
    fillPreview: "ui-table-state__fill-preview",
    searchMatch: "ui-table-state__search-match",
    searchMatchActive: "ui-table-state__search-match-active",
    validationError: "ui-table-state__validation",
    editing: "cursor-text",
    readOnly: "select-none",
    anchorCell: "ui-table-cell--anchor",
    cursorCell: "ui-table-cell--selected",
  },
}

export function resolveBodyClassMap(map?: ImperativeBodyClassMap): ResolvedImperativeBodyClassMap {
  if (!map) return DEFAULT_IMPERATIVE_BODY_CLASS_MAP

  const align: Required<ImperativeBodyAlignmentClassMap> = {
    left: map.align?.left ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.align.left,
    center: map.align?.center ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.align.center,
    right: map.align?.right ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.align.right,
  }

  const state: Required<ImperativeBodyStateClassMap> = {
    activeCell: map.state?.activeCell ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.state.activeCell,
    rowSelected: map.state?.rowSelected ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.state.rowSelected,
    columnSelected: map.state?.columnSelected ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.state.columnSelected,
    rangeSelected: map.state?.rangeSelected ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.state.rangeSelected,
    fillPreview: map.state?.fillPreview ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.state.fillPreview,
    searchMatch: map.state?.searchMatch ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.state.searchMatch,
    searchMatchActive: map.state?.searchMatchActive ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.state.searchMatchActive,
    validationError: map.state?.validationError ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.state.validationError,
    editing: map.state?.editing ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.state.editing,
    readOnly: map.state?.readOnly ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.state.readOnly,
    anchorCell: map.state?.anchorCell ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.state.anchorCell,
    cursorCell: map.state?.cursorCell ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.state.cursorCell,
  }

  return {
    rowLayer: mergeClasses(map.rowLayer ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.rowLayer),
    rowGrid: mergeClasses(map.rowGrid ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.rowGrid),
    groupRow: mergeClasses(map.groupRow ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.groupRow),
    groupCell: mergeClasses(map.groupCell ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.groupCell),
    groupCaret: mergeClasses(map.groupCaret ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.groupCaret),
    groupLabel: mergeClasses(map.groupLabel ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.groupLabel),
    groupCount: mergeClasses(map.groupCount ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.groupCount),
    selectionCell: mergeClasses(map.selectionCell ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.selectionCell),
    selectionCheckbox: mergeClasses(map.selectionCheckbox ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.selectionCheckbox),
    indexCell: mergeClasses(map.indexCell ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.indexCell),
    indexText: mergeClasses(map.indexText ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.indexText),
    dataCell: mergeClasses(map.dataCell ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.dataCell),
    dataCellText: mergeClasses(map.dataCellText ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.dataCellText),
    customRendererHost: mergeClasses(map.customRendererHost ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.customRendererHost),
    moveOverlay: mergeClasses(map.moveOverlay ?? DEFAULT_IMPERATIVE_BODY_CLASS_MAP.moveOverlay),
    align,
    state,
  }
}
