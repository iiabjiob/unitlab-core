import type {
  TableViewportContainerMetrics,
  TableViewportDomStats,
  TableViewportHeaderMetrics,
  TableViewportHostEnvironment,
  TableViewportResizeObserver,
} from "./viewportHostEnvironment"

const GLOBAL_FALLBACK: typeof globalThis = typeof globalThis !== "undefined" ? globalThis : ({} as typeof globalThis)

export interface ViewportClock {
  now(): number
}

export function createMonotonicClock(globalRef: typeof globalThis = GLOBAL_FALLBACK): ViewportClock {
  const performanceRef = (globalRef as typeof globalRef & { performance?: Performance }).performance
  if (performanceRef && typeof performanceRef.now === "function") {
    return {
      now: () => performanceRef.now(),
    }
  }
  const DateCtor = (globalRef as typeof globalRef & { Date?: DateConstructor }).Date ?? Date
  return {
    now: () => new DateCtor().getTime(),
  }
}

export interface DefaultHostEnvironmentOptions {
  scrollListenerOptions?: AddEventListenerOptions
  scrollRemoveOptions?: boolean | EventListenerOptions
  resizeObserverFactory?: (callback: () => void) => TableViewportResizeObserver | null
}

type ResizeObserverFactory = (callback: () => void) => TableViewportResizeObserver | null

function createDomResizeObserverFactory(globalRef: typeof globalThis): ResizeObserverFactory {
  return callback => {
    const ResizeObserverRef = (globalRef as typeof globalRef & { ResizeObserver?: typeof ResizeObserver }).ResizeObserver
    if (typeof ResizeObserverRef !== "function") {
      return null
    }
    const observer = new ResizeObserverRef(() => callback())
    return {
      observe(target) {
        observer.observe(target)
      },
      unobserve(target) {
        if (typeof observer.unobserve === "function") {
          observer.unobserve(target)
        }
      },
      disconnect() {
        observer.disconnect()
      },
    }
  }
}

export function createNoopResizeObserver(): TableViewportResizeObserver {
  return {
    observe() {
      // no-op
    },
    disconnect() {
      // no-op
    },
  }
}

function readDomContainerMetrics(target: HTMLDivElement, _globalRef: typeof globalThis): TableViewportContainerMetrics | null {
  if (!target) return null
  const scrollTop = Number.isFinite(target.scrollTop) ? target.scrollTop : 0
  const scrollLeft = Number.isFinite(target.scrollLeft) ? target.scrollLeft : 0
  const clientHeight = Number.isFinite(target.clientHeight) ? target.clientHeight : 0
  const clientWidth = Number.isFinite(target.clientWidth) ? target.clientWidth : 0
  const scrollHeight = Number.isFinite(target.scrollHeight) ? target.scrollHeight : 0
  const scrollWidth = Number.isFinite(target.scrollWidth) ? target.scrollWidth : 0

  return {
    clientHeight,
    clientWidth,
    scrollHeight,
    scrollWidth,
    scrollTop,
    scrollLeft,
  }
}

function readDomHeaderMetrics(target: HTMLElement | null): TableViewportHeaderMetrics | null {
  if (!target) {
    return { height: 0 }
  }
  const height = Number.isFinite(target.offsetHeight) ? target.offsetHeight : 0
  return { height }
}

function getDomBoundingClientRect(target: HTMLElement): DOMRect | null {
  if (!target || typeof target.getBoundingClientRect !== "function") {
    return null
  }
  const rect = target.getBoundingClientRect()
  if (!rect) return null
  return rect
}

function normalizeDomScrollLeft(target: HTMLElement): number {
  if (!target) return 0
  const raw = target.scrollLeft
  if (!Number.isFinite(raw)) {
    return 0
  }
  if (raw >= 0) {
    return raw
  }
  const dirAttribute = target.dir || target.getAttribute("dir")
  const documentDir = target.ownerDocument?.dir
  const direction = (dirAttribute || documentDir || "").toLowerCase()
  if (direction !== "rtl") {
    return raw
  }
  const maxOffset = target.scrollWidth - target.clientWidth
  if (!Number.isFinite(maxOffset)) {
    return 0
  }
  const normalized = maxOffset + raw
  return normalized >= 0 ? normalized : 0
}

function isDomEventFromContainer(event: Event, container: HTMLElement): boolean {
  if (!container || !event) return false
  if (event.currentTarget !== container) {
    return false
  }
  if (event.target === container) {
    return true
  }
  const path = typeof event.composedPath === "function" ? event.composedPath() : null
  if (!Array.isArray(path)) {
    return false
  }
  return path.includes(container)
}

function queryDomStats(container: HTMLElement): TableViewportDomStats | null {
  if (!container?.querySelectorAll) {
    return null
  }
  const rowLayers = container.querySelectorAll(".ui-table__row-layer").length
  const cells = container.querySelectorAll(".ui-table__row-layer .ui-table-cell").length
  const fillers = container.querySelectorAll(".ui-table__column-filler").length
  return {
    rowLayers,
    cells,
    fillers,
  }
}

export function createDefaultHostEnvironment(
  globalRef: typeof globalThis = GLOBAL_FALLBACK,
  options?: DefaultHostEnvironmentOptions,
): TableViewportHostEnvironment {
  const scrollOptions = options?.scrollListenerOptions ?? { passive: true }
  const removeOptions = options?.scrollRemoveOptions
  const resizeObserverFactory = options?.resizeObserverFactory ?? createDomResizeObserverFactory(globalRef)

  return {
    addScrollListener(target: EventTarget, listener: (event: Event) => void, listenerOptions?: AddEventListenerOptions) {
      const addEventListener = (target as EventTarget | null)?.addEventListener
      if (typeof addEventListener === "function") {
        addEventListener.call(target, "scroll", listener, listenerOptions ?? scrollOptions)
      }
    },
    removeScrollListener(
      target: EventTarget,
      listener: (event: Event) => void,
      listenerOptions?: boolean | EventListenerOptions,
    ) {
      const removeEventListener = (target as EventTarget | null)?.removeEventListener
      if (typeof removeEventListener === "function") {
        const effectiveOptions = listenerOptions ?? removeOptions
        if (effectiveOptions !== undefined) {
          removeEventListener.call(target, "scroll", listener, effectiveOptions)
        } else {
          removeEventListener.call(target, "scroll", listener)
        }
      }
    },
    createResizeObserver(callback: () => void) {
      return resizeObserverFactory(callback)
    },
    removeResizeObserverTarget(observer: TableViewportResizeObserver, target: Element) {
      if (typeof observer.unobserve === "function") {
        observer.unobserve(target)
      } else {
        observer.disconnect()
      }
    },
    readContainerMetrics(target: HTMLDivElement) {
      return readDomContainerMetrics(target, globalRef)
    },
    readHeaderMetrics(target: HTMLElement | null) {
      return readDomHeaderMetrics(target)
    },
    getBoundingClientRect(target: HTMLElement) {
      return getDomBoundingClientRect(target)
    },
    normalizeScrollLeft(target: HTMLElement) {
      return normalizeDomScrollLeft(target)
    },
    isEventFromContainer(event: Event, container: HTMLElement) {
      return isDomEventFromContainer(event, container)
    },
    queryDebugDomStats(container: HTMLElement) {
      return queryDomStats(container)
    },
  }
}
