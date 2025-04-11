<template>
  <div>
    <ButtonComponent @click="discover">Discover</ButtonComponent>

    <div v-if="devicesStore.foundDevices.length > 0" class="mt-4">
      <label class="block mb-2 font-semibold">Select Devices:</label>
      <select v-model="selected" multiple class="w-full border p-2 rounded">
        <option v-for="device in devicesStore.foundDevices" :key="device" :value="device">
          {{ device }}
        </option>
      </select>
    </div>
  </div>

</template>

<script setup lang="ts">
import { ref, watch } from 'vue'
import { useDevicesStore } from '@/stores/devices'
import axios from 'axios'
import ButtonComponent from '@/components/ui/ButtonComponent.vue'

// Store с найденными и выбранными устройствами
const devicesStore = useDevicesStore()

// Выбранные Wi-Fi устройства (по их SSID)
const selected = ref<string[]>([])

// ✅ Поиск Wi-Fi устройств, начинающихся с [fat-simulator]_
const discover = async (): Promise<void> => {
  try {
    const response = await axios.get('/api/wifi/discover')
    const foundDevices: string[] = response.data.devices || []

    devicesStore.setFoundDevices(foundDevices)
  } catch (error) {
    console.error('Error while discovering devices:', error)
  }
}

// ✅ Сохраняем выбор пользователя в store
watch(selected, (newVal) => {
  devicesStore.setSelectedDevices(newVal)
})
</script>
