<template>
  <div class="p-4">
    <h1 class="text-xl mb-4">Devices</h1>

    <div v-if="deviceStore.isLoading">Loading...</div>

    <div v-else-if="!deviceStore.devices.length" class="text-gray-400">
      No devices yet. Try scanning...
    </div>

    <ul v-else class="space-y-2">
      <DeviceComponent
        v-for="device in deviceStore.devices"
        :key="device.unit_id"
        :device="device"
      />
    </ul>
  </div>
</template>

<script setup lang="ts">
import { onMounted } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import { useWebSocketStore } from "@/stores/websocketStore"
import { WSAction } from "@/types/ws/messages"
import DeviceComponent from "@/components/DeviceComponent.vue"

const wsStore = useWebSocketStore()
const deviceStore = useDeviceStore()

onMounted(async () => {
  await deviceStore.fetchDevices()
  wsStore.send({ action: WSAction.SCAN_DEVICES })

})
</script>
