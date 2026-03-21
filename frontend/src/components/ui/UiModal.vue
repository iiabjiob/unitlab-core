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
            <span class="sr-only" tabindex="0" @focus="loopFocus('end')" />
            <div v-if="isMobile" class="pt-2 pb-1 flex justify-center">
              <div class="h-1.5 w-10 rounded-full bg-neutral-300 dark:bg-neutral-700" />
            </div>
            <div :class="headerClasses">
              <slot name="header">
                <span class="text-base font-semibold">{{ title }}</span>
              </slot>
            </div>
            <div ref="contentRef" class="flex-1 overflow-y-auto px-6 py-4">
              <slot />
            </div>
            <div class="flex justify-end gap-2 border-t border-neutral-200 px-6 py-4 dark:border-neutral-800">
              <slot name="footer" />
            </div>
            <span class="sr-only" tabindex="0" @focus="loopFocus('start')" />
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

const props = defineProps<{
  open: boolean
  title?: string
  maxWidthClass?: string
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
    ? "fixed inset-0 z-1000 flex items-end justify-center bg-black/50 dark:bg-black/70"
    : "fixed inset-0 z-1000 flex items-center justify-center bg-black/50 dark:bg-black/70"
))

const overlayStyles = computed(() => {
  if (!isMobile.value) return undefined
  return {
    paddingBottom: `${Math.max(0, keyboardInset.value)}px`,
  }
})

const dialogTransitionName = computed(() => (isMobile.value ? "sheet-modal" : "scale-modal"))

const dialogClasses = computed(() => {
  const base = "relative flex w-full flex-col border border-neutral-200 bg-white text-neutral-900 shadow-2xl dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100 overflow-hidden"
  if (isMobile.value) {
    return `${base} rounded-t-2xl rounded-b-none border-b-0 max-w-none`
  }
  return `${base} rounded-md max-h-[80vh] ${props.maxWidthClass ?? "max-w-2xl"}`
})

const dialogStyles = computed(() => {
  if (!isMobile.value) {
    return undefined
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
    ? "border-b border-neutral-200 px-6 pb-3 pt-2 dark:border-neutral-800"
    : "border-b border-neutral-200 px-6 pb-4 pt-6 dark:border-neutral-800"
))

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
