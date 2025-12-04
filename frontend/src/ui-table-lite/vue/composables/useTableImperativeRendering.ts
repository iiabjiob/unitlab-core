import { computed, onBeforeUnmount, ref, watch } from "vue"
import type { Ref, WatchSource } from "vue"
import type { UiTableBodyBindings } from "../context.js"
import type { HeaderSnapshot } from "../../core/imperative/engine.js"
import type { RowPoolItem } from "./useTableViewport.js"
import type { TableViewportState, ImperativeRowUpdatePayload } from "../../core/viewport/tableViewportController.js"
import { useImperativeTableAdapter } from "../imperative/useImperativeTableAdapter.js"
import type { ImperativeBodyController } from "../imperative/useImperativeTableAdapter.js"
import { createRafScheduler } from "../../core/runtime/rafScheduler.js"
import type { ImperativeContainerMap } from "../../core/imperative/bindings.js"

interface SelectionWatchSources {
  selectionRanges: WatchSource<unknown>
  fillPreviewRange: WatchSource<unknown>
  cutPreviewRanges: WatchSource<unknown>
  isDraggingSelection: WatchSource<boolean>
  isFillDragging: WatchSource<boolean>
  fullRowSelection: WatchSource<unknown>
  selectedCell: WatchSource<unknown>
  anchorCell: WatchSource<unknown>
}

export interface UseTableImperativeRenderingOptions {
  bodyBindings: UiTableBodyBindings
  headerSnapshot: () => HeaderSnapshot
  lastContainer: ImperativeContainerMap | null
  visibleRowsPool: Ref<RowPoolItem[]>
  rowPoolVersion: Ref<number>
  startIndex: Ref<number>
  endIndex: Ref<number>
  viewportState: Ref<TableViewportState>
  totalRowCount: Ref<number>
  effectiveRowHeight: Ref<number>
  scrollTop: Ref<number>
  viewportHeight: Ref<number>
  refresh: (force?: boolean) => void
  selectionSources: SelectionWatchSources
  rowSelectionSelectedKeys: WatchSource<Set<unknown>>
  findReplaceMatches: WatchSource<unknown[]>
  findReplaceActiveMatchKey: WatchSource<string | null>
  isEditingCell: WatchSource<boolean>
}

export interface UseTableImperativeRenderingResult {
  controller: ImperativeBodyController
  requestImperativeHighlightRefresh: (immediate?: boolean) => void
}

export function useTableImperativeRendering(options: UseTableImperativeRenderingOptions): UseTableImperativeRenderingResult {
  const imperativeMode = computed(() => true)
  const controller = useImperativeTableAdapter({
    body: options.bodyBindings,
    mode: imperativeMode,
    headerSnapshot: options.headerSnapshot,
  })

  if (options.lastContainer) {
    controller.registerContainer(options.lastContainer)
  }

  const scheduler = createRafScheduler()
  let pendingHighlightTask: number | null = null

  const cancelPendingHighlight = () => {
    if (pendingHighlightTask !== null) {
      scheduler.cancel(pendingHighlightTask)
      pendingHighlightTask = null
    }
  }

  const runHighlightRefresh = () => {
    pendingHighlightTask = null

    const viewportState = options.viewportState.value
    const payload: ImperativeRowUpdatePayload = {
      pool: options.visibleRowsPool.value,
      startIndex: options.startIndex.value,
      endIndex: options.endIndex.value,
      visibleCount: viewportState.visibleCount,
      totalRowCount: options.totalRowCount.value,
      rowHeight: options.effectiveRowHeight.value,
      scrollTop: options.scrollTop.value,
      viewportHeight: options.viewportHeight.value,
      overscanLeading: viewportState.overscanLeading,
      overscanTrailing: viewportState.overscanTrailing,
      timestamp: typeof performance !== "undefined" ? performance.now() : Date.now(),
      version: options.rowPoolVersion.value,
    }
    controller.handleRows(payload)
  }

  const requestImperativeHighlightRefresh = (immediate = false) => {
    if (immediate) {
      cancelPendingHighlight()
      scheduler.runNow(runHighlightRefresh)
      return
    }
    if (pendingHighlightTask !== null) return
    pendingHighlightTask = scheduler.schedule(runHighlightRefresh, { priority: "high" })
  }

  const refreshToken = ref(0)
  const invalidateImperativeHighlight = () => {
    refreshToken.value = (refreshToken.value + 1) % Number.MAX_SAFE_INTEGER
  }

  const refreshSources: WatchSource<unknown>[] = [
    options.selectionSources.selectionRanges,
    options.selectionSources.fillPreviewRange,
    options.selectionSources.cutPreviewRanges,
    options.selectionSources.isDraggingSelection,
    options.selectionSources.isFillDragging,
    options.selectionSources.fullRowSelection,
    options.selectionSources.selectedCell,
    options.selectionSources.anchorCell,
    options.rowSelectionSelectedKeys,
    options.findReplaceMatches,
    options.findReplaceActiveMatchKey,
    options.isEditingCell,
  ]

  watch(refreshSources, invalidateImperativeHighlight, { deep: true, flush: "post" })
  watch(refreshToken, () => requestImperativeHighlightRefresh(), { flush: "post" })

  watch(
    () => options.rowPoolVersion.value,
    () => requestImperativeHighlightRefresh(),
    { flush: "post" },
  )

  options.refresh(true)
  requestImperativeHighlightRefresh(true)

  onBeforeUnmount(() => {
    cancelPendingHighlight()
    scheduler.dispose()
  })

  return {
    controller,
    requestImperativeHighlightRefresh,
  }
}
