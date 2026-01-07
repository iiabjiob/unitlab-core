<script setup lang="ts">
import type { TestRunSummary } from "@/types/testRuns"

const props = defineProps<{ run: TestRunSummary; active: boolean }>()
const emit = defineEmits<{ (e: "select", id: number): void }>()
</script>

<template>
  <div
    class="group cursor-pointer select-none flex items-center px-3 py-2 text-neutral-400 dark:text-neutral-600 hover:bg-neutral-850 transition-colors"
    :class="{ 'text-neutral-900 dark:text-white': active }"
    @click="emit('select', run.id)"
  >
    <div
      class="w-1 h-5 mr-2 rounded transition-colors"
      :class="active ? 'bg-blue-500' : 'bg-transparent group-hover:bg-neutral-700'"
    />
    <div class="truncate text-sm">{{ run.name }}</div>
  </div>
</template>
