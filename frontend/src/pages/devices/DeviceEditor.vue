<script setup lang="ts">
import { computed, onBeforeUnmount, watch } from "vue"
import { useTabsController } from "@affino/tabs-vue"
import { useRoute } from "vue-router"

import { useDeviceStore } from "@/stores/deviceStore"
import { useChannelStore } from "@/stores/channelStore"
import { useSelectionStore } from "@/stores/selectionStore"
import { useRealtimeScopeStore } from "@/stores/realtimeScopeStore"
import { useViewport } from "@/composables/useViewport"

import DeviceEditorHeader from "./components/DeviceEditorHeader.vue"
import DeviceExecutionLog from "./components/DeviceExecutionLog.vue"
import DeviceDiagnosticsPanel from "./components/DeviceDiagnosticsPanel.vue"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import DeviceChannelsList from "./components/DeviceChannelsList.vue"

const route = useRoute()
const store = useDeviceStore()
const channelStore = useChannelStore()
const selectionStore = useSelectionStore()
const realtimeScopeStore = useRealtimeScopeStore()
const scopeId = "devices:editor"

type DeviceDetailTab = "log" | "diag"
const detailTabs = useTabsController<DeviceDetailTab>("log")
const activeDetailTab = computed<DeviceDetailTab>(() => (detailTabs.state.value.value === "diag" ? "diag" : "log"))

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
  <div class="h-full flex flex-col pe-4">

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

      <!-- LOG / DIAGNOSTICS PANEL -->
      <div v-if="device" class="flex flex-1 min-h-0 flex-col overflow-hidden rounded-lg border border-neutral-200 bg-neutral-50 dark:border-neutral-700 dark:bg-neutral-900/60">
        <div class="flex items-center gap-1 p-2 border-b border-neutral-200 dark:border-neutral-700 bg-white/80 dark:bg-neutral-900/70">
          <button
            type="button"
            class="px-3 py-1.5 text-xs rounded-md transition-colors"
            :class="activeDetailTab === 'log'
              ? 'bg-neutral-200 text-neutral-900 dark:bg-neutral-700 dark:text-neutral-50'
              : 'text-neutral-600 hover:bg-neutral-200/60 dark:text-neutral-300 dark:hover:bg-neutral-800'"
            @click="detailTabs.select('log')"
          >
            Log
          </button>
          <button
            type="button"
            class="px-3 py-1.5 text-xs rounded-md transition-colors"
            :class="activeDetailTab === 'diag'
              ? 'bg-neutral-200 text-neutral-900 dark:bg-neutral-700 dark:text-neutral-50'
              : 'text-neutral-600 hover:bg-neutral-200/60 dark:text-neutral-300 dark:hover:bg-neutral-800'"
            @click="detailTabs.select('diag')"
          >
            Diagnostics
          </button>
        </div>
        <div class="flex-1 min-h-0 overflow-hidden">
          <DeviceExecutionLog v-if="activeDetailTab === 'log'" :device="device" />
          <DeviceDiagnosticsPanel v-else :device="device" />
        </div>
      </div>

    </div>

  </div>
</template>
