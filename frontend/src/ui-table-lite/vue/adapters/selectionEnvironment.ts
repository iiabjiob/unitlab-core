import { watch } from "vue"
import type { Ref } from "vue"
import type {
  SelectionEnvironment,
  SelectionMeasurementHandle,
  SelectionPoint,
  SelectionRange,
  SelectionOverlaySnapshot,
} from "@/ui-table/core/selection/selectionEnvironment"
import type { UiTableColumn } from "@/ui-table/core/types"
import type { SelectionOverlayRect } from "@/ui-table/core/selection/selectionOverlay"
import type { UiTableOverlayRect } from "../types/overlay"
import { createFrameRequestScheduler } from "@/ui-table/core/runtime/frameRequestScheduler"
import { createIdleCallbackScheduler } from "@/ui-table/core/runtime/idleCallbackScheduler"
import type { PointerCoordinates } from "@/ui-table/core/selection/autoScroll"
import type { FillHandleStylePayload } from "@/ui-table/core/selection/fillHandleStylePool"
import {
  acquireOverlayRectArray,
  releaseOverlayRect,
  releaseOverlayRectArray,
} from "@/ui-table/core/selection/selectionOverlayRectPool"

interface SelectionEnvironmentOptions<RowKey> {
  localColumns: Ref<UiTableColumn[]>
  selectionOverlayRects: Ref<UiTableOverlayRect[]>
  activeSelectionOverlayRects: Ref<UiTableOverlayRect[]>
  selectionCursorOverlay: Ref<UiTableOverlayRect | null>
  fillPreviewOverlayRects: Ref<UiTableOverlayRect[]>
  cutPreviewOverlayRects: Ref<UiTableOverlayRect[]>
  focusContainer: () => void
  resolveCellElement?: (rowIndex: number, columnKey: string) => HTMLElement | null
  resolveHeaderCellElement?: (columnKey: string) => HTMLElement | null
  getRowIdByIndex: (rowIndex: number) => RowKey | null
  findRowIndexById: (rowId: RowKey) => number | null
  measureFillHandle: (range: SelectionRange<RowKey>) => SelectionMeasurementHandle<FillHandleStylePayload | null>
  measureCellRect: (point: SelectionPoint<RowKey>) => SelectionMeasurementHandle<DOMRect | null>
  scrollSelectionIntoView: (input: {
    range: SelectionRange<RowKey> | null
    cursor: SelectionPoint<RowKey> | null
    attempt?: number
    maxAttempts?: number
  }) => void
  resolveCellFromPoint?: (clientX: number, clientY: number) => SelectionPoint<RowKey> | null
  resolveRowIndexFromPoint?: (clientX: number, clientY: number) => number | null
  invalidateMetrics?: () => void
  stopAutoScroll?: () => void
  updateAutoScroll?: (pointer: PointerCoordinates) => void
  globalObject?: typeof globalThis
}

interface SelectionEnvironmentBindings<RowKey> {
  environment: SelectionEnvironment<RowKey>
  cancelPending(): void
  dispose(): void
}

