import type { GridSelectionRange } from "./selectionState"
import type { SelectionArea } from "./selectionState"

export interface SelectionEdges {
  top: boolean
  bottom: boolean
  left: boolean
  right: boolean
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

function rangeToArea<TRowKey>(range: GridSelectionRange<TRowKey>): SelectionArea {
  return {
    startRow: Math.min(range.startRow, range.endRow),
    endRow: Math.max(range.startRow, range.endRow),
    startCol: Math.min(range.startCol, range.endCol),
    endCol: Math.max(range.startCol, range.endCol),
  }
}

export function areaContainsCell(area: SelectionArea, rowIndex: number, colIndex: number): boolean {
  const normalized = normalizeArea(area)
  return (
    rowIndex >= normalized.startRow &&
    rowIndex <= normalized.endRow &&
    colIndex >= normalized.startCol &&
    colIndex <= normalized.endCol
  )
}

export function areaContainsRow(area: SelectionArea, rowIndex: number): boolean {
  const normalized = normalizeArea(area)
  return rowIndex >= normalized.startRow && rowIndex <= normalized.endRow
}

export function areaContainsColumn(area: SelectionArea, colIndex: number): boolean {
  const normalized = normalizeArea(area)
  return colIndex >= normalized.startCol && colIndex <= normalized.endCol
}

export function anyAreaContainsRow(areas: readonly SelectionArea[], rowIndex: number): boolean {
  for (const area of areas) {
    if (areaContainsRow(area, rowIndex)) {
      return true
    }
  }
  return false
}

export function anyAreaContainsColumn(areas: readonly SelectionArea[], colIndex: number): boolean {
  for (const area of areas) {
    if (areaContainsColumn(area, colIndex)) {
      return true
    }
  }
  return false
}

export function areaEdges(area: SelectionArea, rowIndex: number, colIndex: number): SelectionEdges | null {
  if (!areaContainsCell(area, rowIndex, colIndex)) {
    return null
  }
  const normalized = normalizeArea(area)
  return {
    top: rowIndex === normalized.startRow,
    bottom: rowIndex === normalized.endRow,
    left: colIndex === normalized.startCol,
    right: colIndex === normalized.endCol,
  }
}

export function findAreaIndexContaining(areas: readonly SelectionArea[], rowIndex: number, colIndex: number): number {
  for (let index = 0; index < areas.length; index += 1) {
    if (areaContainsCell(areas[index], rowIndex, colIndex)) {
      return index
    }
  }
  return -1
}

export function rangeContainsCell<TRowKey>(
  range: GridSelectionRange<TRowKey>,
  rowIndex: number,
  colIndex: number,
): boolean {
  return areaContainsCell(rangeToArea(range), rowIndex, colIndex)
}

export function findRangeIndexContaining<TRowKey>(
  ranges: readonly GridSelectionRange<TRowKey>[],
  rowIndex: number,
  colIndex: number,
): number {
  for (let index = 0; index < ranges.length; index += 1) {
    if (rangeContainsCell(ranges[index], rowIndex, colIndex)) {
      return index
    }
  }
  return -1
}

export function rangeEdges<TRowKey>(
  range: GridSelectionRange<TRowKey>,
  rowIndex: number,
  colIndex: number,
): SelectionEdges | null {
  return areaEdges(rangeToArea(range), rowIndex, colIndex)
}

export function selectionEdges<TRowKey>(
  ranges: readonly GridSelectionRange<TRowKey>[],
  activeIndex: number,
  rowIndex: number,
  colIndex: number,
): (SelectionEdges & { active: boolean }) | null {
  const index = findRangeIndexContaining(ranges, rowIndex, colIndex)
  if (index === -1) {
    return null
  }
  const edges = rangeEdges(ranges[index], rowIndex, colIndex)
  if (!edges) {
    return null
  }
  return {
    ...edges,
    active: index === activeIndex,
  }
}
