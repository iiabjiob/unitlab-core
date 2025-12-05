<template>
  <div class="h-full flex flex-col">
    <div class="p-3">
      <DevicesToolbar/>
    </div>
    <!-- Контент -->
    <div class="flex-1 overflow-auto p-3">
      <div v-if="deviceStore.isLoading">Loading...</div>

      <div v-else-if="!filterStore.filteredDevices.length" class="text-neutral-400">
        No devices yet.
      </div>

      <div v-else>
        <ul class="flex flex-col gap-4">
          <li
            v-for="device in filterStore.filteredDevices"
            :key="device.unit_id"
            class="w-full"
          >
            <DeviceCard
              :device="device"
              @toggle="onToggle"
              @delete="onDelete"
              class="w-full"
            />
          </li>
        </ul>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { useDeviceStore } from "@/stores/deviceStore"
import { useDeviceFilterStore } from "@/stores/deviceFilterStore"
import DeviceCard from "@/components/devices/DeviceCard.vue"
import type { Device } from "@/types/device"
import DevicesToolbar from "@/components/toolbars/DevicesToolbar.vue"

const deviceStore = useDeviceStore()
const filterStore = useDeviceFilterStore()

async function onToggle(item: Device) {
  await deviceStore.toggleDeviceActive(item.id)
}

async function onDelete(item: Device) {
  if (confirm(`Delete device ${item.unit_id}?`)) {
    await deviceStore.deleteDevice(item.id)
  }
}
</script>
