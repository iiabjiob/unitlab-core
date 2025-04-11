// stores/devices.ts
import { defineStore } from 'pinia'

import { ref } from 'vue'

export const useDevicesStore = defineStore('devices', () => {
  const foundDevices = ref<string[]>([])
  const selectedDevices = ref<string[]>([])

  const setFoundDevices = (devices: string[]) => {
    foundDevices.value = devices
  }

  const setSelectedDevices = (devices: string[]) => {
    selectedDevices.value = devices
  }

  return {
    foundDevices,
    selectedDevices,
    setFoundDevices,
    setSelectedDevices
  }
})
