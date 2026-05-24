import { onBeforeUnmount } from "vue"

export type SignalGridRowId = string | number

export type SignalGridRowPatch<TRow> = {
  rowId: SignalGridRowId
  changes: Partial<TRow>
  columns?: readonly string[]
}

export type SignalGridPatchQueueFlushOptions = {
  reason?: string
  recomputeSort?: boolean
  recomputeFilter?: boolean
  recomputeGroup?: boolean
  emit?: boolean
  immediate?: boolean
}

export type SignalGridPatchQueueScheduler = {
  schedule: (callback: () => void) => number
  cancel: (handle: number) => void
}

export type SignalGridPatchQueueDiagnostics = {
  scheduled: boolean
  pendingRowPatchCount: number
  pendingCellRefreshRowCount: number
  flushedBatches: number
  appliedRowPatches: number
  droppedRowPatches: number
  failedRowPatches: number
  appliedCellRefreshes: number
  droppedCellRefreshes: number
}

type DataGridRowPatch<TRow> = {
  rowId: SignalGridRowId
  data: Partial<TRow>
}

type DataGridPatchOptions = {
  recomputeSort?: boolean
  recomputeFilter?: boolean
  recomputeGroup?: boolean
  emit?: boolean
}

type SignalGridPatchQueueGridApi<TRow> = {
  rows?: {
    patchRows?: (patches: readonly DataGridRowPatch<TRow>[], options?: DataGridPatchOptions) => void | Promise<void>
    batch?: <TResult>(callback: () => TResult) => TResult
  }
  view?: {
    refreshCellsByRowKeys?: (
      rowKeys: readonly SignalGridRowId[],
      columnKeys: readonly string[],
      options?: { immediate?: boolean; reason?: string },
    ) => void
  }
}

type SignalGridPatchQueueGrid<TRow> = {
  getApi?: () => SignalGridPatchQueueGridApi<TRow> | null
}

export type SignalGridPatchQueueGridRef<TRow> = {
  readonly value: SignalGridPatchQueueGrid<TRow> | null | undefined
}

type PendingRowPatch<TRow> = {
  rowId: SignalGridRowId
  changes: Partial<TRow>
}

function defaultScheduler(): SignalGridPatchQueueScheduler {
  if (typeof requestAnimationFrame === "function" && typeof cancelAnimationFrame === "function") {
    return {
      schedule: callback => requestAnimationFrame(callback),
      cancel: handle => cancelAnimationFrame(handle),
    }
  }

  return {
    schedule: callback => globalThis.setTimeout(callback, 16),
    cancel: handle => globalThis.clearTimeout(handle),
  }
}

function normalizeColumns(columns: readonly string[] | undefined, changes: object): string[] {
  const normalized = new Set<string>()
  if (columns) {
    columns.forEach((column) => {
      const key = String(column ?? "").trim()
      if (key) {
        normalized.add(key)
      }
    })
  } else {
    Object.keys(changes).forEach((column) => {
      const key = String(column ?? "").trim()
      if (key) {
        normalized.add(key)
      }
    })
  }
  return Array.from(normalized)
}

