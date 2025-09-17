<template>
  <div class="h-full flex flex-col">
    <!-- Контент -->
    <div class="flex-1 overflow-auto p-3">
      <div v-if="deviceStore.isLoading">Loading...</div>

      <div v-else-if="!filterStore.filteredDevices.length" class="text-neutral-400">
        No devices yet. Try scanning...
      </div>

      <div v-else>
        <ul class="flex flex-wrap gap-5 justify-start">
          <li
            v-for="device in filterStore.filteredDevices"
            :key="device.unit_id"
            class="h-full w-[320px]"
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
  </div>
</template>

<script setup lang="ts">
import { useDeviceStore } from "@/stores/deviceStore"
import { useDeviceFilterStore } from "@/stores/deviceFilterStore"
import { useSelectionStore } from "@/stores/selectionStore"
import DeviceCard from "@/components/devices/DeviceCard.vue"
import SelectableCard from "@/components/ui/SelectableCard.vue"
import type { Device } from "@/types/device"

const deviceStore = useDeviceStore()
const filterStore = useDeviceFilterStore()
const selection = useSelectionStore()

function select(item: Device) {
  selection.select({ type: "device", key: item.unit_id })
}

async function onToggle(item: Device) {
  await deviceStore.toggleDeviceActive(item.id)
}

async function onDelete(item: Device) {
  if (confirm(`Delete device ${item.unit_id}?`)) {
    await deviceStore.deleteDevice(item.id)
  }
}
</script>
