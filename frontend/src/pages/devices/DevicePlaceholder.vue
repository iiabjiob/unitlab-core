<script setup lang="ts">
import { computed, watch } from "vue"
import { useRouter, useRoute } from "vue-router"

import WorkspacePlaceholder from "@/components/ui/WorkspacePlaceholder.vue"
import { useDeviceStore } from "@/stores/deviceStore"
import { useSelectionStore } from "@/stores/selectionStore"

const deviceStore = useDeviceStore()
const selectionStore = useSelectionStore()
const router = useRouter()
const route = useRoute()

selectionStore.restore()

const onlineDevices = computed(() => deviceStore.devices.filter(device => device.online))
const hasOnlineDevices = computed(() => onlineDevices.value.length > 0)

const placeholderTitle = computed(() =>
  hasOnlineDevices.value ? "Select a device" : "No devices are online",
)

const placeholderDescription = computed(() =>
  hasOnlineDevices.value
    ? "Pick any device from the sidebar to inspect its telemetry, diagnostics, and channel configuration. Use the button on the left to register a new device whenever you need one."
    : "None of the devices are currently online. Wait for one to check in or verify the physical connection.",
)

function tryNavigateToOnlineDevice() {
  if (route.name !== "devices.list") return
  const lastId = selectionStore.lastDeviceId
  const lastOnline = lastId ? onlineDevices.value.find(device => device.id === lastId) : null
  if (lastOnline) {
    router.replace({ name: "devices.detail", params: { id: lastOnline.id } })
    return
  }

  const fallback = onlineDevices.value[0]
  if (fallback) {
    router.replace({ name: "devices.detail", params: { id: fallback.id } })
  }
}

watch(onlineDevices, () => {
  tryNavigateToOnlineDevice()
})

watch(
  () => selectionStore.lastDeviceId,
  () => {
    tryNavigateToOnlineDevice()
  },
)

// Attempt immediately as the component mounts (in case data already loaded).
tryNavigateToOnlineDevice()
</script>

<template>
  <WorkspacePlaceholder
    tag="Devices"
    :title="placeholderTitle"
    :description="placeholderDescription"
  />
</template>