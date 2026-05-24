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
  scheduler?: SignalGridPatchQueueScheduler
  defaultReason?: string
}

export function createSignalGridRowModel<TRow extends Record<string, unknown>>(
  gridRef: SignalGridPatchQueueGridRef<TRow>,
  options: SignalGridRowModelOptions<TRow>,
) {
  const rows = shallowRef<TRow[]>([])
  const patchQueue = createSignalGridPatchQueue(gridRef, {
    scheduler: options.scheduler,
    defaultReason: options.defaultReason,
  })

  function setRows(nextRows: readonly TRow[]) {
    rows.value = nextRows.slice()
  }

  function enqueueRowPatches(
    patches: readonly SignalGridRowPatch<TRow>[],
    flushOptions?: SignalGridPatchQueueFlushOptions,
  ) {
    if (!patches.length) {
      return
    }
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
