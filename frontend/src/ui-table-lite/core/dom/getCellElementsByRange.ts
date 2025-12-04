import { getCellElement } from "./gridUtils"

export interface SelectionArea {
  startRow: number
  endRow: number
  startCol: number
  endCol: number
}

type ContainerArgument = HTMLElement | Iterable<HTMLElement | null | undefined>

function normalizeContainers(source: ContainerArgument | null | undefined): HTMLElement[] {
  if (!source) {
    return []
  }
  if (source instanceof HTMLElement) {
    return [source]
  }
  const result: HTMLElement[] = []
  for (const maybe of source) {
    if (maybe instanceof HTMLElement) {
      result.push(maybe)
    }
  }
  return result
}

export function getCellElementsByRange(
  container: ContainerArgument,
  range: SelectionArea,
  columns: { key: string }[],
): HTMLElement[] {
  const cells: HTMLElement[] = []
  const containers = normalizeContainers(container)
  if (!containers.length) {
    return cells
  }
  for (let row = range.startRow; row <= range.endRow; row += 1) {
    for (let col = range.startCol; col <= range.endCol; col += 1) {
      const colKey = columns[col]?.key
      if (!colKey) continue
      for (const host of containers) {
        const cell = getCellElement(host, row, colKey)
        if (cell) {
          cells.push(cell)
          break
        }
      }
    }
  }
  return cells
}

export function getColumnCellElements(container: ContainerArgument, columnKey: string): HTMLElement[] {
  const containers = normalizeContainers(container)
  if (!containers.length) {
    return []
  }
  const results: HTMLElement[] = []
  for (const host of containers) {
    host.querySelectorAll<HTMLElement>(`[data-col-key="${columnKey}"]`).forEach(cell => {
      if (!results.includes(cell)) {
        results.push(cell)
      }
    })
  }
  return results
}

export function getRowCellElements(container: ContainerArgument, rowIndex: number): HTMLElement[] {
  const containers = normalizeContainers(container)
  if (!containers.length) {
    return []
  }
  const results: HTMLElement[] = []
  for (const host of containers) {
    host.querySelectorAll<HTMLElement>(`[data-row-index="${rowIndex}"][data-col-key]`).forEach(cell => {
      if (!results.includes(cell)) {
        results.push(cell)
      }
    })
  }
  return results
}
