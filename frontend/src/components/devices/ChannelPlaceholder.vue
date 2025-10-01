<template>
  <div
    class="flex items-center px-2 py-1 w-full border-b border-neutral-300 dark:border-neutral-700 last:border-0 text-xs"
  >
    <!-- Channel name -->
    <span class="text-neutral-400">
      {{ channel.name || `CH${channel.index + 1}` }}
    </span>

    <!-- Placeholder controls -->
    <div class="flex flex-1 items-center justify-end gap-2">
      <template v-if="channel.type === 'do'">
        <div class="h-4 w-10 rounded bg-neutral-200 dark:bg-neutral-700 animate-pulse"></div>
        <div class="h-4 w-10 rounded bg-neutral-200 dark:bg-neutral-700 animate-pulse"></div>
      </template>

      <template v-else-if="channel.type === 'ao'">
        <div class="h-4 w-24 rounded bg-neutral-200 dark:bg-neutral-700 animate-pulse"></div>
        <div class="h-4 w-10 rounded bg-neutral-200 dark:bg-neutral-700 animate-pulse"></div>
      </template>

    </div>

    <!-- Status placeholder -->
    <div class="ml-4 pl-4 border-l border-neutral-300 dark:border-neutral-700">

      <template v-if="channel.type === 'do' || channel.type === 'di'">
        <div class="h-4 w-4 rounded-full bg-neutral-300 dark:bg-neutral-600 animate-pulse"></div>
      </template>

      <template v-else-if="channel.type === 'ao'">
        <div class="h-4 w-10 rounded bg-neutral-200 dark:bg-neutral-700 animate-pulse"></div>
      </template>

    </div>
  </div>
</template>

<script setup lang="ts">
import type { Channel } from '@/types/channel';

const props = defineProps<{
  channel: Channel
}>()
</script>
