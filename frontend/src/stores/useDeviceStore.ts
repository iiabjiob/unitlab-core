import { defineStore } from 'pinia'
import { useWebSocketStore } from './useWebsocketStore'
import { ref, watch } from 'vue'
import axios from 'axios'
import type { Device } from '@/types/device'

import { TopicBuilder } from '@/utils/mqtt'
import { ApiBuilder } from '@/utils/api'

export const useDeviceStore = defineStore('deviceStore', () => {

  const wsStore = useWebSocketStore()

  const devices = ref<Device[]>([])
  const isLoading = ref<boolean>(false)

  // ✅ Публичные методы для компонентов:

  async function fetchDevices() {
    isLoading.value = true  // ✅ старт загрузки
    try {
      const response = await axios.get(ApiBuilder.devices())
      devices.value = response.data
    } catch (error) {
      console.error('❌ Failed to fetch devices:', error)
    } finally {
      isLoading.value = false  // ✅ конец загрузки
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

  function requestScan() {
    wsStore.send({
      action: 'publish',
      topic: TopicBuilder.deviceScan(),
      payload: {}
    })
  }

  function upsertDevice(newDevice: Device) {
    const index = devices.value.findIndex(d => d.unit_id === newDevice.unit_id)
    if (index !== -1) {
      // Обновляем существующее устройство
      devices.value[index] = { ...devices.value[index], ...newDevice }
    } else {
      // Добавляем новое устройство
      const deviceWithNewFlag = { ...newDevice, isNew: true }
      devices.value.push(deviceWithNewFlag)

      // Через 3 секунды убрать подсветку нового устройства
      setTimeout(() => {
        const found = devices.value.find(d => d.unit_id === deviceWithNewFlag.unit_id)
        if (found) {
          found.isNew = false
        }
      }, 3000)
    }
  }

  return {
    requestScan,
    upsertDevice,
    devices,
    isLoading,
    fetchDevices,
    toggleDeviceActive,
    deleteDevice,
  }

})
