<script setup lang="ts">
import type { Device } from "@/types/device"
import { computed } from "vue"

const props = defineProps<{
  device: Device
  active: boolean
}>()

// Цвет индикатора статуса
const statusClass = computed(() => {
  switch (props.device.status) {
    case "online":
      return "bg-green-500"
    case "offline":
      return "bg-gray-600"
    default:
      return "bg-gray-600"
  }
})
</script>

<template>
  <div
    class="group cursor-pointer select-none flex items-center px-3 py-2
           text-neutral-400 dark:text-neutral-600 hover:bg-neutral-850 transition-colors"
    :class="{
      'text-neutral-900 dark:text-white': active
    }"
  >
    <!-- Active indicator -->
    <div
      class="w-1 h-5 mr-2 rounded transition-colors"
      :class="active ? 'bg-blue-500' : 'bg-transparent group-hover:bg-neutral-700'"
    />

    <div class="flex items-center gap-2">

      <!-- STATUS DOT (based on backend status) -->
      <div
        class="w-2 h-2 rounded-full transition-colors"
        :class="statusClass"
      />
  
      <!-- UNIT ID -->
      <div class="truncate text-sm flex-1">
        {{ device.unit_id }}
      </div>
  
      <!-- DEVICE TYPE -->
      <div class="text-xs truncate text-neutral-500 dark:text-neutral-400">
        {{ device.device_type }}
      </div>
    </div>

  </div>
</template>
