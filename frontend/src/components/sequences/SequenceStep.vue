<template>
  <li
    class="flex items-center gap-2 group"
    :class="error ? 'text-red-600' : ''"
  >
    <slot name="prefix" />

    <!-- Completion marker -->
    <span
      class="inline-flex h-4 w-4 items-center justify-center rounded border flex-shrink-0"
      :class="[
        completed
          ? 'bg-green-500 border-green-500'
          : error
            ? 'bg-red-100 border-red-500'
            : 'bg-white dark:bg-neutral-900 border-neutral-300 dark:border-neutral-600'
      ]"
    >
      <span v-if="completed" class="text-[10px] text-white">✓</span>
      <span v-else-if="error" class="text-[10px] text-red-600">!</span>
    </span>

    <span class="font-mono text-xs text-neutral-500">#{{ index + 1 }}</span>
    <span class="flex-1 text-sm">{{ description }}</span>
    <span v-if="error" class="text-xs italic ml-2">({{ error }})</span>

    <!-- Delete button (visible only on hover) -->
    <button
      @click="$emit('delete')"
      class="opacity-0 group-hover:opacity-100 transition-opacity text-neutral-400 hover:text-red-600"
    >
      🗑
    </button>
  </li>
</template>

<script setup lang="ts">
const props = defineProps<{
  index: number
  description: string
  completed: boolean
  error?: string | boolean | null
}>()

const emit = defineEmits<{
  (e: "delete"): void
}>()
</script>
