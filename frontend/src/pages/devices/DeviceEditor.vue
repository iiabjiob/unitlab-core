<script setup lang="ts">
import { computed, watch } from "vue"
import { useRoute } from "vue-router"

import { useDeviceStore } from "@/stores/deviceStore"
import { useSelectionStore } from "@/stores/selectionStore"
import { useViewport } from "@/composables/useViewport"

import DeviceEditorHeader from "./components/DeviceEditorHeader.vue"
import DeviceExecutionLog from "./components/DeviceExecutionLog.vue"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import DeviceChannelsList from "./components/DeviceChannelsList.vue"

const route = useRoute()
const store = useDeviceStore()
const selectionStore = useSelectionStore()

const deviceId = computed(() => Number(route.params.id))

const device = computed(() =>
  store.devices.find(d => d.id === deviceId.value)
)

watch(
  () => device.value?.id ?? null,
  (id) => {
    selectionStore.selectDevice(id)
  },
  { immediate: true }
)

const { isDesktop } = useViewport()

</script>

<template>
  <div class="h-full flex flex-col">

    <!-- HEADER -->
    <DeviceEditorHeader
      v-if="device"
      :device="device"
    />

    <div class="mt-5 flex flex-1 flex-col gap-4 overflow-hidden rounded bg-white p-4 shadow dark:bg-neutral-800 sm:p-5">

      <!-- CHANNELS LIST -->
      <ResizablePanel
        v-if="device && isDesktop"
        :device="device"
        placement="left"
        storageKey="device-channels-list-width"
        :minSize="380"
        :defaultSize="380"
        :maxSize="800"
      >
        <DeviceChannelsList :device="device" />
      </ResizablePanel>

      <div
        v-else-if="device"
        class="rounded-lg border border-neutral-200 bg-neutral-50 p-4 dark:border-neutral-700 dark:bg-neutral-900"
      >
        <DeviceChannelsList :device="device" />
      </div>
      

      <!-- LOG PANEL -->
      <div v-if="device" class="flex-1 overflow-y-auto">
        <DeviceExecutionLog :device="device" />
      </div>

    </div>

  </div>
</template>
