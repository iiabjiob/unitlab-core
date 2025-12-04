import {
  createWritableSignal,
  type CreateWritableSignal,
  type WritableSignal,
} from "../runtime/signals"

export interface ServerRowModelFetchResult<T> {
  rows: T[]
  total?: number
}

export interface ServerRowModelOptions<T> {
  blockSize?: number
  maxCacheBlocks?: number
  preloadThreshold?: number
  loadBlock: (args: {
    start: number
    limit: number
    signal: AbortSignal
    background: boolean
  }) => Promise<ServerRowModelFetchResult<T> | T[] | void> | ServerRowModelFetchResult<T> | T[] | void
  onBlockLoaded?: (payload: {
    blockIndex: number
    start: number
    rows: readonly T[]
    total: number | null
    background: boolean
  }) => void
  onError?: (payload: {
    blockIndex: number
    start: number
    error: Error
    background: boolean
  }) => void
  onProgress?: (payload: {
    progress: number | null
    total: number | null
    loadedRows: number
  }) => void
  adaptivePrefetch?: boolean
  adaptiveScrollTiming?: {
    fast?: number
    slow?: number
  }
}

export interface ServerRowModelDiagnostics {
  cacheBlocks: number
  cachedRows: number
  pendingBlocks: number
  pendingRequests: number
  abortedRequests: number
  cacheHits: number
  cacheMisses: number
  effectivePreloadThreshold: number
}

export interface ServerRowModelDebug<T> {
  cache: WritableSignal<Map<number, T[]>>
  pending: Map<number, PendingFetch>
  metrics: {
    cachedRowCount: WritableSignal<number>
    pendingBlocks: WritableSignal<number>
    pendingRequests: WritableSignal<number>
    abortedRequests: WritableSignal<number>
    cacheHits: WritableSignal<number>
    cacheMisses: WritableSignal<number>
    effectivePreloadThreshold: WritableSignal<number>
  }
}

export interface ServerRowModelSignals<T> {
  rows: WritableSignal<T[]>
  loading: WritableSignal<boolean>
  error: WritableSignal<Error | null>
  blocks: WritableSignal<Map<number, T[]>>
  total: WritableSignal<number | null>
  loadedRanges: WritableSignal<Array<{ start: number; end: number }>>
  progress: WritableSignal<number | null>
  blockErrors: WritableSignal<Map<number, Error>>
  diagnostics: WritableSignal<ServerRowModelDiagnostics>
}

export interface ServerRowModel<T> extends ServerRowModelSignals<T> {
  getRowAt: (index: number) => T | undefined
  getRowCount: () => number
  refreshBlock: (blockIndex: number) => Promise<void>
  fetchBlock: (startIndex: number) => Promise<void>
  reset: () => void
  abortAll: () => void
  dispose: () => void
  __debug?: ServerRowModelDebug<T>
}

export type Direction = -1 | 0 | 1

export type PendingFetch = {
  controller: AbortController
  promise: Promise<void>
}

export interface ScheduledTaskHandle {
  cancel(): void
}

export interface ServerRowModelRuntime {
  scheduleFrame(callback: () => void): ScheduledTaskHandle
  scheduleTimeout(callback: () => void, delay: number): ScheduledTaskHandle
  now(): number
}

function createScheduledTaskHandle(cancel: () => void): ScheduledTaskHandle {
  return {
    cancel,
  }
}

