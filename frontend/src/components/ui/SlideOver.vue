<template>
  <teleport :to="APP_OVERLAY_HOST_SELECTOR">
    <div v-if="isOpen" class="slide-over">
      <div
        class="slide-over__backdrop"
        @click="requestClose('backdrop')"
      />

      <transition :name="transitionName">
        <div
          v-if="isSide"
          ref="dialogRef"
          class="slide-over__panel slide-over__panel--side"
          :class="[sideClasses, defaultWidthClass]"
          :style="sideStyles"
          role="dialog"
          aria-modal="true"
          tabindex="-1"
          @keydown="onDialogKeydown"
        >
          <span class="sr-only" tabindex="0" @focus="loopFocus('end')" />
          <div class="slide-over__header slide-over__header--side">
            <span class="slide-over__title">{{ title }}</span>
            <button
              class="slide-over__close"
              aria-label="Close panel"
              @click="requestClose('pointer')"
            >
              <svg xmlns="http://www.w3.org/2000/svg" class="slide-over__close-icon" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2">
                <path stroke-linecap="round" stroke-linejoin="round" d="M6 6l12 12M6 18L18 6"/>
              </svg>
            </button>
          </div>

          <div class="slide-over__content slide-over__content--side" @click="onContentClick">
            <slot />
          </div>
          <span class="sr-only" tabindex="0" @focus="loopFocus('start')" />
        </div>
      </transition>

      <transition name="slide-bottom">
        <div
          v-if="isBottom"
          ref="dialogRef"
          class="slide-over__panel slide-over__panel--bottom"
          :style="bottomStyles"
          role="dialog"
          aria-modal="true"
          tabindex="-1"
          @keydown="onDialogKeydown"
          @touchstart.passive="onTouchStart"
          @touchmove="onTouchMove"
          @touchend="onTouchEnd"
        >
          <span class="sr-only" tabindex="0" @focus="loopFocus('end')" />
          <div class="slide-over__drag-region">
            <div class="slide-over__drag-handle" />
          </div>

          <div class="slide-over__header slide-over__header--bottom">
            <span class="slide-over__title">{{ title }}</span>
            <button
              class="slide-over__close"
              aria-label="Close sheet"
              @click="requestClose('pointer')"
            >
              <svg xmlns="http://www.w3.org/2000/svg" class="slide-over__close-icon" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2">
                <path stroke-linecap="round" stroke-linejoin="round" d="M6 6l12 12M6 18L18 6"/>
              </svg>
            </button>
          </div>

          <div class="slide-over__content slide-over__content--bottom" :style="{ maxHeight: `${maxHeightVh}dvh` }" @click="onContentClick">
            <slot />
          </div>
          <span class="sr-only" tabindex="0" @focus="loopFocus('start')" />
        </div>
      </transition>
    </div>
  </teleport>
</template>

<script setup lang="ts">
import { computed, ref, watch, onBeforeUnmount } from "vue"
import { createDialogFocusOrchestrator, type DialogCloseReason, useDialogController } from "@affino/dialog-vue"
import { APP_OVERLAY_HOST_SELECTOR } from "@/utils/overlayHost"

type Placement = "left" | "right" | "bottom"

const props = withDefaults(defineProps<{
  open: boolean
  placement?: Placement
  title?: string
  /** Side panel width in px (fallback to Tailwind widths if not provided) */
  widthPx?: number
  /** For bottom sheet: max height as percentage of viewport height */
  maxHeightVh?: number
  /** Whether clicking inside content should close (useful for simple menus) */
  closeOnItemClick?: boolean
  /** Whether clicking backdrop closes the panel */
  closeOnBackdrop?: boolean
}>(), {
  placement: "left",
  title: "Menu",
  widthPx: undefined,
  maxHeightVh: 80,
  closeOnItemClick: false,
  closeOnBackdrop: true,
})

const emit = defineEmits<{ (e: "close"): void }>()

const dialogRef = ref<HTMLElement | null>(null)
const FOCUSABLE_SELECTOR =
  'a[href], button:not([disabled]), input:not([disabled]), textarea:not([disabled]), select:not([disabled]), [tabindex]:not([tabindex="-1"])'

const dialog = useDialogController({
  overlayKind: "sheet",
  focusOrchestrator: createDialogFocusOrchestrator({
    dialog: () => dialogRef.value,
    initialFocus: () => dialogRef.value?.querySelector<HTMLElement>("[data-dialog-initial]") ?? dialogRef.value,
  }),
})
const isOpen = computed(() => dialog.snapshot.value.isOpen)

