import { afterEach, describe, expect, it, vi } from "vitest"
import { createTableViewportScrollIo, type TableViewportScrollStateAdapter } from "../tableViewportScrollIo"
import type { ViewportSyncState } from "../tableViewportTypes"
import * as scrollSyncModule from "../scrollSync"
import type { TableViewportHostEnvironment } from "../viewportHostEnvironment"
import { createFakeRafScheduler } from "./utils/fakeRafScheduler"

type SyncTargets = ReturnType<TableViewportScrollStateAdapter["getSyncTargets"]>

interface TestScrollStateAdapter extends TableViewportScrollStateAdapter {
  setScrollSyncTaskId(value: number | null): void
}

function createScrollStateAdapter(): TestScrollStateAdapter {
  let container: HTMLDivElement | null = null
  let header: HTMLElement | null = null
  let syncTargets: SyncTargets = null
  const syncState: ViewportSyncState = { scrollLeft: 0, scrollTop: 0, pinnedOffsetLeft: 0, pinnedOffsetRight: 0 }
  let attached = false
  let resizeObserver: ReturnType<TableViewportScrollStateAdapter["getResizeObserver"]> = null
  let pendingScrollTop: number | null = null
  let pendingScrollLeft: number | null = null
  let afterScrollTaskId: number | null = null
  let scrollSyncTaskId: number | null = null
  let lastScrollTopSample = 0
  let lastScrollLeftSample = 0
  let pendingHorizontalSettle = false
  let lastAppliedScrollTop = 0
  let lastAppliedScrollLeft = 0
  let driftCorrectionPending = false

  return {
    getContainer: () => container,
    setContainer: value => {
      container = value
    },
    getHeader: () => header,
    setHeader: value => {
      header = value
    },
    getSyncTargets: () => syncTargets,
    setSyncTargets: value => {
      syncTargets = value
    },
    getSyncState: () => syncState,
    getLastAppliedScroll: () => ({ top: lastAppliedScrollTop, left: lastAppliedScrollLeft }),
    setLastAppliedScroll: (top, left) => {
      lastAppliedScrollTop = top
      lastAppliedScrollLeft = left
    },
    isAttached: () => attached,
    setAttached: value => {
      attached = value
    },
    getResizeObserver: () => resizeObserver,
    setResizeObserver: value => {
      resizeObserver = value
    },
    getPendingScrollTop: () => pendingScrollTop,
    setPendingScrollTop: value => {
      pendingScrollTop = value
    },
    getPendingScrollLeft: () => pendingScrollLeft,
    setPendingScrollLeft: value => {
      pendingScrollLeft = value
    },
    getAfterScrollTaskId: () => afterScrollTaskId,
    setAfterScrollTaskId: value => {
      afterScrollTaskId = value
    },
    getScrollSyncTaskId: () => scrollSyncTaskId,
    setScrollSyncTaskId: value => {
      scrollSyncTaskId = value
    },
    getLastScrollSamples: () => ({ top: lastScrollTopSample, left: lastScrollLeftSample }),
    setLastScrollSamples: (top, left) => {
      lastScrollTopSample = top
      lastScrollLeftSample = left
    },
    isPendingHorizontalSettle: () => pendingHorizontalSettle,
    setPendingHorizontalSettle: value => {
      pendingHorizontalSettle = value
    },
    isDriftCorrectionPending: () => driftCorrectionPending,
    setDriftCorrectionPending: value => {
      driftCorrectionPending = value
    },
    resetCachedMeasurements: () => {},
    clearLayoutMeasurement: () => {},
    resetScrollSamples: () => {
      lastScrollTopSample = 0
      lastScrollLeftSample = 0
      lastAppliedScrollTop = 0
      lastAppliedScrollLeft = 0
      driftCorrectionPending = false
      syncState.scrollLeft = 0
      syncState.scrollTop = 0
      syncState.pinnedOffsetLeft = 0
      syncState.pinnedOffsetRight = 0
    },
  }
}

interface ScrollIoHarness {
  scrollIo: ReturnType<typeof createTableViewportScrollIo>
  container: HTMLDivElement
  fakeRaf: ReturnType<typeof createFakeRafScheduler>
  queueHeavyUpdate: ReturnType<typeof vi.fn>
  recordSyncScroll: ReturnType<typeof vi.fn>
  state: TestScrollStateAdapter
}

function createHarness(): ScrollIoHarness {
  const fakeRaf = createFakeRafScheduler()
  const queueHeavyUpdate = vi.fn()
  const recordSyncScroll = vi.fn()
  const state = createScrollStateAdapter()
  const container = document.createElement("div") as HTMLDivElement
  container.scrollLeft = 0
  container.scrollTop = 0
  state.setContainer(container)
  state.setAttached(true)

  const hostEnvironment: TableViewportHostEnvironment = {
    addScrollListener: vi.fn(),
    removeScrollListener: vi.fn(),
    isEventFromContainer: () => true,
    normalizeScrollLeft: element => element.scrollLeft,
  }

  const scrollIo = createTableViewportScrollIo({
    hostEnvironment,
    scheduler: fakeRaf.scheduler,
    recordLayoutRead: vi.fn(),
    recordSyncScroll,
    queueHeavyUpdate,
    flushSchedulers: vi.fn(),
    getOnAfterScroll: () => null,
    state,
    frameDurationMs: 16,
  })

  return {
    scrollIo,
    container,
    fakeRaf,
    queueHeavyUpdate,
    recordSyncScroll,
    state,
  }
}

