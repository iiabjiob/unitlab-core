import type { UiTableSelectionSnapshot } from "../types"
import type { ClipboardAdapter } from "../dom/domAdapters"

export interface SelectionPoint {
  rowId: UiTableSelectionSnapshot["ranges"][number]["anchor"]["rowId"]
  rowIndex: number
  colIndex: number
}

export interface SelectionRange {
  anchor: SelectionPoint
  focus: SelectionPoint
  startRow: number
  endRow: number
  startCol: number
  endCol: number
}

interface UseTableClipboardOptions {
  clipboard: ClipboardAdapter
  getActiveRange: () => SelectionRange | null
  buildSelectionMatrix: (range: SelectionRange | null, options?: { includeHeaders?: boolean; fallbackToAll?: boolean }) => string[][]
  applyMatrixToSelection: (matrix: string[][], baseOverride?: SelectionPoint | null) => void
  getSelectionSnapshot: () => UiTableSelectionSnapshot
  beginCutPreview: (snapshot: UiTableSelectionSnapshot | null) => void
  clearCutPreview: () => void
  commitPendingCut: () => boolean
}

export function useTableClipboard({
  clipboard,
  getActiveRange,
  buildSelectionMatrix,
  applyMatrixToSelection,
  getSelectionSnapshot,
  beginCutPreview,
  clearCutPreview,
  commitPendingCut,
}: UseTableClipboardOptions) {
  const MAX_CLIPBOARD_CELLS = 10_000
  const LARGE_MATRIX_THRESHOLD = 2_500
  const scheduleFrame =
    typeof globalThis !== "undefined" && typeof globalThis.requestAnimationFrame === "function"
      ? globalThis.requestAnimationFrame.bind(globalThis)
      : (callback: FrameRequestCallback) =>
          setTimeout(() => callback(typeof performance !== "undefined" ? performance.now() : Date.now()), 0)

  async function writeTextToClipboard(text: string) {
    await clipboard.writeText(text)
  }

  async function readTextFromClipboard(): Promise<string | null> {
    try {
      return await clipboard.readText()
    } catch {
      return null
    }
  }

  async function copySelectionToClipboard() {
    clearCutPreview()
    const range = getActiveRange()
    if (!range) return
    const matrix = buildSelectionMatrix(range, { includeHeaders: false })
    if (!matrix.length) return
    const totalCells = matrix.reduce((acc, row) => acc + row.length, 0)
    if (!totalCells) return
    if (totalCells > MAX_CLIPBOARD_CELLS) return
    const payload = matrix.map(row => row.join("\t")).join("\n")
    if (!payload.length) return
    await writeTextToClipboard(payload)
  }

  function parseDelimited(text: string, delimiter: string): string[][] {
    const rows: string[][] = []
    let currentRow: string[] = []
    let currentValue = ""
    let inQuotes = false
    const normalized = text.replace(/\r\n?/g, "\n")

    for (let i = 0; i < normalized.length; i += 1) {
      const char = normalized[i]

      if (inQuotes) {
        if (char === "\"") {
          if (normalized[i + 1] === "\"") {
            currentValue += "\""
            i += 1
          } else {
            inQuotes = false
          }
        } else {
          currentValue += char
        }
        continue
      }

      if (char === "\"") {
        inQuotes = true
        continue
      }

      if (char === delimiter) {
        currentRow.push(currentValue)
        currentValue = ""
        continue
      }

      if (char === "\n") {
        currentRow.push(currentValue)
        rows.push(currentRow)
        currentRow = []
        currentValue = ""
        continue
      }

      currentValue += char
    }

    currentRow.push(currentValue)
    rows.push(currentRow)

    return rows.filter((row, index) => !(row.length === 1 && row[0] === "" && index === rows.length - 1))
  }

  function parseClipboardMatrix(text: string): string[][] {
    if (!text.trim()) return []
    const normalized = text.replace(/\r\n?/g, "\n")
    if (normalized.includes("\t")) {
      return parseDelimited(normalized, "\t")
    }
    return parseCsv(normalized)
  }

  async function pasteClipboardData() {
    const text = await readTextFromClipboard()
    if (!text) return
    applyClipboardText(text)
  }

  async function applyClipboardMatrix(matrix: string[][], baseOverride?: SelectionPoint | null) {
    if (!matrix.length || !matrix[0]?.length) return
    const totalCells = matrix.reduce((acc, row) => acc + row.length, 0)
    if (totalCells > MAX_CLIPBOARD_CELLS) return

    commitPendingCut()
    clearCutPreview()

    if (totalCells > LARGE_MATRIX_THRESHOLD) {
      await new Promise<void>(resolve => {
        scheduleFrame(() => resolve())
      })
    }

    applyMatrixToSelection(matrix, baseOverride ?? null)
  }

  async function applyClipboardText(text: string) {
    const matrix = parseClipboardMatrix(text)
    await applyClipboardMatrix(matrix)
  }

  async function cutSelectionToClipboard(): Promise<boolean> {
    clearCutPreview()
    const range = getActiveRange()
    if (!range) return false
    const matrix = buildSelectionMatrix(range, { includeHeaders: false })
    if (!matrix.length) return false
    const payload = matrix.map(row => row.join("\t")).join("\n")
    if (!payload.length) return false
    const snapshot = getSelectionSnapshot()
    try {
      await writeTextToClipboard(payload)
    } catch {
      return false
    }
    beginCutPreview(snapshot)
    return true
  }

  function cancelCutPreview() {
    clearCutPreview()
  }

  function parseCsv(text: string): string[][] {
    return parseDelimited(text, ",")
  }

  function escapeCsvValue(value: string): string {
    if (value.includes("\"")) {
      value = value.replace(/\"/g, "\"\"")
    }
      if (/[",\n]/.test(value)) {
        return `"${value}"`
    }
    return value
  }

  function matrixToCsv(matrix: string[][]): string {
    return matrix.map(row => row.map(cell => escapeCsvValue(cell ?? "")).join(",")).join("\n")
  }

  function exportCSV(options?: { includeHeaders?: boolean; range?: SelectionRange | null }) {
    const matrix = buildSelectionMatrix(options?.range ?? getActiveRange(), {
      includeHeaders: options?.includeHeaders ?? true,
      fallbackToAll: true,
    })
    if (!matrix.length) return ""
    return matrixToCsv(matrix)
  }

  function importCSV(text: string, options?: { base?: SelectionPoint | null }) {
    const matrix = parseCsv(text)
    if (!matrix.length || !matrix[0]?.length) return false
    void applyClipboardMatrix(matrix, options?.base ?? null)
    return true
  }

  return {
    copySelectionToClipboard,
    cutSelectionToClipboard,
    pasteClipboardData,
    applyClipboardText,
    parseClipboardMatrix,
    parseCsv,
    exportCSV,
    importCSV,
    cancelCutPreview,
  }
}
