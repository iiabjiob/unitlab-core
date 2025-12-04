<template>
  <teleport to="body">
    <transition name="fade-modal">
      <div
        v-if="open"
        class="fixed inset-0 z-[1000] flex items-center justify-center bg-black/50 dark:bg-black/70"
        @click.self="onBackdropClick"
      >
        <transition name="scale-modal">
          <div
            v-if="open"
            class="relative flex w-full max-w-2xl flex-col rounded-md border border-neutral-200 bg-white text-neutral-900 shadow-2xl dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100 max-h-[80vh] overflow-hidden"
            role="dialog"
            aria-modal="true"
          >
            <div class="border-b border-neutral-200 px-6 pb-4 pt-6 dark:border-neutral-800">
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
          </div>
        </transition>
      </div>
    </transition>
  </teleport>
</template>

<script setup lang="ts">
import { defineProps, defineEmits, watch, onMounted, onBeforeUnmount } from "vue"

const props = defineProps<{
  open: boolean
  title?: string
}>()

const emit = defineEmits<{
  (e: "close"): void
}>()

function onBackdropClick() {
  emit("close")
}

function onKeydown(e: KeyboardEvent) {
  if (e.key === "Escape" && props.open) {
    emit("close")
  }
}

onMounted(() => {
  window.addEventListener("keydown", onKeydown)
})
onBeforeUnmount(() => {
  window.removeEventListener("keydown", onKeydown)
})
// If the modal closes programmatically, reset focus
watch(() => props.open, (open) => {
  if (!open) {
    // Add focus reset here if required
  }
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
</style>
