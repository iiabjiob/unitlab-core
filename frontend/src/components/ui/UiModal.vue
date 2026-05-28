<template>
  <teleport :to="APP_OVERLAY_HOST_SELECTOR">
    <transition name="fade-modal">
      <div
        v-if="isOpen"
        :class="overlayClasses"
        :style="overlayStyles"
        @click.self="requestClose('backdrop')"
      >
        <transition :name="dialogTransitionName">
          <div
            v-if="isOpen"
            ref="dialogRef"
            :class="dialogClasses"
            :style="dialogStyles"
            role="dialog"
            aria-modal="true"
            tabindex="-1"
            @keydown="onDialogKeydown"
            @focusin.capture="ensureFocusedFieldVisible($event.target)"
            @touchstart.passive="onTouchStart"
            @touchmove="onTouchMove"
            @touchend="onTouchEnd"
          >
            <span class="ui-modal__focus-sentinel" tabindex="0" @focus="loopFocus('end')" />
            <div v-if="isMobile" class="ui-modal__drag-region">
              <div class="ui-modal__drag-handle" />
            </div>
            <div :class="headerClasses">
              <slot name="header">
                <span class="ui-modal__title">{{ title }}</span>
              </slot>
            </div>
            <div ref="contentRef" :class="contentClasses">
              <slot />
            </div>
            <div class="ui-modal__footer">
              <slot name="footer" />
            </div>
            <span class="ui-modal__focus-sentinel" tabindex="0" @focus="loopFocus('start')" />
          </div>
        </transition>
      </div>
    </transition>
  </teleport>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from "vue"
import { createDialogFocusOrchestrator, type DialogCloseReason, useDialogController } from "@affino/dialog-vue"
import { useViewport } from "@/composables/useViewport"
import { APP_OVERLAY_HOST_SELECTOR } from "@/utils/overlayHost"

type ModalMaxWidth = "xl" | "2xl" | "3xl" | "4xl" | "5xl" | "6xl"

const props = defineProps<{
  open: boolean
  title?: string
  maxWidth?: ModalMaxWidth
  desktopHeight?: string
  contentScroll?: boolean
}>()

const emit = defineEmits<{
  (e: "close"): void
}>()

const dialogRef = ref<HTMLDivElement | null>(null)
const { isMobile } = useViewport()
const FOCUSABLE_SELECTOR =
  'a[href], button:not([disabled]), input:not([disabled]), textarea:not([disabled]), select:not([disabled]), [tabindex]:not([tabindex="-1"])'

const dialog = useDialogController({
  focusOrchestrator: createDialogFocusOrchestrator({
    dialog: () => dialogRef.value,
    initialFocus: () => dialogRef.value?.querySelector<HTMLElement>("[data-dialog-initial]") ?? dialogRef.value,
  }),
})

const isOpen = computed(() => dialog.snapshot.value.isOpen)
const startY = ref(0)
const deltaY = ref(0)
const dragging = ref(false)
const keyboardInset = ref(0)
const visualViewportHeight = ref<number | null>(null)
const contentRef = ref<HTMLDivElement | null>(null)
let viewportCleanup: (() => void) | null = null

const overlayClasses = computed(() => (
  isMobile.value
    ? "ui-modal__overlay ui-modal__overlay--mobile"
    : "ui-modal__overlay ui-modal__overlay--desktop"
))

const overlayStyles = computed(() => {
  if (!isMobile.value) return undefined
  return {
    paddingBottom: `${Math.max(0, keyboardInset.value)}px`,
  }
})

const dialogTransitionName = computed(() => (isMobile.value ? "sheet-modal" : "scale-modal"))

const dialogClasses = computed(() => {
  if (isMobile.value) {
    return "ui-modal__dialog ui-modal__dialog--mobile"
  }
  return "ui-modal__dialog ui-modal__dialog--desktop"
})

const dialogStyles = computed(() => {
  if (!isMobile.value) {
    return resolveDesktopDialogStyles(props.maxWidth, props.desktopHeight)
  }
  const inset = Math.max(0, keyboardInset.value)
  const viewportHeight = visualViewportHeight.value
  const maxHeightPx = viewportHeight ? Math.max(300, Math.floor(viewportHeight * 0.92)) : null
  const dragOffset = Math.max(0, deltaY.value)
  return {
    maxHeight: maxHeightPx ? `${maxHeightPx}px` : "78dvh",
    marginBottom: `${Math.max(0, inset * 0.25)}px`,
    ...(dragOffset > 0 ? { transform: `translateY(${dragOffset}px)` } : {}),
  }
})

