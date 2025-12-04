import type { TableViewportResizeObserver } from "./viewportHostEnvironment"
import type { TableViewportScrollStateAdapter } from "./tableViewportScrollIo"
import type { ViewportSyncTargets } from "./tableViewportTypes"
import type { ViewportSyncState } from "./tableViewportTypes"

export interface TableViewportDomState {
  container: HTMLDivElement | null
  header: HTMLElement | null
  resizeObserver: TableViewportResizeObserver | null
  attached: boolean
  pendingScrollTop: number | null
  pendingScrollLeft: number | null
  afterScrollTaskId: number | null
  scrollSyncTaskId: number | null
  lastScrollTopSample: number
  lastScrollLeftSample: number
  pendingHorizontalSettle: boolean
  syncTargets: ViewportSyncTargets | null
  syncState: ViewportSyncState
  lastAppliedScrollTop: number
  lastAppliedScrollLeft: number
  lastHeavyScrollTop: number
  lastHeavyScrollLeft: number
  driftCorrectionPending: boolean
}

export interface TableViewportScrollStateHooks {
  resetCachedMeasurements?: () => void
  clearLayoutMeasurement?: () => void
}

export interface TableViewportScrollState {
  state: TableViewportDomState
  adapter: TableViewportScrollStateAdapter
  reset(): void
}

export function createTableViewportScrollState(
  hooks: TableViewportScrollStateHooks = {},
): TableViewportScrollState {
  const domState: TableViewportDomState = {
    container: null,
    header: null,
    resizeObserver: null,
    attached: false,
    pendingScrollTop: null,
    pendingScrollLeft: null,
    afterScrollTaskId: null,
    scrollSyncTaskId: null,
    lastScrollTopSample: 0,
    lastScrollLeftSample: 0,
    pendingHorizontalSettle: false,
    syncTargets: null,
    syncState: { scrollLeft: 0, scrollTop: 0, pinnedOffsetLeft: 0, pinnedOffsetRight: 0 },
    lastAppliedScrollTop: 0,
    lastAppliedScrollLeft: 0,
    lastHeavyScrollTop: 0,
    lastHeavyScrollLeft: 0,
    driftCorrectionPending: false,
  }

  const adapter: TableViewportScrollStateAdapter = {
    getContainer: () => domState.container,
    setContainer: value => {
      domState.container = value
    },
    getHeader: () => domState.header,
    setHeader: value => {
      domState.header = value
    },
    getSyncTargets: () => domState.syncTargets,
    setSyncTargets: value => {
      domState.syncTargets = value
    },
    getSyncState: () => domState.syncState,
    getLastAppliedScroll: () => ({ top: domState.lastAppliedScrollTop, left: domState.lastAppliedScrollLeft }),
    setLastAppliedScroll: (top, left) => {
      domState.lastAppliedScrollTop = top
      domState.lastAppliedScrollLeft = left
    },
    getLastHeavyScroll: () => ({ top: domState.lastHeavyScrollTop, left: domState.lastHeavyScrollLeft }),
    setLastHeavyScroll: (top, left) => {
      domState.lastHeavyScrollTop = top
      domState.lastHeavyScrollLeft = left
    },
    isAttached: () => domState.attached,
    setAttached: value => {
      domState.attached = value
    },
    getResizeObserver: () => domState.resizeObserver,
    setResizeObserver: value => {
      domState.resizeObserver = value
    },
    getPendingScrollTop: () => domState.pendingScrollTop,
    setPendingScrollTop: value => {
      domState.pendingScrollTop = value
    },
    getPendingScrollLeft: () => domState.pendingScrollLeft,
    setPendingScrollLeft: value => {
      domState.pendingScrollLeft = value
    },
    getAfterScrollTaskId: () => domState.afterScrollTaskId,
    setAfterScrollTaskId: value => {
      domState.afterScrollTaskId = value
    },
    getScrollSyncTaskId: () => domState.scrollSyncTaskId,
    setScrollSyncTaskId: value => {
      domState.scrollSyncTaskId = value
    },
    getLastScrollSamples: () => ({
      top: domState.lastScrollTopSample,
      left: domState.lastScrollLeftSample,
    }),
    setLastScrollSamples: (top, left) => {
      domState.lastScrollTopSample = top
      domState.lastScrollLeftSample = left
    },
    isPendingHorizontalSettle: () => domState.pendingHorizontalSettle,
    setPendingHorizontalSettle: value => {
      domState.pendingHorizontalSettle = value
    },
    isDriftCorrectionPending: () => domState.driftCorrectionPending,
    setDriftCorrectionPending: value => {
      domState.driftCorrectionPending = value
    },
    resetCachedMeasurements: () => {
      hooks.resetCachedMeasurements?.()
    },
    clearLayoutMeasurement: () => {
      hooks.clearLayoutMeasurement?.()
    },
    resetScrollSamples: () => {
      domState.lastScrollTopSample = 0
      domState.lastScrollLeftSample = 0
      domState.lastAppliedScrollTop = 0
      domState.lastAppliedScrollLeft = 0
      domState.lastHeavyScrollTop = 0
      domState.lastHeavyScrollLeft = 0
      domState.driftCorrectionPending = false
      domState.syncState.scrollLeft = 0
      domState.syncState.scrollTop = 0
      domState.syncState.pinnedOffsetLeft = 0
      domState.syncState.pinnedOffsetRight = 0
    },
  }

  function reset() {
    domState.container = null
    domState.header = null
    domState.resizeObserver = null
    domState.attached = false
    domState.pendingScrollTop = null
    domState.pendingScrollLeft = null
    domState.afterScrollTaskId = null
    domState.scrollSyncTaskId = null
    domState.lastScrollTopSample = 0
    domState.lastScrollLeftSample = 0
    domState.pendingHorizontalSettle = false
    domState.syncTargets = null
    domState.syncState.scrollLeft = 0
    domState.syncState.scrollTop = 0
    domState.syncState.pinnedOffsetLeft = 0
    domState.syncState.pinnedOffsetRight = 0
    domState.lastAppliedScrollTop = 0
    domState.lastAppliedScrollLeft = 0
    domState.lastHeavyScrollTop = 0
    domState.lastHeavyScrollLeft = 0
    domState.driftCorrectionPending = false
  }

  return {
    state: domState,
    adapter,
    reset,
  }
}
