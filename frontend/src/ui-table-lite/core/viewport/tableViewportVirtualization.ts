import { createAxisVirtualizer, type AxisVirtualizerState } from "../virtualization/axisVirtualizer"
import { createVerticalAxisStrategy } from "../virtualization/verticalVirtualizer"
import { createVerticalOverscanController } from "../virtualization/dynamicOverscan"
import { clampScrollOffset, computeVerticalScrollLimit } from "../virtualization/scrollLimits"
import type { VisibleRow } from "../types"
import type { TableViewportDiagnostics } from "./tableViewportDiagnostics"
import type { RowPoolItem, TableViewportSignals } from "./tableViewportSignals"
import type {
  TableViewportImperativeCallbacks,
  TableViewportServerIntegration,
} from "./tableViewportTypes"
import type { ViewportClock } from "./tableViewportConfig"
import {
  FRAME_BUDGET_CONSTANTS,
  VERTICAL_VIRTUALIZATION_CONSTANTS,
  type AxisVirtualizationConstants,
  type ViewportFrameBudget,
} from "./tableViewportConstants"

interface VisibleRangePayload {
  start: number
  end: number
}

export interface TableViewportVirtualizationOptions {
  signals: TableViewportSignals
  diagnostics: TableViewportDiagnostics
  clock: ViewportClock
  frameBudget?: ViewportFrameBudget
  verticalConfig?: AxisVirtualizationConstants
}

export interface TableViewportVirtualizationUpdateArgs {
  rows: VisibleRow[]
  totalRowCount: number
  viewportHeight: number
  resolvedRowHeight: number
  zoomFactor: number
  virtualizationEnabled: boolean
  pendingScrollTop: number
  lastScrollTopSample: number
  pendingScrollTopRequest: number | null
  measuredScrollTopFromPending: boolean
  cachedNativeScrollHeight: number
  containerHeight: number
  serverIntegration: TableViewportServerIntegration
  imperativeCallbacks: TableViewportImperativeCallbacks
}

export interface TableViewportVirtualizationResult {
  scrollTop: number
  lastScrollTopSample: number
  pendingScrollTop: number | null
  visibleRange: VisibleRangePayload
  syncedScrollTop: number | null
  timestamp: number
  pendingScrollWrite: number | null
}

export interface TableViewportVirtualizationPrepared {
  state: AxisVirtualizerState<undefined>
  scrollTop: number
  lastScrollTopSample: number
  pendingScrollTop: number | null
  visibleRange: VisibleRangePayload
  syncedScrollTop: number | null
  timestamp: number
  pendingScrollWrite: number | null
}

export interface TableViewportVirtualizationApplyArgs {
  rows: VisibleRow[]
  serverIntegration: TableViewportServerIntegration
  imperativeCallbacks: TableViewportImperativeCallbacks
}

export interface TableViewportVirtualization {
  resetOverscan(timestamp: number): void
  resetScrollState(timestamp: number): void
  resetServerIntegration(): void
  clampScrollTop(value: number): number
  prepare(args: TableViewportVirtualizationUpdateArgs): TableViewportVirtualizationPrepared | null
  applyPrepared(
    prepared: TableViewportVirtualizationPrepared,
    applyArgs: TableViewportVirtualizationApplyArgs,
  ): TableViewportVirtualizationResult
  update(args: TableViewportVirtualizationUpdateArgs): TableViewportVirtualizationResult | null
}

