import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import axios from 'axios'
import { ApiBuilder } from '@/utils/api'
import type { Device } from '@/types/device'
import type { DeviceRegisterEvent, DeviceHeartbeatEvent } from '@/types/ws/events'
import { getLogger } from '@/utils/logger'
import type { ChannelType } from '@/types/channel'
const logger = getLogger('DEV')

export const useDeviceStore = defineStore('deviceStore', () => {

  const devices   = ref<Device[]>([])
  const isLoading = ref<boolean>(false)

  async function updateDeviceField(deviceId: number, changes: Partial<Device>) {
    try {
      const { data } = await axios.patch(ApiBuilder.device(deviceId), changes)

      const index = devices.value.findIndex(d => d.id === deviceId)
      if (index !== -1) {
        devices.value[index] = {
          ...data,
          type: normalizeType(data.type),
        }
      }

      logger.debug(`✅ Device ${deviceId} updated with`, changes)
    } catch (error) {
      logger.error(`💥 Failed to update device ${deviceId}:`, error)
    }
  }

  async function toggleDeviceActive(deviceId: number) {
    const index = devices.value.findIndex(d => d.id === deviceId)
    if (index === -1) return
    await updateDeviceField(deviceId, { is_active: !devices.value[index].is_active })
  }

  async function deleteDevice(deviceId: number) {
    try {
      await axios.delete(ApiBuilder.device(deviceId))

      // ✅ Успешно удалено на сервере — теперь удаляем локально
      const index = devices.value.findIndex(d => d.id === deviceId)
      if (index !== -1) {
        devices.value.splice(index, 1)  // Удаляем из массива
      }

    } catch (error) {
      logger.error('💥 Failed to delete device:', error)
    }
  }

  function upsertDevice(event: DeviceRegisterEvent) {
    const idx = devices.value.findIndex(d => d.id === event.id)
    const newDevice: Device = {
      ...event,
      type: normalizeType(event.type as unknown as string),
    }

    if (idx !== -1) {
      // обновляем
      devices.value[idx] = { ...devices.value[idx], ...newDevice }
    } else {
      // добавляем
      devices.value.push(newDevice)
    }
  }

  function updateStatus(event: DeviceHeartbeatEvent) {
    const idx = devices.value.findIndex(d => d.unit_id === event.unit_id)
    if (idx !== -1) {
      devices.value[idx].status = event.status
      devices.value[idx].last_seen = event.last_seen
    }
  }

  function normalizeType(raw: string): ChannelType {
    switch (raw.toLowerCase()) {
      case "do": return "do"
      case "di": return "di"
      case "ao": return "ao"
      default:
        logger.warn("⚠️ Unknown device type:", raw)
        return "do"
    }
  }

  const onlineDevices = computed(() => devices.value.filter(d => d.status === "online"))
  const offlineDevices = computed(() => devices.value.filter(d => d.status === "offline"))


  return {
    devices,
    onlineDevices,
    offlineDevices,
    isLoading,
    updateDeviceField,
    toggleDeviceActive,
    deleteDevice,
    upsertDevice,
    updateStatus,
  }

})
