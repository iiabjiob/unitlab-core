<template>
  <div
    class="flex flex-wrap items-center text-xs gap-x-2 py-0.5 text-gray-600 dark:text-gray-400">
    <span class="tabular-nums font-mono">{{ timeStore.formatted }}</span>
    <span class="text-gray-500 text-[10px]">{{ timeStore.sourceLabel }}</span>
  </div>
</template>

<script setup lang="ts">
import { useTimeStore } from "@/stores/timeStore"
const timeStore = useTimeStore()
timeStore.fetchTime()
</script>