export function createTableViewportVirtualization(
  options: TableViewportVirtualizationOptions,
): TableViewportVirtualization {
  const { signals, diagnostics, clock } = options
  const frameBudget = options.frameBudget ?? FRAME_BUDGET_CONSTANTS
  const verticalConfig = options.verticalConfig ?? VERTICAL_VIRTUALIZATION_CONSTANTS
  const { input, core, derived } = signals
  const { scrollTop, virtualizationEnabled } = input
  const {
    totalRowCount,
    effectiveRowHeight,
    visibleCount,
    poolSize,
    totalContentHeight,
    startIndex,
    endIndex,
    overscanLeading,
    overscanTrailing,
  } = core
  const {
    rows: { visibleRange },
  } = derived

  const verticalVirtualizer = createAxisVirtualizer("vertical", createVerticalAxisStrategy(), undefined)
  const verticalOverscanController = createVerticalOverscanController({
    minOverscan: verticalConfig.minOverscan,
    velocityRatio: verticalConfig.velocityOverscanRatio,
    viewportRatio: verticalConfig.viewportOverscanRatio,
    decay: verticalConfig.overscanDecay,
    maxViewportMultiplier: verticalConfig.maxViewportMultiplier,
    teleportMultiplier: frameBudget.teleportMultiplier,
    frameDurationMs: frameBudget.frameDurationMs,
    minSampleMs: frameBudget.minVelocitySampleMs,
  })

  const visibleBuffers: VisibleRow[][] = [[], []]
  const rowPool: RowPoolItem[] = []
  let activeBufferIndex = 0
  let rowPoolVersion = 0
  let lastServerFetchIndex: number | null = null
  let lastKnownViewportHeight = 0
  let lastKnownRowHeight = 0
  let lastKnownTotalRows = 0
  let lastKnownNativeLimit = 0
  let lastVirtualizationFlag = true

  function ensureRowPoolSize(size: number): RowPoolItem[] {
    if (rowPool.length > size) {
      rowPool.length = size
      return rowPool
    }
    for (let index = rowPool.length; index < size; index += 1) {
      rowPool.push({ poolIndex: index, entry: null, displayIndex: -1, rowIndex: -1 })
    }
    return rowPool
  }

  function nextBufferIndex(current: number): number {
    return current === 0 ? 1 : 0
  }

  function bumpRowPoolVersion(): number {
    rowPoolVersion = rowPoolVersion >= Number.MAX_SAFE_INTEGER ? 0 : rowPoolVersion + 1
    return rowPoolVersion
  }

  function updateVisibleRows(
    rows: VisibleRow[],
    poolSizeValue: number,
    startIndexValue: number,
  ): { pool: RowPoolItem[]; visibleRows: VisibleRow[]; version: number } {
    const pool = ensureRowPoolSize(poolSizeValue)
    if (!poolSizeValue || totalRowCount.value === 0) {
      activeBufferIndex = nextBufferIndex(activeBufferIndex)
      const emptyBuffer = visibleBuffers[activeBufferIndex]
      if (emptyBuffer.length) {
        emptyBuffer.length = 0
      }
      const version = bumpRowPoolVersion()
      return { pool, visibleRows: [], version }
    }

    const nextIndex = nextBufferIndex(activeBufferIndex)
    const buffer = visibleBuffers[nextIndex]
    let filled = 0

    for (let poolIndexValue = 0; poolIndexValue < poolSizeValue; poolIndexValue += 1) {
      const rowIndexValue = startIndexValue + poolIndexValue
      const entry = rows[rowIndexValue] ?? null
      const item = pool[poolIndexValue]
      item.poolIndex = poolIndexValue
      item.rowIndex = rowIndexValue
      item.entry = entry

      if (entry) {
        if (entry.displayIndex !== rowIndexValue) {
          entry.displayIndex = rowIndexValue
        }
        item.displayIndex = entry.displayIndex ?? rowIndexValue
        buffer[filled] = entry
        filled += 1
      } else {
        item.displayIndex = rowIndexValue
      }
    }

    if (buffer.length !== filled) {
      buffer.length = filled
    }

    activeBufferIndex = nextIndex
    const snapshot = buffer.slice(0, filled)
    const version = bumpRowPoolVersion()
    return { pool, visibleRows: snapshot, version }
  }

  function resetOverscan(timestamp: number): void {
    verticalOverscanController.reset(timestamp)
  }

  function resetScrollState(timestamp: number): void {
    verticalOverscanController.reset(timestamp)
    lastServerFetchIndex = null
    lastKnownViewportHeight = 0
    lastKnownRowHeight = 0
    lastKnownTotalRows = 0
    lastKnownNativeLimit = 0
    lastVirtualizationFlag = true
    const virtualState = verticalVirtualizer.getState()
    virtualState.offset = 0
    virtualState.startIndex = 0
    virtualState.endIndex = 0
    virtualState.visibleCount = 0
    virtualState.poolSize = 0
    startIndex.value = 0
    endIndex.value = 0
    visibleCount.value = 0
    poolSize.value = 0
    totalContentHeight.value = 0
    overscanLeading.value = 0
    overscanTrailing.value = 0
    scrollTop.value = 0
    totalRowCount.value = 0
    effectiveRowHeight.value = 0
    for (const buffer of visibleBuffers) {
      if (buffer.length) {
        buffer.length = 0
      }
    }
    rowPool.length = 0
    rowPoolVersion = 0
    visibleRange.value = { start: 0, end: 0 }
  }

  function resetServerIntegration(): void {
    lastServerFetchIndex = null
  }

  function clampScrollTop(value: number): number {
    if (!Number.isFinite(value)) return 0
    const nativeLimit = Math.max(0, lastKnownNativeLimit)
    if (!lastVirtualizationFlag) {
      return clampScrollOffset({ offset: value, limit: nativeLimit })
    }
    const verticalState = verticalVirtualizer.getState()
    const limit = computeVerticalScrollLimit({
      estimatedItemSize: lastKnownRowHeight || 1,
      totalCount: lastKnownTotalRows,
      viewportSize: lastKnownViewportHeight || 1,
      overscanTrailing: verticalState.overscanTrailing,
      visibleCount: verticalState.visibleCount,
      nativeScrollLimit: nativeLimit,
      trailingPadding: verticalConfig.edgePadding,
    })
    return clampScrollOffset({ offset: value, limit })
  }

  function applyVirtualState(
    result: TableViewportVirtualizationPrepared,
    args: TableViewportVirtualizationApplyArgs,
  ): void {
    const { rows, serverIntegration, imperativeCallbacks } = args
    const {
      state: virtualState,
      scrollTop: nextScrollTop,
      visibleRange: nextRange,
      timestamp,
    } = result

    scrollTop.value = nextScrollTop
    visibleCount.value = virtualState.visibleCount
    poolSize.value = virtualState.poolSize
    startIndex.value = virtualState.startIndex
    endIndex.value = virtualState.endIndex
    totalContentHeight.value = totalRowCount.value * (effectiveRowHeight.value || 0)

    const { pool: poolSnapshot, visibleRows: visibleRowsSnapshot, version: poolVersion } =
      updateVisibleRows(rows, virtualState.poolSize, virtualState.startIndex)

    const currentRange = visibleRange.value
    if (currentRange.start !== nextRange.start || currentRange.end !== nextRange.end) {
      visibleRange.value = nextRange
    }

    startIndex.value = virtualState.startIndex
    endIndex.value = virtualState.endIndex
    visibleCount.value = virtualState.visibleCount
    poolSize.value = virtualState.poolSize
    overscanLeading.value = virtualState.overscanLeading
    overscanTrailing.value = virtualState.overscanTrailing

    if (serverIntegration.enabled && serverIntegration.rowModel) {
      const blockIndex = Math.floor(nextRange.start)
      if (lastServerFetchIndex !== blockIndex) {
        lastServerFetchIndex = blockIndex
        void serverIntegration.rowModel.fetchBlock(blockIndex)
      }
    }

    if (typeof imperativeCallbacks.onRows === "function") {
      imperativeCallbacks.onRows({
        pool: poolSnapshot,
        visibleRows: visibleRowsSnapshot,
        startIndex: virtualState.startIndex,
        endIndex: virtualState.endIndex,
        visibleCount: virtualState.visibleCount,
        totalRowCount: totalRowCount.value,
        rowHeight: effectiveRowHeight.value || 0,
        scrollTop: nextScrollTop,
        viewportHeight: lastKnownViewportHeight,
        overscanLeading: virtualState.overscanLeading,
        overscanTrailing: virtualState.overscanTrailing,
        timestamp,
        version: poolVersion,
      })
    }
  }

  function runUpdate(
    args: TableViewportVirtualizationUpdateArgs,
  ): TableViewportVirtualizationPrepared | null {
    const {
      totalRowCount: totalRows,
      viewportHeight: viewportHeightValue,
      resolvedRowHeight,
      zoomFactor,
      virtualizationEnabled: virtualizationFlag,
      pendingScrollTop,
      lastScrollTopSample,
      pendingScrollTopRequest,
      measuredScrollTopFromPending,
      cachedNativeScrollHeight,
      containerHeight,
    } = args

    lastKnownViewportHeight = viewportHeightValue
    lastKnownRowHeight = resolvedRowHeight
    lastKnownTotalRows = totalRows
    lastKnownNativeLimit = Math.max(0, cachedNativeScrollHeight - containerHeight)
    lastVirtualizationFlag = virtualizationFlag

    if (totalRows <= 0 || viewportHeightValue <= 0) {
      verticalVirtualizer.update({
        axis: "vertical",
        viewportSize: viewportHeightValue,
        scrollOffset: 0,
        virtualizationEnabled: virtualizationFlag,
        estimatedItemSize: resolvedRowHeight || 1,
        totalCount: totalRows,
        overscan: 0,
        meta: {
          zoom: zoomFactor,
          scrollDirection: 0,
          nativeScrollLimit: lastKnownNativeLimit,
          debug: false,
          debugNativeScrollLimit: undefined,
        },
      })

      for (const buffer of visibleBuffers) {
        if (buffer.length) {
          buffer.length = 0
        }
      }
      rowPool.length = 0
      rowPoolVersion = 0
      visibleRange.value = { start: 0, end: 0 }
      scrollTop.value = 0
      const timestamp = clock.now()

      return {
        state: verticalVirtualizer.getState(),
        scrollTop: 0,
        lastScrollTopSample: 0,
        pendingScrollTop: null,
        visibleRange: { start: 0, end: 0 },
        syncedScrollTop: null,
        timestamp,
        pendingScrollWrite: null,
      }
    }

    const nowTs = clock.now()
    const deltaTop = Math.abs(pendingScrollTop - lastScrollTopSample)
    const direction = pendingScrollTop === lastScrollTopSample ? 0 : pendingScrollTop > lastScrollTopSample ? 1 : -1

    let overscan = 0
    if (virtualizationFlag) {
      const overscanResult = verticalOverscanController.update({
        timestamp: nowTs,
        delta: deltaTop,
        viewportSize: viewportHeightValue,
        itemSize: resolvedRowHeight,
        virtualizationEnabled: true,
      })
      overscan = overscanResult.overscan
    } else {
      verticalOverscanController.reset(nowTs)
    }

    const verticalState = verticalVirtualizer.update({
      axis: "vertical",
      viewportSize: viewportHeightValue,
      scrollOffset: pendingScrollTop,
      virtualizationEnabled: virtualizationFlag,
      estimatedItemSize: resolvedRowHeight,
      totalCount: totalRows,
      overscan,
      meta: {
        zoom: zoomFactor,
        scrollDirection: direction,
        nativeScrollLimit: lastKnownNativeLimit,
        debug: diagnostics.isDebugEnabled(),
        debugNativeScrollLimit: diagnostics.isDebugEnabled() ? lastKnownNativeLimit : undefined,
      },
    })

    let nextScrollTop = clampScrollOffset({
      offset: verticalState.offset,
      limit: computeVerticalScrollLimit({
        estimatedItemSize: resolvedRowHeight,
        totalCount: totalRows,
        viewportSize: viewportHeightValue,
        overscanTrailing: verticalState.overscanTrailing,
        visibleCount: verticalState.visibleCount,
        nativeScrollLimit: lastKnownNativeLimit,
        trailingPadding: verticalConfig.edgePadding,
      }),
    })

    const needsScrollWrite =
      Math.abs(lastScrollTopSample - nextScrollTop) > verticalConfig.scrollEpsilon ||
      pendingScrollTopRequest != null ||
      measuredScrollTopFromPending

    const syncedScrollTop = needsScrollWrite ? nextScrollTop : null
    const pendingScrollWrite = needsScrollWrite ? nextScrollTop : null

    if (!needsScrollWrite) {
      nextScrollTop = lastScrollTopSample
    }

    const visibleStartIndex = Math.max(
      verticalState.startIndex,
      Math.min(verticalState.endIndex, verticalState.startIndex + Math.max(0, verticalState.overscanLeading)),
    )
    const visibleEndIndex = Math.min(
      verticalState.endIndex,
      visibleStartIndex + Math.max(0, verticalState.visibleCount),
    )

    return {
      state: verticalState,
      scrollTop: nextScrollTop,
      lastScrollTopSample: nextScrollTop,
      pendingScrollTop: null,
      visibleRange: { start: visibleStartIndex, end: visibleEndIndex },
      syncedScrollTop,
      timestamp: nowTs,
      pendingScrollWrite,
    }
  }

  function prepare(
    args: TableViewportVirtualizationUpdateArgs,
  ): TableViewportVirtualizationPrepared | null {
    totalRowCount.value = args.totalRowCount
    effectiveRowHeight.value = args.resolvedRowHeight
    virtualizationEnabled.value = args.virtualizationEnabled

    return runUpdate(args)
  }

  function applyPrepared(
    prepared: TableViewportVirtualizationPrepared,
    applyArgs: TableViewportVirtualizationApplyArgs,
  ): TableViewportVirtualizationResult {
    applyVirtualState(prepared, applyArgs)

    return {
      scrollTop: prepared.scrollTop,
      lastScrollTopSample: prepared.lastScrollTopSample,
      pendingScrollTop: prepared.pendingScrollTop,
      visibleRange: prepared.visibleRange,
      syncedScrollTop: prepared.syncedScrollTop,
      timestamp: prepared.timestamp,
      pendingScrollWrite: prepared.pendingScrollWrite,
    }
  }

  function update(
    args: TableViewportVirtualizationUpdateArgs,
  ): TableViewportVirtualizationResult | null {
    const prepared = prepare(args)
    if (!prepared) {
      return null
    }

    return applyPrepared(prepared, {
      rows: args.rows,
      serverIntegration: args.serverIntegration,
      imperativeCallbacks: args.imperativeCallbacks,
    })
  }

  return {
    resetOverscan,
    resetScrollState,
    resetServerIntegration,
    clampScrollTop,
    prepare,
    applyPrepared,
    update,
  }
}
