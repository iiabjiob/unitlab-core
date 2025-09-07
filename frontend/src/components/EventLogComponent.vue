<template>

  <!-- Grid container (fills remaining space) -->
  <div class="h-dvh w-full">

    <!-- Toolbar -->
    <div class="p-2 flex gap-2 justify-end">
      <ButtonComponent type="secondary" @click="exportCsv">
        Export CSV
      </ButtonComponent>
    </div>

    <AgGridVue style="height: 100%; width: 100%;" :rowData="rows" :columnDefs="columnDefs"
      :defaultColDef="defaultColDef" :getRowId="getRowId" :enableCellTextSelection="true"
      :suppressDragLeaveHidesColumns="true" :animateRows="false" :autoSizeStrategy="autoSizeStrategy"
      :domLayout="'normal'" :pagination="false" :theme="theme" @grid-ready="onGridReady" />
  </div>

</template>

<script setup lang="ts">

import { ref, computed, onMounted, watch } from "vue"
import { AgGridVue } from "ag-grid-vue3"
import type {
  ColDef,
  GetRowIdParams,
  GridApi,
  GridReadyEvent,
} from "ag-grid-community"
import { themeBalham } from "ag-grid-community"

const theme = themeBalham

let gridApi: GridApi | null = null

function onGridReady(params: GridReadyEvent) {
  gridApi = params.api
}

import { useEventLogStore } from "@/stores/eventLogStore"
import type { EventLogEntry } from "@/types/eventLog"
import { formatTsFull } from "@/utils/datetime"
import ButtonComponent from "./ui/ButtonComponent.vue"


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

function exportCsv() {
  gridApi?.exportDataAsCsv({
    fileName: "event-log.csv",
    exportedRows: "filteredAndSorted", // 👈 учесть фильтры и сортировку
    allColumns: false,                 // только видимые колонки
    columnSeparator: ";",              // если нужен ; вместо ,
    prependContent: [
      [
        { data: { value: "UnitLab Event Log Export" }, mergeAcross: 3 }
      ],
      [
        { data: { value: "Generated: " + new Date().toLocaleString() } }
      ]
    ],
    appendContent: "\n--- End of Report ---",
    processCellCallback: params => {
      if (params.column.getColId() === "time") {
        // Пример: выгружать timestamp вместо formatTsFull
        return params.node?.data?.ts?.toString() ?? ""
      }
      return params.value
    }
  })
}

// ---- Store ----
const store = useEventLogStore()

// ---- Data mapping ----
const rows = computed(() => {
  // Map store items to rows consumable by the grid
  // Keep everything strongly typed and safe
  return store.items.map((e: EventLogEntry , idx: number) => ({
    row_id: `${e.unit_id}-${e.ts}-${idx}`,
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
  return p.data.row_id
}

</script>
