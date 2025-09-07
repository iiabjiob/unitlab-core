<template>
  <div
    class="flex flex-wrap items-center text-xs gap-x-2 py-0.5 text-neutral-600 dark:text-neutral-400">
    <span class="tabular-nums font-mono">{{ timeStore.formatted }}</span>
    <span class="text-neutral-500 text-[10px]">{{ timeStore.sourceLabel }}</span>
  </div>
</template>

<script setup lang="ts">
import { useTimeStore } from "@/stores/timeStore"
const timeStore = useTimeStore()
</script>
