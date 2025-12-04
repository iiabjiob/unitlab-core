import type {
  GridSelectionPoint as CoreSelectionPoint,
  GridSelectionRange as CoreSelectionRange,
} from "./selectionState"
import type { SelectionOverlayRect } from "./selectionOverlay"
import type { UiTableColumn } from "../types"
import type { PointerCoordinates } from "./autoScroll"
import type { FillHandleStylePayload } from "./fillHandleStylePool"

export type SelectionOverlayMode = "ranges" | "active" | "fill" | "cut" | "cursor"

export type SelectionPoint<RowKey> = CoreSelectionPoint<RowKey>
export type SelectionRange<RowKey> = CoreSelectionRange<RowKey>

export interface SelectionOverlaySnapshot {
  ranges: readonly SelectionOverlayRect[]
  activeRange: readonly SelectionOverlayRect[]
  fillPreview: readonly SelectionOverlayRect[]
  cutPreview: readonly SelectionOverlayRect[]
  cursor: SelectionOverlayRect | null
}

export interface SelectionOverlayEnvironment {
  commit(snapshot: SelectionOverlaySnapshot): void
  clear(mode?: SelectionOverlayMode): void
}

export interface SelectionMeasurementHandle<T> {
  promise: Promise<T>
  cancel(): void
}

export interface SelectionMeasurementEnvironment<RowKey> {
  measureFillHandle(range: SelectionRange<RowKey>): SelectionMeasurementHandle<FillHandleStylePayload | null>
  measureCellRect(point: SelectionPoint<RowKey>): SelectionMeasurementHandle<DOMRect | null>
}

export interface SelectionSchedulerRequestOptions {
  mode?: "frame" | "idle"
  timeout?: number
}

export interface SelectionSchedulerEnvironment {
  request(callback: () => void, options?: SelectionSchedulerRequestOptions): number
  cancel(handle: number): void
}

export interface SelectionAutoscrollEnvironment {
  stop(): void
  update(pointer: PointerCoordinates): void
}

export interface SelectionDomEnvironment<RowKey> {
  focusContainer(): void
  resolveCellElement(rowIndex: number, columnKey: string): HTMLElement | null
  resolveHeaderCellElement(columnKey: string): HTMLElement | null
  getRowIdByIndex(rowIndex: number): RowKey | null
  findRowIndexById(rowId: RowKey): number | null
  scrollSelectionIntoView(input: {
    range: SelectionRange<RowKey> | null
    cursor: SelectionPoint<RowKey> | null
    attempt?: number
    maxAttempts?: number
  }): void
  resolveCellFromPoint?(clientX: number, clientY: number): SelectionPoint<RowKey> | null
  resolveRowIndexFromPoint?(clientX: number, clientY: number): number | null
  invalidateMetrics?(): void
}

export interface SelectionEnvironment<RowKey> {
  columns: UiTableColumn[]
  overlays: SelectionOverlayEnvironment
  measurement: SelectionMeasurementEnvironment<RowKey>
  scheduler: SelectionSchedulerEnvironment
  dom: SelectionDomEnvironment<RowKey>
  autoscroll?: SelectionAutoscrollEnvironment
}
