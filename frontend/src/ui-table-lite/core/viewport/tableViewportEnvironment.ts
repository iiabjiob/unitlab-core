import type { TableViewportHostEnvironment } from "./viewportHostEnvironment"

export interface ContainerMetrics {
  clientHeight: number
  clientWidth: number
  scrollHeight: number
  scrollWidth: number
  scrollTop: number
  scrollLeft: number
}

export function sampleContainerMetrics(
  hostEnvironment: TableViewportHostEnvironment,
  recordLayoutRead: (count?: number) => void,
  container: HTMLDivElement,
): ContainerMetrics {
  recordLayoutRead(4)
  const metrics = hostEnvironment.readContainerMetrics?.(container)
  if (metrics) {
    return metrics
  }
  return {
    clientHeight: Number.isFinite(container.clientHeight) ? container.clientHeight : 0,
    clientWidth: Number.isFinite(container.clientWidth) ? container.clientWidth : 0,
    scrollHeight: Number.isFinite(container.scrollHeight) ? container.scrollHeight : 0,
    scrollWidth: Number.isFinite(container.scrollWidth) ? container.scrollWidth : 0,
    scrollTop: Number.isFinite(container.scrollTop) ? container.scrollTop : 0,
    scrollLeft:
      typeof hostEnvironment.normalizeScrollLeft === "function"
        ? hostEnvironment.normalizeScrollLeft(container)
        : Number.isFinite(container.scrollLeft)
          ? container.scrollLeft
          : 0,
  }
}

export function sampleHeaderHeight(
  hostEnvironment: TableViewportHostEnvironment,
  recordLayoutRead: (count?: number) => void,
  header: HTMLElement | null,
): number {
  recordLayoutRead()
  const metrics = hostEnvironment.readHeaderMetrics?.(header)
  if (metrics) {
    return metrics.height
  }
  if (!header) {
    return 0
  }
  return Number.isFinite(header.offsetHeight) ? header.offsetHeight : 0
}

export function sampleBoundingRect(
  hostEnvironment: TableViewportHostEnvironment,
  recordLayoutRead: (count?: number) => void,
  target: HTMLElement,
): DOMRect | null {
  recordLayoutRead()
  const resolved = hostEnvironment.getBoundingClientRect?.(target)
  if (resolved) {
    return resolved
  }
  return typeof target.getBoundingClientRect === "function" ? target.getBoundingClientRect() : null
}

export function resolveDomStats(
  hostEnvironment: TableViewportHostEnvironment,
  container: HTMLElement | null,
): { rowLayers: number; cells: number; fillers: number } {
  if (!container) {
    return { rowLayers: 0, cells: 0, fillers: 0 }
  }
  const stats = hostEnvironment.queryDebugDomStats?.(container)
  if (stats) {
    return stats
  }
  const query = container.querySelectorAll?.bind(container)
  if (!query) {
    return { rowLayers: 0, cells: 0, fillers: 0 }
  }
  return {
    rowLayers: query(".ui-table__row-layer").length,
    cells: query(".ui-table__row-layer .ui-table-cell").length,
    fillers: query(".ui-table__column-filler").length,
  }
}
