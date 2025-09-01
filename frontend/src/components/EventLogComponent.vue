<template>

    <!-- Grid container (fills remaining space) -->
      <div class="h-dvh w-full">

        <AgGridVue
          style="height: 100%; width: 100%;"
          :rowData="rows"
          :columnDefs="columnDefs"
          :defaultColDef="defaultColDef"
          :getRowId="getRowId"
          :enableCellTextSelection="true"
          :suppressDragLeaveHidesColumns="true"
          :animateRows="false"
          :autoSizeStrategy="autoSizeStrategy"
          :domLayout="'normal'"
          :pagination="false"
          :theme="theme"
        />
      </div>

</template>

<script setup lang="ts">

import { ref, computed} from "vue"
import { AgGridVue } from "ag-grid-vue3"
import type {
  ColDef,
  GetRowIdParams,
} from "ag-grid-community"
import { themeBalham } from "ag-grid-community"

const theme = themeBalham

import { useEventLogStore } from "@/stores/eventLogStore"
import type { EventLogEntry } from "@/types/eventLog"
import { formatTsFull } from "@/utils/datetime"

// ---- Store ----
const store = useEventLogStore()

// ---- Data mapping ----
const rows = computed(() => {
  // Map store items to rows consumable by the grid
  // Keep everything strongly typed and safe
  return store.items.map((e: EventLogEntry) => ({
    id: e.id,
    ts: e.ts,
    time: formatTsFull(e.ts),
    dir: e.dir,
    source: e.source,
    unit_id: e.unit_id ?? "",
    type: e.type ?? "",
    summary: e.summary,
  }))
})

// ---- Column defs ----
const columnDefs = ref<ColDef[]>([
  // Engineering-friendly ordering
  {
    headerName: "Time",
    field: "time",
    pinned: "left",
    width: 200,
    cellClass: ["font-mono"],
    sort: "asc",
    comparator: (a, b, nodeA, nodeB) => {
      const tsA = nodeA?.data?.ts ?? 0
      const tsB = nodeB?.data?.ts ?? 0
      return tsA === tsB ? 0 : tsA > tsB ? -1 : 1
    }
  },
  {
    headerName: "Dir",
    field: "dir",
    width: 90,
    valueFormatter: p => (p.value === "IN" ? "📥" : "➡️"),
    cellClass: ["text-xs", "uppercase", "font-medium"]
  },
  {
    headerName: "Source",
    field: "source",
    width: 130,
    cellClass: ["text-xs", "uppercase", "font-medium"]
  },
  {
    headerName: "Unit",
    field: "unit_id",
    minWidth: 170,
    cellClass: ["font-mono"]
  },
  {
    headerName: "Type",
    field: "type",
    width: 110,
    cellClass: ["text-xs", "uppercase"]
  },
  {
    headerName: "Summary",
    field: "summary",
    minWidth: 260,
    wrapText: true,
  }
])

// ---- Default col def ----
const defaultColDef: ColDef = {
  sortable: true,
  filter: true,
  resizable: true,
  suppressHeaderMenuButton: false
}

// ---- Auto size strategy ----
const autoSizeStrategy = {
  type: "fitGridWidth",
  defaultMinWidth: 110
} as const

// ---- Helpers ----
function getRowId(p: GetRowIdParams) {
  return p.data.id as string
}

</script>
