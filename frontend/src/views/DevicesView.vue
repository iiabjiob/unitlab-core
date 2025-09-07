<template>
  <div class="p-5">
    <div v-if="deviceStore.isLoading">Loading...</div>

    <div v-else-if="!deviceStore.devices.length" class="text-neutral-400">
      No devices yet. Try scanning...
    </div>

    <div v-else>
      <!-- подписываемся на события фильтров -->
      <DeviceBulkActions class="mb-4" @update:filters="filters = $event" />

      <ul
        class="grid gap-5 grid-cols-1 sm:grid-cols-2 lg:grid-cols-[repeat(auto-fill,minmax(300px,1fr))] items-stretch"
      >
        <li v-for="device in filteredDevices" :key="device.unit_id" class="h-full">
          <DeviceComponent :device="device" />
        </li>
      </ul>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import DeviceComponent from "@/components/devices/DeviceComponent.vue"
import DeviceBulkActions from "@/components/devices/DeviceBulkActions.vue"

const deviceStore = useDeviceStore()

// локальное состояние фильтров
const filters = ref<{ onlyOnline: boolean; types: string[] }>({
  onlyOnline: false,
  types: [],
})

// вычисляемый список устройств с фильтрами
const filteredDevices = computed(() =>
  deviceStore.devices.filter((d) => {
    if (filters.value.onlyOnline && d.status !== "online") return false
    if (filters.value.types.length && !filters.value.types.includes(d.type)) return false
    return true
  })
)
</script>
