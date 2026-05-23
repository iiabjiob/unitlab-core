import type { SignalAllocationRow, SignalSheet } from "@/types/signal"

const SOURCE_COLUMN_MIN_WIDTH = 36
const SOURCE_COLUMN_INITIAL_MIN_WIDTH = 44
const SOURCE_COLUMN_INITIAL_MAX_WIDTH = 240
const SOURCE_COLUMN_CHARACTER_WIDTH = 8
const SOURCE_COLUMN_HORIZONTAL_PADDING = 32

type SheetData = {
  default_sheet_index?: number
  sheets?: Array<{ index?: number; headers?: unknown[] }>
}

export function normalizeHeaderList(raw: unknown): string[] {
  if (!Array.isArray(raw)) return []
  const seen = new Set<string>()
  const headers: string[] = []
  raw.forEach((item) => {
    const header = String(item ?? "").trim()
    if (!header || seen.has(header)) return
    seen.add(header)
    headers.push(header)
  })
  return headers
}

export function extractSourceRowFromSignalMetadata(signalMetadata: unknown): Record<string, unknown> {
  if (!signalMetadata || typeof signalMetadata !== "object" || Array.isArray(signalMetadata)) {
    return {}
  }
  const row = (signalMetadata as Record<string, unknown>).row
  if (!row || typeof row !== "object" || Array.isArray(row)) {
    return {}
  }
  return row as Record<string, unknown>
}

function resolveSheetHeaders(sheet: SignalSheet | null): string[] {
  if (!sheet || !sheet.data || typeof sheet.data !== "object") return []

  const typedData = sheet.data as SheetData
  const sheets = Array.isArray(typedData.sheets) ? typedData.sheets : []
  if (!sheets.length) return []

  const defaultSheetIndex = Number(typedData.default_sheet_index)
  const byDefaultIndex = Number.isFinite(defaultSheetIndex)
    ? sheets.find(item => Number(item?.index) === defaultSheetIndex) ?? null
    : null
  const targetSheet = byDefaultIndex ?? sheets[0]

  return normalizeHeaderList(targetSheet?.headers ?? [])
}

function resolveHeadersFromRows(rows: readonly SignalAllocationRow[]): string[] {
  const seen = new Set<string>()
  const headers: string[] = []

  rows.forEach((row) => {
    const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
    Object.keys(sourceRow).forEach((key) => {
      const normalized = String(key).trim()
      if (!normalized || seen.has(normalized)) return
      seen.add(normalized)
      headers.push(normalized)
    })
  })

  return headers
}

export function resolveAllSourceColumnHeaders(
  sheet: SignalSheet | null,
  rows: readonly SignalAllocationRow[] = [],
): string[] {
  const fromSheet = resolveSheetHeaders(sheet)
  if (fromSheet.length > 0) {
    return fromSheet
  }
  return resolveHeadersFromRows(rows)
}

function countColumnLabelCharacters(label: string): number {
  return Array.from(label.trim()).length
}

export function resolveSourceColumnMinWidth(_header?: string): number {
  return SOURCE_COLUMN_MIN_WIDTH
}

export function resolveSourceColumnInitialWidth(header: string): number {
  const characterCount = Math.max(1, countColumnLabelCharacters(header))
  const estimatedWidth = characterCount * SOURCE_COLUMN_CHARACTER_WIDTH + SOURCE_COLUMN_HORIZONTAL_PADDING
  return Math.min(
    Math.max(estimatedWidth, SOURCE_COLUMN_INITIAL_MIN_WIDTH),
    SOURCE_COLUMN_INITIAL_MAX_WIDTH,
  )
}
