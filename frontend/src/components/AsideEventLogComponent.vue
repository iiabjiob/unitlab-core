<template>
  <aside class="p-3 border-t border-gray-200 dark:border-gray-700">
    <div class="flex items-center justify-between mb-2">
      <h3 class="text-sm font-semibold text-gray-700 dark:text-gray-200">Event Log</h3>
      <RouterLink
        to="/events"
        class="text-xs underline text-gray-600 dark:text-gray-300 hover:opacity-80"
      >
        More
      </RouterLink>
    </div>

    <ul class="space-y-2 divide-y divide-gray-200 dark:divide-gray-700 max-h-48 overflow-y-auto">
      <li
        v-for="e in store.items"
        :key="e.id"
        class="text-xs"
        :class="e.highlight
        ? 'bg-yellow-100 dark:bg-yellow-900'
        : ''"
      >
        <!-- Row 1: timestamp + device -->
        <div class="flex items-center gap-2">
          <span :title="e.dir">
            {{ e.dir === 'IN' ? '📥' : '➡️' }}
          </span>

          <span class="font-mono text-gray-500 dark:text-gray-400 text-xs">
            {{ formatTs(e.ts) }}
          </span>

          <span v-if="e.unit_id" class="text-gray-600 dark:text-gray-300 text-xs">
            {{ e.unit_id }}
          </span>
        </div>

        <!-- Row 2: short description -->
        <div class="pl-6 truncate">
          <span class="font-medium text-gray-800 dark:text-gray-100">
            {{ e.summary }}
          </span>
        </div>
      </li>
    </ul>
  </aside>
</template>

<script lang="ts" setup>
import { useEventLogStore } from "@/stores/eventLogStore";
import { formatTs } from "@/utils/datetime";

const store = useEventLogStore();

</script>
