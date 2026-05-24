import { createSignalGridRowModel } from "@/pages/signals/composables/useSignalGridRowModel"
import type {
  SignalGridPatchQueueFlushOptions,
  SignalGridPatchQueueScheduler,
  SignalGridRowId,
} from "@/pages/signals/composables/useSignalGridPatchQueue"
import { resolveSignalGridSelectedRowKeys } from "@/pages/signals/utils/rowSelection"
import { createSignalAllocationProjectionCache } from "@/pages/signals/utils/signalAllocationProjectionCache"
import { createSignalGridPatchIngress, type SignalGridPatchIngressResult } from "@/pages/signals/utils/signalGridPatchIngress"
import {
  createSignalGridRows,
  type SignalGridRow,
} from "@/pages/signals/utils/signalGridProjection"
import { createSignalRuntimeStateCache } from "@/pages/signals/utils/signalRuntimeStateCache"
import { resolveSignalStaticRefreshReason } from "@/pages/signals/utils/signalStaticRefreshPolicy"
import type { SignalAllocationRow } from "@/types/signal"

type SignalListPerformanceHarnessOptions = {
  rowCount?: number
  patchCount?: number
  visibleRowCount?: number
  now?: () => number
}

type SignalListPerformanceHarnessMetrics = {
  rowCount: number
  patchCount: number
  visibleRowCount: number
  initialProjectionRows: number
  initialGridRows: number
  initialProjectionMs: number
  initialGridRowsMs: number
  visiblePatch: SignalGridPatchIngressResult
  nonVisiblePatch: SignalGridPatchIngressResult
  allocationBurst: SignalGridPatchIngressResult
  runtimeBurst: SignalGridPatchIngressResult
  patchCallsBeforeFlush: number
  pendingRowPatchesBeforeFlush: number
  pendingCellRefreshRowsBeforeFlush: number
  pendingRowPatchesAfterFlush: number
  pendingCellRefreshRowsAfterFlush: number
  flushedBatches: number
  appliedRowPatches: number
  appliedCellRefreshes: number
  gridPatchPayloadRows: number
  gridRefreshPayloadRows: number
  selectionAllCount: number
  selectionVisibleCount: number
  channelPickerOwnerSignalId: number | null
  normalAllocationReloadReason: string | null
  normalRuntimeReloadReason: string | null
  unknownRowReloadReason: string | null
}

type ManualScheduler = {
  scheduler: SignalGridPatchQueueScheduler
  flush: () => void
}

function defaultNow(): number {
  return typeof performance !== "undefined" ? performance.now() : Date.now()
}

function createManualScheduler(): ManualScheduler {
  let nextHandle = 1
  const callbacks = new Map<number, () => void>()

  return {
    scheduler: {
      schedule(callback) {
        const handle = nextHandle
        nextHandle += 1
        callbacks.set(handle, callback)
        return handle
      },
      cancel(handle) {
        callbacks.delete(handle)
      },
    },
    flush() {
      const pending = Array.from(callbacks.values())
      callbacks.clear()
      pending.forEach(callback => callback())
    },
  }
}

function createBenchmarkRows(rowCount: number): SignalAllocationRow[] {
  const rows: SignalAllocationRow[] = []
  for (let index = 0; index < rowCount; index += 1) {
    const signalId = index + 1
    const assigned = signalId % 3 !== 0
    const channelId = assigned ? signalId + 10_000 : null
    rows.push({
      row_id: `signal-${signalId}`,
      signal_id: signalId,
      signal_key: `SIG-${String(signalId).padStart(5, "0")}`,
      signal_name: `Signal ${signalId}`,
      signal_direction: signalId % 5 === 0 ? "AO" : signalId % 2 === 0 ? "DO" : "DI",
      signal_category: signalId % 2 === 0 ? "control" : "monitoring",
      signal_metadata: {
        row: {
          Cabinet: `CAB-${String((signalId % 80) + 1).padStart(2, "0")}`,
          Terminal: `XT${(signalId % 200) + 1}`,
        },
      },
      allocation_id: assigned ? signalId + 20_000 : null,
      allocation_status: assigned ? "assigned" : "unassigned",
      allocation_health: assigned ? {} : null,
      channel_id: channelId,
      channel_type: assigned ? (signalId % 5 === 0 ? "AO" : signalId % 2 === 0 ? "DO" : "DI") : null,
      channel_index: assigned ? signalId % 32 : null,
      channel_label: assigned ? `CH-${signalId % 32}` : null,
      device_id: assigned ? Math.floor(signalId / 32) + 1 : null,
      unit_id: assigned ? `unit-${String(Math.floor(signalId / 32) + 1).padStart(3, "0")}` : null,
      unit_online: assigned ? true : null,
      unit_last_seen_at: assigned ? "2026-05-24T00:00:00Z" : null,
      tested_at: null,
    })
  }
  return rows
}

