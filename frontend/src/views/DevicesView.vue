<!-- pages/DevicesPage.vue -->
<template>
  <div class="p-4">
    <h1 class="text-xl mb-4">Devices</h1>

    <div v-if="deviceStore.isLoading">Loading...</div>
    <div v-else-if="!deviceStore.devices.length" class="text-gray-400">
      No devices yet. Try scanning...
    </div>
    <ul>
      <li v-for="device in deviceStore.devices" :key="device.unit_id">
        {{ device.unit_id }} - {{ device.device_type }} - {{ device.status }}
      </li>
    </ul>

    <button class="mt-4 px-3 py-1 bg-blue-500 text-white rounded"
            @click="scanDevices">
      Scan devices
    </button>
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
})

function scanDevices() {
  wsStore.send({ action: WSAction.SCAN_DEVICES })
}

</script>
