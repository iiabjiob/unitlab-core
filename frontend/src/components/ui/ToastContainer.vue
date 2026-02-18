<template>
  <div
    v-for="position in positions"
    :key="position"
    class="pointer-events-none fixed z-50 flex flex-col gap-2 p-4"
    :class="positionClass(position)"
  >
    <transition-group name="toast" tag="div" class="flex flex-col gap-2 w-full max-w-sm">
      <div
        v-for="toast in toastStore.getByPosition(position)"
        :key="toast.id"
        class="pointer-events-auto rounded-md border bg-neutral-50/95 p-2.5 text-neutral-900 shadow-md shadow-neutral-900/10 backdrop-blur-sm dark:bg-neutral-900/95 dark:text-neutral-100 dark:shadow-black/40"
        :class="variantFrameClass(toast.variant)"
        role="status"
        aria-live="polite"
      >
        <div class="flex items-start justify-between gap-3">
          <div class="min-w-0 flex items-start gap-2.5">
            <span
              class="mt-0.5 inline-flex shrink-0 items-center rounded border px-1.5 py-0.5 text-[10px] font-medium uppercase tracking-[0.06em]"
              :class="variantBadgeClass(toast.variant)"
            >
              {{ variantLabel(toast.variant) }}
            </span>
            <p class="text-xs leading-4 text-neutral-800 dark:text-neutral-100">
              {{ toast.message }}
            </p>
          </div>
          <button
            type="button"
            class="text-[10px] font-medium uppercase tracking-[0.06em] text-neutral-500 transition hover:text-neutral-700 dark:text-neutral-400 dark:hover:text-neutral-200"
            aria-label="Dismiss notification"
            @click="remove(toast.id)"
          >
            dismiss
          </button>
        </div>
      </div>
    </transition-group>
  </div>
</template>

<script setup lang="ts">
import { useToastStore, type ToastVariant, type ToastPosition } from "@/stores/toastStore"

const toastStore = useToastStore()

const positions: ToastPosition[] = [
  "top-left",
  "top-right",
  "bottom-left",
  "bottom-right",
  "top-center",
  "bottom-center",
]

function remove(id: number) {
  toastStore.remove(id)
}

function variantLabel(variant: ToastVariant) {
  switch (variant) {
    case "success":
      return "ok"
    case "error":
      return "error"
    default:
      return "info"
  }
}

function variantFrameClass(variant: ToastVariant) {
  switch (variant) {
    case "success":
      return "border-neutral-300/80 dark:border-neutral-700"
    case "error":
      return "border-neutral-300/80 dark:border-neutral-700"
    default:
      return "border-neutral-300/80 dark:border-neutral-700"
  }
}

function variantBadgeClass(variant: ToastVariant) {
  switch (variant) {
    case "success":
      return "border-emerald-700/25 bg-emerald-600/10 text-emerald-700 dark:border-emerald-300/25 dark:bg-emerald-300/10 dark:text-emerald-200"
    case "error":
      return "border-rose-700/25 bg-rose-600/10 text-rose-700 dark:border-rose-300/25 dark:bg-rose-300/10 dark:text-rose-200"
    default:
      return "border-neutral-500/25 bg-neutral-500/10 text-neutral-700 dark:border-neutral-300/25 dark:bg-neutral-300/10 dark:text-neutral-200"
  }
}

function positionClass(pos: ToastPosition) {
  switch (pos) {
    case "top-left":
      return "top-4 left-4 items-start"
    case "top-right":
      return "top-4 right-4 items-end"
    case "bottom-left":
      return "bottom-4 left-4 items-start"
    case "bottom-right":
      return "bottom-4 right-4 items-end"
    case "top-center":
      return "top-4 left-1/2 -translate-x-1/2 items-center"
    case "bottom-center":
      return "bottom-4 left-1/2 -translate-x-1/2 items-center"
  }
  return ""
}
</script>

<style scoped>
.toast-enter-active,
.toast-leave-active {
  transition: all 0.3s ease;
}
.toast-enter-from,
.toast-leave-to {
  opacity: 0;
  transform: translateY(-10px);
}
</style>