function createAllocationPatchRows(rows: readonly SignalAllocationRow[], patchCount: number): SignalAllocationRow[] {
  return rows.slice(0, patchCount).map((row, index) => ({
    ...row,
    allocation_id: row.signal_id + 50_000,
    allocation_status: "assigned",
    allocation_health: {},
    channel_id: row.signal_id + 100_000,
    channel_type: row.signal_direction === "AO" ? "AO" : row.signal_direction === "DO" ? "DO" : "DI",
    channel_index: index % 32,
    channel_label: `BENCH-CH-${index % 32}`,
    device_id: Math.floor(index / 32) + 1,
    unit_id: `bench-unit-${String(Math.floor(index / 32) + 1).padStart(3, "0")}`,
    unit_online: true,
    unit_last_seen_at: "2026-05-24T01:00:00Z",
  }))
}

function createGridHarness() {
  const patchCalls: Array<{ patches: readonly unknown[]; options: Record<string, unknown> | undefined }> = []
  const refreshCalls: Array<{
    rowIds: readonly SignalGridRowId[]
    columns: readonly string[]
    options: Record<string, unknown> | undefined
  }> = []

  const gridRef = {
    value: {
      getApi: () => ({
        rows: {
          batch: <TResult>(callback: () => TResult) => callback(),
          patchRows: (patches: readonly unknown[], options?: Record<string, unknown>) => {
            patchCalls.push({ patches, options })
          },
        },
        view: {
          refreshCellsByRowKeys: (
            rowIds: readonly SignalGridRowId[],
            columns: readonly string[],
            options?: SignalGridPatchQueueFlushOptions,
          ) => {
            refreshCalls.push({ rowIds, columns, options })
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

export function runSignalListPerformanceHarness(
  options: SignalListPerformanceHarnessOptions = {},
): SignalListPerformanceHarnessMetrics {
  const rowCount = Math.max(1, Math.floor(options.rowCount ?? 20_000))
  const patchCount = Math.min(rowCount, Math.max(1, Math.floor(options.patchCount ?? 5_000)))
  const visibleRowCount = Math.min(rowCount, Math.max(1, Math.floor(options.visibleRowCount ?? 120)))
  const now = options.now ?? defaultNow

  const sourceRows = createBenchmarkRows(rowCount)
  const sourceHeaders = ["Cabinet", "Terminal"]
  const projectionCache = createSignalAllocationProjectionCache()
  const runtimeCache = createSignalRuntimeStateCache()
  const grid = createGridHarness()
  const scheduler = createManualScheduler()
  const rowModel = createSignalGridRowModel<SignalGridRow>(grid.gridRef, {
    resolveRowId: row => row.rowId,
    scheduler: scheduler.scheduler,
    defaultReason: "signal-list-performance-harness",
  })
  const ingress = createSignalGridPatchIngress({
    cache: projectionCache,
    rowModel,
    getHeaders: () => sourceHeaders,
    defaultColumns: ["channel_select", "allocation_status", "allocation_health", "tested_at"],
    getRuntime: () => ({
      workspaceId: 1,
      getTestedAt: signalId => runtimeCache.getTestedAt(signalId),
    }),
  })

  const projectionStartedAt = now()
  projectionCache.replaceRows(sourceRows)
  const initialProjectionMs = now() - projectionStartedAt

  const gridRowsStartedAt = now()
  rowModel.setRows(createSignalGridRows(projectionCache.getRows(), sourceHeaders))
  const initialGridRowsMs = now() - gridRowsStartedAt

  const patchRows = createAllocationPatchRows(sourceRows, patchCount)
  const visiblePatch = ingress.applyAllocationRows([patchRows[0]], {
    reason: "benchmark-visible-row",
    columns: ["channel_select", "allocation_status"],
  })
  const nonVisiblePatch = ingress.applyAllocationRows([patchRows[patchCount - 1]], {
    reason: "benchmark-non-visible-row",
    columns: ["channel_select", "allocation_status"],
  })
  const allocationBurst = ingress.applyAllocationRows(patchRows, {
    reason: "benchmark-allocation-burst",
    columns: ["channel_select", "allocation_status", "allocation_health"],
  })

  const testedAtBySignal: Record<number, string> = {}
  patchRows.forEach((row, index) => {
    testedAtBySignal[row.signal_id] = `2026-05-24T02:${String(index % 60).padStart(2, "0")}:00Z`
  })
  runtimeCache.patchTestedAtBySignal(testedAtBySignal)
  const runtimeBurst = ingress.applyRuntimeSignals(
    patchRows.map(row => row.signal_id),
    {
      reason: "benchmark-runtime-burst",
      columns: ["tested_at"],
      recomputeSort: false,
      recomputeFilter: false,
      recomputeGroup: false,
    },
  )

  const diagnosticsBeforeFlush = rowModel.diagnostics()
  const patchCallsBeforeFlush = grid.patchCalls.length
  scheduler.flush()
  const diagnosticsAfterFlush = rowModel.diagnostics()

  const rowKeys = rowModel.rows.value.map(row => row.rowId)
  const visibleRowKeys = rowKeys.slice(0, visibleRowCount)
  const selectionAllCount = resolveSignalGridSelectedRowKeys(
    { mode: "all", excludedRows: [rowKeys[0]] },
    rowKeys,
  ).length
  const selectionVisibleCount = resolveSignalGridSelectedRowKeys(
    { selectedRows: visibleRowKeys },
    rowKeys,
  ).length

  const patchedChannelId = Number(patchRows[0]?.channel_id)
  const channelPickerOwnerSignalId = Number.isFinite(patchedChannelId)
    ? projectionCache.getOwnerSignalIdByChannelId(patchedChannelId)
    : null

  return {
    rowCount,
    patchCount,
    visibleRowCount,
    initialProjectionRows: projectionCache.rowCount,
    initialGridRows: rowModel.rows.value.length,
    initialProjectionMs,
    initialGridRowsMs,
    visiblePatch,
    nonVisiblePatch,
    allocationBurst,
    runtimeBurst,
    patchCallsBeforeFlush,
    pendingRowPatchesBeforeFlush: diagnosticsBeforeFlush.pendingRowPatchCount,
    pendingCellRefreshRowsBeforeFlush: diagnosticsBeforeFlush.pendingCellRefreshRowCount,
    pendingRowPatchesAfterFlush: diagnosticsAfterFlush.pendingRowPatchCount,
    pendingCellRefreshRowsAfterFlush: diagnosticsAfterFlush.pendingCellRefreshRowCount,
    flushedBatches: diagnosticsAfterFlush.flushedBatches,
    appliedRowPatches: diagnosticsAfterFlush.appliedRowPatches,
    appliedCellRefreshes: diagnosticsAfterFlush.appliedCellRefreshes,
    gridPatchPayloadRows: grid.patchCalls.reduce((count, call) => count + call.patches.length, 0),
    gridRefreshPayloadRows: grid.refreshCalls.reduce((count, call) => count + call.rowIds.length, 0),
    selectionAllCount,
    selectionVisibleCount,
    channelPickerOwnerSignalId,
    normalAllocationReloadReason: resolveSignalStaticRefreshReason({ kind: "allocation_job_completed" }),
    normalRuntimeReloadReason: resolveSignalStaticRefreshReason({ kind: "runtime_patch_burst" }),
    unknownRowReloadReason: resolveSignalStaticRefreshReason({
      kind: "signal_rows_patched",
      missingSignalIds: [rowCount + 1],
    }),
  }
}
