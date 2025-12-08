<script setup lang="ts">
import { ref, computed } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import UiButton from "@/components/ui/UiButton.vue"
import type { Device } from "@/types/device";
import OnlineStatusComponent from "@/components/misc/OnlineStatusComponent.vue";

const props = defineProps<{
  device: Device
}>()

const store = useDeviceStore()


const lastSeen = computed(() => {
  const d = new Date(props.device.last_seen || Date.now())
  return d.toLocaleString("en-GB", {
    year: "numeric",
    month: "short",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
  })
})
</script>

<template>
  <div class="px-4 py-3 flex items-start justify-between border-b border-neutral-300 dark:border-neutral-800">
    <!-- LEFT SIDE -->
    <div class="flex flex-col gap-1">

      <!-- Device name -->
      <div class="flex items-center gap-2">
        <div class="text-lg font-medium tracking-tight hover:text-blue-400 cursor-pointer">
          {{ device.unit_id }}
        </div>

        <OnlineStatusComponent :status="device.status" />
      </div>

      <!-- Metadata -->
      <div class="text-xs text-neutral-500 leading-normal">
        <template v-if="device.name">
          <div>{{ device.name }}</div>
        </template>
        <div class="opacity-70">Type: {{ device.type.toUpperCase() }}</div>
        <div class="opacity-70">Firmware Version: {{ device.firmware_version }}</div>
        <div class="opacity-70">Last seen: {{ lastSeen }}</div>
      </div>
    </div>
  </div>
</template>
