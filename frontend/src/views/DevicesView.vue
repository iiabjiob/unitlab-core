<template>
  <div class="p-5">
    <!-- Keep title only on small screens -->
    <h1 class="text-xl mb-4 md:hidden">Devices</h1>

    <div v-if="deviceStore.isLoading">Loading...</div>

    <div v-else-if="!deviceStore.devices.length" class="text-neutral-400">
      No devices yet. Try scanning...
    </div>

    <!-- Grid on md+; simple list on mobile -->
    <ul
      v-else
      class="
        grid gap-5
        grid-cols-1
        sm:grid-cols-1
        lg:grid-cols-2
        xl:grid-cols-3
        2xl:grid-cols-4
        3xl:grid-cols-5
        items-stretch
      "
    >
      <li v-for="device in deviceStore.devices" :key="device.unit_id" class="h-full">
        <DeviceComponent
          :key="device.unit_id"
          :device="device"
          />
      </li>
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
