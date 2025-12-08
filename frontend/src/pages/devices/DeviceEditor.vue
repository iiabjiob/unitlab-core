<script setup lang="ts">
import { computed } from "vue"
import { useRoute } from "vue-router"

import { useDeviceStore } from "@/stores/deviceStore"

import DeviceEditorHeader from "./components/DeviceEditorHeader.vue"
import DeviceExecutionLog from "./components/DeviceExecutionLog.vue"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"

const route = useRoute()
const store = useDeviceStore()

const deviceId = computed(() => Number(route.params.id))

const device = computed(() =>
  store.devices.find(d => d.id === deviceId.value)
)

</script>

<template>
  <div class="h-full flex flex-col">

    <!-- HEADER -->
    <DeviceEditorHeader
      v-if="device"
      :device="device"
    />

    <div class="flex flex-1 overflow-hidden bg-white dark:bg-neutral-800 shadow rounded p-5">

      <!-- STEP LIST -->
      <ResizablePanel
        v-if="device" :device="device"
        placement="left"
        storageKey="device-steps-list-width"
        :minSize="380"
        :defaultSize="380"
        :maxSize="800"
        >
        <DeviceChannelsList :device="device" />
      </ResizablePanel>
      

      <!-- LOG PANEL -->
      <div v-if="device" class="flex-1 overflow-y-auto" >
        <DeviceExecutionLog :device="device"/>
      </div>

    </div>

  </div>
</template>
