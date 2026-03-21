<template>
  <teleport :to="APP_OVERLAY_HOST_SELECTOR">
    <div v-if="isOpen" class="fixed inset-0 z-1000">
      <!-- Backdrop -->
      <div
        class="absolute inset-0 bg-black/50 dark:bg-black/70"
        @click="requestClose('backdrop')"
      />

      <!-- Slide-over LEFT/RIGHT -->
      <transition :name="transitionName">
        <div
          v-if="isSide"
          ref="dialogRef"
             class="absolute top-0 h-full border-neutral-200 dark:border-neutral-800 shadow-xl
               bg-white dark:bg-neutral-800 text-neutral-900 dark:text-neutral-100"
          :class="[sideClasses, defaultWidthClasses]"
          :style="sideStyles"
          role="dialog"
          aria-modal="true"
          tabindex="-1"
          @keydown="onDialogKeydown"
        >
          <span class="sr-only" tabindex="0" @focus="loopFocus('end')" />
          <!-- Header -->
          <div class="flex items-center justify-between px-3 py-2 border-b border-neutral-200 dark:border-neutral-800">
            <span class="text-sm font-semibold">{{ title }}</span>
            <button
              class="w-8 h-8 inline-flex items-center justify-center rounded-lg hover:bg-neutral-100 dark:hover:bg-neutral-800"
              aria-label="Close panel"
              @click="requestClose('pointer')"
            >
              <svg xmlns="http://www.w3.org/2000/svg" class="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2">
                <path stroke-linecap="round" stroke-linejoin="round" d="M6 6l12 12M6 18L18 6"/>
              </svg>
            </button>
          </div>

          <!-- Content -->
          <div class="h-[calc(100%-2.5rem)] overflow-y-auto" @click="onContentClick">
            <slot />
          </div>
          <span class="sr-only" tabindex="0" @focus="loopFocus('start')" />
        </div>
      </transition>

      <!-- Bottom sheet -->
      <transition name="slide-bottom">
        <div
          v-if="isBottom"
          ref="dialogRef"
             class="absolute left-0 right-0 rounded-t-2xl shadow-2xl border-t border-neutral-200 dark:border-neutral-800
               bg-white dark:bg-neutral-800 text-neutral-900 dark:text-neutral-100"
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
          <!-- Drag handle -->
          <div class="pt-2 pb-1 flex justify-center">
            <div class="h-1.5 w-10 rounded-full bg-neutral-300 dark:bg-neutral-700" />
          </div>

          <!-- Header -->
          <div class="flex items-center justify-between px-4 py-2">
            <span class="text-sm font-semibold">{{ title }}</span>
            <button
              class="w-8 h-8 inline-flex items-center justify-center rounded-lg hover:bg-neutral-100 dark:hover:bg-neutral-800"
              aria-label="Close sheet"
              @click="requestClose('pointer')"
            >
              <svg xmlns="http://www.w3.org/2000/svg" class="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2">
                <path stroke-linecap="round" stroke-linejoin="round" d="M6 6l12 12M6 18L18 6"/>
              </svg>
            </button>
          </div>

          <!-- Content -->
          <div class="px-4 pb-4 overflow-y-auto" :style="{ maxHeight: `${maxHeightVh}dvh` }" @click="onContentClick">
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
  "border-r", // always show a divider; for right we visually keep consistency
  props.placement === "left" ? "left-0" : "right-0",
  "top-0",
])
const sideStyles = computed(() => ({
  width: props.widthPx ? `${props.widthPx}px` : undefined,
  // Provide sensible default widths via CSS classes if widthPx not set
  // (Fallback handled via class on container below)
}))
// Add default Tailwind width classes when widthPx is not provided
// (We can't bind classes conditionally by presence cleanly in computed above; do it inline)
const defaultWidthClasses = computed(() => (!props.widthPx ? "w-80 md:w-96" : ""))

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
  void dialog.close(reason).then((closed) => {
    if (closed) {
      emit("close")
    }
  })
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