function createDefaultServerRowModelRuntime(globalRef: typeof globalThis = globalThis): ServerRowModelRuntime {
  const raf = (globalRef as typeof globalRef & { requestAnimationFrame?: typeof requestAnimationFrame }).requestAnimationFrame
  const cancelRaf = (globalRef as typeof globalRef & { cancelAnimationFrame?: typeof cancelAnimationFrame }).cancelAnimationFrame
  const setTimeoutRef = (globalRef as typeof globalRef & { setTimeout?: typeof setTimeout }).setTimeout ?? setTimeout
  const clearTimeoutRef = (globalRef as typeof globalRef & { clearTimeout?: typeof clearTimeout }).clearTimeout ?? clearTimeout
  const performanceNow = (() => {
    const performanceRef = (globalRef as typeof globalRef & { performance?: Performance }).performance
    if (performanceRef && typeof performanceRef.now === "function") {
      return () => performanceRef.now()
    }
    return null
  })()

  return {
    scheduleFrame(callback: () => void): ScheduledTaskHandle {
      if (typeof raf === "function") {
        const handle = raf.call(globalRef, () => callback())
        const cancel = typeof cancelRaf === "function"
          ? () => cancelRaf.call(globalRef, handle)
          : () => { /* no-op */ }
        return createScheduledTaskHandle(cancel)
      }
      const timeoutId = setTimeoutRef.call(globalRef, callback, 16)
      return createScheduledTaskHandle(() => clearTimeoutRef.call(globalRef, timeoutId))
    },
    scheduleTimeout(callback: () => void, delay: number): ScheduledTaskHandle {
      const timeoutId = setTimeoutRef.call(globalRef, callback, delay)
      return createScheduledTaskHandle(() => clearTimeoutRef.call(globalRef, timeoutId))
    },
    now(): number {
      if (performanceNow) {
        return performanceNow()
      }
      const DateCtor = (globalRef as typeof globalRef & { Date?: DateConstructor }).Date ?? Date
      return new DateCtor().getTime()
    },
  }
}

function isAbortError(error: unknown): boolean {
  if (!error) return false
  if (error instanceof DOMException) {
    return error.name === "AbortError"
  }
  return (error as { name?: string }).name === "AbortError"
}

const DEFAULT_PROGRESS_DEBOUNCE_MS = 120

export interface CreateServerRowModelConfig {
  createSignal?: CreateWritableSignal<unknown>
  progressDebounceMs?: number
  runtime?: ServerRowModelRuntime
}

