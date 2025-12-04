<template>
  <div class="ui-table__viewport-wrapper" :class="viewportWrapperClass">
    <div ref="layoutRef" class="ui-table__layout">
      <div
        ref="splitViewportRef"
        class="ui-table__viewport ui-table__container ui-table__scroll-container ui-table__scroll-container--simple"
        :class="tableContainerClass"
        data-testid="table-container"
        role="grid"
        :aria-rowcount="ariaRowCount"
        :aria-colcount="ariaColCount"
        aria-multiselectable="true"
        tabindex="0"
        @focusin="onGridFocusIn"
        @focusout="onGridFocusOut"
        @keydown="handleKeydown"
        @wheel.passive.stop="handleWheel"
        @scroll="handleBodyScroll"
      >
        <div class="ui-table__header-layout ui-table__header-layout--simple">
          <div class="ui-table__header-section ui-table__header-section--main">
            <div
              ref="headerMainRef"
              class="ui-table__header-main ui-table__header-surface ui-table__header-surface--main"
            >
              <div
                ref="headerMainContentRef"
                class="ui-table__header-main-content ui-table__header-surface-content"
              >
                <slot name="header-main" />
              </div>
            </div>
          </div>
        </div>

        <div
          ref="viewportContentRef"
          class="ui-table__viewport-content ui-table__viewport-content--simple"
        >
          <div ref="rowsLayerRef" class="ui-table__rows-layer ui-table__rows-layer--simple">
            <div class="ui-table__body-section ui-table__body-section--main">
              <div
                ref="bodyMainSurfaceRef"
                class="ui-table__body-surface ui-table__body-surface--main"
                @focusin="onGridFocusIn"
                @focusout="onGridFocusOut"
                @keydown="handleKeydown"
                @wheel.passive.stop="handleWheel"
              >
                <div
                  ref="bodyMainContentRef"
                  class="ui-table__body-main-content ui-table__body-surface-content"
                >
                  <slot name="body-main" />
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, watchEffect, onBeforeUnmount, watch, toRef } from "vue"
import type { UiTableExposeBindings } from "../context"
import { createWheelAccumulator } from "../../../ui-table/vue/utils/createWheelAccumulator"

const props = defineProps<{
  viewportWrapperClass?: string | string[] | Record<string, boolean>
  tableContainerClass?: string | string[] | Record<string, boolean>
  ariaRowCount: number
  ariaColCount: number
  containerRef: HTMLDivElement | null
  exposedApi?: UiTableExposeBindings
}>()

const viewportWrapperClass = toRef(props, "viewportWrapperClass")
const tableContainerClass = toRef(props, "tableContainerClass")
const ariaRowCount = toRef(props, "ariaRowCount")
const ariaColCount = toRef(props, "ariaColCount")
const externalContainerRef = toRef(props, "containerRef")

const emit = defineEmits<{
  (event: "focus-in", payload: FocusEvent): void
  (event: "focus-out", payload: FocusEvent): void
  (event: "keydown", payload: KeyboardEvent): void
  (event: "wheel", payload: WheelEvent): void
  (event: "scroll", payload: Event): void
  (event: "update:containerRef", payload: HTMLDivElement | null): void
}>()

const splitViewportRef = ref<HTMLDivElement | null>(null)
const layoutRef = ref<HTMLDivElement | null>(null)
const headerMainRef = ref<HTMLDivElement | null>(null)
const headerMainContentRef = ref<HTMLDivElement | null>(null)
const bodyMainSurfaceRef = ref<HTMLDivElement | null>(null)
const bodyMainContentRef = ref<HTMLDivElement | null>(null)
const viewportContentRef = ref<HTMLDivElement | null>(null)
const rowsLayerRef = ref<HTMLDivElement | null>(null)

type ViewportExpose = {
  clampScrollTopValue?: (value: number) => number
}

const overlayLayerRef = computed<HTMLDivElement | null>(() => null)
const exposedViewport = computed<ViewportExpose | null>(() => {
  const api = props.exposedApi as { viewport?: ViewportExpose } | undefined
  return api?.viewport ?? null
})
const clampViewportScrollTop = computed(() => exposedViewport.value?.clampScrollTopValue ?? null)