// Derived flags
const isSide = computed(() => props.placement === "left" || props.placement === "right")
const isBottom = computed(() => props.placement === "bottom")

// Transition name based on placement
const transitionName = computed(() => {
  if (props.placement === "left") return "slide-left"
  if (props.placement === "right") return "slide-right"
  return "slide-bottom"
})

// Side panel classes and inline styles
const sideClasses = computed(() => [
  props.placement === "left" ? "slide-over__panel--left" : "slide-over__panel--right",
])
const sideStyles = computed(() => ({
  width: props.widthPx ? `${props.widthPx}px` : undefined,
  // Provide sensible default widths via CSS classes if widthPx not set
  // (Fallback handled via class on container below)
}))
// Add default Tailwind width classes when widthPx is not provided
// (We can't bind classes conditionally by presence cleanly in computed above; do it inline)
const defaultWidthClass = computed(() => (!props.widthPx ? "slide-over__panel--default-width" : ""))

// Bottom sheet drag-to-close logic
const startY = ref(0)
const deltaY = ref(0)
const dragging = ref(false)
const openedAtMs = ref<number>(0)
const bottomStyles = computed(() => {
  const dragOffset = Math.max(0, deltaY.value)
  return {
    bottom: "0",
    ...(dragOffset > 0 ? { transform: `translateY(${dragOffset}px)` } : {}),
  }
})

function onTouchStart(e: TouchEvent) {
  const container = dialogRef.value
  if (!container) return
  const rect = container.getBoundingClientRect()
  dragging.value = e.touches[0].clientY - rect.top <= 64
  if (!dragging.value) return
  startY.value = e.touches[0].clientY
  deltaY.value = 0
}
function onTouchMove(e: TouchEvent) {
  if (!dragging.value) return
  e.preventDefault()
  const currentY = e.touches[0].clientY
  deltaY.value = Math.max(0, currentY - startY.value)
}
function onTouchEnd() {
  if (!dragging.value) return
  // Close if dragged enough, otherwise snap back
  if (deltaY.value > 80) {
    requestClose("pointer")
  }
  dragging.value = false
  deltaY.value = 0
}

function requestClose(reason: DialogCloseReason) {
  if (reason === "backdrop" && !props.closeOnBackdrop) return
  if (reason === "backdrop" && Date.now() - openedAtMs.value < 260) return
  emit("close")
  if (isOpen.value) {
    void dialog.close(reason)
  }
}

function onContentClick(event: MouseEvent) {
  if (!props.closeOnItemClick) return
  const target = event.target as HTMLElement | null
  if (!target) {
    requestClose("pointer")
    return
  }
  if (target.closest("input, label, select, textarea, option")) {
    return
  }
  requestClose("pointer")
}

function onDialogKeydown(e: KeyboardEvent) {
  if (e.key === "Escape" && isOpen.value) {
    requestClose("escape-key")
  }
}

function onWindowKeydown(e: KeyboardEvent) {
  if (e.key !== "Escape" || !isOpen.value) {
    return
  }
  requestClose("escape-key")
}

function loopFocus(edge: "start" | "end") {
  const container = dialogRef.value
  if (!container) return

  const nodes = Array.from(container.querySelectorAll<HTMLElement>(FOCUSABLE_SELECTOR)).filter((node) => {
    if (node.classList.contains("sr-only")) return false
    if (node.getAttribute("aria-hidden") === "true") return false
    return true
  })
  if (!nodes.length) {
    container.focus()
    return
  }

  const target = edge === "start" ? nodes[0] : nodes[nodes.length - 1]
  target?.focus()
}

// Lock body scroll when open (simple version)
function lockScroll(lock: boolean) {
  const body = document.body
  if (lock) {
    body.dataset.prevOverflow = body.style.overflow || ""
    body.style.overflow = "hidden"
  } else {
    body.style.overflow = body.dataset.prevOverflow ?? ""
    delete body.dataset.prevOverflow
  }
}
watch(() => isOpen.value, (v) => lockScroll(v), { immediate: true })

watch(() => isOpen.value, (open) => {
  if (typeof window === "undefined") {
    return
  }
  if (open) {
    window.addEventListener("keydown", onWindowKeydown, true)
    return
  }
  window.removeEventListener("keydown", onWindowKeydown, true)
}, { immediate: true })

watch(() => props.open, (open) => {
  if (open && !isOpen.value) {
    openedAtMs.value = Date.now()
    deltaY.value = 0
    dragging.value = false
    dialog.open("programmatic")
    return
  }
  if (!open && isOpen.value) {
    void dialog.close("programmatic")
  }
}, { immediate: true })

