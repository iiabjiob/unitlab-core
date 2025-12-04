import { reactive, ref } from "vue"
import type { ComputedRef, Ref } from "vue"
import type { CellEditEvent, UiTableColumn, UiTableRowId, VisibleRow } from "../../core/types"
import type { RowKey } from "@/composables/useSelectableRows"
import type { HistoryEntry } from "./useTableHistory"

// Central editing logic for UiTable (validation, history, programmatic edits).

interface SetCellOptions {
  collector?: CellEditEvent[]
  historyCollector?: HistoryEntry[]
  suppressHistory?: boolean
  force?: boolean
}

interface UseTableEditingOptions {
  processedRows: ComputedRef<VisibleRow[]>
  localColumns: Ref<UiTableColumn[]>
  emitCellEdit: (event: CellEditEvent) => void
  emitBatchEdit: (events: CellEditEvent[]) => void
  recordHistory: (entries: HistoryEntry[]) => void
  isApplyingHistory: Ref<boolean>
  findRowIndexById: (rowId: RowKey) => number | null
  getRowIdByIndex: (rowIndex: number) => RowKey | null
}

type RowRef = RowKey | number

/**
 * Determines whether a column can be edited based on its editor configuration.
 */
export function isColumnEditable(column: UiTableColumn | undefined) {
  if (!column) return false
  if (column.editable === false) return false
  const editor = column.editor ?? "text"
  return editor !== "none"
}

/**
 * Checks if clipboard pastes are allowed for the provided column definition.
 */
export function canPasteIntoColumn(column: UiTableColumn | undefined) {
  if (!column) return false
  if (column.editable === false) return false
  if (column.editor === "none") return false
  if (column.editor === "checkbox") return false
  return column.editor === undefined || column.editor === "text" || column.editor === "number" || column.editor === "select"
}

/**
 * Returns a unique key for the validation map.
 */
function getValidationKey(rowId: UiTableRowId | null, rowIndex: number, columnKey: string) {
  const prefix = rowId !== null && rowId !== undefined ? `id:${String(rowId)}` : `idx:${rowIndex}`
  return `${prefix}:${columnKey}`
}

/**
 * Coordinates edit state, validation, history, and event emission for UiTable cells.
 */
