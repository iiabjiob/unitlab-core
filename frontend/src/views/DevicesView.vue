<template>
  <div class="p-5">
    <!-- Keep title only on small screens -->
    <h1 class="text-xl mb-4 md:hidden">Devices</h1>

    <div v-if="deviceStore.isLoading">Loading...</div>

    <div v-else-if="!deviceStore.devices.length" class="text-neutral-400">
      No devices yet. Try scanning...
    </div>

    <!-- Grid on md+; simple list on mobile -->
    <ul v-else class="
    grid gap-5
    grid-cols-1
    sm:grid-cols-2
    lg:grid-cols-[repeat(auto-fill,minmax(300px,1fr))]
    items-stretch
  ">
      <li v-for="device in deviceStore.devices" :key="device.unit_id" class="h-full">
        <DeviceComponent :device="device" />
      </li>
    </ul>
  </div>
</template>


<script setup lang="ts">

import { useDeviceStore } from "@/stores/deviceStore"
import DeviceComponent from "@/components/DeviceComponent.vue"

const deviceStore = useDeviceStore()

</script>
