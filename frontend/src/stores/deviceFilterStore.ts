// src/stores/deviceFilterStore.ts
import { defineStore } from "pinia"
import { ref, computed } from "vue"
import { useDeviceStore } from "./deviceStore"

export const useDeviceFilterStore = defineStore("deviceFilter", () => {
  const onlyOnline = ref(false)
  const types = ref<string[]>([])

  // удобный объект со всеми фильтрами
  const filters = computed(() => ({
    onlyOnline: onlyOnline.value,
    types: types.value,
  }))

  // отфильтрованные устройства
  const deviceStore = useDeviceStore()
  const filteredDevices = computed(() =>
    deviceStore.devices.filter((d) => {
      if (onlyOnline.value && d.status !== "online") return false
      if (types.value.length && !types.value.includes(d.type)) return false
      return true
    })
  )

  return {
    onlyOnline,
    types,
    filters,
    filteredDevices,
  }
})
