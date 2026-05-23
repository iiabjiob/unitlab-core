import { describe, expect, it } from "vitest"

import { createSignalGridPatchQueue, type SignalGridPatchQueueScheduler } from "./useSignalGridPatchQueue"

type TestGridRow = Record<string, unknown> & {
  rowId: string
  tested_at?: string | null
  channel_select?: string
}

function createManualScheduler() {
  let nextHandle = 1
  const callbacks = new Map<number, () => void>()
  const scheduler: SignalGridPatchQueueScheduler = {
    schedule(callback) {
      const handle = nextHandle
      nextHandle += 1
      callbacks.set(handle, callback)
      return handle
    },
    cancel(handle) {
      callbacks.delete(handle)
    },
  }

  return {
    scheduler,
    runAll() {
      const pending = Array.from(callbacks.entries())
      callbacks.clear()
      pending.forEach(([, callback]) => callback())
    },
    get size() {
      return callbacks.size
    },
  }
}

function createGridHarness(options?: { patchSupport?: boolean }) {
  const patchCalls: Array<{ patches: unknown[]; options: Record<string, unknown> | undefined }> = []
  const refreshCalls: Array<{ rowKeys: unknown[]; columnKeys: string[]; options: Record<string, unknown> | undefined }> = []
  let batchCalls = 0

  const gridRef = {
    value: {
      getApi: () => ({
        rows: {
          hasPatchSupport: () => options?.patchSupport ?? true,
          batch: <TResult>(callback: () => TResult) => {
            batchCalls += 1
            return callback()
          },
          patch: (patches: readonly unknown[], patchOptions?: Record<string, unknown>) => {
            patchCalls.push({ patches: [...patches], options: patchOptions })
          },
        },
        view: {
          refreshCellsByRowKeys: (
            rowKeys: readonly unknown[],
            columnKeys: readonly string[],
            refreshOptions?: Record<string, unknown>,
          ) => {
            refreshCalls.push({
              rowKeys: [...rowKeys],
              columnKeys: [...columnKeys],
              options: refreshOptions,
            })
          },
        },
      }),
    },
  }

  return {
    gridRef,
    patchCalls,
    refreshCalls,
    get batchCalls() {
      return batchCalls
    },
  }
}

describe("createSignalGridPatchQueue", () => {
  it("coalesces row patches by row id and flushes them in one batch", () => {
    const scheduler = createManualScheduler()
    const grid = createGridHarness()
    const queue = createSignalGridPatchQueue<TestGridRow>(grid.gridRef, {
      scheduler: scheduler.scheduler,
      defaultReason: "test-grid-patch",
    })

    queue.enqueueRowPatch("signal-1", { tested_at: "2026-01-01T00:00:00Z" }, {
      columns: ["tested_at"],
    })
    queue.enqueueRowPatch("signal-1", { tested_at: "2026-01-01T00:00:01Z", channel_select: "unit-a/ch1" }, {
      columns: ["channel_select"],
    })

    expect(scheduler.size).toBe(1)
    expect(grid.patchCalls).toHaveLength(0)

    scheduler.runAll()

    expect(grid.batchCalls).toBe(1)
    expect(grid.patchCalls).toHaveLength(1)
    expect(grid.patchCalls[0].patches).toEqual([
      {
        rowId: "signal-1",
        data: {
          tested_at: "2026-01-01T00:00:01Z",
          channel_select: "unit-a/ch1",
        },
      },
    ])
    expect(grid.patchCalls[0].options).toMatchObject({
      recomputeSort: false,
      recomputeFilter: false,
      recomputeGroup: false,
    })
    expect(grid.refreshCalls).toEqual([
      {
        rowKeys: ["signal-1"],
        columnKeys: ["channel_select", "tested_at"],
        options: { immediate: undefined, reason: "test-grid-patch" },
      },
    ])
  })

  it("can refresh cells without changing row data", () => {
    const scheduler = createManualScheduler()
    const grid = createGridHarness()
    const queue = createSignalGridPatchQueue<TestGridRow>(grid.gridRef, {
      scheduler: scheduler.scheduler,
    })

    queue.enqueueCellRefresh(["signal-1", "signal-2"], ["tested_at"], {
      reason: "runtime-refresh",
    })
    scheduler.runAll()

    expect(grid.patchCalls).toHaveLength(0)
    expect(grid.refreshCalls).toEqual([
      {
        rowKeys: ["signal-1", "signal-2"],
        columnKeys: ["tested_at"],
        options: { immediate: undefined, reason: "runtime-refresh" },
      },
    ])
  })

  it("drops row patches without throwing when the grid has no patch support", () => {
    const scheduler = createManualScheduler()
    const grid = createGridHarness({ patchSupport: false })
    const queue = createSignalGridPatchQueue<TestGridRow>(grid.gridRef, {
      scheduler: scheduler.scheduler,
    })

    queue.enqueueRowPatch("signal-1", { tested_at: "2026-01-01T00:00:00Z" })
    scheduler.runAll()

    expect(grid.patchCalls).toHaveLength(0)
    expect(queue.diagnostics().droppedRowPatches).toBe(1)
  })
})