export function createVueSelectionEnvironment<RowKey>(
  options: SelectionEnvironmentOptions<RowKey>,
): SelectionEnvironmentBindings<RowKey> {
  const {
    localColumns,
    selectionOverlayRects,
    activeSelectionOverlayRects,
    selectionCursorOverlay,
    fillPreviewOverlayRects,
    cutPreviewOverlayRects,
    focusContainer,
    resolveCellElement,
    resolveHeaderCellElement,
    getRowIdByIndex,
    findRowIndexById,
    measureFillHandle,
    measureCellRect,
    scrollSelectionIntoView,
    resolveCellFromPoint,
    resolveRowIndexFromPoint,
    invalidateMetrics,
    stopAutoScroll,
    updateAutoScroll,
    globalObject: providedGlobal,
  } = options

  const globalObject: typeof globalThis = providedGlobal ?? (typeof window !== "undefined" ? window : globalThis)
  const frameScheduler = createFrameRequestScheduler({ globalObject })
  const idleScheduler = createIdleCallbackScheduler({ globalObject })

  let pendingFrameHandle: number | null = null
  let pendingIdleHandle: number | null = null

  function requestFrame(callback: () => void) {
    if (pendingFrameHandle !== null) {
      return pendingFrameHandle
    }
    pendingFrameHandle = frameScheduler.request(() => {
      pendingFrameHandle = null
      callback()
    })
    return pendingFrameHandle
  }

  function requestIdle(callback: () => void, timeout?: number) {
    if (pendingIdleHandle !== null) {
      return pendingIdleHandle
    }
    pendingIdleHandle = idleScheduler.request(() => {
      pendingIdleHandle = null
      callback()
    }, { timeout })
    return pendingIdleHandle
  }

  function cancelFrame(handle: number) {
    frameScheduler.cancel(handle)
    if (pendingFrameHandle === handle) {
      pendingFrameHandle = null
    }
  }

  function cancelIdle(handle: number) {
    idleScheduler.cancel(handle)
    if (pendingIdleHandle === handle) {
      pendingIdleHandle = null
    }
  }

  function cancelPending() {
    if (pendingFrameHandle !== null) {
      frameScheduler.cancel(pendingFrameHandle)
      pendingFrameHandle = null
    }
    if (pendingIdleHandle !== null) {
      idleScheduler.cancel(pendingIdleHandle)
      pendingIdleHandle = null
    }
  }

  const environment: SelectionEnvironment<RowKey> = {
    columns: localColumns.value,
    overlays: {
      commit(snapshot: SelectionOverlaySnapshot) {
        const nextRanges = snapshot.ranges as SelectionOverlayRect[]
        const nextActive = snapshot.activeRange as SelectionOverlayRect[]
        const nextFill = snapshot.fillPreview as SelectionOverlayRect[]
        const nextCut = snapshot.cutPreview as SelectionOverlayRect[]
        const nextCursor = snapshot.cursor

        const previousRanges = selectionOverlayRects.value
        const previousActive = activeSelectionOverlayRects.value
        const previousFill = fillPreviewOverlayRects.value
        const previousCut = cutPreviewOverlayRects.value
        const previousCursor = selectionCursorOverlay.value

        selectionOverlayRects.value = nextRanges
        activeSelectionOverlayRects.value = nextActive
        fillPreviewOverlayRects.value = nextFill
        cutPreviewOverlayRects.value = nextCut
        selectionCursorOverlay.value = nextCursor

        if (previousRanges && previousRanges !== nextRanges) {
          releaseOverlayRectArray(previousRanges as SelectionOverlayRect[])
        }
        if (previousActive && previousActive !== nextActive) {
          releaseOverlayRectArray(previousActive as SelectionOverlayRect[])
        }
        if (previousFill && previousFill !== nextFill) {
          releaseOverlayRectArray(previousFill as SelectionOverlayRect[])
        }
        if (previousCut && previousCut !== nextCut) {
          releaseOverlayRectArray(previousCut as SelectionOverlayRect[])
        }
        if (previousCursor && previousCursor !== nextCursor) {
          releaseOverlayRect(previousCursor)
        }
      },
      clear(mode) {
        if (!mode || mode === "ranges") {
          const previousRanges = selectionOverlayRects.value
          const previousActive = activeSelectionOverlayRects.value
          const previousFill = fillPreviewOverlayRects.value
          const previousCut = cutPreviewOverlayRects.value

          selectionOverlayRects.value = acquireOverlayRectArray()
          activeSelectionOverlayRects.value = acquireOverlayRectArray()
          fillPreviewOverlayRects.value = acquireOverlayRectArray()
          cutPreviewOverlayRects.value = acquireOverlayRectArray()

          if (previousRanges) {
            releaseOverlayRectArray(previousRanges as SelectionOverlayRect[])
          }
          if (previousActive) {
            releaseOverlayRectArray(previousActive as SelectionOverlayRect[])
          }
          if (previousFill) {
            releaseOverlayRectArray(previousFill as SelectionOverlayRect[])
          }
          if (previousCut) {
            releaseOverlayRectArray(previousCut as SelectionOverlayRect[])
          }
        }
        if (mode === "active") {
          const previousActive = activeSelectionOverlayRects.value
          activeSelectionOverlayRects.value = acquireOverlayRectArray()
          if (previousActive) {
            releaseOverlayRectArray(previousActive as SelectionOverlayRect[])
          }
        }
        if (!mode || mode === "cursor") {
          const previousCursor = selectionCursorOverlay.value
          selectionCursorOverlay.value = null
          if (previousCursor) {
            releaseOverlayRect(previousCursor)
          }
        }
      },
    },
    measurement: {
      measureFillHandle,
      measureCellRect,
    },
    scheduler: {
      request(callback, options) {
        if (options?.mode === "idle") {
          return requestIdle(callback, options.timeout)
        }
        return requestFrame(callback)
      },
      cancel(handle) {
        if (handle === pendingFrameHandle) {
          cancelFrame(handle)
          return
        }
        if (handle === pendingIdleHandle) {
          cancelIdle(handle)
          return
        }
        frameScheduler.cancel(handle)
        idleScheduler.cancel(handle)
      },
    },
    dom: {
      focusContainer,
      resolveCellElement: (rowIndex, columnKey) => resolveCellElement?.(rowIndex, columnKey) ?? null,
      resolveHeaderCellElement: columnKey => resolveHeaderCellElement?.(columnKey) ?? null,
      getRowIdByIndex,
      findRowIndexById,
      scrollSelectionIntoView,
      resolveCellFromPoint,
      resolveRowIndexFromPoint,
      invalidateMetrics,
    },
  }

  if (stopAutoScroll || updateAutoScroll) {
    environment.autoscroll = {
      stop() {
        stopAutoScroll?.()
      },
      update(pointer: PointerCoordinates) {
        updateAutoScroll?.(pointer)
      },
    }
  }

  const stopColumnsWatch = watch(
    localColumns,
    columns => {
      environment.columns = Array.isArray(columns) ? columns : []
    },
    { immediate: true },
  )

  function dispose() {
    cancelPending()
    frameScheduler.dispose()
    idleScheduler.dispose()
    stopColumnsWatch()
  }

  return {
    environment,
    cancelPending,
    dispose,
  }
}
