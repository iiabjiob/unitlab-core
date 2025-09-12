<template>

  <div class="h-dvh overflow-hidden flex flex-col">
    <AgGridVue
      class="flex-1 w-full"
      :rowData="channels"
      :columnDefs="columnDefs"
      :theme="theme"
      :pagination="false"
      :animateRows="false"
      @cellValueChanged="onCellValueChanged"
    />
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted } from "vue"
import { AgGridVue } from "ag-grid-vue3"
import { useChannelStore } from "@/stores/channelStore"
import type { Channel } from "@/types/channel"

import { themeBalham } from "ag-grid-community"

const theme = themeBalham

onMounted(() => {
  // синхронизация с системной темой
  const mql = window.matchMedia("(prefers-color-scheme: dark)")
  document.documentElement.setAttribute(
    "data-ag-theme-mode",
    mql.matches ? "dark" : "light"
  )
  mql.addEventListener("change", e => {
    document.documentElement.setAttribute(
      "data-ag-theme-mode",
      e.matches ? "dark" : "light"
    )
  })
})

const props = defineProps<{
  deviceId: number
}>()

const channelStore = useChannelStore()

const channels = computed<Channel[]>(() =>
  channelStore.channelsByDevice(props.deviceId)
)

const columnDefs = [
  {
    headerName: "Index",
    field: "index",
    editable: false,
    width: 80,
    valueGetter: (params: any) => params.data.index + 1,
  },
  {
    headerName: "Name",
    field: "name",
    editable: true,
    flex: 1
  },
]

function onCellValueChanged(event: any) {
  const { data, colDef, newValue } = event
  if (colDef.field) {
    channelStore.updateChannelField(data.id, { [colDef.field]: newValue })
  }
}
</script>
