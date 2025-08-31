<!-- DeviceComponent.vue -->
<template>
  <li
    class="flex flex-col p-3 rounded-md bg-white dark:bg-gray-800 shadow-sm"
  >
    <!-- Верхняя строка: ID + тип + статус -->
    <div class="flex items-center justify-between">
      <div class="flex items-center gap-2">
        <span class="font-mono font-semibold">{{ device.unit_id }}</span>
        <span class="text-sm text-gray-500 uppercase">{{ device.type }}</span>
      </div>

      <span
        :class="device.status === 'online'
          ? 'text-green-500 font-medium'
          : 'text-red-500 font-medium'"
      >
        {{ device.status || "unknown" }}
      </span>
    </div>

    <!-- Информация -->
    <div
      class="mt-1 text-xs text-gray-500 flex flex-wrap gap-x-4 gap-y-1"
    >
      <span>Channels: {{ device.channels }}</span>
      <span>FW: {{ device.firmware_version || "n/a" }}</span>
      <span>Active: {{ device.is_active ? "yes" : "no" }}</span>
      <span v-if="device.last_seen">
        Last seen: {{ new Date(device.last_seen).toLocaleTimeString() }}
      </span>
      <span v-if="device.location">Location: {{ device.location }}</span>
    </div>

    <!-- Каналы -->
    <div class="mt-3">
      <ChannelsComponent
        :unit-id="device.unit_id"
        :device-type="device.type"
        :channels="device.channels"
        :disabled="device.status !== 'online'"
      />
    </div>

    <!-- Действия -->
    <div class="mt-3 flex gap-2">
      <button
        class="px-2 py-1 text-xs rounded border bg-gray-100 dark:bg-gray-700 hover:bg-gray-200 dark:hover:bg-gray-600"
        @click="toggleActive"
      >
        {{ device.is_active ? "Deactivate" : "Activate" }}
      </button>
      <button
        class="px-2 py-1 text-xs rounded border bg-red-500 text-white hover:bg-red-600"
        @click="deleteDev"
      >
        Delete
      </button>
    </div>
  </li>
</template>

<script setup lang="ts">
import { useDeviceStore } from "@/stores/deviceStore"
import type { Device } from "@/types/device"
import ChannelsComponent from "./ChannelsComponent.vue"

const props = defineProps<{ device: Device }>()
const deviceStore = useDeviceStore()

async function toggleActive() {
  await deviceStore.toggleDeviceActive(props.device.unit_id)
}

async function deleteDev() {
  if (confirm(`Delete device ${props.device.unit_id}?`)) {
    await deviceStore.deleteDevice(props.device.unit_id)
  }
}
</script>
