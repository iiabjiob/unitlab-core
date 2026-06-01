<template>
  <div class="iec61850-diagnostics-grid">
    <div class="iec61850-diagnostics-grid__toolbar">
      <div class="iec61850-diagnostics-grid__counts">
        <span>{{ totalRows }} diagnostics</span>
      </div>
      <div class="iec61850-diagnostics-grid__counts">
        <span>Use the grid toolbar search</span>
      </div>
    </div>

    <div v-if="!totalRows" class="iec61850-diagnostics-grid__empty">
      No parser diagnostics.
    </div>

    <div v-else class="affino-native-data-grid iec61850-diagnostics-grid__grid-shell">
      <div class="affino-native-data-grid__shell iec61850-diagnostics-grid__shell">
        <DataGrid
          class="affino-native-data-grid__grid iec61850-diagnostics-grid__grid"
          :rows="gridRows"
          :columns="columns"
          :theme="theme"
          :grid-lines="gridLines"
          :client-row-model-options="clientRowModelOptions"
          :virtualization="virtualizationOptions"
          :row-selection="false"
          :base-row-height="34"
          :column-menu="true"
          :column-layout="true"
          :quick-filter="quickFilter"
          :hidden-column-keys="hiddenColumnKeys"
          render-mode="virtualization"
          layout-mode="fill"
          row-hover
          striped-rows
        />
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, h } from "vue"
import { defineDataGridComponent, type DataGridAppCellRendererContext, type DataGridAppColumnInput, type DataGridProps } from "@affino/datagrid-vue-app"

import { useAffinoDataGridTheme } from "@/components/ui/affinoDataGridTheme"
import "@/components/ui/affinoDataGridNative.css"
import type { ScdDiagnostic } from "@/modules/scd-sld-core"

type DiagnosticRow = {
  rowId: string
  severity: ScdDiagnostic["severity"]
  stage: string
  code: string
  message: string
  sourcePath: string
  sourceId: string
  location: string
  index: number
}

const DataGrid = defineDataGridComponent<DiagnosticRow>()
const props = defineProps<{ diagnostics: readonly ScdDiagnostic[] }>()
const { gridLines, theme } = useAffinoDataGridTheme()

const totalRows = computed(() => props.diagnostics.length)

const gridRows = computed<DiagnosticRow[]>(() => props.diagnostics
  .map((diagnostic, index) => ({ diagnostic, index }))
  .sort((left, right) => compareDiagnostics(left.diagnostic, right.diagnostic, left.index, right.index))
  .map(({ diagnostic, index }) => ({
    rowId: buildRowId(diagnostic, index),
    severity: diagnostic.severity,
    stage: diagnostic.stage,
    code: diagnostic.code,
    message: diagnostic.message,
    sourcePath: diagnostic.sourcePath ?? "",
    sourceId: diagnostic.sourceId ?? "",
    location: formatLocation(diagnostic.sourceLocation),
    index,
  })))

const quickFilter = {
  placeholder: "Search diagnostics",
  columns: ["severity", "stage", "code", "message", "sourcePath", "sourceId", "location"],
}

const hiddenColumnKeys = ["sourceId"]

const clientRowModelOptions: NonNullable<DataGridProps<DiagnosticRow>["clientRowModelOptions"]> = {
  resolveRowId: row => row.rowId,
}

const virtualizationOptions = computed(() => ({
  rows: true,
  columns: true,
  rowOverscan: 10,
  columnOverscan: 2,
}))

const columns = computed<DataGridAppColumnInput<DiagnosticRow>[]>(() => [
  {
    key: "severity",
    label: "Severity",
    minWidth: 100,
    initialState: { width: 110 },
    presentation: { align: "left", headerAlign: "left" },
    cellRenderer: (context: DataGridAppCellRendererContext<DiagnosticRow>) => h(
      "span",
      { class: ["iec61850-diagnostics-grid__severity", `is-${context.row?.severity ?? "info"}`] },
      String(context.row?.severity ?? "info"),
    ),
  },
  {
    key: "stage",
    label: "Stage",
    minWidth: 110,
    initialState: { width: 120 },
    presentation: { align: "left", headerAlign: "left" },
    cellRenderer: ({ row }) => h("span", { class: "iec61850-diagnostics-grid__cell" }, String(row?.stage ?? "—")),
  },
  {
    key: "code",
    label: "Code",
    minWidth: 220,
    initialState: { width: 240 },
    presentation: { align: "left", headerAlign: "left" },
    cellRenderer: ({ row }) => h("code", { class: "iec61850-diagnostics-grid__code" }, String(row?.code ?? "—")),
  },
  {
    key: "message",
    label: "Message",
    flex: 1,
    minWidth: 260,
    initialState: { width: 480 },
    presentation: { align: "left", headerAlign: "left" },
    cellRenderer: ({ row }) => h("span", { class: "iec61850-diagnostics-grid__message" }, String(row?.message ?? "—")),
  },
  {
    key: "sourcePath",
    label: "Source path",
    flex: 1,
    minWidth: 300,
    initialState: { width: 420 },
    presentation: { align: "left", headerAlign: "left" },
    cellRenderer: ({ row }) => h("span", { class: "iec61850-diagnostics-grid__path" }, String(row?.sourcePath ?? "—")),
  },
  {
    key: "sourceId",
    label: "Source ID",
    minWidth: 220,
    initialState: { width: 260 },
    presentation: { align: "left", headerAlign: "left" },
    cellRenderer: ({ row }) => h("span", { class: "iec61850-diagnostics-grid__cell" }, String(row?.sourceId ?? "—")),
  },
  {
    key: "location",
    label: "Location",
    minWidth: 90,
    initialState: { width: 110 },
    presentation: { align: "right", headerAlign: "right" },
    cellRenderer: ({ row }) => h("span", { class: "iec61850-diagnostics-grid__cell" }, String(row?.location ?? "—")),
  },
])

