<template>
  <label
    class="relative inline-block w-10 h-5 cursor-pointer select-none"
    :title="title"
  >
    <input
      type="checkbox"
      class="sr-only peer"
      :checked="modelValue"
      @change="$emit('update:modelValue', !modelValue)"
    />

    <!-- Track -->
    <div
      class="absolute inset-0 rounded-full transition-colors
             bg-gray-300 peer-checked:bg-emerald-500 dark:bg-gray-600 dark:peer-checked:bg-emerald-400"
    ></div>

    <!-- Knob -->
    <div
      class="absolute top-[2px] left-[2px] w-4 h-4 rounded-full shadow transition-transform
             bg-white dark:bg-gray-900 peer-checked:translate-x-5"
    ></div>
  </label>
</template>

<script setup>
defineProps({
  modelValue: Boolean,
  title: {
    type: String,
    default: 'Toggle',
  },
})

defineEmits(['update:modelValue'])
</script>
