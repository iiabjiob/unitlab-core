<script setup lang="ts">
defineProps<{
  label: string
  description?: string | null
  active?: boolean
}>()
</script>

<template>
  <div
    class="cursor-default select-none rounded-xl px-3 py-2 transition-colors"
    :class="active
      ? 'bg-neutral-100 text-neutral-900 dark:bg-neutral-800 dark:text-neutral-100'
      : 'text-neutral-700 hover:bg-neutral-50 dark:text-neutral-300 dark:hover:bg-neutral-900'"
  >
    <div class="text-sm font-medium leading-tight">{{ label }}</div>
    <div v-if="description" class="mt-0.5 text-[11px] leading-snug text-neutral-500 dark:text-neutral-400">
      {{ description }}
    </div>
  </div>
</template>

