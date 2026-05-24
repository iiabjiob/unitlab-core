import { onBeforeUnmount, shallowRef } from "vue"

import {
  createSignalGridPatchQueue,
  type SignalGridPatchQueueFlushOptions,
  type SignalGridPatchQueueGridRef,
  type SignalGridPatchQueueScheduler,
  type SignalGridRowId,
  type SignalGridRowPatch,
} from "./useSignalGridPatchQueue"

type SignalGridRowModelOptions<TRow> = {
  resolveRowId: (row: TRow) => SignalGridRowId | null | undefined
  scheduler?: SignalGridPatchQueueScheduler
  defaultReason?: string
}

export function createSignalGridRowModel<TRow extends Record<string, unknown>>(
  gridRef: SignalGridPatchQueueGridRef<TRow>,
  options: SignalGridRowModelOptions<TRow>,
) {
  const rows = shallowRef<TRow[]>([])
  const rowsById = new Map<SignalGridRowId, TRow>()
  const patchQueue = createSignalGridPatchQueue(gridRef, {
    scheduler: options.scheduler,
    defaultReason: options.defaultReason,
  })

  function setRows(nextRows: readonly TRow[]) {
    const snapshot = nextRows.slice()
    rowsById.clear()
    snapshot.forEach((row) => {
      const rowId = options.resolveRowId(row)
      if (rowId !== null && rowId !== undefined) {
        rowsById.set(rowId, row)
      }
    })
    rows.value = snapshot
  }

  function getRow(rowId: SignalGridRowId): TRow | null {
    return rowsById.get(rowId) ?? null
  }

  function patchCachedRows(patches: readonly SignalGridRowPatch<TRow>[]) {
    patches.forEach((patch) => {
      const currentRow = rowsById.get(patch.rowId)
      if (!currentRow || Object.keys(patch.changes).length === 0) {
        return
      }
      rowsById.set(patch.rowId, {
        ...currentRow,
        ...patch.changes,
      })
    })
  }

  function enqueueRowPatches(
    patches: readonly SignalGridRowPatch<TRow>[],
    flushOptions?: SignalGridPatchQueueFlushOptions,
  ) {
    if (!patches.length) {
      return
    }
    patchCachedRows(patches)
    patchQueue.enqueueRowPatches(patches, flushOptions)
  }

  function enqueueCellRefresh(
    rowIds: readonly SignalGridRowId[],
    columns: readonly string[],
    flushOptions?: SignalGridPatchQueueFlushOptions,
  ) {
    patchQueue.enqueueCellRefresh(rowIds, columns, flushOptions)
  }

  function flushPatches(flushOptions?: SignalGridPatchQueueFlushOptions) {
    patchQueue.flush(flushOptions)
  }

  function cancel() {
    patchQueue.cancel()
  }

  return {
    rows,
    setRows,
    getRow,
    enqueueRowPatches,
    enqueueCellRefresh,
    flushPatches,
    cancel,
    diagnostics: patchQueue.diagnostics,
  }
}

export function useSignalGridRowModel<TRow extends Record<string, unknown>>(
  gridRef: SignalGridPatchQueueGridRef<TRow>,
  options: SignalGridRowModelOptions<TRow>,
) {
  const model = createSignalGridRowModel(gridRef, options)
  onBeforeUnmount(() => {
    model.cancel()
  })
  return model
}
