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
        class="pointer-events-auto rounded-md border bg-slate-50/95 p-3 text-slate-900 shadow-md shadow-slate-900/10 backdrop-blur-sm dark:bg-slate-900/95 dark:text-slate-100 dark:shadow-black/40"
        :class="variantFrameClass(toast.variant)"
        role="status"
        aria-live="polite"
      >
        <div class="flex items-start justify-between gap-3">
          <div class="min-w-0 flex items-start gap-2.5">
            <span
              class="mt-0.5 inline-flex shrink-0 items-center rounded border px-1.5 py-0.5 text-[10px] font-semibold uppercase tracking-[0.08em]"
              :class="variantBadgeClass(toast.variant)"
            >
              {{ variantLabel(toast.variant) }}
            </span>
            <p class="text-sm leading-5 text-slate-800 dark:text-slate-100">
              {{ toast.message }}
            </p>
          </div>
          <button
            type="button"
            class="text-[11px] font-semibold uppercase tracking-[0.08em] text-slate-500 transition hover:text-slate-700 dark:text-slate-400 dark:hover:text-slate-200"
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
      return "border-slate-300/80 border-l-4 border-l-emerald-600/60 dark:border-slate-700 dark:border-l-emerald-400/50"
    case "error":
      return "border-slate-300/80 border-l-4 border-l-rose-600/60 dark:border-slate-700 dark:border-l-rose-400/50"
    default:
      return "border-slate-300/80 border-l-4 border-l-slate-500/70 dark:border-slate-700 dark:border-l-slate-400/60"
  }
}

function variantBadgeClass(variant: ToastVariant) {
  switch (variant) {
    case "success":
      return "border-emerald-700/30 bg-emerald-600/10 text-emerald-700 dark:border-emerald-300/30 dark:bg-emerald-300/10 dark:text-emerald-200"
    case "error":
      return "border-rose-700/30 bg-rose-600/10 text-rose-700 dark:border-rose-300/30 dark:bg-rose-300/10 dark:text-rose-200"
    default:
      return "border-slate-500/30 bg-slate-500/10 text-slate-700 dark:border-slate-300/30 dark:bg-slate-300/10 dark:text-slate-200"
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
