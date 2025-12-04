/**
 * Pure selection geometry helpers shared between framework adapters.
 * Encapsulates rectangular math so higher-level composables can focus on
 * wiring events and state.
 */

export interface Anchor {
  rowIndex: number
  colIndex: number
}

export interface SelectionArea {
  startRow: number
  endRow: number
  startCol: number
  endCol: number
}

export interface Range extends SelectionArea {
  anchor: Anchor
  focus: Anchor
}

export interface SelectionGrid {
  rowCount: number
  colCount: number
}

function clampIndex(value: number, min: number, max: number): number {
  if (!Number.isFinite(value)) return min
  if (value < min) return min
  if (value > max) return max
  return value
}

function clampScalar(value: number, min: number, max: number): number {
  if (!Number.isFinite(value)) return min
  if (value < min) return min
  if (value > max) return max
  return value
}

export interface GridSelectionPointLike<TRowKey = unknown> {
  rowIndex: number
  colIndex: number
  rowId?: TRowKey | null
}

export interface GridSelectionPoint<TRowKey = unknown> extends GridSelectionPointLike<TRowKey> {
  rowId: TRowKey | null
}

export interface GridSelectionRangeInput<TRowKey = unknown> extends SelectionArea {
  anchor?: GridSelectionPointLike<TRowKey>
  focus?: GridSelectionPointLike<TRowKey>
}

export interface GridSelectionRange<TRowKey = unknown> extends SelectionArea {
  anchor: GridSelectionPoint<TRowKey>
  focus: GridSelectionPoint<TRowKey>
  startRowId?: TRowKey | null
  endRowId?: TRowKey | null
}

export interface GridSelectionContext<TRowKey = unknown> {
  grid: SelectionGrid
  getRowIdByIndex?: (rowIndex: number) => TRowKey | null
}

export function clampGridSelectionPoint<TRowKey = unknown>(
  point: GridSelectionPointLike<TRowKey>,
  context: GridSelectionContext<TRowKey>,
): GridSelectionPoint<TRowKey> {
  const rowCount = Math.max(0, context.grid.rowCount)
  const colCount = Math.max(0, context.grid.colCount)
  const maxRow = Math.max(rowCount - 1, 0)
  const maxCol = Math.max(colCount - 1, 0)

  const rowIndex = rowCount > 0 ? clampIndex(point.rowIndex, 0, maxRow) : 0
  const colIndex = colCount > 0 ? clampIndex(point.colIndex, 0, maxCol) : 0
  const resolveRowId = context.getRowIdByIndex
  const rowId =
    point.rowId != null
      ? point.rowId
      : resolveRowId
        ? resolveRowId(rowIndex) ?? null
        : null

  return {
    rowIndex,
    colIndex,
    rowId,
  }
}

export function createGridSelectionRange<TRowKey = unknown>(
  anchor: GridSelectionPointLike<TRowKey>,
  focus: GridSelectionPointLike<TRowKey>,
  context: GridSelectionContext<TRowKey>,
): GridSelectionRange<TRowKey> {
  const clampedAnchor = clampGridSelectionPoint(anchor, context)
  const clampedFocus = clampGridSelectionPoint(focus, context)

  const startRow = Math.min(clampedAnchor.rowIndex, clampedFocus.rowIndex)
  const endRow = Math.max(clampedAnchor.rowIndex, clampedFocus.rowIndex)
  const startCol = Math.min(clampedAnchor.colIndex, clampedFocus.colIndex)
  const endCol = Math.max(clampedAnchor.colIndex, clampedFocus.colIndex)

  const resolveRowId = context.getRowIdByIndex

  const startRowIdCandidate = clampedAnchor.rowIndex === startRow ? clampedAnchor.rowId : null
  const startRowIdFallback = clampedFocus.rowIndex === startRow ? clampedFocus.rowId : null
  const endRowIdCandidate = clampedFocus.rowIndex === endRow ? clampedFocus.rowId : null
  const endRowIdFallback = clampedAnchor.rowIndex === endRow ? clampedAnchor.rowId : null

  const startRowId =
    startRowIdCandidate ??
    startRowIdFallback ??
    (resolveRowId ? resolveRowId(startRow) ?? null : null)

  const endRowId =
    endRowIdCandidate ??
    endRowIdFallback ??
    (resolveRowId ? resolveRowId(endRow) ?? null : null)

  return {
    anchor: clampedAnchor,
    focus: clampedFocus,
    startRow,
    endRow,
    startCol,
    endCol,
    startRowId: startRowId ?? null,
    endRowId: endRowId ?? null,
  }
}

