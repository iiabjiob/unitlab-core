<template>
  <div class="border-t border-gray-200 dark:border-gray-700 mt-auto">
    <div class="p-2">
      <div
        v-for="(event, index) in events"
        :key="index"
        class="flex items-center h-8 hover:bg-gray-100 dark:hover:bg-gray-700 rounded px-2 cursor-pointer font-mono text-xs"
        :title="isCollapsed ? event.message : ''"
      >
        <!-- Время -->
        <span class="w-24 shrink-0 text-gray-500">
          {{ event.time }}
        </span>

        <!-- Сообщение -->
        <span v-if="!isCollapsed" class="truncate">
          {{ event.message }}
        </span>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
defineProps<{
  isCollapsed: boolean
}>()

// Пример данных (заглушка)
const events = [
  { time: "12:45:01.234", message: "DO Unit #1 Connected" },
  { time: "12:43:58.876", message: "Voltage drop detected" },
  { time: "12:40:15.002", message: "Device #3 Offline" }
]
</script>
