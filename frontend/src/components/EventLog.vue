<template>
  <div class="p-3 h-full flex flex-col">
    <!-- Header -->
    <div class="flex items-center justify-between mb-2 shrink-0">
      <h3 class="text-sm font-semibold text-neutral-700 dark:text-neutral-200">Event Log</h3>
      <RouterLink
        to="/events"
        class="text-xs underline text-neutral-600 dark:text-neutral-300 hover:opacity-80"
      >
        More
      </RouterLink>
    </div>

    <!-- Empty state -->
    <div
      v-if="!store.items.length"
      class="flex-1 flex items-center justify-center text-neutral-400 dark:text-neutral-500 text-sm"
    >
      No events yet
    </div>

    <!-- List -->
    <ul
      v-else
      class="log-list flex-1 overflow-y-auto divide-y divide-neutral-200 dark:divide-neutral-700"
    >
      <li
        v-for="e in store.items"
        :key="e.id"
        class="text-xs flex items-center gap-2 px-1 py-0.5 transition-colors truncate"
        :class="e.highlight ? 'bg-yellow-50 dark:bg-yellow-950' : ''"
      >
        <!-- Direction -->
        <span v-if="e.dir === 'IN'">⬅️</span>
        <span v-else-if="e.dir === 'OUT'">➡️</span>

        <!-- Timestamp -->
        <span class="font-mono text-neutral-500 dark:text-neutral-400">
          {{ formatTs(e.ts) }}
        </span>

        <!-- Device -->
        <span v-if="e.unit_id" class="text-neutral-600 dark:text-neutral-300">
          {{ e.unit_id }}
        </span>

        <!-- Summary -->
        <span class="font-medium text-neutral-800 dark:text-neutral-100 truncate">
          {{ e.summary }}
        </span>
      </li>
    </ul>
  </div>
</template>

<script setup lang="ts">
import { useEventLogStore } from "@/stores/eventLogStore"
import { formatTs } from "@/utils/datetime"

const store = useEventLogStore()
</script>

<style scoped>
.log-list {
  scrollbar-width: none;       /* Firefox */
  -ms-overflow-style: none;    /* IE/Edge */
}
.log-list::-webkit-scrollbar {
  display: none;               /* Chrome, Safari, Edge */
}
</style>
