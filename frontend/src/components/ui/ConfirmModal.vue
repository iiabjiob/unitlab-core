<template>
  <UiModal :open="open" :title="title" @close="$emit('cancel')">
    <p v-if="message" class="mb-4 text-sm text-neutral-600 dark:text-neutral-400">
      {{ message }}
    </p>

    <template #footer>
      <button class="btn btn-secondary btn-base" @click="$emit('cancel')">
        {{ cancelLabel }}
      </button>
      <button ref="confirmBtn" class="btn btn-danger btn-base" @click="$emit('confirm')">
        {{ confirmLabel }}
      </button>
    </template>
  </UiModal>
</template>

<script setup lang="ts">
import { onBeforeUnmount, ref, watch } from "vue"
import UiModal from "./UiModal.vue"

const props = defineProps<{
  open: boolean
  title: string
  message?: string
  confirmLabel?: string
  cancelLabel?: string
  enterConfirms?: boolean
}>()

const emit = defineEmits<{
  (e: "cancel"): void
  (e: "confirm"): void
}>()

const confirmBtn = ref<HTMLButtonElement | null>(null)

function handleKeydown(e: KeyboardEvent) {
  if (!props.open) return
  if (e.key === "Enter" && props.enterConfirms) {
    e.preventDefault()
    emit("confirm")
  }
}

watch(
  () => props.open,
  (v) => {
    document.body.style.overflow = v ? "hidden" : ""
    if (v) {
      window.addEventListener("keydown", handleKeydown)
      requestAnimationFrame(() => confirmBtn.value?.focus())
    } else {
      window.removeEventListener("keydown", handleKeydown)
    }
  },
  { immediate: true },
)

onBeforeUnmount(() => {
  window.removeEventListener("keydown", handleKeydown)
  document.body.style.overflow = ""
})
</script>
