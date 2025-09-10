<template>
  <div class="fixed bottom-0 left-0 right-0 h-2/3 bg-white dark:bg-neutral-900 shadow-2xl border-t border-neutral-300 dark:border-neutral-700 z-50 flex flex-col">
    <!-- Header -->
    <div class="p-2 flex justify-between items-center border-b border-neutral-200 dark:border-neutral-700">
      <h3 class="text-sm font-semibold">Pick {{ kind }} Signal</h3>
      <button class="text-xs px-2 py-1 border rounded" @click="$emit('close')">Close</button>
    </div>

    <!-- Grid -->
    <AgGridVue
      class="flex-1 w-full"
      :rowData="rows"
      :columnDefs="columnDefs"
      rowSelection="single"
      @rowDoubleClicked="onRowPick"
    />
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { useDeviceStore } from "@/stores/deviceStore"
import { AgGridVue } from "ag-grid-vue3"

const props = defineProps<{
  kind: "DI" | "DO"
}>()

const emit = defineEmits<{
  (e: "pick", value: { unitId: string; channel: number }): void
  (e: "close"): void
}>()

const deviceStore = useDeviceStore()

// Flatten signals into rows for AG Grid
const rows = computed(() =>
  deviceStore.devices
    .filter(d => d.type === props.kind)
)

const columnDefs = [
  { headerName: "Device", field: "deviceName", sortable: true, filter: true, flex: 1 },
  { headerName: "Unit ID", field: "unitId", flex: 1 },
  { headerName: "Channel", field: "channel", sortable: true, filter: true, width: 100 },
  { headerName: "Channel Name", field: "channelName", sortable: true, filter: true, flex: 1 },
]

function onRowPick(event: any) {
  emit("pick", { unitId: event.data.unitId, channel: event.data.channel })
}
</script>