function buildRowId(diagnostic: ScdDiagnostic, index: number): string {
  return [
    diagnostic.severity,
    diagnostic.stage,
    diagnostic.code,
    diagnostic.sourceId ?? "",
    diagnostic.sourcePath ?? "",
    diagnostic.sourceLocation?.line ?? "",
    diagnostic.sourceLocation?.column ?? "",
    index,
  ].join("|")
}

function compareDiagnostics(
  left: ScdDiagnostic,
  right: ScdDiagnostic,
  leftIndex: number,
  rightIndex: number,
): number {
  const severityOrder = diagnosticSeverityOrder(left.severity) - diagnosticSeverityOrder(right.severity)
  if (severityOrder !== 0) return severityOrder

  const stageOrder = left.stage.localeCompare(right.stage)
  if (stageOrder !== 0) return stageOrder

  const codeOrder = left.code.localeCompare(right.code)
  if (codeOrder !== 0) return codeOrder

  const pathOrder = (left.sourcePath ?? "").localeCompare(right.sourcePath ?? "")
  if (pathOrder !== 0) return pathOrder

  const lineOrder = (left.sourceLocation?.line ?? Number.POSITIVE_INFINITY) - (right.sourceLocation?.line ?? Number.POSITIVE_INFINITY)
  if (lineOrder !== 0) return lineOrder

  const columnOrder = (left.sourceLocation?.column ?? Number.POSITIVE_INFINITY) - (right.sourceLocation?.column ?? Number.POSITIVE_INFINITY)
  if (columnOrder !== 0) return columnOrder

  return leftIndex - rightIndex
}

function diagnosticSeverityOrder(severity: ScdDiagnostic["severity"]): number {
  switch (severity) {
    case "error": return 0
    case "warning": return 1
    case "info": return 2
    default: return 3
  }
}

function formatLocation(location: ScdDiagnostic["sourceLocation"]): string {
  if (!location) return ""
  return `${location.line}:${location.column}`
}
</script>

<style scoped>
.iec61850-diagnostics-grid {
  display: flex;
  min-width: 0;
  width: 100%;
  flex-direction: column;
  gap: 12px;
}

.iec61850-diagnostics-grid__toolbar {
  display: flex;
  justify-content: space-between;
  gap: 16px;
  align-items: end;
  flex-wrap: wrap;
  width: 100%;
}

.iec61850-diagnostics-grid__counts {
  display: flex;
  gap: 12px;
  align-items: center;
  font-size: 12px;
  color: var(--color-neutral-500);
  letter-spacing: 0.02em;
  text-transform: uppercase;
}

.iec61850-diagnostics-grid__empty {
  border: 1px dashed color-mix(in srgb, var(--color-neutral-300) 80%, transparent);
  border-radius: 12px;
  padding: 16px;
  color: var(--color-neutral-500);
  background: color-mix(in srgb, var(--color-surface-1) 96%, transparent);
}

.iec61850-diagnostics-grid__grid-shell {
  min-height: 24rem;
  height: clamp(24rem, 54vh, 42rem);
  min-width: 0;
  width: 100%;
  max-width: 100%;
  align-self: stretch;
}

.iec61850-diagnostics-grid__shell {
  overflow: hidden;
  min-width: 0;
  width: 100%;
  max-width: 100%;
  align-self: stretch;
}

.iec61850-diagnostics-grid__grid {
  height: 100%;
  min-width: 0;
  width: 100%;
}

.iec61850-diagnostics-grid__severity {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-width: 72px;
  padding: 4px 10px;
  border-radius: 999px;
  font-size: 12px;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.04em;
}

.iec61850-diagnostics-grid__severity.is-error {
  color: #7f1d1d;
  background: #fee2e2;
}

.iec61850-diagnostics-grid__severity.is-warning {
  color: #92400e;
  background: #fef3c7;
}

.iec61850-diagnostics-grid__severity.is-info {
  color: #0f766e;
  background: #ccfbf1;
}

.iec61850-diagnostics-grid__cell,
.iec61850-diagnostics-grid__message,
.iec61850-diagnostics-grid__path,
.iec61850-diagnostics-grid__code {
  display: block;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.iec61850-diagnostics-grid__code {
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  font-size: 12px;
}
</style>