export function createServerRowModel<T>(
  options: ServerRowModelOptions<T>,
  config: CreateServerRowModelConfig = {},
): ServerRowModel<T> {
  const runtime = config.runtime ?? createDefaultServerRowModelRuntime()
  const blockSize = Math.max(1, options.blockSize ?? 300)
  const maxCacheBlocks = Math.max(2, options.maxCacheBlocks ?? 3)
  const preloadThreshold = Math.min(Math.max(options.preloadThreshold ?? 0.6, 0.1), 0.95)
  const adaptiveTiming = options.adaptiveScrollTiming ?? {}
  const fastScrollDeltaMs = Math.max(0, adaptiveTiming.fast ?? 120)
  const slowScrollDeltaMs = Math.max(fastScrollDeltaMs + 1, adaptiveTiming.slow ?? 400)
  const adaptivePrefetch = options.adaptivePrefetch !== false
  const progressDebounceMs = Math.max(0, config.progressDebounceMs ?? DEFAULT_PROGRESS_DEBOUNCE_MS)
  const signalFactory = config.createSignal as CreateWritableSignal<unknown> | undefined

  const createSignal = <S,>(initial: S): WritableSignal<S> => {
    if (signalFactory) {
      return (signalFactory as CreateWritableSignal<S>)(initial)
    }
    return createWritableSignal(initial)
  }

  let disposed = false

  const rows = createSignal<T[]>([])
  const loading = createSignal(false)
  const error = createSignal<Error | null>(null)
  const blocks = createSignal(new Map<number, T[]>())
  const total = createSignal<number | null>(null)
  const loadedRanges = createSignal<Array<{ start: number; end: number }>>([])
  const progress = createSignal<number | null>(null)
  const blockErrors = createSignal(new Map<number, Error>())
  const diagnostics = createSignal<ServerRowModelDiagnostics>({
    cacheBlocks: 0,
    cachedRows: 0,
    pendingBlocks: 0,
    pendingRequests: 0,
    abortedRequests: 0,
    cacheHits: 0,
    cacheMisses: 0,
    effectivePreloadThreshold: preloadThreshold,
  })

  const cachedRowCountSignal = createSignal(0)
  const pendingBlockCountSignal = createSignal(0)
  const pendingRequestsSignal = createSignal(0)
  const abortedRequestsSignal = createSignal(0)
  const cacheHitsSignal = createSignal(0)
  const cacheMissesSignal = createSignal(0)
  const effectivePreloadThresholdSignal = createSignal(preloadThreshold)

  const pending = new Map<number, PendingFetch>()
  let cachedRowCount = 0
  let cacheUpdateHandle: ScheduledTaskHandle | null = null
  let snapshotDirty = true
  let sortedKeysDirty = true
  let rowSnapshot: T[] = []
  let rangeSnapshot: Array<{ start: number; end: number }> = []
  let sortedKeys: number[] = []
  let currentBlockIndex: number | null = null
  let lastFetchTimestamp: number | null = null
  let lastRequestedBlock: number | null = null
  let lastDirection: Direction = 0
  let progressTimeout: ScheduledTaskHandle | null = null
  let effectivePreloadThreshold = preloadThreshold

  function scheduleCacheUpdate() {
    if (disposed) {
      return
    }
    snapshotDirty = true
    sortedKeysDirty = true
    if (cacheUpdateHandle) {
      return
    }
    cacheUpdateHandle = runtime.scheduleFrame(() => {
      cacheUpdateHandle = null
      if (disposed) {
        return
      }
      flushCacheUpdate()
    })
  }

  function flushCacheUpdate() {
    if (disposed) {
      return
    }
    if (snapshotDirty) {
      const keys = getSortedCacheKeys()
      const merged: T[] = []
      for (const start of keys) {
        const blockRows = blocks.value.get(start)
        if (blockRows && blockRows.length) {
          merged.push(...blockRows)
        }
      }
      rowSnapshot = merged
      rows.value = rowSnapshot
      snapshotDirty = false
    }

    if (sortedKeysDirty) {
      const keys = Array.from(blocks.value.keys()).sort((a, b) => a - b)
      sortedKeys = keys
      sortedKeysDirty = false
    }

    const nextRanges: Array<{ start: number; end: number }> = []
    for (const start of sortedKeys) {
      const blockRows = blocks.value.get(start)
      const length = blockRows?.length ?? 0
      nextRanges.push({
        start,
        end: start + Math.max(length - 1, 0),
      })
    }
    rangeSnapshot = nextRanges
    loadedRanges.value = rangeSnapshot

    updateProgress()
    updateDiagnostics()
  }

  function updateProgress() {
    const currentTotal = total.value
    let value: number | null = null
    if (currentTotal == null) {
      value = null
    } else if (currentTotal <= 0) {
      value = currentTotal === 0 ? 1 : 0
    } else {
      value = Math.min(1, Math.max(0, cachedRowCount / Math.max(currentTotal, 1)))
    }
    progress.value = value
    notifyProgress()
  }

  function notifyProgress() {
    if (!options.onProgress || disposed) {
      return
    }
    if (progressTimeout != null) {
      progressTimeout.cancel()
      progressTimeout = null
    }
    progressTimeout = runtime.scheduleTimeout(() => {
      progressTimeout = null
      if (disposed) {
        return
      }
      options.onProgress?.({
        progress: progress.value,
        total: total.value,
        loadedRows: cachedRowCount,
      })
    }, progressDebounceMs)
  }

  function updateDiagnostics() {
    diagnostics.value = {
      cacheBlocks: blocks.value.size,
      cachedRows: cachedRowCount,
      pendingBlocks: pending.size,
      pendingRequests: pendingRequestsSignal.value,
      abortedRequests: abortedRequestsSignal.value,
      cacheHits: cacheHitsSignal.value,
      cacheMisses: cacheMissesSignal.value,
      effectivePreloadThreshold: effectivePreloadThreshold,
    }
  }

  function getSortedCacheKeys(): number[] {
    if (sortedKeysDirty) {
      sortedKeys = Array.from(blocks.value.keys()).sort((a, b) => a - b)
      sortedKeysDirty = false
    }
    return sortedKeys
  }

  function resolvePreloadThreshold(): number {
    const now = runtime.now()
    if (!adaptivePrefetch) {
      lastFetchTimestamp = now
      effectivePreloadThreshold = preloadThreshold
      effectivePreloadThresholdSignal.value = effectivePreloadThreshold
      return effectivePreloadThreshold
    }
    if (lastFetchTimestamp == null) {
      lastFetchTimestamp = now
      effectivePreloadThreshold = preloadThreshold
      effectivePreloadThresholdSignal.value = effectivePreloadThreshold
      return effectivePreloadThreshold
    }
    const delta = now - lastFetchTimestamp
    lastFetchTimestamp = now
    if (delta <= fastScrollDeltaMs) {
      effectivePreloadThreshold = Math.max(0.2, preloadThreshold - 0.2)
    } else if (delta >= slowScrollDeltaMs) {
      effectivePreloadThreshold = Math.min(0.9, preloadThreshold + 0.1)
    } else {
      effectivePreloadThreshold = preloadThreshold
    }
    effectivePreloadThresholdSignal.value = effectivePreloadThreshold
    updateDiagnostics()
    return effectivePreloadThreshold
  }

  function abortAllPending() {
    const entries = Array.from(pending.entries())
    for (const [start, pendingFetch] of entries) {
      if (!pendingFetch.controller.signal.aborted) {
        pendingFetch.controller.abort()
        abortedRequestsSignal.value = abortedRequestsSignal.value + 1
      }
      pending.delete(start)
    }
    pendingBlockCountSignal.value = pending.size
    updateDiagnostics()
  }

  function setCacheBlock(start: number, blockRows: T[]) {
    const next = new Map(blocks.value)
    const previous = next.get(start)
    next.set(start, blockRows)
    blocks.value = next
    cachedRowCount += blockRows.length - (previous?.length ?? 0)
    cachedRowCountSignal.value = cachedRowCount
    scheduleCacheUpdate()
  }

  function trimCache(anchorBlockIndex: number) {
    const keys = getSortedCacheKeys()
    if (keys.length <= maxCacheBlocks) {
      return
    }
    const firstBlockIndex = Math.floor((keys[0] ?? 0) / blockSize)
    const lastBlockIndex = Math.floor((keys[keys.length - 1] ?? 0) / blockSize)
    let windowStart = Math.max(firstBlockIndex, anchorBlockIndex - Math.floor((maxCacheBlocks - 1) / 2))
    let windowEnd = windowStart + maxCacheBlocks - 1
    if (windowEnd > lastBlockIndex) {
      windowEnd = lastBlockIndex
      windowStart = Math.max(firstBlockIndex, windowEnd - maxCacheBlocks + 1)
    }

    const next = new Map<number, T[]>()
    let nextCount = 0
    keys.forEach(start => {
      const blockIdx = Math.floor(start / blockSize)
      if (blockIdx >= windowStart && blockIdx <= windowEnd) {
        const blockRows = blocks.value.get(start)
        if (blockRows) {
          next.set(start, blockRows)
          nextCount += blockRows.length
        }
      }
    })
    blocks.value = next
    cachedRowCount = nextCount
    cachedRowCountSignal.value = cachedRowCount
    scheduleCacheUpdate()
  }

  function preloadBlock(targetBlockIndex: number) {
    if (disposed) return
    if (targetBlockIndex < 0) return
    const start = targetBlockIndex * blockSize
    if (blocks.value.has(start)) return
    if (pending.has(start)) return
    void loadBlock(targetBlockIndex, { background: true })
  }

  function cancelPendingOutsideWindow(centerBlockIndex: number) {
    if (!pending.size) return
    const tolerance = Math.max(2, Math.ceil(maxCacheBlocks / 2))
    const entries = Array.from(pending.entries())
    for (const [start, pendingFetch] of entries) {
      const blockIndex = Math.floor(start / blockSize)
      if (Math.abs(blockIndex - centerBlockIndex) > tolerance) {
        if (!pendingFetch.controller.signal.aborted) {
          pendingFetch.controller.abort()
          abortedRequestsSignal.value = abortedRequestsSignal.value + 1
        }
        pending.delete(start)
      }
    }
    pendingBlockCountSignal.value = pending.size
    updateDiagnostics()
  }

  function updateLoadingSignal() {
    loading.value = pendingRequestsSignal.value > 0
  }

  async function loadBlock(blockIndex: number, { background = false }: { background?: boolean } = {}): Promise<void> {
    if (disposed) return
    if (blockIndex < 0) return
    const start = blockIndex * blockSize
    if (blocks.value.has(start)) return

    const existing = pending.get(start)
    if (existing) {
      await existing.promise
      return
    }

    const controller = new AbortController()
    const fetchPromise = (async () => {
      pendingRequestsSignal.value = pendingRequestsSignal.value + 1
      pendingBlockCountSignal.value = pending.size
      updateLoadingSignal()
      updateDiagnostics()
      try {
        const result = await options.loadBlock({
          start,
          limit: blockSize,
          signal: controller.signal,
          background,
        })
        if (controller.signal.aborted || disposed) return

        let blockRows: T[] = []
        if (Array.isArray(result)) {
          blockRows = result.slice()
        } else if (result && typeof result === "object") {
          const payload = result as ServerRowModelFetchResult<T>
          if (Array.isArray(payload.rows)) {
            blockRows = payload.rows.slice()
          }
          if (typeof payload.total === "number" && Number.isFinite(payload.total)) {
            const normalizedTotal = Math.max(0, Math.floor(payload.total))
            if (normalizedTotal !== total.value) {
              total.value = normalizedTotal
            }
          }
        }
        if (disposed) return
        setCacheBlock(start, blockRows)
        updateBlockError(start)
        options.onBlockLoaded?.({
          blockIndex,
          start,
          rows: blockRows,
          total: total.value,
          background,
        })
        const anchor = currentBlockIndex ?? blockIndex
        trimCache(anchor)
      } catch (err) {
        if (isAbortError(err)) return
        if (!background) {
          const normalized = err instanceof Error ? err : new Error(String(err))
          error.value = normalized
          updateBlockError(start, normalized)
          options.onError?.({
            blockIndex,
            start,
            error: normalized,
            background,
          })
        }
      } finally {
        pending.delete(start)
        pendingBlockCountSignal.value = pending.size
        pendingRequestsSignal.value = Math.max(0, pendingRequestsSignal.value - 1)
        updateLoadingSignal()
        updateDiagnostics()
      }
    })()

    pending.set(start, { controller, promise: fetchPromise })
    pendingBlockCountSignal.value = pending.size
    if (!background) {
      error.value = null
      await fetchPromise
    }
  }

  function maybePreload(blockIndex: number, direction: Direction, relativeIndex: number, threshold: number) {
    if (blockSize <= 0) return
    const jitter = adaptivePrefetch ? Math.random() * 0.1 - 0.05 : 0
    const normalizedThreshold = Math.min(Math.max(threshold + jitter, 0.1), 0.95)
    const blockProgress = Math.min(Math.max(relativeIndex / blockSize, 0), 1)
    if (direction >= 0 && blockProgress >= normalizedThreshold) {
      preloadBlock(blockIndex + 1)
    } else if (direction <= 0 && 1 - blockProgress >= normalizedThreshold) {
      preloadBlock(blockIndex - 1)
    }
  }

  async function fetchBlock(startIndex: number): Promise<void> {
    if (disposed) return
    const targetBlockIndex = Math.max(0, Math.floor(startIndex / blockSize))
    const targetStart = targetBlockIndex * blockSize
    const effectiveThresholdValue = resolvePreloadThreshold()

    const direction: Direction = lastRequestedBlock == null
      ? 0
      : targetBlockIndex > lastRequestedBlock
        ? 1
        : targetBlockIndex < lastRequestedBlock
          ? -1
          : lastDirection

    if (lastRequestedBlock != null && ((direction !== 0 && direction !== lastDirection) || Math.abs(targetBlockIndex - lastRequestedBlock) > 1)) {
      cancelPendingOutsideWindow(targetBlockIndex)
    }

    lastRequestedBlock = targetBlockIndex
    if (direction !== 0) {
      lastDirection = direction
    }

    currentBlockIndex = targetBlockIndex

    if (!blocks.value.has(targetStart)) {
      if (!pending.has(targetStart)) {
        cacheMissesSignal.value = cacheMissesSignal.value + 1
      }
      await loadBlock(targetBlockIndex)
      if (disposed) return
    } else {
      cacheHitsSignal.value = cacheHitsSignal.value + 1
      trimCache(targetBlockIndex)
    }

    const relativeIndex = startIndex - targetStart
    const preloadDirection: Direction = direction === 0 ? 1 : direction
    if (disposed) {
      return
    }
    maybePreload(targetBlockIndex, preloadDirection, Math.max(0, relativeIndex), effectiveThresholdValue)
    updateDiagnostics()
  }

  async function refreshBlock(blockIndex: number): Promise<void> {
    if (disposed) return
    if (blockIndex < 0) return
    const start = blockIndex * blockSize
    const existing = blocks.value.get(start)
    if (existing) {
      cachedRowCount = Math.max(0, cachedRowCount - existing.length)
      cachedRowCountSignal.value = cachedRowCount
      const next = new Map(blocks.value)
      next.delete(start)
      blocks.value = next
      scheduleCacheUpdate()
    }
    updateBlockError(start)
    await loadBlock(blockIndex)
  }

  function updateBlockError(start: number, blockError?: Error) {
    const next = new Map(blockErrors.value)
    if (blockError) {
      next.set(start, blockError)
    } else {
      next.delete(start)
    }
    blockErrors.value = next
  }

  function getRowAt(index: number): T | undefined {
    if (!Number.isFinite(index)) {
      return undefined
    }
    const normalized = Math.floor(index)
    if (normalized < 0) {
      return undefined
    }
    const blockIndex = Math.floor(normalized / blockSize)
    const blockStart = blockIndex * blockSize
    const blockRows = blocks.value.get(blockStart)
    if (!blockRows) {
      return undefined
    }
    const offset = normalized - blockStart
    return blockRows[offset]
  }

  function getRowCount(): number {
    if (typeof total.value === "number" && Number.isFinite(total.value)) {
      return total.value
    }
    return cachedRowCount
  }

  function reset() {
    if (disposed) {
      return
    }
    abortAllPending()
    blocks.value = new Map()
    cachedRowCount = 0
    cachedRowCountSignal.value = 0
    pendingRequestsSignal.value = 0
    pendingBlockCountSignal.value = pending.size
    abortedRequestsSignal.value = 0
    cacheHitsSignal.value = 0
    cacheMissesSignal.value = 0
    loading.value = false
    error.value = null
    total.value = null
    currentBlockIndex = null
    lastRequestedBlock = null
    lastDirection = 0
    blockErrors.value = new Map()
    rowSnapshot = []
    rangeSnapshot = []
    snapshotDirty = true
    sortedKeys = []
    sortedKeysDirty = true
    lastFetchTimestamp = null
    effectivePreloadThreshold = preloadThreshold
    effectivePreloadThresholdSignal.value = effectivePreloadThreshold
    if (progressTimeout != null) {
      progressTimeout.cancel()
      progressTimeout = null
    }
    if (cacheUpdateHandle) {
      cacheUpdateHandle.cancel()
      cacheUpdateHandle = null
    }
    scheduleCacheUpdate()
  }

  function dispose() {
    if (disposed) {
      return
    }
    disposed = true
    if (progressTimeout) {
      progressTimeout.cancel()
      progressTimeout = null
    }
    if (cacheUpdateHandle) {
      cacheUpdateHandle.cancel()
      cacheUpdateHandle = null
    }
    abortAllPending()
    pendingRequestsSignal.value = 0
    pendingBlockCountSignal.value = pending.size
    updateLoadingSignal()
    loading.value = false
  }

  const debug: ServerRowModelDebug<T> = {
    cache: blocks,
    pending,
    metrics: {
      cachedRowCount: cachedRowCountSignal,
      pendingBlocks: pendingBlockCountSignal,
      pendingRequests: pendingRequestsSignal,
      abortedRequests: abortedRequestsSignal,
      cacheHits: cacheHitsSignal,
      cacheMisses: cacheMissesSignal,
      effectivePreloadThreshold: effectivePreloadThresholdSignal,
    },
  }

  const model: ServerRowModel<T> = {
    rows,
    loading,
    error,
    blocks,
    total,
    loadedRanges,
    progress,
    blockErrors,
    diagnostics,
    getRowAt,
    getRowCount,
    refreshBlock,
    fetchBlock,
    reset,
    abortAll: abortAllPending,
    dispose,
    __debug: debug,
  }

  return model
}