const activeContainerRef = computed<HTMLDivElement | null>(() => splitViewportRef.value)

const wheelAccumulator = createWheelAccumulator({
  requestFrame:
    typeof window !== "undefined" && typeof window.requestAnimationFrame === "function"
      ? window.requestAnimationFrame.bind(window)
      : undefined,
  cancelFrame:
    typeof window !== "undefined" && typeof window.cancelAnimationFrame === "function"
      ? window.cancelAnimationFrame.bind(window)
      : undefined,
  apply: ({ deltaX, deltaY }) => {
    const viewport = splitViewportRef.value
    if (!viewport || (deltaX === 0 && deltaY === 0)) {
      return
    }

    const maxScrollLeft = Math.max(0, viewport.scrollWidth - viewport.clientWidth)
    const maxScrollTop = Math.max(0, viewport.scrollHeight - viewport.clientHeight)

    const nextScrollLeft = viewport.scrollLeft + deltaX
    const nextScrollTop = viewport.scrollTop + deltaY

    const clampScrollTop = clampViewportScrollTop.value
    const clampedScrollTop = clampScrollTop
      ? clampScrollTop(nextScrollTop)
      : Math.max(0, Math.min(maxScrollTop, nextScrollTop))
    const clampedScrollLeft = Math.max(0, Math.min(maxScrollLeft, nextScrollLeft))

    if (clampedScrollLeft !== viewport.scrollLeft) {
      viewport.scrollLeft = clampedScrollLeft
    }
    if (clampedScrollTop !== viewport.scrollTop) {
      viewport.scrollTop = clampedScrollTop
    }
  },
})

watchEffect(() => {
  emit("update:containerRef", activeContainerRef.value)
})

watch(
  () => externalContainerRef.value,
  container => {
    if (container !== splitViewportRef.value) {
      splitViewportRef.value = container
    }
  },
)

onBeforeUnmount(() => {
  emit("update:containerRef", null)
  wheelAccumulator.cancel()
})

function onGridFocusIn(event: FocusEvent) {
  emit("focus-in", event)
}

function onGridFocusOut(event: FocusEvent) {
  emit("focus-out", event)
}

function handleKeydown(event: KeyboardEvent) {
  if (event.defaultPrevented) {
    return
  }
  emit("keydown", event)
}

function handleWheel(event: WheelEvent) {
  emit("wheel", event)

  const viewport = splitViewportRef.value
  if (!viewport) {
    return
  }

  if (event.ctrlKey || event.metaKey) {
    return
  }

  const currentTarget = event.currentTarget as HTMLElement | null
  if (!currentTarget || currentTarget === viewport) {
    return
  }

  const scaleDelta = (delta: number) => {
    if (event.deltaMode === WheelEvent.DOM_DELTA_LINE) {
      return delta * 16
    }
    if (event.deltaMode === WheelEvent.DOM_DELTA_PAGE) {
      return delta * viewport.clientHeight
    }
    return delta
  }

  let deltaX = 0
  let deltaY = 0

  if (event.shiftKey) {
    if (event.deltaY !== 0) {
      deltaX += scaleDelta(event.deltaY)
    }
    if (event.deltaX !== 0) {
      deltaX += scaleDelta(event.deltaX)
    }
  } else {
    if (event.deltaY !== 0) {
      deltaY += scaleDelta(event.deltaY)
    }

    if (event.deltaX !== 0) {
      deltaX += scaleDelta(event.deltaX)
    }
  }

  if (deltaX !== 0 || deltaY !== 0) {
    wheelAccumulator.queue(deltaX, deltaY)
  }
}

function handleBodyScroll(event: Event) {
  emit("scroll", event)
}

defineExpose({
  containerRef: activeContainerRef,
  headerMainRef,
  headerMainContentRef,
  bodyMainSurfaceRef,
  bodyMainContentRef,
  viewportContentRef,
  rowsLayerRef,
  splitViewportRef,
  layoutRef,
  overlayRef: overlayLayerRef,
  ...(props.exposedApi ?? {}),
})
</script>
