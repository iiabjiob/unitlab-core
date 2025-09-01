<template>
  <span :class="['inline-flex items-center px-2 py-0.5 rounded text-xs font-medium', colorClass]">
    <slot />
  </span>
</template>

<script setup lang="ts">
import { computed } from 'vue'

const props = defineProps<{
  variant?: 'success' | 'danger' | 'neutral'
}>()

const colorClass = computed(() => {
  switch (props.variant) {
    case 'success':
      return 'bg-green-100 text-green-800 dark:bg-green-900 dark:text-green-300'
    case 'danger':
      return 'bg-red-100 text-red-800 dark:bg-red-900 dark:text-red-300'
    default:
      return 'bg-neutral-100 text-neutral-800 dark:bg-neutral-800 dark:text-neutral-200'
  }
})
</script>
