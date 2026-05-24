import { describe, expect, it } from "vitest"

import { createSignalGridRowModel } from "./useSignalGridRowModel"
import type { SignalGridPatchQueueScheduler } from "./useSignalGridPatchQueue"

type TestGridRow = Record<string, unknown> & {
  rowId: string
  signal_id: number
  channel_select?: string
  tested_at?: string | null
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
  }
}

function createGridHarness() {
  const patchCalls: Array<{ patches: unknown[]; options: Record<string, unknown> | undefined }> = []
  const refreshCalls: Array<{ rowKeys: unknown[]; columnKeys: string[]; options: Record<string, unknown> | undefined }> = []

  const gridRef = {
    value: {
      getApi: () => ({
        rows: {
          patchRows: (patches: readonly unknown[], patchOptions?: Record<string, unknown>) => {
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
  }
}

describe("createSignalGridRowModel", () => {
  it("sets the base rows and resolves rows by stable id", () => {
    const scheduler = createManualScheduler()
    const grid = createGridHarness()
    const model = createSignalGridRowModel<TestGridRow>(grid.gridRef, {
      scheduler: scheduler.scheduler,
      resolveRowId: row => row.rowId,
    })

    model.setRows([
      { rowId: "signal-1", signal_id: 1, channel_select: "unassigned" },
      { rowId: "signal-2", signal_id: 2, channel_select: "unit-a/ch1" },
    ])

    expect(model.rows.value).toHaveLength(2)
    expect(model.getRow("signal-2")).toMatchObject({
      rowId: "signal-2",
      signal_id: 2,
      channel_select: "unit-a/ch1",
    })

    model.setRows([{ rowId: "signal-3", signal_id: 3 }])

    expect(model.getRow("signal-2")).toBeNull()
    expect(model.getRow("signal-3")).toMatchObject({ signal_id: 3 })
  })

  it("patches the grid without replacing the row array", () => {
    const scheduler = createManualScheduler()
    const grid = createGridHarness()
    const model = createSignalGridRowModel<TestGridRow>(grid.gridRef, {
      scheduler: scheduler.scheduler,
      defaultReason: "signals-grid-patch",
      resolveRowId: row => row.rowId,
    })

    model.setRows([{ rowId: "signal-1", signal_id: 1, channel_select: "unassigned" }])
    const rowArray = model.rows.value

    model.enqueueRowPatches([
      {
        rowId: "signal-1",
        changes: {
          channel_select: "unit-a/ch1",
          tested_at: "2026-01-01T00:00:00Z",
        },
        columns: ["channel_select", "tested_at"],
      },
    ])

    expect(model.rows.value).toBe(rowArray)
    expect(model.getRow("signal-1")).toMatchObject({
      channel_select: "unit-a/ch1",
      tested_at: "2026-01-01T00:00:00Z",
    })
    expect(grid.patchCalls).toHaveLength(0)

    scheduler.runAll()

    expect(model.rows.value).toBe(rowArray)
    expect(grid.patchCalls).toEqual([
      {
        patches: [
          {
            rowId: "signal-1",
            data: {
              channel_select: "unit-a/ch1",
              tested_at: "2026-01-01T00:00:00Z",
            },
          },
        ],
        options: {
          recomputeSort: false,
          recomputeFilter: false,
          recomputeGroup: false,
          emit: undefined,
        },
      },
    ])
    expect(grid.refreshCalls).toEqual([
      {
        rowKeys: ["signal-1"],
        columnKeys: ["channel_select", "tested_at"],
        options: { immediate: undefined, reason: "signals-grid-patch" },
      },
    ])
  })
})
