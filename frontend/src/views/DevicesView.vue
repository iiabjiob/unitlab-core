<!-- pages/DevicesPage.vue -->
<template>
  <div class="p-4">
    <h1 class="text-xl mb-4">Devices</h1>

    <div v-if="deviceStore.isLoading">Loading...</div>
    <div v-else-if="!deviceStore.devices.length" class="text-gray-400">
      No devices yet. Try scanning...
    </div>
    <ul>
    <li v-for="device in deviceStore.devices" :key="device.unit_id"
        class="flex items-center gap-2">
      <span class="font-mono">{{ device.unit_id }}</span>
      <span class="text-sm text-gray-500">{{ device.device_type }}</span>
      <span :class="device.status === 'online' ? 'text-green-500' : 'text-red-500'">
        {{ device.status }}
      </span>
      <span class="text-xs text-gray-400">
        last seen: {{ device.last_seen ? new Date(device.last_seen).toLocaleTimeString() : 'never' }}
      </span>
    </li>
</ul>

  </div>
</template>

<script setup lang="ts">
import { onMounted } from 'vue'
import { useDeviceStore } from '@/stores/deviceStore'
import { useWebSocketStore } from '@/stores/websocketStore'
import { WSAction } from '@/types/ws/messages'

const wsStore = useWebSocketStore()
const deviceStore = useDeviceStore()

onMounted(async () => {
  await deviceStore.fetchDevices()
  wsStore.send({ action: WSAction.SCAN_DEVICES })
})

</script>
