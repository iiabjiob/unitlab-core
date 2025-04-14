<template>
  <div class="border border-gray-300 dark:border-gray-700 rounded p-4 bg-white dark:bg-gray-800 shadow w-full space-y-3">
    <!-- Header -->
    <div class="flex items-center justify-between">
      <div>
        <div class="text-sm text-gray-500 dark:text-gray-400">Digital Inputs Unit</div>
        <div class="font-bold text-lg">{{ props.unitId }}</div>
      </div>
    </div>

    <!-- Body -->
    <div class="divide-y divide-gray-300 dark:divide-gray-700">
      <!-- Signal Rows -->
      <div v-for="(signal, index) in props.signals" :key="index" class="flex items-center justify-between py-1">
        <!-- Signal Name -->
        <div>{{ signal.name }}</div>

        <!-- Status Only -->
        <span
          class="cursor-default"
          :title="signal.state ? 'ON' : 'OFF'"
        >
          {{ signal.state ? '🟢' : '⚪' }}
        </span>
      </div>
    </div>
  </div>
</template>


<script setup lang="ts">

import type { Signal } from '@/types/signal'

const props = defineProps<{
  unitId: string
  signals: Signal[]
}>()
</script>
