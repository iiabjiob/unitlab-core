<template>
  <div
    class="flex flex-wrap text-xs gap-x-2 py-0.5 text-gray-600 dark:text-gray-400">
    <span>{{ timeStore.sourceLabel }}</span>
    <span class="tabular-nums font-mono">{{ timeStore.formatted }}</span>
  </div>
</template>

<script setup lang="ts">
import { useTimeStore } from "@/stores/timeStore"
const timeStore = useTimeStore()
timeStore.fetchTime()
</script>
