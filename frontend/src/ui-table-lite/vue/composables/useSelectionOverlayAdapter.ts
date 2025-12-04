import { onScopeDispose, shallowRef, watch, type Ref } from "vue"
import type {
  UiTableOverlayHandle,
  UiTableOverlayRect,
  UiTableOverlayRectGroups,
  UiTableOverlayTransformInput,
} from "../types/overlay"
import { releaseFillHandleStyle, type FillHandleStylePayload } from "@/ui-table/core/selection/fillHandleStylePool"
import type { TableOverlayScrollEmitter, TableOverlayScrollSnapshot } from "./useTableOverlayScrollState"

interface SelectionOverlayViewportState {
  width: number
  height: number
  scrollLeft: number
  scrollTop: number
}

export interface UseSelectionOverlayAdapterOptions {
  overlayComponentRef: Ref<UiTableOverlayHandle | null>
  selectionRects: Ref<UiTableOverlayRect[]>
  activeSelectionRects: Ref<UiTableOverlayRect[]>
  fillPreviewRects: Ref<UiTableOverlayRect[]>
  cutPreviewRects: Ref<UiTableOverlayRect[]>
  cursorRect: Ref<UiTableOverlayRect | null>
  fillHandleStyle: Ref<FillHandleStylePayload | null>
  overlayViewport: Ref<SelectionOverlayViewportState>
  pinnedLeftOffset: Ref<number>
  pinnedRightOffset: Ref<number>
  overlayScrollState?: TableOverlayScrollEmitter
}

export interface SelectionOverlayAdapterHandle {
  setRects(payload: UiTableOverlayRectGroups): void
  setTransform(snapshot: UiTableOverlayTransformInput): void
  applyScrollTransform(scrollTop: number, scrollLeft: number): void
  setFillHandleStyle(style: FillHandleStylePayload | null | undefined): void
  refresh(): void
  getLastRects(): UiTableOverlayRectGroups | null
  getLastTransform(): UiTableOverlayTransformInput | null
}

function cloneTransform(input: UiTableOverlayTransformInput): UiTableOverlayTransformInput {
  return {
    viewportWidth: input.viewportWidth,
    viewportHeight: input.viewportHeight,
    pinnedLeftTranslateX: input.pinnedLeftTranslateX,
    pinnedRightTranslateX: input.pinnedRightTranslateX,
  }
}

function normalizeViewportSnapshot(snapshot: SelectionOverlayViewportState | undefined): SelectionOverlayViewportState {
  if (!snapshot) {
    return { width: 0, height: 0, scrollLeft: 0, scrollTop: 0 }
  }
  const width = Number.isFinite(snapshot.width) ? Math.max(0, snapshot.width) : 0
  const height = Number.isFinite(snapshot.height) ? Math.max(0, snapshot.height) : 0
  const scrollLeft = Number.isFinite(snapshot.scrollLeft) ? snapshot.scrollLeft : 0
  const scrollTop = Number.isFinite(snapshot.scrollTop) ? snapshot.scrollTop : 0
  return { width, height, scrollLeft, scrollTop }
}

function buildTransform(
  viewportState: SelectionOverlayViewportState,
  pinnedLeftOffset: number,
  pinnedRightOffset: number,
): UiTableOverlayTransformInput {
  const { width, height, scrollLeft } = viewportState
  return {
    viewportWidth: width,
    viewportHeight: height,
    pinnedLeftTranslateX: scrollLeft + pinnedLeftOffset,
    pinnedRightTranslateX: scrollLeft - pinnedRightOffset,
  }
}