onBeforeUnmount(() => {
  if (typeof window !== "undefined") {
    window.removeEventListener("keydown", onWindowKeydown, true)
  }
  lockScroll(false)
})
</script>

<style scoped>
.slide-over {
  inset: 0;
  position: fixed;
  z-index: 1000;
}

.slide-over__backdrop {
  background: rgb(0 0 0 / 0.5);
  inset: 0;
  position: absolute;
}

.slide-over__panel {
  background: var(--color-white);
  color: var(--color-neutral-900);
  position: absolute;
}

.slide-over__panel--side {
  border-color: var(--color-neutral-200);
  box-shadow: 0 20px 25px -5px rgb(0 0 0 / 0.1), 0 8px 10px -6px rgb(0 0 0 / 0.1);
  height: 100%;
  top: 0;
}

.slide-over__panel--left {
  border-right: 1px solid var(--color-neutral-200);
  left: 0;
}

.slide-over__panel--right {
  border-left: 1px solid var(--color-neutral-200);
  right: 0;
}

.slide-over__panel--default-width {
  width: 20rem;
}

.slide-over__panel--bottom {
  border-radius: 1rem 1rem 0 0;
  border-top: 1px solid var(--color-neutral-200);
  box-shadow: 0 25px 50px -12px rgb(0 0 0 / 0.25);
  left: 0;
  right: 0;
}

.slide-over__header {
  align-items: center;
  display: flex;
  justify-content: space-between;
}

.slide-over__header--side {
  border-bottom: 1px solid var(--color-neutral-200);
  padding: 0.5rem 0.75rem;
}

.slide-over__header--bottom {
  padding: 0.5rem 1rem;
}

.slide-over__title {
  font-size: var(--text-sm);
  font-weight: 600;
  line-height: 1.25rem;
}

.slide-over__close {
  align-items: center;
  border-radius: 0.5rem;
  display: inline-flex;
  height: 2rem;
  justify-content: center;
  width: 2rem;
}

.slide-over__close:hover {
  background: var(--color-neutral-100);
}

.slide-over__close-icon {
  height: 1rem;
  width: 1rem;
}

.slide-over__content {
  overflow-y: auto;
}

.slide-over__content--side {
  height: calc(100% - 2.5rem);
}

.slide-over__content--bottom {
  padding: 0 1rem 1rem;
}

.slide-over__drag-region {
  display: flex;
  justify-content: center;
  padding: 0.5rem 0 0.25rem;
}

.slide-over__drag-handle {
  background: var(--color-neutral-300);
  border-radius: 999px;
  height: 0.375rem;
  width: 2.5rem;
}

.dark .slide-over__backdrop {
  background: rgb(0 0 0 / 0.7);
}

.dark .slide-over__panel {
  background: var(--color-neutral-800);
  color: var(--color-neutral-100);
}

.dark .slide-over__panel--side,
.dark .slide-over__panel--left,
.dark .slide-over__panel--right,
.dark .slide-over__panel--bottom,
.dark .slide-over__header--side {
  border-color: var(--color-neutral-800);
}

.dark .slide-over__close:hover {
  background: var(--color-neutral-800);
}

.dark .slide-over__drag-handle {
  background: var(--color-neutral-700);
}

@media (min-width: 768px) {
  .slide-over__panel--default-width {
    width: 24rem;
  }
}

/* Slide from LEFT */
.slide-left-enter-from,
.slide-left-leave-to {
  transform: translateX(-100%);
}
.slide-left-enter-active,
.slide-left-leave-active {
  transition: transform 200ms ease-out;
}
.slide-left-enter-to,
.slide-left-leave-from {
  transform: translateX(0);
}

/* Slide from RIGHT */
.slide-right-enter-from,
.slide-right-leave-to {
  transform: translateX(100%);
}
.slide-right-enter-active,
.slide-right-leave-active {
  transition: transform 200ms ease-out;
}
.slide-right-enter-to,
.slide-right-leave-from {
  transform: translateX(0);
}

/* Slide from BOTTOM */
.slide-bottom-enter-from,
.slide-bottom-leave-to {
  transform: translateY(100%);
}
.slide-bottom-enter-active,
.slide-bottom-leave-active {
  transition: transform 220ms ease-out;
}
.slide-bottom-enter-to,
.slide-bottom-leave-from {
  transform: translateY(0);
}

.sr-only {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip: rect(0 0 0 0);
  white-space: nowrap;
  border: 0;
}
</style>
