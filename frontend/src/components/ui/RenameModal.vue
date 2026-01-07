<script setup lang="ts">
import UiModal from "./UiModal.vue"

const props = withDefaults(defineProps<{
  open: boolean
  title: string
  modelValue: string
  label?: string
  confirmLabel?: string
  cancelLabel?: string
  loading?: boolean
  error?: string
}>(), {
  label: "Name",
  confirmLabel: "Save",
  cancelLabel: "Cancel",
  loading: false,
  error: "",
})

const emit = defineEmits<{
  (e: "update:modelValue", value: string): void
  (e: "cancel"): void
  (e: "confirm"): void
}>()

function onInput(e: Event) {
  const target = e.target as HTMLInputElement
  emit("update:modelValue", target.value)
}

function handleSubmit() {
  if (props.loading) return
  emit("confirm")
}
</script>

<template>
  <UiModal :open="open" :title="title" @close="emit('cancel')">
    <label class="block text-xs uppercase tracking-[0.3em] text-neutral-500 dark:text-neutral-400">
      {{ label }}
    </label>
    <input
      :value="modelValue"
      :disabled="loading"
      type="text"
      class="mt-2 w-full rounded border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 placeholder-neutral-500 focus:outline-none focus:ring-1 focus:ring-blue-500 disabled:opacity-60 dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
      @input="onInput"
      @keydown.enter.prevent="handleSubmit"
    />
    <p v-if="error" class="mt-2 text-xs text-red-500">
      {{ error }}
    </p>

    <template #footer>
      <button
        type="button"
        class="btn btn-secondary btn-base"
        :disabled="loading"
        @click="emit('cancel')"
      >
        {{ cancelLabel }}
      </button>
      <button
        type="button"
        class="btn btn-primary btn-base"
        :disabled="loading"
        @click="handleSubmit"
      >
        {{ loading ? 'Saving…' : confirmLabel }}
      </button>
    </template>
  </UiModal>
</template>
