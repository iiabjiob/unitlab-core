<template>
  <div ref="overlayRef" class="ui-table__overlay-layer">
    <div ref="viewportRef" class="ui-table__overlay-viewport">
      <slot />

      <div ref="selectionGroupRef"></div>
      <div ref="activeSelectionGroupRef"></div>
      <div ref="fillPreviewGroupRef"></div>
      <div ref="cutPreviewGroupRef"></div>

      <div ref="cursorRef"
           class="ui-table__overlay-rect ui-table__overlay-selection-cursor"></div>

      <div ref="fillHandleRef"
           class="ui-table__fill-handle ui-table__overlay-interactive"
           @mousedown.prevent.stop="handleFillDrag"
           @dblclick.prevent.stop="handleAutoFill">
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { onMounted, onBeforeUnmount, ref } from "vue"
import type {
  UiTableOverlayHandle,
  UiTableOverlayRect,
  UiTableOverlayRectGroups,
  UiTableOverlayTransformInput,
} from "../types/overlay"
import type { FillHandleStylePayload } from "@/ui-table/core/selection/fillHandleStylePool"

const props = defineProps<{
  startFillDrag?: (e: MouseEvent) => void
  autoFillDown?: (e: MouseEvent) => void
}>()

/* DOM refs */
const overlayRef = ref<HTMLDivElement | null>(null)
const viewportRef = ref<HTMLDivElement | null>(null)

const selectionGroupRef = ref<HTMLDivElement | null>(null)
const activeSelectionGroupRef = ref<HTMLDivElement | null>(null)
const fillPreviewGroupRef = ref<HTMLDivElement | null>(null)
const cutPreviewGroupRef = ref<HTMLDivElement | null>(null)
const cursorRef = ref<HTMLDivElement | null>(null)
const fillHandleRef = ref<HTMLDivElement | null>(null)

/* Latest data */
let latestRects: UiTableOverlayRectGroups | null = null
let latestTransform: UiTableOverlayTransformInput | null = null
let latestScroll: { scrollTop: number; scrollLeft: number } | null = null
let latestFillStyle: FillHandleStylePayload | null = null

/* Dirty flags */
let dirtyRects = false
let dirtyTransform = false
let dirtyScroll = false
let dirtyFill = false

let rafId: number | null = null

/* Utils */
function translate3d(x: number, y: number) {
  return `translate3d(${x}px, ${y}px, 0)`
}

function scheduleFlush() {
  if (rafId != null) return
  rafId = requestAnimationFrame(() => {
    rafId = null
    flush()
  })
}

/* --- Rendering --- */

function applyViewportTransform(transform: UiTableOverlayTransformInput | null) {
  const vp = viewportRef.value
  if (!vp || !transform) return

  vp.style.width = `${transform.viewportWidth}px`
  vp.style.height = `${transform.viewportHeight}px`
  // pinned transforms are applied inside rects, not viewport
}

function applyScrollTransform(scroll: { scrollTop: number; scrollLeft: number } | null) {
  const vp = viewportRef.value
  if (!vp || !scroll) return
  vp.style.transform = translate3d(-scroll.scrollLeft, -scroll.scrollTop)
}

function applyRects(rects: UiTableOverlayRectGroups | null, transform: UiTableOverlayTransformInput | null) {
  if (!rects || !transform) return

  const pinnedLeft = transform.pinnedLeftTranslateX
  const pinnedRight = transform.pinnedRightTranslateX

  function drawGroup(
    target: HTMLDivElement | null,
    items: readonly UiTableOverlayRect[] | undefined
  ) {
    if (!target) return
    target.innerHTML = ""
    if (!items) return

    for (const r of items) {
      const el = document.createElement("div")
      el.className = "ui-table__overlay-rect"
      if (r.active) {
        el.classList.add("ui-table__overlay-selection-range--active")
      }

      let x = r.left
      if (r.pin === "left") x += pinnedLeft
      if (r.pin === "right") x += pinnedRight

      el.style.transform = translate3d(x, r.top)
      el.style.width = `${r.width}px`
      el.style.height = `${r.height}px`
      target.appendChild(el)
    }
  }

  drawGroup(selectionGroupRef.value, rects.selection)
  drawGroup(activeSelectionGroupRef.value, rects.activeSelection)
  drawGroup(fillPreviewGroupRef.value, rects.fillPreview)
  drawGroup(cutPreviewGroupRef.value, rects.cutPreview)

  const cursor = cursorRef.value
  if (cursor) {
    if (!rects.cursor) {
      cursor.style.visibility = "hidden"
    } else {
      cursor.style.visibility = "visible"
      let x = rects.cursor.left
      if (rects.cursor.pin === "left") x += pinnedLeft
      if (rects.cursor.pin === "right") x += pinnedRight
      cursor.style.transform = translate3d(x, rects.cursor.top)
      cursor.style.width = `${rects.cursor.width}px`
      cursor.style.height = `${rects.cursor.height}px`
    }
  }
}

function applyFillHandle(style: FillHandleStylePayload | null) {
  const el = fillHandleRef.value
  if (!el) return

  if (!style) {
    el.style.visibility = "hidden"
    return
  }

  el.style.visibility = "visible"
  el.style.transform = translate3d(style.x, style.y)
  el.style.width = `${style.widthValue}px`
  el.style.height = `${style.heightValue}px`
}

function flush() {
  if (dirtyTransform) {
    applyViewportTransform(latestTransform)
  }
  if (dirtyScroll) {
    applyScrollTransform(latestScroll)
  }
  if (dirtyRects || dirtyTransform || dirtyScroll) {
    applyRects(latestRects, latestTransform)
  }
  if (dirtyFill) {
    applyFillHandle(latestFillStyle)
  }

  dirtyRects = dirtyTransform = dirtyScroll = dirtyFill = false
}

/* --- Exposed API for adapter --- */

function updateRects(payload: UiTableOverlayRectGroups) {
  latestRects = payload
  dirtyRects = true
  scheduleFlush()
}

function updateTransforms(snapshot: UiTableOverlayTransformInput) {
  latestTransform = snapshot
  dirtyTransform = true
  scheduleFlush()
}

function updateScrollTransform(scrollTop: number, scrollLeft: number) {
  latestScroll = { scrollTop, scrollLeft }
  dirtyScroll = true
  scheduleFlush()
}

function updateFillHandleStyle(style: FillHandleStylePayload | null) {
  latestFillStyle = style
  dirtyFill = true
  scheduleFlush()
}

function handleFillDrag(e: MouseEvent) {
  props.startFillDrag?.(e)
}

function handleAutoFill(e: MouseEvent) {
  props.autoFillDown?.(e)
}

/* Expose */
const api: UiTableOverlayHandle = {
  overlayRef: overlayRef.value,
  viewportRef: viewportRef.value,
  updateRects,
  updateTransforms,
  updateScrollTransform,
  updateFillHandleStyle,
}

defineExpose(api)

/* Lifecycle */
onMounted(() => scheduleFlush())
onBeforeUnmount(() => {
  if (rafId != null) cancelAnimationFrame(rafId)
})
</script>
