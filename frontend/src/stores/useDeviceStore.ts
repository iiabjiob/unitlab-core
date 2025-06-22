import { defineStore } from 'pinia'
import { ref } from 'vue'
import axios from 'axios'
import type { Device } from '@/types/device'
import { ApiBuilder } from '@/utils/api'

export const useDeviceStore = defineStore('deviceStore', () => {

  const devices   = ref<Device[]>([])
  const isLoading = ref<boolean>(false)

  async function fetchDevices() {
    isLoading.value = true
    try {
      const response = await axios.get(ApiBuilder.devices())
      devices.value = response.data
    } catch (error) {
      console.error('❌ Failed to fetch devices:', error)
    } finally {
      isLoading.value = false
    }
  }

  async function toggleDeviceActive(unitId: string) {
    try {
      const { data } = await axios.patch(ApiBuilder.device(unitId))
      const index = devices.value.findIndex(d => d.unit_id === unitId)
      if (index !== -1) {
        devices.value[index].is_active = data.is_active
      }
    } catch (error) {
      console.error('❌ Failed to toggle device active status:', error)
    }
  }

  async function deleteDevice(unitId: string) {
    try {
      await axios.delete(ApiBuilder.device(unitId))

      // ✅ Успешно удалено на сервере — теперь удаляем локально
      const index = devices.value.findIndex(d => d.unit_id === unitId)
      if (index !== -1) {
        devices.value.splice(index, 1)  // Удаляем из массива
      }

    } catch (error) {
      console.error('❌ Failed to delete device:', error)
    }
  }

  return {
    devices,
    isLoading,
    fetchDevices,
    toggleDeviceActive,
    deleteDevice,
  }

})
