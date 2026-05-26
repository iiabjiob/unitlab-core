<template>
  <select
    :id="fieldId"
    class="ui-select"
    :value="modelValue ?? ''"
    :name="fieldName"
    :disabled="disabled"
    @change="onChange"
  >
    <option value="" v-if="placeholder">{{ placeholder }}</option>
    <slot />
  </select>
</template>

<script setup lang="ts">
import { computed, getCurrentInstance } from "vue"

const props = defineProps<{
  modelValue?: string | number | null
  id?: string
  name?: string
  placeholder?: string
  disabled?: boolean
}>()

const instance = getCurrentInstance()

const fieldId = computed(() => {
  const explicit = String(props.id ?? "").trim()
  if (explicit.length > 0) {
    return explicit
  }
  return `ui-select-${instance?.uid ?? "field"}`
})

const fieldName = computed(() => {
  const explicit = String(props.name ?? "").trim()
  if (explicit.length > 0) {
    return explicit
  }
  return fieldId.value
})

const emit = defineEmits<{
  (e: "update:modelValue", value: string | null): void
}>()

function onChange(e: Event) {
  const target = e.target as HTMLSelectElement
  // Always either an id string or null
  emit("update:modelValue", target.value === "" ? null : target.value)
}
</script>

<style scoped>
.ui-select {
  background: var(--color-white);
  border: none;
  font-size: var(--text-xs);
  line-height: 1rem;
  padding: 0.125rem 0.5rem;
  width: 100%;
}

.ui-select:focus {
  outline: none;
}

.ui-select:disabled {
  opacity: 0.6;
}

.dark .ui-select {
  background: var(--color-neutral-800);
  color: var(--color-neutral-100);
}
</style>
