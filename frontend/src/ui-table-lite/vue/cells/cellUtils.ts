import type { UiTableColumn } from "../../core/types"

export type AlignOption = "left" | "center" | "right"

export const ALIGN_CLASS_MAP: Record<AlignOption, string> = {
  left: "text-left justify-start",
  center: "text-center justify-center",
  right: "text-right justify-end",
}

export function normalizeAlign(value: string | null | undefined, fallback: AlignOption): AlignOption {
  if (typeof value === "string") {
    const lowered = value.toLowerCase()
    if (lowered === "left" || lowered === "center" || lowered === "right") {
      return lowered as AlignOption
    }
  }
  return fallback
}

export interface ColumnOption {
  label: string
  value: unknown
}

export function resolveColumnOptions(
  options: UiTableColumn["options"],
  row: Record<string, any>,
): ColumnOption[] {
  if (!options) return []
  const resolved = typeof options === "function" ? options(row) : options
  return Array.isArray(resolved) ? resolved : []
}

export interface EditCommand {
  rowIndex: number
  key: string
}

export interface CellProps {
  row: Record<string, any>
  col: UiTableColumn
  rowIndex: number
  colIndex: number
  originalRowIndex?: number
  isSelected?: boolean
  isSelectionAnchor?: boolean
  isRowSelected?: boolean
  isColumnSelected?: boolean
  editCommand?: EditCommand | null
  isRangeSelected?: boolean
  zoomScale?: number
  editable?: boolean
  validationError?: string | null
  tabIndex?: number
  ariaRowIndex?: number | string
  ariaColIndex?: number | string
  cellId?: string
  visualWidth?: number | null
  sticky?: boolean
  stickyLeftOffset?: number
  stickyTop?: boolean
  stickyTopOffset?: number
  stickySide?: "left" | "right" | null
  stickyRightOffset?: number
  searchMatch?: boolean
  activeSearchMatch?: boolean
}

export interface CellEditEventPayload {
  rowIndex: number
  originalRowIndex?: number | null
  displayRowIndex: number
  row: Record<string, any>
  key: string
  value: unknown
}

export interface NextCellPayload {
  rowIndex: number
  key: string
  colIndex: number
  shift?: boolean
  handled?: boolean
  direction?: "up" | "down"
}

export interface SelectEventPayload {
  rowIndex: number
  key: string
  colIndex: number
  focus: boolean
  event: MouseEvent
}

export interface DragStartPayload {
  rowIndex: number
  colIndex: number
  event: MouseEvent
}

export interface DragEnterPayload {
  rowIndex: number
  colIndex: number
  event: MouseEvent
}

export interface CellFocusPayload {
  rowIndex: number
  colIndex: number
  columnKey: string
  event: FocusEvent
}

export interface SelectComponentLike {
  focus?: () => void
  open?: () => void
}

export type CellEmitFn = {
  (event: "edit", payload: CellEditEventPayload): void
  (event: "next-cell", payload: NextCellPayload): void
  (event: "select", payload: SelectEventPayload): void
  (event: "editing-change", payload: boolean): void
  (event: "drag-start", payload: DragStartPayload): void
  (event: "drag-enter", payload: DragEnterPayload): void
  (event: "cell-focus", payload: CellFocusPayload): void
}
