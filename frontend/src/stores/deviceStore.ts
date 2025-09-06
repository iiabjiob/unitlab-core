import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import axios from 'axios'
import { ApiBuilder } from '@/utils/api'
import type { Device } from '@/types/device'
import type { DeviceRegisterEvent, DeviceHeartbeatEvent } from '@/types/ws/events'
import { getLogger } from '@/utils/logger'

const logger = getLogger('DEV')

export const useDeviceStore = defineStore('deviceStore', () => {

  const devices   = ref<Device[]>([])
  const isLoading = ref<boolean>(false)

  async function fetchDevices() {
    isLoading.value = true
    try {
      logger.debug("⏳ Fetching /api/devices ...")
      const response = await axios.get(ApiBuilder.devices())
      logger.debug("✅ Fetched:", response.data)
      devices.value = response.data
    } catch (error) {
      logger.error('💥 Failed to fetch devices:', error)
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
      logger.error('💥 Failed to toggle device active status:', error)
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
      logger.error('💥 Failed to delete device:', error)
    }
  }

  function upsertDevice(event: DeviceRegisterEvent) {
    const idx = devices.value.findIndex(d => d.unit_id === event.unit_id)
    if (idx !== -1) {
      devices.value[idx] = { ...devices.value[idx], ...event }
    }
  }

  function updateStatus(event: DeviceHeartbeatEvent) {
    const idx = devices.value.findIndex(d => d.unit_id === event.unit_id)
    if (idx !== -1) {
      devices.value[idx].status = event.status
      devices.value[idx].last_seen = event.last_seen
    }
  }

  const onlineDevices = computed(() => devices.value.filter(d => d.status === "online"))
  const offlineDevices = computed(() => devices.value.filter(d => d.status === "offline"))

  return {
    devices,
    onlineDevices,
    offlineDevices,
    isLoading,
    fetchDevices,
    toggleDeviceActive,
    deleteDevice,
    upsertDevice,
    updateStatus,
  }

})
