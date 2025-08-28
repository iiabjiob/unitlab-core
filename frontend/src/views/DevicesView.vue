<!-- pages/DevicesPage.vue -->
<template>
  <div class="p-4">
    <h1 class="text-xl mb-4">Devices</h1>

    <div v-if="deviceStore.isLoading">Loading...</div>
    <div v-else-if="!deviceStore.devices.length" class="text-gray-400">
      No devices yet. Try scanning...
    </div>
    <ul>
      <li v-for="d in deviceStore.devices" :key="d.unit_id">
        {{ d.unit_id }} - {{ d.device_type }} - {{ d.status }}
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

const deviceStore = useDeviceStore()
const wsStore = useWebSocketStore()

onMounted(async () => {
  await deviceStore.fetchDevices()
})

function scanDevices() {
  wsStore.send({ action: WSAction.SCAN_DEVICES })
}
</script>
