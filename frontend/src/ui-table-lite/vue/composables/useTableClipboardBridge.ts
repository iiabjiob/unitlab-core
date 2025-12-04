import { nextTick } from "vue"
import type { ComputedRef, Ref } from "vue"
import { useTableClipboard } from "../../core/clipboard/useTableClipboard"
import { getCellElementsByRange, getRowCellElements } from "../../core/dom/getCellElementsByRange"
import type { SelectionPoint, SelectionRange } from "./useTableSelection"
import type { UiTableColumn, UiTableSelectionSnapshot } from "../../core/types"
import { createClipboardAdapter } from "../adapters/clipboardAdapter"

type SelectionAreaRect = {
  startRow: number
  endRow: number
  startCol: number
  endCol: number
}

interface UseTableClipboardBridgeOptions {
  containerRef: Ref<HTMLDivElement | null>
  cellContainers: ComputedRef<HTMLElement[]>
  visibleColumns: ComputedRef<UiTableColumn[]>
  anchorCell: Ref<SelectionPoint | null>
  selectedCell: Ref<SelectionPoint | null>
  fullRowSelection: Ref<{ start: number; end: number } | null>
  getSelectionSnapshot: () => UiTableSelectionSnapshot
  getActiveRange: () => SelectionRange | null
  buildSelectionMatrix: (range: SelectionRange | null, options?: { includeHeaders?: boolean; fallbackToAll?: boolean }) => string[][]
  applyMatrixToSelection: (matrix: string[][], baseOverride?: SelectionPoint | null) => void
  beginCutPreview: (snapshot: UiTableSelectionSnapshot | null) => void
  clearCutPreview: () => void
  commitPendingCut: () => boolean
  flash: (elements: HTMLElement[], type: "copy" | "paste" | "fill" | "undo" | "redo") => void
}