const headerClasses = computed(() => (
  isMobile.value
    ? "ui-modal__header ui-modal__header--mobile"
    : "ui-modal__header ui-modal__header--desktop"
))
const contentClasses = computed(() => [
  "ui-modal__content",
  props.contentScroll === false ? "ui-modal__content--no-scroll" : null,
])

function resolveDesktopDialogStyles(maxWidth?: ModalMaxWidth, desktopHeight?: string): Record<string, string> {
  const styles: Record<string, string> = {
    maxWidth: "42rem",
  }
  const widths: Record<ModalMaxWidth, string> = {
    xl: "36rem",
    "2xl": "42rem",
    "3xl": "48rem",
    "4xl": "56rem",
    "5xl": "64rem",
    "6xl": "72rem",
  }
  if (maxWidth) {
    styles.maxWidth = widths[maxWidth]
  }
  if (desktopHeight) {
    styles.height = desktopHeight
  }
  return styles
}

function requestClose(reason: DialogCloseReason) {
  void dialog.close(reason).then((closed) => {
    if (closed) {
      emit("close")
    }
  })
}

function onDialogKeydown(e: KeyboardEvent) {
  if (e.defaultPrevented) {
    return
  }
  if (e.key === "Escape" && isOpen.value) {
    requestClose("escape-key")
  }
}

function onTouchStart(e: TouchEvent) {
  if (!isMobile.value) return
  const container = dialogRef.value
  if (!container) return
  const rect = container.getBoundingClientRect()
  dragging.value = e.touches[0].clientY - rect.top <= 64
  if (!dragging.value) return
  startY.value = e.touches[0].clientY
  deltaY.value = 0
}

function onTouchMove(e: TouchEvent) {
  if (!isMobile.value || !dragging.value) return
  e.preventDefault()
  const currentY = e.touches[0].clientY
  deltaY.value = Math.max(0, currentY - startY.value)
}

function onTouchEnd() {
  if (!isMobile.value || !dragging.value) return
  if (deltaY.value > 80) {
    requestClose("pointer")
  }
  dragging.value = false
  deltaY.value = 0
}

function updateKeyboardInset() {
  if (!isMobile.value || !isOpen.value) {
    keyboardInset.value = 0
    return
  }
  const viewport = window.visualViewport
  if (!viewport) {
    keyboardInset.value = 0
    visualViewportHeight.value = null
    return
  }
  visualViewportHeight.value = viewport.height
  const inset = Math.max(0, window.innerHeight - viewport.height - viewport.offsetTop)
  keyboardInset.value = inset
}

function ensureFocusedFieldVisible(target?: EventTarget | null) {
  if (!isMobile.value || !isOpen.value) return
  const content = contentRef.value
  if (!content) return
  const node = target instanceof HTMLElement ? target : (document.activeElement as HTMLElement | null)
  if (!node || !content.contains(node)) return
  requestAnimationFrame(() => {
    node.scrollIntoView({ block: "center", inline: "nearest", behavior: "smooth" })
  })
}

function startViewportTracking() {
  if (typeof window === "undefined") return
  const viewport = window.visualViewport
  const onViewportChange = () => updateKeyboardInset()
  viewport?.addEventListener("resize", onViewportChange)
  viewport?.addEventListener("scroll", onViewportChange)
  window.addEventListener("resize", onViewportChange)
  updateKeyboardInset()
  viewportCleanup = () => {
    viewport?.removeEventListener("resize", onViewportChange)
    viewport?.removeEventListener("scroll", onViewportChange)
    window.removeEventListener("resize", onViewportChange)
    keyboardInset.value = 0
    visualViewportHeight.value = null
  }
}

function stopViewportTracking() {
  viewportCleanup?.()
  viewportCleanup = null
}

