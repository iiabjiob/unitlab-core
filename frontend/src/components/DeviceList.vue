<template>
  <div>
    <div v-if="!devices.length" class="text-center py-12 text-gray-400">
      <span class="text-3xl mb-2 block">🛰️</span>
      <p>No devices found.</p>
      <p class="text-xs text-gray-400 mt-1">Waiting for device registration...</p>
    </div>
    <div v-else class="grid gap-2
      grid-cols-1
      sm:grid-cols-2
      md:grid-cols-2
      xl:grid-cols-4
      2xl:grid-cols-5
      3xl:grid-cols-6">

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
import type { Channel } from '@/types/channel';
import DeviceCard from './DeviceCard.vue'
import type { Device } from '@/types/device'

defineProps<{ devices: Device[] }>()

// Для теста — моковые каналы (выходы/входы устройства)
function getChannelsForDevice(unitId: string) {
  // Здесь можешь интегрировать с signalStore, пока отдаём мок:
  if (unitId === 'DO-unit-ABCD') {
    return [
      { index: 0, device_id: unitId, name: 'DO1', type: 'DO', state: false },
      { index: 1, device_id: unitId, name: 'DO2', type: 'DO', state: true },
      { index: 2, device_id: unitId, name: 'DO3', type: 'DO', state: false },
      { index: 3, device_id: unitId, name: 'DO4', type: 'DO', state: false },
      { index: 4, device_id: unitId, name: 'DO5', type: 'DO', state: true },
      { index: 5, device_id: unitId, name: 'DO6', type: 'DO', state: false },
      { index: 6, device_id: unitId, name: 'DO7', type: 'DO', state: false },
      { index: 7, device_id: unitId, name: 'DO8', type: 'DO', state: true },
      { index: 8, device_id: unitId, name: 'DO9', type: 'DO', state: false },
      { index: 9, device_id: unitId, name: 'D10', type: 'DO', state: false },
      { index: 10, device_id: unitId, name: 'D11', type: 'DO', state: true },
      { index: 11, device_id: unitId, name: 'D12', type: 'DO', state: false },
      { index: 12, device_id: unitId, name: 'D13', type: 'DO', state: false },
      { index: 13, device_id: unitId, name: 'D14', type: 'DO', state: true },
      { index: 14, device_id: unitId, name: 'D15', type: 'DO', state: false },
      { index: 15, device_id: unitId, name: 'D16', type: 'DO', state: true },
      { index: 16, device_id: unitId, name: 'D17', type: 'DO', state: true },
      { index: 17, device_id: unitId, name: 'D18', type: 'DO', state: false },
      { index: 18, device_id: unitId, name: 'D19', type: 'DO', state: true },
      { index: 19, device_id: unitId, name: 'D20', type: 'DO', state: false },
      { index: 20, device_id: unitId, name: 'D21', type: 'DO', state: false },
      { index: 21, device_id: unitId, name: 'D22', type: 'DO', state: true },
      { index: 22, device_id: unitId, name: 'D23', type: 'DO', state: false },
      { index: 23, device_id: unitId, name: 'D24', type: 'DO', state: false },
      { index: 24, device_id: unitId, name: 'D25', type: 'DO', state: true },
      { index: 25, device_id: unitId, name: 'D26', type: 'DO', state: false },
      { index: 26, device_id: unitId, name: 'D27', type: 'DO', state: false },
      { index: 27, device_id: unitId, name: 'D28', type: 'DO', state: true },
      { index: 28, device_id: unitId, name: 'D29', type: 'DO', state: false },
      { index: 29, device_id: unitId, name: 'D30', type: 'DO', state: false },
      { index: 30, device_id: unitId, name: 'D31', type: 'DO', state: true },
      { index: 31, device_id: unitId, name: 'D32', type: 'DO', state: false }
    ] as Channel[]
  }
  if (unitId === 'AO-unit-XYZ') {
    return [
      { index: 0, device_id: unitId, name: 'AO1', type: 'AO', state: 12.5 },
      { index: 1, device_id: unitId, name: 'AO2', type: 'AO', state: 4.0 },
      { index: 0, device_id: unitId, name: 'AO3', type: 'AO', state: 12.5 },
      { index: 1, device_id: unitId, name: 'AO4', type: 'AO', state: 4.0 },
      { index: 0, device_id: unitId, name: 'AO5', type: 'AO', state: 12.5 },
      { index: 1, device_id: unitId, name: 'AO6', type: 'AO', state: 4.0 },
      { index: 0, device_id: unitId, name: 'AO7', type: 'AO', state: 12.5 },
      { index: 1, device_id: unitId, name: 'AO8', type: 'AO', state: 4.0 }
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
      { index: 7, device_id: unitId, name: 'DI8', type: 'DI', state: true },
      { index: 0, device_id: unitId, name: 'DI9', type: 'DI', state: true },
      { index: 1, device_id: unitId, name: 'DI10', type: 'DI', state: false },
      { index: 2, device_id: unitId, name: 'DI11', type: 'DI', state: false },
      { index: 3, device_id: unitId, name: 'DI12', type: 'DI', state: true },
      { index: 4, device_id: unitId, name: 'DI13', type: 'DI', state: false },
      { index: 5, device_id: unitId, name: 'DI14', type: 'DI', state: true },
      { index: 6, device_id: unitId, name: 'DI15', type: 'DI', state: false },
      { index: 7, device_id: unitId, name: 'DI16', type: 'DI', state: true },
      { index: 0, device_id: unitId, name: 'DI17', type: 'DI', state: true },
      { index: 1, device_id: unitId, name: 'DI18', type: 'DI', state: false },
      { index: 2, device_id: unitId, name: 'DI19', type: 'DI', state: false },
      { index: 3, device_id: unitId, name: 'DI20', type: 'DI', state: true },
      { index: 4, device_id: unitId, name: 'DI21', type: 'DI', state: false },
      { index: 5, device_id: unitId, name: 'DI22', type: 'DI', state: true },
      { index: 6, device_id: unitId, name: 'DI23', type: 'DI', state: false },
      { index: 7, device_id: unitId, name: 'DI24', type: 'DI', state: true },
      { index: 0, device_id: unitId, name: 'DI25', type: 'DI', state: true },
      { index: 1, device_id: unitId, name: 'DI26', type: 'DI', state: false },
      { index: 2, device_id: unitId, name: 'DI27', type: 'DI', state: false },
      { index: 3, device_id: unitId, name: 'DI28', type: 'DI', state: true },
      { index: 4, device_id: unitId, name: 'DI29', type: 'DI', state: false },
      { index: 5, device_id: unitId, name: 'DI30', type: 'DI', state: true },
      { index: 6, device_id: unitId, name: 'DI31', type: 'DI', state: false },
      { index: 7, device_id: unitId, name: 'DI32', type: 'DI', state: true }
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
