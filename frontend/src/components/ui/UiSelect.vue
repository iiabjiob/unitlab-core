<template>
  <select
    class="w-full px-2 py-0.5 text-xs
           bg-white dark:bg-neutral-800 focus:outline-none"
    :value="modelValue ?? ''"
    :name="name"
    :disabled="disabled"
    @change="onChange"
  >
    <option value="" v-if="placeholder">{{ placeholder }}</option>
    <slot />
  </select>
</template>

<script setup lang="ts">
const props = defineProps<{
  modelValue?: string | number | null
  name?: string
  placeholder?: string
  disabled?: boolean
}>()

const emit = defineEmits<{
  (e: "update:modelValue", value: string | null): void
}>()

function onChange(e: Event) {
  const target = e.target as HTMLSelectElement
  // Always either an id string or null
  emit("update:modelValue", target.value === "" ? null : target.value)
}
</script>
