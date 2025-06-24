<template>
  <div>
    <div v-if="!devices.length" class="text-center py-12 text-gray-400">
      <span class="text-3xl mb-2 block">🛰️</span>
      <p>No devices found.</p>
      <p class="text-xs text-gray-400 mt-1">Waiting for device registration...</p>
    </div>
    <div v-else class="grid gap-6
      grid-cols-1
      sm:grid-cols-2
      md:grid-cols-3
      xl:grid-cols-4
      3xl:grid-cols-5">

      <DeviceCard
        v-for="device in devices"
        :key="device.unit_id"
        :device="device"
        :channels="getChannelsForDevice(device.unit_id)"
        @toggle="toggleDevice"
        @delete="deleteDevice"
        @toggle-channel="toggleChannel"
        @pulse-channel="pulseChannel"
      />
    </div>
  </div>
</template>

<script setup lang="ts">
import { Channel } from '@/types/channel';
import DeviceCard from './DeviceCard.vue'
import { Device } from '@/types/device'

defineProps<{ devices: Device[] }>()

// Для теста — моковые каналы (выходы/входы устройства)
function getChannelsForDevice(unitId: string) {
  // Здесь можешь интегрировать с signalStore, пока отдаём мок:
  if (unitId === 'DO-unit-ABCD') {
    return [
      { index: 0, device_id: unitId, name: 'DO1', type: 'DO', state: false },
      { index: 1, device_id: unitId, name: 'DO2', type: 'DO', state: true },
      { index: 2, device_id: unitId, name: 'DO3', type: 'DO', state: false },
      { index: 3, device_id: unitId, name: 'DO4', type: 'DO', state: true }
    ] as Channel[]
  }
  if (unitId === 'AO-unit-XYZ') {
    return [
      { index: 0, device_id: unitId, name: 'AO1', type: 'AO', state: 12.5 },
      { index: 1, device_id: unitId, name: 'AO2', type: 'AO', state: 4.0 }
    ] as Channel[]
  }
  if (unitId === 'DI-unit-QWER') {
    return [
      { index: 0, device_id: unitId, name: 'DI1', type: 'DI', state: true },
      { index: 1, device_id: unitId, name: 'DI2', type: 'DI', state: false },
      { index: 2, device_id: unitId, name: 'DI3', type: 'DI', state: false },
      { index: 3, device_id: unitId, name: 'DI4', type: 'DI', state: true },
      { index: 4, device_id: unitId, name: 'DI5', type: 'DI', state: false },
      { index: 5, device_id: unitId, name: 'DI6', type: 'DI', state: true },
      { index: 6, device_id: unitId, name: 'DI7', type: 'DI', state: false },
      { index: 7, device_id: unitId, name: 'DI8', type: 'DI', state: true }
    ] as Channel[]
  }
  return []
}

// Заглушки для управления
function toggleDevice(unitId: string) {
  console.log('Toggle active for ' + unitId)
}
function deleteDevice(unitId: string) {
  console.log('Delete device ' + unitId)
}

function toggleChannel(unitId: string, channelIndex: number) {
  console.log('Toggle channel ' + unitId);

}
function pulseChannel(unitId: string, channelIndex: number) {
  console.log('Pulse channel ' + unitId)
}

</script>
