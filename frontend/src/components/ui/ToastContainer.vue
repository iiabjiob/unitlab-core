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
        class="pointer-events-auto rounded border border-slate-200 bg-white/95 p-3 text-slate-900 shadow-lg shadow-slate-900/10 backdrop-blur-sm dark:border-slate-800 dark:bg-slate-900/95 dark:text-slate-100 dark:shadow-black/40"
      >
        <div class="flex items-start justify-between gap-3">
          <p class="text-sm font-medium leading-5" :class="variantTextClass(toast.variant)">
            {{ toast.message }}
          </p>
          <button
            type="button"
            class="text-xs font-semibold uppercase tracking-wide text-slate-500 transition hover:text-slate-700 dark:text-slate-300 dark:hover:text-white"
            aria-label="Dismiss notification"
            @click="remove(toast.id)"
          >
            close
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

function variantTextClass(variant: ToastVariant) {
  switch (variant) {
    case "success":
      return "text-emerald-600 dark:text-emerald-300"
    case "error":
      return "text-rose-600 dark:text-rose-300"
    default:
      return "text-slate-900 dark:text-slate-100"
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
