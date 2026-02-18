<template>
  <select
    :id="fieldId"
    class="w-full px-2 py-0.5 text-xs
           bg-white dark:bg-neutral-800 focus:outline-none"
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