function loopFocus(edge: "start" | "end") {
  const container = dialogRef.value
  if (!container) return

  const nodes = Array.from(container.querySelectorAll<HTMLElement>(FOCUSABLE_SELECTOR)).filter((node) => {
    if (node.classList.contains("ui-modal__focus-sentinel")) return false
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

watch(() => props.open, (open) => {
  if (open && !isOpen.value) {
    deltaY.value = 0
    dialog.open("programmatic")
    return
  }
  if (!open && isOpen.value) {
    void dialog.close("programmatic")
  }
}, { immediate: true })

watch([isOpen, isMobile], ([open, mobile]) => {
  if (open && mobile) {
    startViewportTracking()
    return
  }
  stopViewportTracking()
}, { immediate: true })

onBeforeUnmount(() => {
  stopViewportTracking()
})
</script>

<style scoped>
.ui-modal__overlay {
  background: rgb(0 0 0 / 0.5);
  display: flex;
  inset: 0;
  position: fixed;
  z-index: 1000;
}

.ui-modal__overlay--desktop {
  align-items: center;
  justify-content: center;
}

.ui-modal__overlay--mobile {
  align-items: flex-end;
  justify-content: center;
}

.ui-modal__dialog {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-200);
  box-shadow: 0 25px 50px -12px rgb(0 0 0 / 0.25);
  color: var(--color-neutral-900);
  display: flex;
  flex-direction: column;
  outline: none;
  overflow: hidden;
  position: relative;
  width: 100%;
}

.ui-modal__dialog--desktop {
  border-radius: var(--radius-md);
  max-height: 80vh;
}

.ui-modal__dialog--mobile {
  border-bottom: 0;
  border-radius: 1rem 1rem 0 0;
  max-width: none;
}

.ui-modal__drag-region {
  display: flex;
  justify-content: center;
  padding: 0.5rem 0 0.25rem;
}

.ui-modal__drag-handle {
  background: var(--color-neutral-300);
  border-radius: 999px;
  height: 0.375rem;
  width: 2.5rem;
}

.ui-modal__header {
  border-bottom: 1px solid var(--color-neutral-200);
  padding-left: 1.5rem;
  padding-right: 1.5rem;
}

.ui-modal__header--desktop {
  padding-bottom: 1rem;
  padding-top: 1.5rem;
}

.ui-modal__header--mobile {
  padding-bottom: 0.75rem;
  padding-top: 0.5rem;
}

.ui-modal__title {
  font-size: var(--text-base);
  font-weight: 600;
  line-height: 1.5rem;
}

.ui-modal__content {
  flex: 1 1 auto;
  min-height: 0;
  overflow-y: auto;
  padding: 1rem 1.5rem;
}

.ui-modal__content--no-scroll {
  overflow: hidden;
}

.ui-modal__footer {
  border-top: 1px solid var(--color-neutral-200);
  display: flex;
  gap: 0.5rem;
  justify-content: flex-end;
  padding: 1rem 1.5rem;
}

:global(.dark .ui-modal__overlay) {
  background: rgb(0 0 0 / 0.7);
}

:global(.dark .ui-modal__dialog) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-100);
}

:global(.dark .ui-modal__drag-handle) {
  background: var(--color-neutral-700);
}

:global(.dark .ui-modal__header),
:global(.dark .ui-modal__footer) {
  border-color: var(--color-neutral-800);
}

.fade-modal-enter-active,
.fade-modal-leave-active {
  transition: opacity 0.15s;
}
.fade-modal-enter-from,
.fade-modal-leave-to {
  opacity: 0;
}
.fade-modal-enter-to,
.fade-modal-leave-from {
  opacity: 1;
}

.scale-modal-enter-active,
.scale-modal-leave-active {
  transition: transform 0.18s cubic-bezier(0.4,0,0.2,1), opacity 0.18s cubic-bezier(0.4,0,0.2,1);
}
.scale-modal-enter-from,
.scale-modal-leave-to {
  opacity: 0;
  transform: scale(0.96);
}
.scale-modal-enter-to,
.scale-modal-leave-from {
  opacity: 1;
  transform: scale(1);
}

.sheet-modal-enter-active,
.sheet-modal-leave-active {
  transition: transform 220ms ease-out, opacity 220ms ease-out;
}

.sheet-modal-enter-from,
.sheet-modal-leave-to {
  opacity: 0.96;
  transform: translateY(100%);
}

.sheet-modal-enter-to,
.sheet-modal-leave-from {
  opacity: 1;
  transform: translateY(0);
}

.ui-modal__focus-sentinel {
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
