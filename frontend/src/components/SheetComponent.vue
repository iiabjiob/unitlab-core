<template>
  <div class="flex flex-col h-full">
    <ag-grid-vue
      class="flex-1"
      :theme="theme"
      :rowData="rowData"
      :columnDefs="columnDefs"
      :defaultColDef="defaultColDef"
      :rowSelection="rowSelection"
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
import { themeBalham, type ColDef, type RowSelectionOptions, type GridApi, type GridReadyEvent } from "ag-grid-community";

// Генерируем excel-style буквы для заголовков колонок (A, B, ..., Z, AA, AB, ...)
function getExcelColumnLabel(index: number): string {
  let label = '';
  let n = index;
  while (n >= 0) {
    label = String.fromCharCode((n % 26) + 65) + label;
    n = Math.floor(n / 26) - 1;
  }
  return label;
}

// Генерируем массив заголовков нужной длины
function getExcelColumnLabels(count: number): string[] {
  return Array.from({ length: count }, (_, i) => getExcelColumnLabel(i));
}

// Количество колонок и строк (можно сделать пропсами)
const NUM_COLS = 20;   // количество колонок (A, B, ..., T)
const NUM_ROWS = 50;   // количество строк

const colLetters = getExcelColumnLabels(NUM_COLS);

// Первая колонка — номер строки (как в Excel)
const columnDefs = ref<ColDef[]>([
  {
    headerName: "№",
    field: "_rowNumber",
    pinned: "left",
    width: 60,
    suppressMovable: true,
    editable: false,
    cellClass: "text-gray-400 text-center",
    // Показ только номера строки, не меняется
    valueGetter: params => (params.node?.rowIndex != null ? params.node.rowIndex + 1 : ''),
    sortable: false,
    filter: false,
    lockVisible: true,
  },
  ...colLetters.map(letter => ({
    field: letter,
    headerName: letter,
    editable: true,
    filter: "agTextColumnFilter"
  }))
]);

// rowData — строки с ключами A, B, ... и пустыми значениями
const rowData = ref(
  Array.from({ length: NUM_ROWS }, (_, rowIndex) => {
    const row: Record<string, string> = {};
    colLetters.forEach(letter => {
      row[letter] = "";
    });
    // _rowNumber для удобства, но не обязателен, т.к. valueGetter считает сам
    row["_rowNumber"] = (rowIndex + 1).toString();
    return row;
  })
);

const defaultColDef = ref<ColDef>({
  filter: "agTextColumnFilter",
  floatingFilter: true,
  sortable: true
});

const rowSelection = ref<RowSelectionOptions>({
  mode: "multiRow",
  headerCheckbox: false
});

const gridApi = shallowRef<GridApi | null>(null);

const theme = themeBalham.withParams({
  wrapperBorder: false,
  headerRowBorder: true,
  rowBorder: { style: "solid", color: "#9696C8" },
  columnBorder: { style: "solid", color: "#9696C8" }
});

function onGridReady(params: GridReadyEvent) {
  gridApi.value = params.api;
}
</script>