afterEach(() => {
  vi.restoreAllMocks()
})

describe("ui-table rAF scroll sync", () => {
  it("defers transforms and heavy path work to rAF", () => {
    const harness = createHarness()
    const applySpy = vi.spyOn(scrollSyncModule, "applyViewportSyncTransforms")

    harness.container.scrollTop = 24
    harness.container.scrollLeft = 11
    harness.scrollIo.handleScroll(new Event("scroll"))

    expect(applySpy).not.toHaveBeenCalled()
    expect(harness.queueHeavyUpdate).not.toHaveBeenCalled()

    harness.fakeRaf.tickFrame()

    expect(applySpy).toHaveBeenCalledTimes(1)
    expect(harness.queueHeavyUpdate).toHaveBeenCalledTimes(1)
  })

  it("coalesces multiple scroll events into a single rAF sync", () => {
    const harness = createHarness()
    const applySpy = vi.spyOn(scrollSyncModule, "applyViewportSyncTransforms")

    harness.container.scrollTop = 5
    harness.container.scrollLeft = 2
    harness.scrollIo.handleScroll(new Event("scroll"))

    harness.container.scrollTop = 15
    harness.container.scrollLeft = 6
    harness.scrollIo.handleScroll(new Event("scroll"))

    harness.container.scrollTop = 27
    harness.container.scrollLeft = 12
    harness.scrollIo.handleScroll(new Event("scroll"))

    expect(applySpy).not.toHaveBeenCalled()
    expect(harness.queueHeavyUpdate).not.toHaveBeenCalled()

    harness.fakeRaf.tickFrame()

    expect(applySpy).toHaveBeenCalledTimes(1)
    expect(harness.queueHeavyUpdate).toHaveBeenCalledTimes(1)
    expect(harness.fakeRaf.pendingTasks()).toBe(0)
  })

  it("applies the latest sampled scroll values in rAF", () => {
    const harness = createHarness()
    const applySpy = vi.spyOn(scrollSyncModule, "applyViewportSyncTransforms")

    const syncState = harness.state.getSyncState()
    syncState.pinnedOffsetLeft = 12
    syncState.pinnedOffsetRight = 34

    harness.container.scrollTop = 3
    harness.container.scrollLeft = 1
    harness.scrollIo.handleScroll(new Event("scroll"))

    harness.container.scrollTop = 18
    harness.container.scrollLeft = 4
    harness.scrollIo.handleScroll(new Event("scroll"))

    harness.container.scrollTop = 42
    harness.container.scrollLeft = 9
    harness.scrollIo.handleScroll(new Event("scroll"))

    harness.fakeRaf.tickFrame()

    expect(applySpy).toHaveBeenCalledTimes(1)
    const [, nextState] = applySpy.mock.calls[0] as [unknown, ViewportSyncState]
    expect(nextState.scrollTop).toBe(42)
    expect(nextState.scrollLeft).toBe(9)
    expect(nextState.pinnedOffsetLeft).toBe(12)
    expect(nextState.pinnedOffsetRight).toBe(34)
  })

  it("runs the heavy path exactly once per frame", () => {
    const harness = createHarness()
    const applySpy = vi.spyOn(scrollSyncModule, "applyViewportSyncTransforms")

    harness.container.scrollTop = 12
    harness.scrollIo.handleScroll(new Event("scroll"))
    harness.container.scrollTop = 22
    harness.scrollIo.handleScroll(new Event("scroll"))

    expect(harness.queueHeavyUpdate).not.toHaveBeenCalled()
    expect(applySpy).not.toHaveBeenCalled()

    harness.fakeRaf.tickFrame()

    expect(applySpy).toHaveBeenCalledTimes(1)
    expect(harness.queueHeavyUpdate).toHaveBeenCalledTimes(1)
  })

  it("schedules independent rAF syncs for successive frames", () => {
    const harness = createHarness()
    const applySpy = vi.spyOn(scrollSyncModule, "applyViewportSyncTransforms")

    harness.container.scrollTop = 14
    harness.container.scrollLeft = 5
    harness.scrollIo.handleScroll(new Event("scroll"))
    harness.fakeRaf.tickFrame()

    expect(applySpy).toHaveBeenCalledTimes(1)
    expect(harness.queueHeavyUpdate).toHaveBeenCalledTimes(1)

    applySpy.mockClear()
    harness.queueHeavyUpdate.mockClear()

    harness.container.scrollTop = 31
    harness.container.scrollLeft = 13
    harness.scrollIo.handleScroll(new Event("scroll"))
    harness.fakeRaf.tickFrame()

    expect(applySpy).toHaveBeenCalledTimes(1)
    expect(harness.queueHeavyUpdate).toHaveBeenCalledTimes(1)
    const [, nextState] = applySpy.mock.calls[0] as [unknown, ViewportSyncState]
    expect(nextState.scrollTop).toBe(31)
    expect(nextState.scrollLeft).toBe(13)
  })
})
