<script setup lang="ts">
import { computed } from "vue"
import type { Device } from "@/types/device"
import OnlineStatusComponent from "@/components/misc/OnlineStatusComponent.vue"

const props = defineProps<{
  device: Device
}>()

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

const deviceTypeLabel = computed(() => {
  const base = props.device.type ?? (props.device as any).device_type ?? ""
  return base ? base.toUpperCase() : "UNKNOWN"
})
</script>

<template>
  <div class="px-4 py-3 flex items-start justify-between border-b border-neutral-300 dark:border-neutral-800">
    <!-- LEFT SIDE -->
    <div class="flex flex-col gap-1">

      <div class="text-lg font-medium tracking-tight text-neutral-900 dark:text-white">
        {{ device.unit_id }}
      </div>

      <div class="flex flex-wrap items-center gap-3 text-sm text-neutral-500 dark:text-neutral-400">
        <span class="text-xs uppercase tracking-[0.3em]">Device</span>
        <span>·</span>
        <span>Type {{ deviceTypeLabel }}</span>
        <template v-if="device.name">
          <span>·</span>
          <span>{{ device.name }}</span>
        </template>
        <template v-if="device.firmware_version">
          <span>·</span>
          <span>Firmware {{ device.firmware_version }}</span>
        </template>
        <span>·</span>
        <span>Last seen {{ lastSeen }}</span>
        <span>·</span>
        <OnlineStatusComponent :status="device.status" />
      </div>
    </div>
  </div>
</template>
