import { describe, expect, it } from "vitest"

import { runSignalListPerformanceHarness } from "./signalListPerformanceHarness"

describe("runSignalListPerformanceHarness", () => {
  it("exercises the 20,000-row signal-list target without normal full reload paths", () => {
    const metrics = runSignalListPerformanceHarness()

    expect(metrics.rowCount).toBe(20_000)
    expect(metrics.patchCount).toBe(5_000)
    expect(metrics.initialProjectionRows).toBe(20_000)
    expect(metrics.initialGridRows).toBe(20_000)
    expect(metrics.initialProjectionMs).toBeGreaterThanOrEqual(0)
    expect(metrics.initialGridRowsMs).toBeGreaterThanOrEqual(0)

    expect(metrics.visiblePatch).toMatchObject({
      requested: 1,
      changed: 1,
      missingSignalIds: [],
      enqueuedPatches: 1,
    })
    expect(metrics.nonVisiblePatch).toMatchObject({
      requested: 1,
      changed: 1,
      missingSignalIds: [],
      enqueuedPatches: 1,
    })
    expect(metrics.allocationBurst).toMatchObject({
      requested: 5_000,
      changed: 5_000,
      missingSignalIds: [],
      enqueuedPatches: 5_000,
    })
    expect(metrics.runtimeBurst).toMatchObject({
      requested: 5_000,
      changed: 5_000,
      missingSignalIds: [],
      enqueuedPatches: 5_000,
    })

    expect(metrics.normalAllocationReloadReason).toBeNull()
    expect(metrics.normalRuntimeReloadReason).toBeNull()
    expect(metrics.unknownRowReloadReason).toBe("unknown_row_patch")
  })

  it("keeps the patch queue bounded and defers grid work until flush", () => {
    const metrics = runSignalListPerformanceHarness()

    expect(metrics.patchCallsBeforeFlush).toBe(0)
    expect(metrics.pendingRowPatchesBeforeFlush).toBeLessThanOrEqual(metrics.patchCount)
    expect(metrics.pendingCellRefreshRowsBeforeFlush).toBeLessThanOrEqual(metrics.patchCount)
    expect(metrics.pendingRowPatchesAfterFlush).toBe(0)
    expect(metrics.pendingCellRefreshRowsAfterFlush).toBe(0)
    expect(metrics.flushedBatches).toBe(1)
    expect(metrics.appliedRowPatches).toBe(5_000)
    expect(metrics.appliedCellRefreshes).toBe(5_000)
    expect(metrics.gridPatchPayloadRows).toBe(5_000)
    expect(metrics.gridRefreshPayloadRows).toBe(5_000)
  })

  it("covers selection and channel-picker lookup paths over the full row set", () => {
    const metrics = runSignalListPerformanceHarness()

    expect(metrics.selectionAllCount).toBe(19_999)
    expect(metrics.selectionVisibleCount).toBe(metrics.visibleRowCount)
    expect(metrics.channelPickerOwnerSignalId).toBe(1)
  })
})