export function useTableClipboardBridge(options: UseTableClipboardBridgeOptions) {
  const {
    containerRef,
    cellContainers,
    visibleColumns,
    anchorCell,
    selectedCell,
    fullRowSelection,
    getSelectionSnapshot,
    getActiveRange,
    buildSelectionMatrix,
    applyMatrixToSelection,
    beginCutPreview,
    clearCutPreview,
    commitPendingCut,
    flash,
  } = options

  const clipboard = useTableClipboard({
    clipboard: createClipboardAdapter(),
    getActiveRange,
    buildSelectionMatrix,
    getSelectionSnapshot,
    beginCutPreview,
    clearCutPreview,
    commitPendingCut,
    applyMatrixToSelection: (matrix, baseOverride) => {
      const activeRangeBeforePaste = getActiveRange()
      const selectionHeight = activeRangeBeforePaste
        ? activeRangeBeforePaste.endRow - activeRangeBeforePaste.startRow + 1
        : 0
      const selectionWidth = activeRangeBeforePaste
        ? activeRangeBeforePaste.endCol - activeRangeBeforePaste.startCol + 1
        : 0
      const matrixRows = matrix.length
      const matrixCols = matrix[0]?.length ?? 0
      const matrixFitsSelection = Boolean(
        activeRangeBeforePaste &&
        (selectionHeight > 1 || selectionWidth > 1) &&
        matrixRows > 0 &&
        matrixCols > 0 &&
        matrixRows <= selectionHeight &&
        matrixCols <= selectionWidth,
      )
      const activeCursor = selectedCell.value ?? anchorCell.value ?? getActiveRange()?.anchor ?? null
      const normalizedBase = baseOverride
        ? {
            rowId: (("rowId" in baseOverride ? baseOverride.rowId : activeCursor?.rowId) ?? null) as SelectionPoint["rowId"],
            rowIndex: baseOverride.rowIndex,
            colIndex: baseOverride.colIndex,
          }
        : activeCursor
          ? {
              rowId: activeCursor.rowId,
              rowIndex: activeCursor.rowIndex,
              colIndex: activeCursor.colIndex,
            }
          : null
      applyMatrixToSelection(matrix, normalizedBase)
      if (!containerRef.value) {
        return
      }
      const area = matrixFitsSelection && activeRangeBeforePaste
        ? {
            startRow: Math.min(activeRangeBeforePaste.startRow, activeRangeBeforePaste.endRow),
            endRow: Math.max(activeRangeBeforePaste.startRow, activeRangeBeforePaste.endRow),
            startCol: Math.min(activeRangeBeforePaste.startCol, activeRangeBeforePaste.endCol),
            endCol: Math.max(activeRangeBeforePaste.startCol, activeRangeBeforePaste.endCol),
          }
        : normalizedBase && matrixRows > 0 && matrixCols > 0
          ? {
              startRow: normalizedBase.rowIndex,
              endRow: normalizedBase.rowIndex + matrixRows - 1,
              startCol: normalizedBase.colIndex,
              endCol: normalizedBase.colIndex + matrixCols - 1,
            }
          : null
      if (!area) {
        return
      }
      const cells = getCellsForArea(area)
      if (cells.length) {
        flash(cells, "paste")
      }
    },
  })

  const {
    copySelectionToClipboard,
    cutSelectionToClipboard,
    pasteClipboardData,
    exportCSV,
    importCSV,
    cancelCutPreview,
  } = clipboard

  function getCellsForArea(area: SelectionAreaRect): HTMLElement[] {
    const containers = cellContainers.value
    if (!containers.length) return []
    const normalized = {
      startRow: Math.min(area.startRow, area.endRow),
      endRow: Math.max(area.startRow, area.endRow),
      startCol: Math.min(area.startCol, area.endCol),
      endCol: Math.max(area.startCol, area.endCol),
    }
    return getCellElementsByRange(containers, normalized, visibleColumns.value)
  }

  function collectCellsForCopyFlash(): HTMLElement[] {
    const containers = cellContainers.value
    if (!containers.length) return []
    const unique = new Set<HTMLElement>()
    const snapshot = getSelectionSnapshot()

    for (const range of snapshot.ranges) {
      const cells = getCellsForArea({
        startRow: range.startRow,
        endRow: range.endRow,
        startCol: range.startCol,
        endCol: range.endCol,
      })
      cells.forEach(cell => unique.add(cell))
    }

    if (!snapshot.ranges.length) {
      const active = anchorCell.value ?? selectedCell.value
      if (active) {
  const cells = getCellsForArea({
          startRow: active.rowIndex,
          endRow: active.rowIndex,
          startCol: active.colIndex,
          endCol: active.colIndex,
        })
        cells.forEach(cell => unique.add(cell))
      }
    }

    const rowSelection = fullRowSelection.value
    if (rowSelection) {
      const startRow = Math.min(rowSelection.start, rowSelection.end)
      const endRow = Math.max(rowSelection.start, rowSelection.end)
      for (let row = startRow; row <= endRow; row += 1) {
        getRowCellElements(containers, row).forEach(cell => unique.add(cell))
      }
    }

    return Array.from(unique)
  }

  async function copySelectionToClipboardWithFlash() {
    const cells = collectCellsForCopyFlash()
    if (cells.length) {
      flash(cells, "copy")
    }
    await copySelectionToClipboard()
  }

  async function cutSelectionToClipboardWithFlash() {
    const cells = collectCellsForCopyFlash()
    const success = await cutSelectionToClipboard()
    if (success && cells.length) {
      flash(cells, "copy")
    }
    return success
  }

  async function pasteClipboardDataWithFlash() {
    await pasteClipboardData()
    await nextTick()
    const container = containerRef.value
    if (!container) return
    const snapshot = getSelectionSnapshot()
    const unique = new Set<HTMLElement>()

    if (snapshot.ranges.length) {
      for (const range of snapshot.ranges) {
  const cells = getCellsForArea({
          startRow: Math.min(range.startRow, range.endRow),
          endRow: Math.max(range.startRow, range.endRow),
          startCol: Math.min(range.startCol, range.endCol),
          endCol: Math.max(range.startCol, range.endCol),
        })
        cells.forEach(cell => unique.add(cell))
      }
    } else {
      const active = anchorCell.value ?? selectedCell.value
      if (active) {
        const cells = getCellsForArea({
          startRow: active.rowIndex,
          endRow: active.rowIndex,
          startCol: active.colIndex,
          endCol: active.colIndex,
        })
        cells.forEach(cell => unique.add(cell))
      }
    }

    if (unique.size) {
      flash(Array.from(unique), "paste")
    }
  }

  return {
    copySelectionToClipboard,
    cutSelectionToClipboard,
    pasteClipboardData,
    exportCSV,
    importCSV,
    copySelectionToClipboardWithFlash,
    cutSelectionToClipboardWithFlash,
    pasteClipboardDataWithFlash,
    getCellsForArea,
    cancelCutPreview,
  }
}
