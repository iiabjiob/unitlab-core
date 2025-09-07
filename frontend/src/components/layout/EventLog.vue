<template>
  <div class="p-3">
    <div class="flex items-center justify-between mb-2">
      <h3 class="text-sm font-semibold text-neutral-700 dark:text-neutral-200">Event Log</h3>
      <RouterLink
        to="/events"
        class="text-xs underline text-neutral-600 dark:text-neutral-300 hover:opacity-80"
      >
        More
      </RouterLink>
    </div>

    <ul
      class="log-list space-y-2 divide-y divide-neutral-200 dark:divide-neutral-700 max-h-48 overflow-y-auto"
    >
      <li
        v-for="e in store.items"
        :key="e.id"
        class="text-xs transition-colors"
        :class="e.highlight
          ? 'bg-yellow-50 dark:bg-yellow-950'
          : ''"
      >
        <!-- Row 1: timestamp + device -->
        <div class="flex items-center gap-2">
          <span v-if="e.dir === 'IN'">⬅️</span>
          <span v-else-if="e.dir === 'OUT'">➡️</span>

          <span class="font-mono text-neutral-500 dark:text-neutral-400 text-xs">
            {{ formatTs(e.ts) }}
          </span>

          <span v-if="e.unit_id" class="text-neutral-600 dark:text-neutral-300 text-xs">
            {{ e.unit_id }}
          </span>
        </div>

        <!-- Row 2: short description -->
        <div class="pl-6 truncate">
          <span class="font-medium text-neutral-800 dark:text-neutral-100">
            {{ e.summary }}
          </span>
        </div>
      </li>
    </ul>
  </div>
</template>

<script lang="ts" setup>
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
