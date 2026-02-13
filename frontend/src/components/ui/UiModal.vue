<template>
  <teleport to="body">
    <transition name="fade-modal">
      <div
        v-if="isOpen"
        class="fixed inset-0 z-1000 flex items-center justify-center bg-black/50 dark:bg-black/70"
        @click.self="requestClose('backdrop')"
      >
        <transition name="scale-modal">
          <div
            v-if="isOpen"
            ref="dialogRef"
            :class="[
              'relative flex w-full flex-col rounded-md border border-neutral-200 bg-white text-neutral-900 shadow-2xl dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100 max-h-[80vh] overflow-hidden',
              props.maxWidthClass ?? 'max-w-2xl',
            ]"
            role="dialog"
            aria-modal="true"
            tabindex="-1"
            @keydown="onDialogKeydown"
          >
            <span class="sr-only" tabindex="0" @focus="loopFocus('end')" />
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

const props = defineProps<{
  open: boolean
  title?: string
  maxWidthClass?: string
}>()

const emit = defineEmits<{
  (e: "close"): void
}>()

const dialogRef = ref<HTMLDivElement | null>(null)
const FOCUSABLE_SELECTOR =
  'a[href], button:not([disabled]), input:not([disabled]), textarea:not([disabled]), select:not([disabled]), [tabindex]:not([tabindex="-1"])'

const dialog = useDialogController({
  focusOrchestrator: createDialogFocusOrchestrator({
    dialog: () => dialogRef.value,
    initialFocus: () => dialogRef.value?.querySelector<HTMLElement>("[data-dialog-initial]") ?? dialogRef.value,
  }),
})

const isOpen = computed(() => dialog.snapshot.value.isOpen)

function requestClose(reason: DialogCloseReason) {
  void dialog.close(reason).then((closed) => {
    if (closed) {
      emit("close")
    }
  })
}

function onDialogKeydown(e: KeyboardEvent) {
  if (e.key === "Escape" && isOpen.value) {
    requestClose("escape-key")
  }
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
