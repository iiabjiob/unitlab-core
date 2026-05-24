import type { SignalAllocationRow } from "@/types/signal"
import type { SignalGridPatchQueueFlushOptions, SignalGridRowId } from "@/pages/signals/composables/useSignalGridPatchQueue"
import {
  createSignalGridRowPatch,
  type SignalGridProjectionPatch,
  type SignalGridRuntimeOverlay,
} from "@/pages/signals/utils/signalGridProjection"

export type SignalGridPatchIngressCache = {
  patchRows: (rows: readonly SignalAllocationRow[]) => {
    changed: number
    missingSignalIds: number[]
  }
  getRowBySignalId: (signalId: number) => SignalAllocationRow | null
  hasSignalId: (signalId: number) => boolean
}

export type SignalGridPatchIngressRowModel = {
  enqueueRowPatches: (
    patches: readonly SignalGridProjectionPatch[],
    flushOptions?: SignalGridPatchQueueFlushOptions,
  ) => void
  enqueueCellRefresh?: (
    rowIds: readonly SignalGridRowId[],
    columns: readonly string[],
    flushOptions?: SignalGridPatchQueueFlushOptions,
  ) => void
  flushPatches: (flushOptions?: SignalGridPatchQueueFlushOptions) => void
}

export type SignalGridPatchIngressOptions = {
  cache: SignalGridPatchIngressCache
  rowModel: SignalGridPatchIngressRowModel
  getHeaders: () => readonly string[]
  getRuntime?: () => SignalGridRuntimeOverlay | undefined
  onProjectionChanged?: () => void
  defaultColumns?: readonly string[]
}

export type SignalGridPatchIngressApplyOptions = SignalGridPatchQueueFlushOptions & {
  columns?: readonly string[]
  flush?: boolean
}

export type SignalGridPatchIngressResult = {
  requested: number
  changed: number
  missingSignalIds: number[]
  enqueuedPatches: number
}

function normalizeSignalId(value: unknown): number | null {
  const signalId = Number(value)
  return Number.isFinite(signalId) && signalId > 0 ? signalId : null
}

function uniqueSignalIds(signalIds: readonly unknown[]): number[] {
  const seen = new Set<number>()
  const normalized: number[] = []
  signalIds.forEach((rawSignalId) => {
    const signalId = normalizeSignalId(rawSignalId)
    if (signalId === null || seen.has(signalId)) {
      return
    }
    seen.add(signalId)
    normalized.push(signalId)
  })
  return normalized
}

function uniqueRowsBySignalId(rows: readonly SignalAllocationRow[]): SignalAllocationRow[] {
  const bySignalId = new Map<number, SignalAllocationRow>()
  rows.forEach((row) => {
    const signalId = normalizeSignalId(row.signal_id)
    if (signalId === null) {
      return
    }
    bySignalId.set(signalId, row)
  })
  return Array.from(bySignalId.values())
}

export function createSignalGridPatchIngress(options: SignalGridPatchIngressOptions) {
  function createPatchForCachedRow(
    row: SignalAllocationRow,
    columns: readonly string[] | undefined,
  ): SignalGridProjectionPatch {
    return createSignalGridRowPatch(
      row,
      options.getHeaders(),
      columns,
      options.getRuntime?.(),
    )
  }

  function enqueuePatches(
    patches: readonly SignalGridProjectionPatch[],
    applyOptions?: SignalGridPatchIngressApplyOptions,
  ) {
    if (!patches.length) {
      return
    }
    const { columns: _columns, flush, ...flushOptions } = applyOptions ?? {}
    options.rowModel.enqueueRowPatches(patches, flushOptions)
    if (flush) {
      options.rowModel.flushPatches(flushOptions)
    }
  }

  function applyAllocationRows(
    rows: readonly SignalAllocationRow[],
    applyOptions?: SignalGridPatchIngressApplyOptions,
  ): SignalGridPatchIngressResult {
    const uniqueRows = uniqueRowsBySignalId(rows)
    if (!uniqueRows.length) {
      return { requested: 0, changed: 0, missingSignalIds: [], enqueuedPatches: 0 }
    }

    const patchResult = options.cache.patchRows(uniqueRows)
    if (patchResult.changed > 0) {
      options.onProjectionChanged?.()
    }

    const missing = new Set(patchResult.missingSignalIds)
    const columns = applyOptions?.columns ?? options.defaultColumns
    const patches = uniqueRows.flatMap((row) => {
      const signalId = normalizeSignalId(row.signal_id)
      if (signalId === null || missing.has(signalId)) {
        return []
      }
      const cachedRow = options.cache.getRowBySignalId(signalId)
      return cachedRow ? [createPatchForCachedRow(cachedRow, columns)] : []
    })

    enqueuePatches(patches, applyOptions)

    return {
      requested: uniqueRows.length,
      changed: patchResult.changed,
      missingSignalIds: patchResult.missingSignalIds,
      enqueuedPatches: patches.length,
    }
  }

  function applySignalRowsById(
    signalIds: readonly unknown[],
    applyOptions?: SignalGridPatchIngressApplyOptions,
  ): SignalGridPatchIngressResult {
    const uniqueIds = uniqueSignalIds(signalIds)
    if (!uniqueIds.length) {
      return { requested: 0, changed: 0, missingSignalIds: [], enqueuedPatches: 0 }
    }

    const columns = applyOptions?.columns ?? options.defaultColumns
    const missingSignalIds: number[] = []
    const patches: SignalGridProjectionPatch[] = []

    uniqueIds.forEach((signalId) => {
      if (!options.cache.hasSignalId(signalId)) {
        missingSignalIds.push(signalId)
        return
      }
      const cachedRow = options.cache.getRowBySignalId(signalId)
      if (!cachedRow) {
        missingSignalIds.push(signalId)
        return
      }
      patches.push(createPatchForCachedRow(cachedRow, columns))
    })

    enqueuePatches(patches, applyOptions)

    return {
      requested: uniqueIds.length,
      changed: patches.length,
      missingSignalIds,
      enqueuedPatches: patches.length,
    }
  }

  function applyRuntimeSignals(
    signalIds: readonly unknown[],
    applyOptions?: SignalGridPatchIngressApplyOptions,
  ): SignalGridPatchIngressResult {
    return applySignalRowsById(signalIds, applyOptions)
  }

  function refreshSignalCells(
    signalIds: readonly unknown[],
    columns: readonly string[],
    flushOptions?: SignalGridPatchQueueFlushOptions,
  ): SignalGridPatchIngressResult {
    const uniqueIds = uniqueSignalIds(signalIds)
    const missingSignalIds: number[] = []
    const rowIds: SignalGridRowId[] = []
    uniqueIds.forEach((signalId) => {
      const row = options.cache.getRowBySignalId(signalId)
      if (!row) {
        missingSignalIds.push(signalId)
        return
      }
      const patch = createPatchForCachedRow(row, columns)
      rowIds.push(patch.rowId)
    })

    if (rowIds.length > 0) {
      options.rowModel.enqueueCellRefresh?.(rowIds, columns, flushOptions)
    }

    return {
      requested: uniqueIds.length,
      changed: rowIds.length,
      missingSignalIds,
      enqueuedPatches: 0,
    }
  }

  return {
    applyAllocationRows,
    applySignalRowsById,
    applyRuntimeSignals,
    refreshSignalCells,
  }
}
