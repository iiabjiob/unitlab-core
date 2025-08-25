<template>
  <div class="flex flex-col h-full">
    <ag-grid-vue
      class="flex-1"
      :theme="theme"
      :rowData="rowData"
      :columnDefs="colDefs"
      :defaultColDef="defaultColDef"
      :suppressColumnVirtualisation="true"
      :suppressRowVirtualisation="true"
      :animateRows="false"
      :pagination="false"
      @grid-ready="onGridReady"
    />
  </div>
</template>

<script lang="ts" setup>
import { ref, shallowRef } from "vue";
import { AgGridVue } from "ag-grid-vue3";
import { themeBalham, type ColDef, type RowSelectionOptions, type GridApi, type GridReadyEvent, colorSchemeDark } from "ag-grid-community";

// Row Data: The data to be displayed.
const rowData = ref([

]);

const colDefs = ref([
        { field: "unit_id" },
        { field: "channel" },
        { field: "control" },
        { field: "status" },
        { field: "terminal" },
        { field: "bay" },
        { field: "signal_name" },
        { field: "hmi_presentation_text" },
        { field: "signal_type" },
        { field: "group" },
        { field: "reaction matrix" },
        { field: "external_address" },
        { field: "test_result" },
        { field: "tested_at" }
    ]);

const defaultColDef = ref<ColDef>({
  filter: "agTextColumnFilter",
  floatingFilter: true,
  sortable: true,
});

// const rowSelection = ref<RowSelectionOptions>({
//   mode: "multiRow",
//   headerCheckbox: false
// });

const gridApi = shallowRef<GridApi | null>(null);

const theme = themeBalham.withParams({
  wrapperBorder: false,
  headerRowBorder: true,
  fontSize: "0.7rem",
  rowBorder: { style: "solid", color: "#9696C8" },
  columnBorder: { style: "solid", color: "#9696C8" }
}).withPart(colorSchemeDark);

function onGridReady(params: GridReadyEvent) {
  gridApi.value = params.api;
}
</script>