export function useTableEditing({
  processedRows,
  localColumns,
  emitCellEdit,
  emitBatchEdit,
  recordHistory,
  isApplyingHistory,
  findRowIndexById,
  getRowIdByIndex,
}: UseTableEditingOptions) {
  function normalizeRowTarget(target: RowRef): number | null {
    if (typeof target === "number") {
      return Number.isFinite(target) ? target : null
    }
    const index = findRowIndexById(target)
    return index ?? null
  }

  function resolveRowId(rowIndex: number): RowKey | null {
    return getRowIdByIndex(rowIndex)
  }
  const validationErrors = reactive<Record<string, string | null>>({})
  const isEditingCell = ref(false)
  const editCommand = ref<{ rowIndex: number; key: string; token: number } | null>(null)

  /**
   * Runs the column validator and normalizes the returned message.
   */
  function runValidator(column: UiTableColumn | undefined, value: unknown, row: any) {
    if (!column || !column.validator) return null
    try {
      const result = column.validator(value, row)
      if (result === false) return "Invalid value"
      if (typeof result === "string") return result
      if (result === true || result === undefined) return null
      return result ? null : "Invalid value"
    } catch (error) {
      return (error instanceof Error ? error.message : "Invalid value") || "Invalid value"
    }
  }

  /**
   * Writes the validator result into the reactive error map.
   */
  function updateValidationState(rowId: RowKey | null, rowIndex: number, column: UiTableColumn | undefined, value: unknown, row: any) {
    if (!column) return
    const key = getValidationKey(rowId ?? null, rowIndex, column.key)
    const message = runValidator(column, value, row)
    if (message) {
      validationErrors[key] = message
    } else if (Object.prototype.hasOwnProperty.call(validationErrors, key)) {
      delete validationErrors[key]
    }
  }

  /**
   * Returns the stored validation message for a cell if present.
   */
  function getValidationError(rowIndex: number, colIndex: number) {
    const column = localColumns.value[colIndex]
    if (!column) return null
    const key = getValidationKey(resolveRowId(rowIndex), rowIndex, column.key)
    return validationErrors[key] ?? null
  }

  /**
   * Grabs the raw value from the processed row cache.
   */
  function getCellRawValue(rowRef: RowRef, colIndex: number) {
    const rowIndex = normalizeRowTarget(rowRef)
    if (rowIndex == null) return undefined
    const entry = processedRows.value[rowIndex]
    const column = localColumns.value[colIndex]
    if (!entry || !column) return undefined
    return entry.row?.[column.key]
  }

  /**
   * Mutates a cell value directly, handling history, validation, and emit behavior.
   */
  function setCellValueDirect(rowRef: RowRef, colIndex: number, value: unknown, options: SetCellOptions = {}) {
    const rowIndex = normalizeRowTarget(rowRef)
    if (rowIndex == null) return false
    const entry = processedRows.value[rowIndex]
    const column = localColumns.value[colIndex]
    if (!entry || !column) return false
    if (column.editable === false && !options.force) return false
    if (!canPasteIntoColumn(column) && !options.force) return false
    const current = entry.row?.[column.key]
    const valuesEqual = current === value || (current == null && value == null)
    if (valuesEqual) {
      updateValidationState(entry.rowId ?? resolveRowId(rowIndex), rowIndex, column, value, entry.row)
      return false
    }
    if (entry.row) {
      entry.row[column.key] = value
    }
    const stableRowId = (entry.rowId as RowKey | undefined) ?? resolveRowId(rowIndex) ?? rowIndex
    updateValidationState(stableRowId, rowIndex, column, value, entry.row)
    const event: CellEditEvent = {
      rowId: stableRowId,
      rowIndex: entry.originalIndex ?? rowIndex,
      originalRowIndex: entry.originalIndex ?? rowIndex,
      displayRowIndex: rowIndex,
      row: entry.row,
      key: column.key,
      value,
    }
    const historyEntry: HistoryEntry = {
      rowId: stableRowId,
      rowIndex,
      colIndex,
      columnKey: column.key,
      oldValue: current,
      newValue: value,
    }
    if (options.collector) {
      options.collector.push(event)
    } else {
      emitCellEdit(event)
    }
    if (!isApplyingHistory.value && !options.suppressHistory) {
      if (options.historyCollector) {
        options.historyCollector.push(historyEntry)
      } else {
        recordHistory([historyEntry])
      }
    }
    return true
  }

  /**
   * Emits edit events individually while still allowing batch notifications.
   */
  function dispatchEvents(events: CellEditEvent[]) {
    if (!events.length) return
    if (events.length > 1) {
      emitBatchEdit(events)
    }
    for (const event of events) {
      emitCellEdit(event)
    }
  }

  /**
   * Resolves select options safely, catching any user-land errors.
   */
  function getColumnOptions(column: UiTableColumn | undefined, row: any) {
    if (!column?.options) return []
    try {
      return typeof column.options === "function" ? column.options(row) ?? [] : column.options ?? []
    } catch {
      return []
    }
  }

  /**
   * Normalizes raw pasted text to the column data type (number/select).
   */
  function normalizePastedValue(column: UiTableColumn, row: any, rawValue: string) {
    const trimmed = rawValue.trim()
    if (column.editor === "number") {
      if (trimmed === "") return null
      const numeric = Number(trimmed)
      return Number.isFinite(numeric) ? numeric : trimmed
    }
    if (column.editor === "select") {
      if (trimmed === "") return null
      const options = getColumnOptions(column, row)
      const matchByValue = options.find(option => String(option.value) === trimmed)
      if (matchByValue) return matchByValue.value
      const matchByLabel = options.find(option => String(option.label ?? "") === trimmed)
      if (matchByLabel) return matchByLabel.value
      return trimmed
    }
    return rawValue
  }

  /**
   * Applies a pasted value to the target cell with validation and history bookkeeping.
   */
  function setCellValueFromPaste(rowRef: RowRef, colIndex: number, rawValue: string, options?: SetCellOptions) {
    const rowIndex = normalizeRowTarget(rowRef)
    if (rowIndex == null) return false
    const entry = processedRows.value[rowIndex]
    const column = localColumns.value[colIndex]
    if (!entry || !column || !canPasteIntoColumn(column)) return false
    const normalized = normalizePastedValue(column, entry.row, rawValue)
    return setCellValueDirect(rowIndex, colIndex, normalized, options)
  }

  /**
   * Reads the current value for the given row/column identifier.
   */
  function getCellValue(rowRef: RowRef, column: number | string) {
    const colIndex = typeof column === "number" ? column : localColumns.value.findIndex(col => col.key === column)
    if (colIndex < 0) return undefined
    const rowIndex = normalizeRowTarget(rowRef)
    if (rowIndex == null) return undefined
    const entry = processedRows.value[rowIndex]
    const columnDef = localColumns.value[colIndex]
    if (!entry || !columnDef) return undefined
    return entry.row?.[columnDef.key]
  }

  /**
   * Convenience wrapper over setCellValueDirect that accepts a column key.
   */
  function setCellValue(rowRef: RowRef, column: number | string, value: unknown, options?: { force?: boolean }) {
    const colIndex = typeof column === "number" ? column : localColumns.value.findIndex(col => col.key === column)
    if (colIndex < 0) return false
    return setCellValueDirect(rowRef, colIndex, value, { force: options?.force })
  }

  /**
   * Handles edit events coming from child components and updates backing data.
   */
  function onCellEdit(event: CellEditEvent) {
    const rowRef: RowRef = (event.rowId as RowKey | undefined) ?? (event.displayRowIndex ?? event.rowIndex ?? 0)
    const columnKey = String(event.key)
    const colIndex = localColumns.value.findIndex(col => col.key === columnKey)
    if (colIndex < 0) {
      emitCellEdit(event)
      return
    }
    const batchEvents: CellEditEvent[] = []
    const historyEntries: HistoryEntry[] = []
    const changed = setCellValueDirect(rowRef, colIndex, event.value, {
      collector: batchEvents,
      historyCollector: historyEntries,
    })
    if (changed) {
      if (historyEntries.length) {
        recordHistory(historyEntries)
      }
      const emitted = batchEvents[0] ?? event
      emitCellEdit(emitted)
    } else {
      emitCellEdit(event)
    }
  }

  /**
   * Tracks whether a cell editor is currently active.
   */
  function onCellEditingChange(editing: boolean) {
    isEditingCell.value = editing
  }

  /**
   * Queues a deferred edit command that focus handlers can consume.
   */
  function requestEdit(rowIndex: number, columnKey: string) {
    editCommand.value = {
      rowIndex,
      key: columnKey,
      token: Date.now(),
    }
  }

  return {
    validationErrors,
    getValidationError,
    setCellValueDirect,
    setCellValue,
    setCellValueFromPaste,
    getCellValue,
    getCellRawValue,
    dispatchEvents,
    onCellEdit,
    onCellEditingChange,
    normalizePastedValue,
    requestEdit,
    isEditingCell,
    editCommand,
    canPasteIntoColumn,
    runValidator,
    updateValidationState,
  }
}
