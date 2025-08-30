<template>
  <div class="p-4">
    <h2 class="text-lg font-bold mb-4">Signals</h2>

    <!-- Устройства -->
    <div v-for="dev in deviceStore.devices" :key="dev.unit_id" class="mb-6">
      <h3 class="font-semibold mb-2">
        {{ dev.unit_id }} ({{ dev.type }} / {{ dev.channels }} ch)
      </h3>
      <ChannelsComponent :unit-id="dev.unit_id" />
    </div>

  </div>
</template>

<script setup lang="ts">
import ChannelsComponent from "@/components/ChannelsComponent.vue"
import { onMounted } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import { useChannelStore } from "@/stores/channelStore"

const deviceStore = useDeviceStore()
const channelStore = useChannelStore()

onMounted(async () => {
  await deviceStore.fetchDevices()

  deviceStore.devices.forEach((dev) => {
    channelStore.requestStates(dev.unit_id, dev.type)
  })
})

</script>
