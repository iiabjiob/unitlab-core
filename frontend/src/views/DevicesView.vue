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
        <li
          v-for="device in filteredDevices"
          :key="device.unit_id"
          class="h-full"
        >
          <SelectableCard
            :selected="selection.isSelected('device', device)"
            @click="select(device)"
            >

            <DeviceCard
              :device="device"
              @toggle="onToggle"
              @delete="onDelete"
            />

          </SelectableCard>
        </li>

      </ul>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import { useSelectionStore } from "@/stores/selectionStore"
import DeviceBulkActions from "@/components/devices/DeviceBulkActions.vue"
import DeviceCard from "@/components/devices/DeviceCard.vue"
import SelectableCard from "@/components/ui/SelectableCard.vue"
import type { Device } from "@/types/device"

const deviceStore = useDeviceStore()
const selection = useSelectionStore()

function select(item: Device) {
  selection.select({ type: "device", id: item.unit_id })
}

async function onToggle(item: Device) {
  await deviceStore.toggleDeviceActive(item.unit_id)
}

async function onDelete(item: Device) {
  if (confirm(`Delete device ${item.unit_id}?`)) {
    await deviceStore.deleteDevice(item.unit_id)
  }
}

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