export function normalizeGridSelectionRange<TRowKey = unknown>(
  range: GridSelectionRange<TRowKey>,
  context: GridSelectionContext<TRowKey>,
): GridSelectionRange<TRowKey> | null {
  if (context.grid.rowCount <= 0 || context.grid.colCount <= 0) {
    return null
  }
  return createGridSelectionRange(range.anchor, range.focus, context)
}

export function createGridSelectionRangeFromInput<TRowKey = unknown>(
  input: GridSelectionRangeInput<TRowKey>,
  context: GridSelectionContext<TRowKey>,
): GridSelectionRange<TRowKey> {
  const anchor = input.anchor ?? {
    rowIndex: Math.min(input.startRow, input.endRow),
    colIndex: Math.min(input.startCol, input.endCol),
  }
  const focus = input.focus ?? {
    rowIndex: Math.max(input.startRow, input.endRow),
    colIndex: Math.max(input.startCol, input.endCol),
  }

  return createGridSelectionRange(anchor, focus, context)
}

export function clampSelectionArea<TRowKey = unknown>(
  area: SelectionArea,
  context: GridSelectionContext<TRowKey>,
): SelectionArea | null {
  const normalized = normalizeArea(area)
  const rowCount = Math.max(0, context.grid.rowCount)
  const colCount = Math.max(0, context.grid.colCount)
  if (rowCount === 0 || colCount === 0) {
    return null
  }
  const maxRow = Math.max(rowCount - 1, 0)
  const maxCol = Math.max(colCount - 1, 0)

  const startRow = clampScalar(normalized.startRow, 0, maxRow)
  const endRow = clampScalar(normalized.endRow, 0, maxRow)
  const startCol = clampScalar(normalized.startCol, 0, maxCol)
  const endCol = clampScalar(normalized.endCol, 0, maxCol)

  if (startRow > endRow || startCol > endCol) {
    return null
  }

  return {
    startRow,
    endRow,
    startCol,
    endCol,
  }
}

export function resolveSelectionBounds<TRowKey = unknown>(
  range: GridSelectionRange<TRowKey> | null,
  context: GridSelectionContext<TRowKey>,
  fallbackToAll = false,
): SelectionArea | null {
  if (range) {
    return clampSelectionArea(range, context)
  }

  if (!fallbackToAll) {
    return null
  }

  const fullArea: SelectionArea = {
    startRow: 0,
    endRow: context.grid.rowCount - 1,
    startCol: 0,
    endCol: context.grid.colCount - 1,
  }

  return clampSelectionArea(fullArea, context)
}

function normalizeArea(area: SelectionArea): SelectionArea {
  const startRow = Math.min(area.startRow, area.endRow)
  const endRow = Math.max(area.startRow, area.endRow)
  const startCol = Math.min(area.startCol, area.endCol)
  const endCol = Math.max(area.startCol, area.endCol)

  return {
    startRow,
    endRow,
    startCol,
    endCol,
  }
}

function areasOverlap(a: SelectionArea, b: SelectionArea): boolean {
  return (
    a.startRow <= b.endRow &&
    a.endRow >= b.startRow &&
    a.startCol <= b.endCol &&
    a.endCol >= b.startCol
  )
}

function mergePair(a: SelectionArea, b: SelectionArea): SelectionArea {
  return {
    startRow: Math.min(a.startRow, b.startRow),
    endRow: Math.max(a.endRow, b.endRow),
    startCol: Math.min(a.startCol, b.startCol),
    endCol: Math.max(a.endCol, b.endCol),
  }
}

