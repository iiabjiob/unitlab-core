<script setup lang="ts">
import { computed, onBeforeUnmount, watch } from "vue"
import { useRoute } from "vue-router"

import { useDeviceStore } from "@/stores/deviceStore"
import { useChannelStore } from "@/stores/channelStore"
import { useSelectionStore } from "@/stores/selectionStore"
import { useRealtimeScopeStore } from "@/stores/realtimeScopeStore"
import { useViewport } from "@/composables/useViewport"

import DeviceEditorHeader from "./components/DeviceEditorHeader.vue"
import DeviceExecutionLog from "./components/DeviceExecutionLog.vue"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import DeviceChannelsList from "./components/DeviceChannelsList.vue"

const route = useRoute()
const store = useDeviceStore()
const channelStore = useChannelStore()
const selectionStore = useSelectionStore()
const realtimeScopeStore = useRealtimeScopeStore()
const scopeId = "devices:editor"

const deviceId = computed(() => Number(route.params.id))

const device = computed(() =>
  store.devices.find(d => d.id === deviceId.value)
)

watch(
  () => device.value?.id ?? null,
  async (id) => {
    selectionStore.selectDevice(id)
    if (!id) {
      realtimeScopeStore.clearRealtimeUnitScope(scopeId)
      return
    }
    try {
      await channelStore.ensureDeviceChannelsLoaded(id)
    } catch {
      realtimeScopeStore.clearRealtimeUnitScope(scopeId)
      return
    }
    const target = store.devices.find(item => item.id === id)
    if (!target) {
      realtimeScopeStore.clearRealtimeUnitScope(scopeId)
      return
    }
    realtimeScopeStore.setRealtimeUnitScope(scopeId, [target.unit_id])
  },
  { immediate: true }
)

const { isDesktop } = useViewport()

onBeforeUnmount(() => {
  realtimeScopeStore.clearRealtimeUnitScope(scopeId)
})

</script>

<template>
  <div class="h-full flex flex-col">

    <!-- HEADER -->
    <DeviceEditorHeader
      v-if="device"
      :device="device"
    />

    <div class="mt-5 flex flex-1 min-h-0 flex-col gap-4 overflow-hidden rounded bg-white p-4 shadow dark:bg-neutral-800 sm:p-5 lg:flex-row lg:gap-5">

      <!-- CHANNELS LIST -->
      <div
        v-if="device"
        class="flex flex-col min-h-0 lg:flex-none"
      >
        <ResizablePanel
          v-if="isDesktop"
          class="flex flex-col min-h-0 h-full"
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
          v-else
          class="flex-1 min-h-0 overflow-hidden rounded-lg border border-neutral-200 bg-neutral-50 p-4 dark:border-neutral-700 dark:bg-neutral-900"
        >
          <div class="h-full min-h-0 overflow-y-auto">
            <DeviceChannelsList :device="device" />
          </div>
        </div>
      </div>

      <!-- LOG PANEL -->
      <div v-if="device" class="flex flex-1 min-h-0 flex-col overflow-hidden">
        <div class="flex-1 min-h-0 overflow-hidden">
          <DeviceExecutionLog :device="device" />
        </div>
      </div>

    </div>

  </div>
</template>
