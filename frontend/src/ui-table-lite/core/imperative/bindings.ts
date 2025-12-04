import type { UiTableColumn, VisibleRow } from "../types"
import type { HeaderRenderableEntry } from "../types/internal"
import type { RowPoolItem } from "../viewport/tableViewportController"
import type { ColumnMetric } from "../virtualization/columnSnapshot"
import type { ImperativeBodyRegion } from "./types"

export type ReadonlySignal<T> = { readonly value: T }

export type ClassLike = string | string[] | Record<string, boolean> | undefined | null

export interface ImperativeContainerRegistration {
  region: ImperativeBodyRegion
  element: HTMLDivElement | null
}

export type ImperativeContainerMap = Partial<Record<ImperativeBodyRegion, HTMLDivElement | null>>

export interface EdgeState {
  top: boolean
  bottom: boolean
  left: boolean
  right: boolean
  active?: boolean
}

export interface SelectionAreaDescriptor {
  startRow: number
  endRow: number
  startCol: number
  endCol: number
}

export interface GroupSelectionState {
  checked: boolean
  indeterminate: boolean
  selectable: boolean
  total: number
  selected: number
}

export interface UiTableColumnBinding {
  column: UiTableColumn
  columnIndex: number
  visualWidth: number
  cellClass?: string
  sticky: boolean
  stickySide: "left" | "right" | null
  stickyLeftOffset?: number
  stickyRightOffset?: number
  ariaColIndex: number
}

export type TableSlots = Record<string, ((props: Record<string, unknown>) => unknown) | undefined>

