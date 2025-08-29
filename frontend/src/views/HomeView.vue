<template>
  <div class="p-4">
    <h2 class="text-lg font-bold mb-4">Signals</h2>

    <!-- Реальные устройства -->
    <div v-for="dev in deviceStore.devices" :key="dev.unit_id" class="mb-6">
      <h3 class="font-semibold mb-2">
        {{ dev.unit_id }} ({{ dev.device_type }} / {{ dev.channels }} ch)
      </h3>
      <ChannelsComponent :unit-id="dev.unit_id" />
    </div>

    <!-- Заглушки устройств, если нет реальных -->
    <div v-if="!deviceStore.devices.length" class="opacity-70">
      <div
        v-for="mock in mockDevices"
        :key="mock.unit_id"
        class="mb-6 border border-dashed border-gray-400 dark:border-gray-600 rounded p-2"
      >
        <h3 class="font-semibold mb-2 text-gray-500">
          {{ mock.unit_id }} ({{ mock.device_type }} / {{ mock.channels }} ch)
        </h3>
        <ChannelsComponent
          :unit-id="mock.unit_id"
          :device-type="mock.device_type"
          :channels="mock.channels"
        />
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import ChannelsComponent from "@/components/ChannelsComponent.vue"
import { onMounted, reactive } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import { useChannelStore } from "@/stores/channelStore"

const deviceStore = useDeviceStore()
const channelStore = useChannelStore()

onMounted(() => {
  // deviceStore.devices.forEach((dev) => {
  //   channelStore.requestStates(dev.unit_id, dev.device_type)
  // })

  mockDevices.forEach((dev) => {
    channelStore.requestStates(dev.unit_id, dev.device_type)
  })
})

const mockDevices = reactive([
  { unit_id: "mock-di-1", device_type: "DI", channels: 5 },
  { unit_id: "mock-do-1", device_type: "DO", channels: 5 },
  { unit_id: "mock-ao-1", device_type: "AO", channels: 5 },
])
</script>
