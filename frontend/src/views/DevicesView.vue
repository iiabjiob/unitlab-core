<template>
  <div>
    <!-- <PageHeader title="Devices" /> -->

    <!-- Индикатор загрузки -->
    <LoadingSpinner size="small" position="left" v-if="deviceStore.isLoading" />

    <!-- Если устройств нет -->
    <AlertComponent v-else-if="!deviceStore.devices.length" type="warning" message="No devices found." />

    <!-- Таблица устройств -->
    <DeviceList
      v-else
      :devices="deviceStore.devices"
      @toggle="toggle"
      @delete="confirmDelete"
      @delete-multiple="confirmDeleteMultiple"
    />
  </div>
</template>

<script setup lang="ts">

import { onMounted, onUnmounted } from 'vue'

import { useDeviceStore } from '@/stores/useDeviceStore'
import AlertComponent from '@/components/ui/AlertComponent.vue'
import PageHeader from '@/components/PageHeader.vue'
import DeviceList from '@/components/DeviceList.vue'
import LoadingSpinner from '@/components/LoadingSpinner.vue'

const deviceStore = useDeviceStore()

function toggle(unitId: string) {
  deviceStore.toggleDeviceActive(unitId)
}

function confirmDelete(unitId: string) {
  if (confirm(`Delete Device ${unitId}?`)) {
    deviceStore.deleteDevice(unitId)
  }
}

function confirmDeleteMultiple(unitIds: string[]) {
  if (confirm(`Delete selected devices (${unitIds.length})?`)) {
    unitIds.forEach(deviceStore.deleteDevice)
  }
}

onMounted(async () => {
  // Subscribe to device events via WebSocket (to catch new registrations)
  deviceStore.subscribeToDeviceEvents()

  // Fetch initial list of devices via REST API
  await deviceStore.fetchDevices()

  // Trigger a scan to discover devices via WS (will call upsertDevice)
  deviceStore.requestScan()
})

onUnmounted(() => {
  deviceStore.unsubscribeFromDeviceEvents()
})

</script>