export function mergeRanges(ranges: SelectionArea[]): SelectionArea[] {
  const normalized = ranges.map(normalizeArea)
  const result: SelectionArea[] = []

  for (const range of normalized) {
    let merged = range
    let keepMerging = true

    while (keepMerging) {
      keepMerging = false
      for (let index = 0; index < result.length; index += 1) {
        const current = result[index]
        if (areasOverlap(current, merged)) {
          merged = mergePair(current, merged)
          result.splice(index, 1)
          keepMerging = true
          break
        }
      }
    }

    result.push(merged)
  }

  return result
}

function subtractArea(base: SelectionArea, removal: SelectionArea): SelectionArea[] {
  const normalizedBase = normalizeArea(base)
  const normalizedRemoval = normalizeArea(removal)

  if (!areasOverlap(normalizedBase, normalizedRemoval)) {
    return [normalizedBase]
  }

  const overlapStartRow = Math.max(normalizedBase.startRow, normalizedRemoval.startRow)
  const overlapEndRow = Math.min(normalizedBase.endRow, normalizedRemoval.endRow)
  const overlapStartCol = Math.max(normalizedBase.startCol, normalizedRemoval.startCol)
  const overlapEndCol = Math.min(normalizedBase.endCol, normalizedRemoval.endCol)

  const pieces: SelectionArea[] = []

  if (normalizedBase.startRow <= overlapStartRow - 1) {
    pieces.push({
      startRow: normalizedBase.startRow,
      endRow: overlapStartRow - 1,
      startCol: normalizedBase.startCol,
      endCol: normalizedBase.endCol,
    })
  }

  if (overlapEndRow + 1 <= normalizedBase.endRow) {
    pieces.push({
      startRow: overlapEndRow + 1,
      endRow: normalizedBase.endRow,
      startCol: normalizedBase.startCol,
      endCol: normalizedBase.endCol,
    })
  }

  const middleRowStart = Math.max(normalizedBase.startRow, overlapStartRow)
  const middleRowEnd = Math.min(normalizedBase.endRow, overlapEndRow)

  if (middleRowStart <= middleRowEnd) {
    if (normalizedBase.startCol <= overlapStartCol - 1) {
      pieces.push({
        startRow: middleRowStart,
        endRow: middleRowEnd,
        startCol: normalizedBase.startCol,
        endCol: overlapStartCol - 1,
      })
    }

    if (overlapEndCol + 1 <= normalizedBase.endCol) {
      pieces.push({
        startRow: middleRowStart,
        endRow: middleRowEnd,
        startCol: overlapEndCol + 1,
        endCol: normalizedBase.endCol,
      })
    }
  }

  return pieces
}

export function addRange(ranges: SelectionArea[], next: SelectionArea): SelectionArea[] {
  return mergeRanges([...ranges, next])
}

export function removeRange(ranges: SelectionArea[], target: SelectionArea): SelectionArea[] {
  const normalizedTarget = normalizeArea(target)
  const result: SelectionArea[] = []

  for (const range of ranges) {
    const pieces = subtractArea(range, normalizedTarget)
    for (const piece of pieces) {
      result.push(piece)
    }
  }

  return mergeRanges(result)
}

export function isCellSelected(ranges: SelectionArea[], rowIndex: number, colIndex: number): boolean {
  for (const range of ranges) {
    if (
      rowIndex >= range.startRow &&
      rowIndex <= range.endRow &&
      colIndex >= range.startCol &&
      colIndex <= range.endCol
    ) {
      return true
    }
  }
  return false
}

export function rangesFromSelection(ranges: Range[]): SelectionArea[] {
  return ranges.map(range => normalizeArea(range))
}

export function selectionFromAreas(
  areas: SelectionArea[],
  createAnchor: (rowIndex: number, colIndex: number) => Anchor,
): Range[] {
  return areas.map(area => {
    const normalized = normalizeArea(area)
    return {
      anchor: createAnchor(normalized.startRow, normalized.startCol),
      focus: createAnchor(normalized.endRow, normalized.endCol),
      ...normalized,
    }
  })
}
