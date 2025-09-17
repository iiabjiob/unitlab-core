<template>
  <teleport to="body">
    <div v-if="open" class="fixed inset-0 z-40 ">
      <!-- Backdrop -->
      <div
        class="absolute inset-0 bg-black/10"
        @click="onBackdropClick"
      />

      <!-- Slide-over LEFT/RIGHT -->
      <transition :name="transitionName">
        <div
          v-if="isSide"
          class="absolute top-0 h-full border-neutral-200 dark:border-neutral-800 shadow-xl
                 transform will-change-transform bg-white dark:bg-neutral-800 text-neutral-900 dark:text-neutral-100"
          :class="sideClasses"
          :style="sideStyles"
          role="dialog"
          aria-modal="true"
        >
          <!-- Header -->
          <div class="flex items-center justify-between px-3 py-2 border-b border-neutral-200 dark:border-neutral-800">
            <span class="text-sm font-semibold">{{ title }}</span>
            <button
              class="w-8 h-8 inline-flex items-center justify-center rounded-lg hover:bg-neutral-100 dark:hover:bg-neutral-800"
              aria-label="Close panel"
              @click="$emit('close')"
            >
              <svg xmlns="http://www.w3.org/2000/svg" class="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2">
                <path stroke-linecap="round" stroke-linejoin="round" d="M6 6l12 12M6 18L18 6"/>
              </svg>
            </button>
          </div>

          <!-- Content -->
          <div class="h-[calc(100%-2.5rem)] overflow-y-auto" @click="closeOnItemClick && $emit('close')">
            <slot />
          </div>
        </div>
      </transition>

      <!-- Bottom sheet -->
      <transition name="slide-bottom">
        <div
          v-if="isBottom"
          class="absolute left-0 right-0 rounded-t-2xl shadow-2xl border-t border-neutral-200 dark:border-neutral-800
                 transform will-change-transform bg-white dark:bg-neutral-800 text-neutral-900 dark:text-neutral-100"
          :style="bottomStyles"
          role="dialog"
          aria-modal="true"
          @touchstart.passive="onTouchStart"
          @touchmove.prevent="onTouchMove"
          @touchend="onTouchEnd"
        >
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
              @click="$emit('close')"
            >
              <svg xmlns="http://www.w3.org/2000/svg" class="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="2">
                <path stroke-linecap="round" stroke-linejoin="round" d="M6 6l12 12M6 18L18 6"/>
              </svg>
            </button>
          </div>

          <!-- Content -->
          <div class="px-4 pb-4 overflow-y-auto" :style="{ maxHeight: `${maxHeightVh}vh` }" @click="closeOnItemClick && $emit('close')">
            <slot />
          </div>
        </div>
      </transition>
    </div>
  </teleport>
</template>

<script setup lang="ts">
import { computed, ref, watch, onMounted, onBeforeUnmount } from "vue"

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
const bottomStyles = computed(() => ({
  bottom: "0",
  // Apply translate based on drag delta
  transform: `translateY(${Math.max(0, deltaY.value)}px)`,
}))

function onTouchStart(e: TouchEvent) {
  // Start tracking only if started near the handle/top area to avoid conflicts with inner scrolls
  startY.value = e.touches[0].clientY
  deltaY.value = 0
}
function onTouchMove(e: TouchEvent) {
  const currentY = e.touches[0].clientY
  deltaY.value = Math.max(0, currentY - startY.value)
}
function onTouchEnd() {
  // Close if dragged enough, otherwise snap back
  if (deltaY.value > 80) emit("close")
  deltaY.value = 0
}

function onBackdropClick() {
  if (props.closeOnBackdrop) emit("close")
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
watch(() => props.open, (v) => lockScroll(v), { immediate: true })
onMounted(() => {
  const onKey = (e: KeyboardEvent) => { if (e.key === "Escape" && props.open) emit("close") }
  window.addEventListener("keydown", onKey)
  onBeforeUnmount(() => window.removeEventListener("keydown", onKey))
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
</style>