export function useSelectionOverlayAdapter(options: UseSelectionOverlayAdapterOptions): SelectionOverlayAdapterHandle {
  const rectSnapshot: UiTableOverlayRectGroups = {
    selection: undefined,
    activeSelection: undefined,
    fillPreview: undefined,
    cutPreview: undefined,
    cursor: null,
  }
  const cachedRects = shallowRef<UiTableOverlayRectGroups | null>(rectSnapshot)
  const cachedTransform = shallowRef<UiTableOverlayTransformInput | null>(null)
  const cachedFillHandle = shallowRef<FillHandleStylePayload | null | undefined>(undefined)
  const cachedScroll = shallowRef<{ scrollTop: number; scrollLeft: number } | null>(null)

  function applyRects(handle: UiTableOverlayHandle | null, payload: UiTableOverlayRectGroups | null) {
    if (!handle || !payload) {
      return
    }
    handle.updateRects(payload)
  }

  function applyTransform(handle: UiTableOverlayHandle | null, snapshot: UiTableOverlayTransformInput | null) {
    if (!handle || !snapshot) {
      return
    }
    handle.updateTransforms(snapshot)
  }

  function applyScroll(handle: UiTableOverlayHandle | null, scroll: { scrollTop: number; scrollLeft: number } | null) {
    if (!handle || !scroll) {
      return
    }
    handle.updateScrollTransform(scroll.scrollTop, scroll.scrollLeft)
  }

  function applyFillHandleStyle(handle: UiTableOverlayHandle | null, style: FillHandleStylePayload | null | undefined) {
    if (!handle) {
      return
    }
    handle.updateFillHandleStyle(style)
  }

  function refresh() {
    const handle = options.overlayComponentRef.value
    if (!handle) {
      return
    }
    if (cachedTransform.value) {
      handle.updateTransforms(cachedTransform.value)
    }
    if (cachedScroll.value) {
      handle.updateScrollTransform(cachedScroll.value.scrollTop, cachedScroll.value.scrollLeft)
    }
    if (cachedRects.value) {
      handle.updateRects(cachedRects.value)
    }
    if (cachedFillHandle.value !== undefined) {
      handle.updateFillHandleStyle(cachedFillHandle.value ?? null)
    }
  }

  function updateRectSources(
    selection: readonly UiTableOverlayRect[] | undefined,
    activeSelection: readonly UiTableOverlayRect[] | undefined,
    fillPreview: readonly UiTableOverlayRect[] | undefined,
    cutPreview: readonly UiTableOverlayRect[] | undefined,
    cursor: UiTableOverlayRect | null | undefined,
  ) {
    rectSnapshot.selection = selection && selection.length ? selection : undefined
    rectSnapshot.activeSelection = activeSelection && activeSelection.length ? activeSelection : undefined
    rectSnapshot.fillPreview = fillPreview && fillPreview.length ? fillPreview : undefined
    rectSnapshot.cutPreview = cutPreview && cutPreview.length ? cutPreview : undefined
    rectSnapshot.cursor = cursor ?? null
    cachedRects.value = rectSnapshot
    applyRects(options.overlayComponentRef.value, rectSnapshot)
  }

  function setRects(payload: UiTableOverlayRectGroups) {
    updateRectSources(
      payload.selection,
      payload.activeSelection,
      payload.fillPreview,
      payload.cutPreview,
      payload.cursor,
    )
  }

  function setTransform(snapshot: UiTableOverlayTransformInput) {
    cachedTransform.value = cloneTransform(snapshot)
    applyTransform(options.overlayComponentRef.value, cachedTransform.value)
  }

  function setScrollTransform(scrollTop: number, scrollLeft: number) {
    const normalizedTop = Number.isFinite(scrollTop) ? scrollTop : 0
    const normalizedLeft = Number.isFinite(scrollLeft) ? scrollLeft : 0
    const previous = cachedScroll.value
    if (previous && previous.scrollTop === normalizedTop && previous.scrollLeft === normalizedLeft) {
      return
    }
    const snapshot = { scrollTop: normalizedTop, scrollLeft: normalizedLeft }
    cachedScroll.value = snapshot
    applyScroll(options.overlayComponentRef.value, snapshot)
  }

  function setFillHandleStyle(style: FillHandleStylePayload | null | undefined) {
    const nextStyle = style ?? null
    const previous = cachedFillHandle.value
    if (previous && previous !== nextStyle) {
      releaseFillHandleStyle(previous)
    }
    cachedFillHandle.value = nextStyle
    applyFillHandleStyle(options.overlayComponentRef.value, cachedFillHandle.value ?? null)
  }

  watch(
    () => options.overlayComponentRef.value,
    handle => {
      if (handle) {
        refresh()
      }
    },
    { flush: "sync" },
  )

  watch(
    [
      () => options.selectionRects.value,
      () => options.activeSelectionRects.value,
      () => options.fillPreviewRects.value,
      () => options.cutPreviewRects.value,
      () => options.cursorRect.value,
    ],
    ([selection, activeSelection, fillPreview, cutPreview, cursor]) => {
      updateRectSources(selection, activeSelection, fillPreview, cutPreview, cursor)
    },
    { flush: "sync" },
  )

  watch(
    () => options.fillHandleStyle.value,
    style => {
      setFillHandleStyle(style ?? null)
    },
    { flush: "sync" },
  )

  if (options.overlayScrollState) {
    const unsubscribe = options.overlayScrollState.subscribe((snapshot: TableOverlayScrollSnapshot) => {
      const viewportState: SelectionOverlayViewportState = {
        width: snapshot.viewportWidth,
        height: snapshot.viewportHeight,
        scrollLeft: snapshot.scrollLeft,
        scrollTop: snapshot.scrollTop,
      }
      const transform = buildTransform(
        viewportState,
        snapshot.pinnedOffsetLeft,
        snapshot.pinnedOffsetRight,
      )
      setTransform(transform)
      setScrollTransform(viewportState.scrollTop, viewportState.scrollLeft)
    })
    onScopeDispose(() => {
      unsubscribe()
    })
  } else {
    watch(
      () => ({
        viewport: normalizeViewportSnapshot(options.overlayViewport.value),
        pinnedLeftOffset: Number.isFinite(options.pinnedLeftOffset.value)
          ? options.pinnedLeftOffset.value
          : 0,
        pinnedRightOffset: Number.isFinite(options.pinnedRightOffset.value)
          ? options.pinnedRightOffset.value
          : 0,
      }),
      ({ viewport, pinnedLeftOffset, pinnedRightOffset }) => {
        const transform = buildTransform(viewport, pinnedLeftOffset, pinnedRightOffset)
        setTransform(transform)
        setScrollTransform(viewport.scrollTop, viewport.scrollLeft)
      },
      { flush: "sync" },
    )
  }

  onScopeDispose(() => {
    if (cachedFillHandle.value) {
      releaseFillHandleStyle(cachedFillHandle.value)
      cachedFillHandle.value = null
    }
  })

  return {
    setRects,
    setTransform,
    applyScrollTransform: setScrollTransform,
    setFillHandleStyle,
    refresh,
    getLastRects: () => cachedRects.value,
    getLastTransform: () => cachedTransform.value,
  }
}
