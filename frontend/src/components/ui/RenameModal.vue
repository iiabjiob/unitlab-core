<script setup lang="ts">
import { computed, ref, watch } from "vue"
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
  inputId?: string
  inputName?: string
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

const generatedId = `rename-modal-${Math.random().toString(36).slice(2, 9)}`
const fieldId = computed(() => props.inputId || generatedId)
const fieldName = computed(() => props.inputName || fieldId.value)
const inputRef = ref<HTMLInputElement | null>(null)

watch(
  () => props.open,
  (open) => {
    if (!open) return
    requestAnimationFrame(() => {
      inputRef.value?.focus()
      inputRef.value?.select()
    })
  },
)
</script>

<template>
  <UiModal :open="open" :title="title" @close="emit('cancel')">
    <label
      :for="fieldId"
      class="rename-modal__label"
    >
      {{ label }}
    </label>
    <input
      ref="inputRef"
      :id="fieldId"
      :name="fieldName"
      :value="modelValue"
      :disabled="loading"
      type="text"
      autocomplete="off"
      data-dialog-initial
      class="rename-modal__input"
      @input="onInput"
      @keydown.enter.prevent="handleSubmit"
    />
    <p v-if="error" class="rename-modal__error">
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

<style scoped>
.rename-modal__label {
  color: var(--color-neutral-500);
  display: block;
  font-size: var(--text-xs);
  letter-spacing: 0.3em;
  line-height: 1rem;
  text-transform: uppercase;
}

.rename-modal__input {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-300);
  border-radius: 0.25rem;
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  line-height: 1.25rem;
  margin-top: 0.5rem;
  padding: 0.5rem 0.75rem;
  width: 100%;
}

.rename-modal__input::placeholder {
  color: var(--color-neutral-500);
}

.rename-modal__input:focus {
  box-shadow: 0 0 0 1px var(--color-blue-500);
  outline: none;
}

.rename-modal__input:disabled {
  opacity: 0.6;
}

.rename-modal__error {
  color: var(--color-red-500);
  font-size: var(--text-xs);
  line-height: 1rem;
  margin: 0.5rem 0 0;
}

.dark .rename-modal__label {
  color: var(--color-neutral-400);
}

.dark .rename-modal__input {
  background: var(--color-neutral-950);
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-100);
}
</style>