export function createSignalGridPatchQueue<TRow extends Record<string, unknown>>(
  gridRef: SignalGridPatchQueueGridRef<TRow>,
  options?: {
    scheduler?: SignalGridPatchQueueScheduler
    defaultReason?: string
  },
) {
  const scheduler = options?.scheduler ?? defaultScheduler()
  const rowPatches = new Map<SignalGridRowId, PendingRowPatch<TRow>>()
  const refreshColumnsByRowId = new Map<SignalGridRowId, Set<string>>()
  const diagnostics: SignalGridPatchQueueDiagnostics = {
    scheduled: false,
    pendingRowPatchCount: 0,
    pendingCellRefreshRowCount: 0,
    flushedBatches: 0,
    appliedRowPatches: 0,
    droppedRowPatches: 0,
    failedRowPatches: 0,
    appliedCellRefreshes: 0,
    droppedCellRefreshes: 0,
  }

  let scheduledHandle: number | null = null
  let pendingOptions: SignalGridPatchQueueFlushOptions = {}

  function updatePendingDiagnostics() {
    diagnostics.scheduled = scheduledHandle !== null
    diagnostics.pendingRowPatchCount = rowPatches.size
    diagnostics.pendingCellRefreshRowCount = refreshColumnsByRowId.size
  }

  function mergeFlushOptions(flushOptions?: SignalGridPatchQueueFlushOptions) {
    if (!flushOptions) {
      return
    }
    pendingOptions = {
      ...pendingOptions,
      reason: flushOptions.reason ?? pendingOptions.reason,
      recomputeSort: pendingOptions.recomputeSort === true || flushOptions.recomputeSort === true ? true : pendingOptions.recomputeSort,
      recomputeFilter: pendingOptions.recomputeFilter === true || flushOptions.recomputeFilter === true ? true : pendingOptions.recomputeFilter,
      recomputeGroup: pendingOptions.recomputeGroup === true || flushOptions.recomputeGroup === true ? true : pendingOptions.recomputeGroup,
      emit: flushOptions.emit ?? pendingOptions.emit,
      immediate: pendingOptions.immediate === true || flushOptions.immediate === true ? true : pendingOptions.immediate,
    }
  }

  function scheduleFlush() {
    if (scheduledHandle !== null) {
      updatePendingDiagnostics()
      return
    }
    scheduledHandle = scheduler.schedule(() => {
      scheduledHandle = null
      updatePendingDiagnostics()
      flush()
    })
    updatePendingDiagnostics()
  }

  function enqueueCellRefresh(
    rowIds: readonly SignalGridRowId[],
    columns: readonly string[],
    flushOptions?: SignalGridPatchQueueFlushOptions,
  ) {
    const normalizedColumns = normalizeColumns(columns, {})
    if (!rowIds.length || normalizedColumns.length === 0) {
      return
    }

    mergeFlushOptions(flushOptions)
    rowIds.forEach((rowId) => {
      let existing = refreshColumnsByRowId.get(rowId)
      if (!existing) {
        existing = new Set<string>()
        refreshColumnsByRowId.set(rowId, existing)
      }
      normalizedColumns.forEach(column => existing.add(column))
    })
    scheduleFlush()
  }

  function enqueueRowPatch(
    rowId: SignalGridRowId,
    changes: Partial<TRow>,
    flushOptions?: SignalGridPatchQueueFlushOptions & { columns?: readonly string[] },
  ) {
    const changeKeys = Object.keys(changes)
    const refreshColumns = normalizeColumns(flushOptions?.columns, changes)
    if (changeKeys.length === 0 && refreshColumns.length === 0) {
      return
    }

    mergeFlushOptions(flushOptions)
    if (changeKeys.length > 0) {
      const existing = rowPatches.get(rowId)
      rowPatches.set(rowId, {
        rowId,
        changes: {
          ...(existing?.changes ?? {}),
          ...changes,
        },
      })
    }

    if (refreshColumns.length > 0) {
      let existingColumns = refreshColumnsByRowId.get(rowId)
      if (!existingColumns) {
        existingColumns = new Set<string>()
        refreshColumnsByRowId.set(rowId, existingColumns)
      }
      refreshColumns.forEach(column => existingColumns.add(column))
    }
    scheduleFlush()
  }

  function enqueueRowPatches(
    patches: readonly SignalGridRowPatch<TRow>[],
    flushOptions?: SignalGridPatchQueueFlushOptions,
  ) {
    if (!patches.length) {
      return
    }
    mergeFlushOptions(flushOptions)
    patches.forEach((patch) => {
      enqueueRowPatch(patch.rowId, patch.changes, {
        ...flushOptions,
        columns: patch.columns,
      })
    })
  }

  function flushRowPatches(
    api: SignalGridPatchQueueGridApi<TRow> | null,
    patches: readonly PendingRowPatch<TRow>[],
    flushOptions: SignalGridPatchQueueFlushOptions,
  ) {
    if (!patches.length) {
      return
    }

    const rowsApi = api?.rows
    const patchRows = rowsApi?.patchRows
    if (!rowsApi || !patchRows) {
      diagnostics.droppedRowPatches += patches.length
      return
    }

    const rowPatchPayload = patches.map(patch => ({
      rowId: patch.rowId,
      data: patch.changes,
    }))
    const patchOptions: DataGridPatchOptions = {
      recomputeSort: flushOptions.recomputeSort ?? false,
      recomputeFilter: flushOptions.recomputeFilter ?? false,
      recomputeGroup: flushOptions.recomputeGroup ?? false,
      emit: flushOptions.emit,
    }

    try {
      const applyPatches = () => {
        void patchRows(rowPatchPayload, patchOptions)
      }
      if (rowsApi.batch) {
        rowsApi.batch(applyPatches)
      } else {
        applyPatches()
      }
      diagnostics.appliedRowPatches += patches.length
    } catch {
      diagnostics.failedRowPatches += patches.length
    }
  }

  function flushCellRefreshes(
    api: SignalGridPatchQueueGridApi<TRow> | null,
    refreshes: readonly [SignalGridRowId, Set<string>][],
    flushOptions: SignalGridPatchQueueFlushOptions,
  ) {
    if (!refreshes.length) {
      return
    }

    if (!api?.view?.refreshCellsByRowKeys) {
      diagnostics.droppedCellRefreshes += refreshes.length
      return
    }

    const rowsByColumnSignature = new Map<string, { columns: string[]; rowIds: SignalGridRowId[] }>()
    refreshes.forEach(([rowId, columns]) => {
      const normalizedColumns = Array.from(columns).sort()
      if (normalizedColumns.length === 0) {
        return
      }
      const signature = normalizedColumns.join("\u0000")
      let bucket = rowsByColumnSignature.get(signature)
      if (!bucket) {
        bucket = { columns: normalizedColumns, rowIds: [] }
        rowsByColumnSignature.set(signature, bucket)
      }
      bucket.rowIds.push(rowId)
    })

    rowsByColumnSignature.forEach((bucket) => {
      api.view!.refreshCellsByRowKeys!(bucket.rowIds, bucket.columns, {
        immediate: flushOptions.immediate,
        reason: flushOptions.reason ?? options?.defaultReason,
      })
      diagnostics.appliedCellRefreshes += bucket.rowIds.length
    })
  }

  function flush(flushOptions?: SignalGridPatchQueueFlushOptions) {
    if (scheduledHandle !== null) {
      scheduler.cancel(scheduledHandle)
      scheduledHandle = null
    }
    mergeFlushOptions(flushOptions)

    const activeOptions = {
      reason: pendingOptions.reason ?? options?.defaultReason,
      recomputeSort: pendingOptions.recomputeSort,
      recomputeFilter: pendingOptions.recomputeFilter,
      recomputeGroup: pendingOptions.recomputeGroup,
      emit: pendingOptions.emit,
      immediate: pendingOptions.immediate,
    }
    pendingOptions = {}

    const patches = Array.from(rowPatches.values())
    const refreshes = Array.from(refreshColumnsByRowId.entries())
    rowPatches.clear()
    refreshColumnsByRowId.clear()
    updatePendingDiagnostics()

    if (!patches.length && !refreshes.length) {
      return
    }

    diagnostics.flushedBatches += 1
    const api = gridRef.value?.getApi?.() ?? null
    flushRowPatches(api, patches, activeOptions)
    flushCellRefreshes(api, refreshes, activeOptions)
  }

  function cancel() {
    if (scheduledHandle !== null) {
      scheduler.cancel(scheduledHandle)
      scheduledHandle = null
    }
    rowPatches.clear()
    refreshColumnsByRowId.clear()
    pendingOptions = {}
    updatePendingDiagnostics()
  }

  function snapshotDiagnostics(): SignalGridPatchQueueDiagnostics {
    updatePendingDiagnostics()
    return { ...diagnostics }
  }

  return {
    enqueueRowPatch,
    enqueueRowPatches,
    enqueueCellRefresh,
    flush,
    cancel,
    diagnostics: snapshotDiagnostics,
  }
}

export function useSignalGridPatchQueue<TRow extends Record<string, unknown>>(
  gridRef: SignalGridPatchQueueGridRef<TRow>,
  options?: {
    defaultReason?: string
  },
) {
  const queue = createSignalGridPatchQueue(gridRef, options)
  onBeforeUnmount(() => {
    queue.cancel()
  })
  return queue
}
