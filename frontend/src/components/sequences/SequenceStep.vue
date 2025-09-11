<template>
  <li class="flex items-center gap-2">
    <!-- Completion marker -->
    <span
      class="inline-flex h-4 w-4 items-center justify-center rounded border"
      :class="completed ? 'bg-green-500 border-green-500' : 'bg-white dark:bg-neutral-900'"
    >
      <span v-if="completed" class="text-[10px] text-white">✓</span>
    </span>

    <!-- Index -->
    <span class="font-mono text-xs text-neutral-500">#{{ index + 1 }}</span>

    <!-- Description -->
    <span>{{ description }}</span>

    <!-- Error (будет расширяться в будущем) -->
    <span v-if="error" class="text-xs text-red-600">({{ error }})</span>
  </li>
</template>

<script setup lang="ts">
const props = defineProps<{
  index: number
  description: string
  completed: boolean
  error?: string | null
}>()
</script>
