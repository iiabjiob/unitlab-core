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

  async function updateDeviceField(unitId: string, changes: Partial<Device>) {
    try {
      const { data } = await axios.patch(ApiBuilder.device(unitId), changes)

      const index = devices.value.findIndex(d => d.unit_id === unitId)
      if (index !== -1) {
        devices.value[index] = data
      }

      logger.debug(`✅ Device ${unitId} updated with`, changes)
    } catch (error) {
      logger.error(`💥 Failed to update device ${unitId}:`, error)
    }
  }

  async function toggleDeviceActive(unitId: string) {
    const index = devices.value.findIndex(d => d.unit_id === unitId)
    if (index === -1) return
    await updateDeviceField(unitId, { is_active: !devices.value[index].is_active })
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
      case "do": return "DO"
      case "di": return "DI"
      case "ao": return "AO"
      default:
        logger.warn("⚠️ Unknown device type:", raw)
        return "DO"
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
