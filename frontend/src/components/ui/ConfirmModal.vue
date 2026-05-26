<template>
  <UiModal :open="open" :title="title" @close="$emit('cancel')">
    <p v-if="message" class="confirm-modal__message">
      {{ message }}
    </p>

    <template #footer>
      <button type="button" class="btn btn-secondary btn-base" @click="$emit('cancel')">
        {{ cancelLabel }}
      </button>
      <button
        ref="confirmBtn"
        type="button"
        class="btn btn-danger btn-base"
        data-dialog-initial
        @click="$emit('confirm')"
      >
        {{ confirmLabel }}
      </button>
    </template>
  </UiModal>
</template>

<script setup lang="ts">
import { ref, watch } from "vue"
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

watch(
  () => props.open,
  (v) => {
    if (v) {
      requestAnimationFrame(() => confirmBtn.value?.focus())
    }
  },
)
</script>

<style scoped>
.confirm-modal__message {
  color: var(--color-neutral-600);
  font-size: var(--text-sm);
  line-height: 1.25rem;
  margin: 0 0 1rem;
}

.dark .confirm-modal__message {
  color: var(--color-neutral-400);
}
</style>
