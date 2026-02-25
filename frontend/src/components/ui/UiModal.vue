<template>
  <teleport to="body">
    <transition name="fade-modal">
      <div
        v-if="isOpen"
        :class="overlayClasses"
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
            <div class="flex-1 overflow-y-auto px-6 py-4">
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
import { computed, ref, watch } from "vue"
import { createDialogFocusOrchestrator, type DialogCloseReason, useDialogController } from "@affino/dialog-vue"
import { useViewport } from "@/composables/useViewport"

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

const overlayClasses = computed(() => (
  isMobile.value
    ? "fixed inset-0 z-1000 flex items-end justify-center bg-black/50 dark:bg-black/70"
    : "fixed inset-0 z-1000 flex items-center justify-center bg-black/50 dark:bg-black/70"
))

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
  return {
    maxHeight: "78dvh",
    transform: `translateY(${Math.max(0, deltaY.value)}px)`,
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
