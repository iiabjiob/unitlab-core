import { describe, expect, it } from "vitest"

import { createSignalAllocationProjectionCache } from "./signalAllocationProjectionCache"
import { createSignalGridPatchIngress } from "./signalGridPatchIngress"
import type { SignalAllocationRow } from "@/types/signal"

function buildRow(overrides: Partial<SignalAllocationRow> = {}): SignalAllocationRow {
  return {
    row_id: `signal-${overrides.signal_id ?? 1}`,
    signal_id: 1,
    signal_key: "S1",
    signal_name: "Signal 1",
    signal_direction: "DI",
    signal_category: null,
    signal_metadata: { row: { Cabinet: "A1" } },
    allocation_id: null,
    allocation_status: "unassigned",
    allocation_health: null,
    channel_id: null,
    channel_type: null,
    channel_index: null,
    channel_label: null,
    device_id: null,
    unit_id: null,
    unit_online: null,
    unit_last_seen_at: null,
    tested_at: null,
    ...overrides,
  }
}

function createHarness() {
  const cache = createSignalAllocationProjectionCache()
  const rowPatchCalls: unknown[] = []
  const refreshCalls: unknown[] = []
  let projectionChanged = 0
  let flushed = 0

  const ingress = createSignalGridPatchIngress({
    cache,
    getHeaders: () => ["Cabinet"],
    defaultColumns: ["channel_select", "tested_at"],
    getRuntime: () => ({
      workspaceId: 7,
      getTestedAt: (signalId, workspaceId) => (
        signalId === 1 && workspaceId === 7 ? "2026-01-01T00:00:00Z" : null
      ),
    }),
    onProjectionChanged: () => {
      projectionChanged += 1
    },
    rowModel: {
      enqueueRowPatches: (patches, options) => {
        rowPatchCalls.push({ patches, options })
      },
      enqueueCellRefresh: (rowIds, columns, options) => {
        refreshCalls.push({ rowIds, columns, options })
      },
      flushPatches: (options) => {
        flushed += 1
        rowPatchCalls.push({ flushed: true, options })
      },
    },
  })

  return {
    cache,
    ingress,
    rowPatchCalls,
    refreshCalls,
    get projectionChanged() {
      return projectionChanged
    },
    get flushed() {
      return flushed
    },
  }
}

describe("createSignalGridPatchIngress", () => {
  it("patches the projection cache before enqueueing grid row patches", () => {
    const harness = createHarness()
    harness.cache.replaceRows([buildRow({ signal_id: 1, row_id: "signal-1" })])

    const result = harness.ingress.applyAllocationRows([
      buildRow({
        signal_id: 1,
        row_id: "signal-1",
        channel_id: 10,
        channel_index: 0,
        unit_id: "unit-a",
      }),
    ], {
      reason: "allocation",
      flush: true,
    })

    expect(result).toEqual({
      requested: 1,
      changed: 1,
      missingSignalIds: [],
      enqueuedPatches: 1,
    })
    expect(harness.projectionChanged).toBe(1)
    expect(harness.cache.getOwnerSignalIdByChannelId(10)).toBe(1)
    expect(harness.rowPatchCalls[0]).toMatchObject({
      patches: [
        expect.objectContaining({
          rowId: "signal-1",
          changes: expect.objectContaining({
            channel_select: "unit-a/ch1",
            tested_at: "2026-01-01T00:00:00Z",
          }),
          columns: ["channel_select", "tested_at"],
        }),
      ],
      options: { reason: "allocation" },
    })
    expect(harness.flushed).toBe(1)
  })

  it("deduplicates runtime signal patches and preserves first-seen order", () => {
    const harness = createHarness()
    harness.cache.replaceRows([
      buildRow({ signal_id: 1, row_id: "signal-1" }),
      buildRow({ signal_id: 2, row_id: "signal-2" }),
    ])
    const projectionVersion = harness.cache.version

    const result = harness.ingress.applyRuntimeSignals([2, 1, 2, "bad"], {
      reason: "runtime",
      columns: ["tested_at"],
    })

    expect(result).toEqual({
      requested: 2,
      changed: 2,
      missingSignalIds: [],
      enqueuedPatches: 2,
    })
    expect(harness.rowPatchCalls[0]).toMatchObject({
      patches: [
        expect.objectContaining({ rowId: "signal-2" }),
        expect.objectContaining({ rowId: "signal-1" }),
      ],
      options: {
        reason: "runtime",
        recomputeSort: false,
        recomputeFilter: false,
        recomputeGroup: false,
      },
    })
    expect(harness.cache.version).toBe(projectionVersion)
  })

  it("applies column recompute policy and allows explicit overrides", () => {
    const harness = createHarness()
    harness.cache.replaceRows([buildRow({ signal_id: 1, row_id: "signal-1" })])

    harness.ingress.applySignalRowsById([1], {
      reason: "allocation-filter",
      columns: ["allocation_status"],
    })
    harness.ingress.applySignalRowsById([1], {
      reason: "bulk-allocation",
      columns: ["allocation_status"],
      recomputeSort: false,
      recomputeFilter: false,
      recomputeGroup: false,
    })

    expect(harness.rowPatchCalls[0]).toMatchObject({
      options: {
        reason: "allocation-filter",
        recomputeSort: true,
        recomputeFilter: true,
        recomputeGroup: true,
      },
    })
    expect(harness.rowPatchCalls[1]).toMatchObject({
      options: {
        reason: "bulk-allocation",
        recomputeSort: false,
        recomputeFilter: false,
        recomputeGroup: false,
      },
    })
  })

  it("reports unknown rows without enqueueing patches for them", () => {
    const harness = createHarness()
    harness.cache.replaceRows([buildRow({ signal_id: 1, row_id: "signal-1" })])

    const result = harness.ingress.applySignalRowsById([1, 9], {
      reason: "runtime",
    })

    expect(result).toEqual({
      requested: 2,
      changed: 1,
      missingSignalIds: [9],
      enqueuedPatches: 1,
    })
    expect(harness.rowPatchCalls[0]).toMatchObject({
      patches: [expect.objectContaining({ rowId: "signal-1" })],
    })
  })

  it("can patch the projection cache without enqueueing grid row patches", () => {
    const harness = createHarness()
    harness.cache.replaceRows([buildRow({ signal_id: 1, row_id: "signal-1" })])

    const result = harness.ingress.patchAllocationRowsCache([
      buildRow({
        signal_id: 1,
        row_id: "signal-1",
        channel_id: 10,
        channel_index: 0,
        unit_id: "unit-a",
      }),
    ])

    expect(result).toEqual({
      requested: 1,
      changed: 1,
      missingSignalIds: [],
      enqueuedPatches: 0,
    })
    expect(harness.projectionChanged).toBe(1)
    expect(harness.cache.getOwnerSignalIdByChannelId(10)).toBe(1)
    expect(harness.rowPatchCalls).toHaveLength(0)
  })

  it("can route cell refreshes through the same signal-id ingress", () => {
    const harness = createHarness()
    harness.cache.replaceRows([buildRow({ signal_id: 1, row_id: "signal-1" })])

    const result = harness.ingress.refreshSignalCells([1, 7], ["tested_at"], {
      reason: "refresh-only",
    })

    expect(result).toEqual({
      requested: 2,
      changed: 1,
      missingSignalIds: [7],
      enqueuedPatches: 0,
    })
    expect(harness.refreshCalls).toEqual([
      {
        rowIds: ["signal-1"],
        columns: ["tested_at"],
        options: { reason: "refresh-only" },
      },
    ])
  })
})