export interface UiTableBodyBindings {
  virtualContainerStyle: ReadonlySignal<Record<string, unknown>>
  pooledRows: ReadonlySignal<RowPoolItem[]>
  rowLayerStyle: (pooled: RowPoolItem) => Record<string, unknown>
  isHoverableTable: ReadonlySignal<boolean>
  isGroupRowEntry: (entry: VisibleRow | null | undefined) => boolean
  groupRowClass: ReadonlySignal<ClassLike>
  groupCellClass: ReadonlySignal<ClassLike>
  groupCaretClass: ReadonlySignal<ClassLike>
  groupCellStyle: (level: number | null | undefined) => Record<string, string>
  toggleGroupRow: (key: string) => void
  isGroupExpanded: (key: string) => boolean
  ariaColCount: ReadonlySignal<number>
  rowGridClass: (row: any) => string | undefined
  rowGridStyle: ReadonlySignal<Record<string, unknown>>
  splitPinnedLayoutEnabled: ReadonlySignal<boolean>
  gridTemplateColumns: ReadonlySignal<string>
  gridTemplateColumnsPinnedLeft: ReadonlySignal<string>
  gridTemplateColumnsMain: ReadonlySignal<string>
  gridTemplateColumnsPinnedRight: ReadonlySignal<string>
  headerRenderableEntries: ReadonlySignal<HeaderRenderableEntry[]>
  headerPinnedLeftEntries: ReadonlySignal<HeaderRenderableEntry[]>
  headerMainEntries: ReadonlySignal<HeaderRenderableEntry[]>
  headerPinnedRightEntries: ReadonlySignal<HeaderRenderableEntry[]>
  columnBindings: ReadonlySignal<Map<string, UiTableColumnBinding>>
  getColumnBinding: (columnKey: string) => UiTableColumnBinding
  systemColumnStyle: (column: UiTableColumn) => Record<string, unknown>
  bodySelectionCellClass: ReadonlySignal<ClassLike>
  bodyRowClass: ReadonlySignal<ClassLike>
  bodyCellClass: (column: UiTableColumn) => string | undefined
  bodyIndexCellClass: ReadonlySignal<ClassLike>
  rowHeightDom: ReadonlySignal<number>
  rowSelectionName: ReadonlySignal<string>
  isCheckboxRowSelected: (row: any) => boolean
  handleRowCheckboxToggle: (row: any) => void
  groupSelectionVisible: ReadonlySignal<boolean>
  getGroupSelectionState: (groupKey: string) => GroupSelectionState
  toggleGroupSelection: (groupKey: string, next?: boolean) => void
  isRowFullySelected: (rowIndex: number) => boolean
  isColumnFullySelected: (columnIndex: number) => boolean
  isCellSelected: (rowIndex: number, columnIndex: number) => boolean
  isSelectionCursorCell: (rowIndex: number, columnIndex: number) => boolean
  isSelectionAnchorCell: (rowIndex: number, columnIndex: number) => boolean
  isCellInSelectionRange: (rowIndex: number, columnIndex: number) => boolean
  isCellInFillPreview: (rowIndex: number, columnIndex: number) => boolean
  isCellInCutPreview: (rowIndex: number, columnIndex: number) => boolean
  getFillPreviewEdges: (rowIndex: number, columnIndex: number) => EdgeState | null
  getSelectionEdges: (rowIndex: number, columnIndex: number) => EdgeState | null
  getCutPreviewEdges: (rowIndex: number, columnIndex: number) => EdgeState | null
  hasCutPreview: () => boolean
  buildSelectionSnapshot?: (payload: { rowIndices: number[]; columnIndices: number[] }) => unknown
  getColumnIndex: (columnKey: string) => number
  getAriaColIndex: (columnIndex: number) => number
  getAriaRowIndex: (rowIndex: number) => number
  getHeaderTabIndex: (columnIndex: number) => number
  getCellTabIndex: (rowIndex: number, columnIndex: number) => number
  getCellDomId: (rowIndex: number, columnKey: string) => string
  columnWidthDomMap: ReadonlySignal<Map<string, number>>
  isColumnSticky: (column: UiTableColumn) => boolean
  getStickySide: (column: UiTableColumn) => "left" | "right" | null
  getStickyLeftOffset: (column: UiTableColumn) => number | undefined
  getStickyRightOffset: (column: UiTableColumn) => number | undefined
  supportsCssZoom: boolean
  zoom: ReadonlySignal<number>
  editCommand: ReadonlySignal<{ rowIndex: number; key: string; token: number } | null>
  isColumnEditable: (column: UiTableColumn | undefined) => boolean
  getValidationError: (rowIndex: number, columnIndex: number) => string | null
  onCellEdit: (event: Record<string, unknown>) => void
  onCellSelect: (event: Record<string, unknown>) => void
  focusNextCell: (payload: Record<string, unknown>) => void
  handleCellEditingChange: (editing: boolean, columnKey: string, originalIndex: number | null | undefined) => void
  onCellDragStart: (event: Record<string, unknown>) => void
  onCellDragEnter: (event: Record<string, unknown>) => void
  onCellComponentFocus: (payload: { rowIndex: number; colIndex: number; columnKey: string }) => void
  startFillDrag?: (event: MouseEvent) => void
  autoFillDown?: (event?: MouseEvent) => void
  isFillDragging: () => boolean
  hasCustomRenderer: (columnKey: string) => boolean
  tableSlots: TableSlots
  getStickyTopOffset: (row: any) => number | undefined
  isSearchMatchCell: (rowIndex: number | null | undefined, columnKey: string) => boolean
  isActiveSearchMatchCell: (rowIndex: number | null | undefined, columnKey: string) => boolean
  SELECTION_COLUMN_KEY: string
  totalRowCountDisplay: ReadonlySignal<number | null | undefined>
  loadingState: ReadonlySignal<boolean>
  rowHeaderClass: (rowIndex: number) => ClassLike
  onRowIndexClick: (rowIndex: number, event?: MouseEvent) => void
  registerImperativeContainer: (registration: ImperativeContainerRegistration | ImperativeContainerMap | null) => void
  getActiveSelectionRange: () => SelectionAreaDescriptor | null
  canMoveActiveSelection: () => boolean
  moveActiveSelectionTo: (rowStart: number, colStart: number) => boolean
  hoverOverlayVisible: ReadonlySignal<boolean>
  hoverOverlayStyle: ReadonlySignal<Record<string, unknown> | null>
  pinnedLeftEntries: ReadonlySignal<ColumnMetric<UiTableColumn>[]>
  visibleScrollableEntries: ReadonlySignal<ColumnMetric<UiTableColumn>[]>
  pinnedRightEntries: ReadonlySignal<ColumnMetric<UiTableColumn>[]>
  leftPaddingDom: ReadonlySignal<number>
  rightPaddingDom: ReadonlySignal<number>
  viewportWidthDom: ReadonlySignal<number>
}
