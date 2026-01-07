<script setup lang="ts">
import { ref, computed } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import { useRouter, useRoute } from "vue-router"
import DeviceListItem from "./DeviceListItem.vue"

const store = useDeviceStore()
const router = useRouter()
const route = useRoute()

function isActive(id: number) {
  return Number(route.params.id) === id
}

function openDevice(id: number) {
  router.push(`/devices/${id}`)
}


// SEARCH
const query = ref("")

const filteredDevices = computed(() => {
  if (!query.value.trim()) return store.devices

  const q = query.value.toLowerCase()

  return store.devices.filter(s =>
    s.unit_id.toLowerCase().includes(q) ||
    (s.name && s.name.toLowerCase().includes(q))
  )
})
</script>

<template>
  <div class="h-full flex flex-col">

    <!-- SEARCH FIELD -->
    <div class="mb-3">
      <input
        v-model="query"
        type="text"
        name="device-search"
        placeholder="Search devices…"
        class="w-full rounded-lg border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 placeholder-neutral-500 focus:outline-none dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
      />
    </div>

    <!-- LIST -->
    <div class="flex-1 overflow-y-auto space-y-1">
      <div
        v-for="device in filteredDevices"
        :key="device.id"
      >
        <DeviceListItem
          :device="device"
          :active="isActive(device.id)"
          @click="openDevice(device.id)"
        />
      </div>

      <div
        v-if="filteredDevices.length === 0"
        class="text-gray-500 text-xs italic px-2 py-2"
      >
        No devices found
      </div>
    </div>

  </div>
</template>
