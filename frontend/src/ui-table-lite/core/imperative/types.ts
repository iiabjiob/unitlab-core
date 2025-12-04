import type { HeaderRenderableEntry } from "../types/internal"
import type { UiTableColumn } from "../types"
import type { UiTableColumnBinding, SelectionAreaDescriptor, EdgeState } from "./bindings"

export type StyleCache = Map<string, unknown>

export type CellType = "selection" | "index" | "data"

export type ImperativeBodyRegion = "pinned-left" | "main" | "pinned-right"

export interface RowSlotRegionState {
  region: ImperativeBodyRegion
  layer: HTMLDivElement
  layerStyleCache: StyleCache
  grid: HTMLDivElement
  gridStyleCache: StyleCache
}

export interface CellSlot {
  wrapper: HTMLDivElement
  content: HTMLElement | null
  columnKey: string
  columnIndex: number
  type: CellType
  region: ImperativeBodyRegion
  value: unknown
  checkbox?: HTMLInputElement
  classCache: string
  styleCache: StyleCache
  baseClass: string
  leftFiller?: HTMLDivElement
  rightFiller?: HTMLDivElement
  rowIndexClickHandler?: (event: MouseEvent) => void
  rendererHost?: HTMLDivElement
}

export type RowSlotType = "empty" | "group" | "data"

export interface RowSlot {
  regions: Partial<Record<ImperativeBodyRegion, RowSlotRegionState>>
  primaryRegion: ImperativeBodyRegion
  layer: HTMLDivElement
  layerStyleCache: StyleCache
  grid: HTMLDivElement
  gridStyleCache: StyleCache
  groupRow: HTMLDivElement
  groupCell: HTMLSpanElement
  groupCellStyleCache: StyleCache
  groupCaret: HTMLSpanElement
  groupLabel: HTMLSpanElement
  groupCount: HTMLSpanElement
  type: RowSlotType
  cells: CellSlot[]
  rowIndex: number
  displayIndex: number
  originalIndex: number | null
  rowData: any
  gridClassName: string
}

export interface ColumnBlueprint {
  renderable: HeaderRenderableEntry
  column: UiTableColumn
  binding: UiTableColumnBinding
  cachedClassName: string
  region: ImperativeBodyRegion
}

export enum InteractionMode {
  None = "none",
  Selecting = "selecting",
  FillHandle = "fill",
  Moving = "moving",
}

export interface MoveSelectionGeometry {
  containerLeft: number
  containerTop: number
  width: number
  height: number
  rowHeight: number
  avgColumnWidth: number
}

export interface MoveSelectionState {
  source: SelectionAreaDescriptor
  offsetRow: number
  offsetCol: number
  pointerStart: { x: number; y: number }
  geometry: MoveSelectionGeometry
  clientBounds: { left: number; top: number; right: number; bottom: number }
  deltaRow: number
  deltaCol: number
  maxRowStart: number
  maxColStart: number
}

export interface CellRenderState {
  row: any
  column: UiTableColumn
  rowIndex: number
  columnIndex: number
  originalIndex?: number
  zoom: number
  isSelected: boolean
  isAnchor: boolean
  isCursor: boolean
  isRowSelected: boolean
  isColumnSelected: boolean
  isRangeSelected: boolean
  isFillPreview: boolean
  fillPreviewEdges: EdgeState | null
  isCutPreview: boolean
  cutPreviewEdges: EdgeState | null
  rangeEdges: EdgeState | null
  editable: boolean
  validationError: string | null
  tabIndex: number | null | undefined
  ariaRowIndex: number | null | undefined
  ariaColIndex: number
  sticky: boolean
  stickySide: "left" | "right" | null
  stickyLeftOffset?: number
  stickyRightOffset?: number
  stickyTopOffset?: number
  stickyTop: boolean
  searchMatch: boolean
  activeSearchMatch: boolean
  rowHeaderClass: string
}

export interface ImperativeRendererAdapter {
  mountCustomCell: (payload: { slotName: string; columnKey: string; props: Record<string, unknown>; host: HTMLElement }) => void
  unmountCustomCell: (host: HTMLElement) => void
}

export interface ImperativeSelectionCellState {
  isSelected: boolean
  isRangeSelected: boolean
  isFillPreview: boolean
  fillPreviewEdges: EdgeState | null
  isCutPreview: boolean
  cutPreviewEdges: EdgeState | null
  rangeEdges: EdgeState | null
  generation: number
}

export interface ImperativeRowClassState {
  value: string
  generation: number
}

export interface ImperativeSelectionSnapshot {
  hasCutPreview: boolean
  rowSelection: Set<number>
  columnSelection: Set<number>
  anchorKey: string | null
  cursorKey: string | null
  cells: Map<string, ImperativeSelectionCellState>
  rowClasses: Map<number, ImperativeRowClassState>
  generation: number
}

