<template>
  <UiModal :open="props.open" :title="props.title" max-width-class="max-w-6xl" @close="emit('close')">
    <template #header>
      <div class="flex items-center justify-between gap-4">
        <div>
          <div class="text-sm font-semibold text-neutral-900 dark:text-neutral-100">{{ props.title }}</div>
          <div class="text-xs text-neutral-500 dark:text-neutral-400">
            {{ summaryText }}
          </div>
        </div>
      </div>
    </template>

    <div class="h-[62vh] min-h-[420px]">
      <div
        v-if="workspaceMissing"
        class="flex h-full items-center justify-center rounded-lg border border-dashed border-neutral-300 bg-neutral-50 text-sm text-neutral-500 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-400"
      >
        Select a workspace first.
      </div>

      <div
        v-else-if="loading"
        class="flex h-full items-center justify-center rounded-lg border border-dashed border-neutral-300 bg-neutral-50 text-sm text-neutral-500 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-400"
      >
        Loading signal rows…
      </div>

      <div
        v-else-if="gridRows.length === 0"
        class="flex h-full items-center justify-center rounded-lg border border-dashed border-neutral-300 bg-neutral-50 text-sm text-neutral-500 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-400"
      >
        No matching signals found.
      </div>

      <div v-else class="affino-native-data-grid h-full">
        <div class="affino-native-data-grid__toolbar">
          <div class="affino-native-data-grid__toolbar-meta">
            <span class="affino-native-data-grid__stat">Selected: {{ selectedCount }}</span>
            <button
              v-if="selectedCount > 0"
              type="button"
              class="affino-native-data-grid__button"
              @click="clearSelection"
            >
              Clear selection
            </button>
          </div>
        </div>

        <div class="affino-native-data-grid__shell">
          <DataGrid
            class="affino-native-data-grid__grid"
            :rows="gridRows"
            :columns="resolvedColumns"
            :theme="theme"
            :state="gridState"
            :client-row-model-options="clientRowModelOptions"
            :virtualization="virtualizationOptions"
            :base-row-height="34"
            :row-selection="true"
            column-menu
            column-layout
            render-mode="virtualization"
            layout-mode="fill"
            row-hover
            striped-rows
            @selection-change="handleGridSelectionChange"
            @update:state="handleGridStateUpdate"
          />
        </div>
      </div>
    </div>

    <template #footer>
      <UiButton variant="ghost" size="sm" @click="emit('close')">
        Cancel
      </UiButton>
      <UiButton
        variant="primary"
        size="sm"
        :disabled="confirmDisabled"
        @click="confirmSelection"
      >
        {{ confirmButtonLabel }}
      </UiButton>
    </template>
  </UiModal>
</template>

<script setup lang="ts">
import { computed, h, ref, watch } from "vue"
import { DataGrid, type DataGridAppCellRendererContext, type DataGridAppColumnInput } from "@affino/datagrid-vue-app"

import UiModal from "@/components/ui/UiModal.vue"
import UiButton from "@/components/ui/UiButton.vue"
import { useAffinoDataGridTheme } from "@/components/ui/affinoDataGridTheme"
import "@/components/ui/affinoDataGridNative.css"

import type { SignalAllocationRow, SignalIODirection } from "@/types/signal"
import { extractSourceRowFromSignalMetadata, resolveAllSourceColumnHeaders } from "@/pages/signals/utils/sourceColumns"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"

type GridRow = Record<string, unknown> & {
  signal_id: number
  rowId: string
}

type RowSelectionSnapshot = {
  focusedRow: string | number | null
  selectedRows: Array<string | number>
}

type UnifiedGridState = {
  version: 1
  rows: unknown
  columns: unknown
  selection: unknown
  rowSelection: RowSelectionSnapshot | null
  transaction: unknown
}

const props = withDefaults(defineProps<{
  open: boolean
  title: string
  allowedDirections?: SignalIODirection[]
  excludeSignalIds?: number[]
  multiple?: boolean
  minSelected?: number
  maxSelected?: number | null
  confirmLabel?: string
  tableId?: string
}>(), {
  allowedDirections: () => [],
  excludeSignalIds: () => [],
  multiple: false,
  minSelected: 1,
  maxSelected: null,
  confirmLabel: "Select",
  tableId: "signal-selection-grid",
})

const emit = defineEmits<{
  (e: "close"): void
  (e: "confirm", rows: SignalAllocationRow[]): void
}>()

const workspaceStore = useWorkspaceStore()
const signalSheetStore = useSignalSheetStore()
const DATA_FRESHNESS_WINDOW_MS = 20_000

const loading = ref(false)
const selectedRowKeys = ref<string[]>([])
const gridState = ref<UnifiedGridState | null>(null)
const { theme } = useAffinoDataGridTheme()

const workspaceMissing = computed(() => !workspaceStore.activeWorkspaceId)

const filteredRows = computed(() => {
  const allowedDirections = new Set(
    (props.allowedDirections ?? []).map(item => String(item).trim().toUpperCase()).filter(Boolean),
  )
  const excludedSignalIds = new Set((props.excludeSignalIds ?? []).map(item => Number(item)).filter(Number.isFinite))

  return signalSheetStore.allocationRows.filter((row) => {
    if (excludedSignalIds.has(row.signal_id)) {
      return false
    }
    if (allowedDirections.size > 0 && !allowedDirections.has(String(row.signal_direction).toUpperCase())) {
      return false
    }
    return true
  })
})

const sourceColumnHeaders = computed(() => (
  resolveAllSourceColumnHeaders(signalSheetStore.sheet, filteredRows.value)
))

function renderDefaultCell(context: DataGridAppCellRendererContext<GridRow>) {
  return h("span", { class: "text-xs text-neutral-700 dark:text-neutral-100" }, formatCell(context.value))
}

const resolvedColumns = computed<DataGridAppColumnInput<GridRow>[]>(() => {
  const sourceColumns: DataGridAppColumnInput<GridRow>[] = sourceColumnHeaders.value.map((header, index) => ({
    key: sourceColumnKey(index),
    label: header,
    minWidth: 120,
    initialState: { width: Math.min(Math.max(header.length * 11, 140), 360) },
    presentation: { align: "left", headerAlign: "left" },
    cellRenderer: renderDefaultCell,
  }))

  return [
    ...sourceColumns,
    {
      key: "signal_direction",
      label: "Type",
      minWidth: 90,
      initialState: { width: 110, pin: "right" as const },
      presentation: { align: "left", headerAlign: "left" },
      cellRenderer: renderDefaultCell,
    },
    {
      key: "unit_channel",
      label: "Unit/Channel",
      minWidth: 130,
      initialState: { width: 170, pin: "right" as const },
      presentation: { align: "left", headerAlign: "left" },
      cellRenderer: renderDefaultCell,
    },
    {
      key: "allocation_status",
      label: "Status",
      minWidth: 110,
      initialState: { width: 130, pin: "right" as const },
      presentation: { align: "left", headerAlign: "left" },
      cellRenderer: (context: DataGridAppCellRendererContext<GridRow>) => h(
        "span",
        {
          class: [
            "text-xs font-semibold uppercase tracking-[0.08em]",
            String(context.value) === "allocated" ? "text-emerald-600 dark:text-emerald-300" : "text-amber-600 dark:text-amber-300",
          ],
        },
        String(context.value) === "allocated" ? "Allocated" : "Unallocated",
      ),
    },
  ]
})

const clientRowModelOptions = computed(() => ({
  resolveRowId: (row: unknown) => rowKey(row as Record<string, unknown>),
}))

const virtualizationOptions = computed(() => ({
  rows: true,
  columns: true,
  rowOverscan: 8,
  columnOverscan: 2,
}))

const gridRows = computed<GridRow[]>(() => (
  filteredRows.value.map((row) => createGridRow(row, sourceColumnHeaders.value))
))

const rowsBySignalId = computed(() => {
  const map = new Map<number, SignalAllocationRow>()
  filteredRows.value.forEach((row) => {
    map.set(row.signal_id, row)
  })
  return map
})

const selectedRows = computed<SignalAllocationRow[]>(() => (
  selectedRowKeys.value
    .map((rowKey) => {
      const signalId = parseSignalId(rowKey)
      return signalId === null ? null : rowsBySignalId.value.get(signalId) ?? null
    })
    .filter((row): row is SignalAllocationRow => row !== null)
))

const selectedCount = computed(() => selectedRows.value.length)

const confirmDisabled = computed(() => {
  if (workspaceMissing.value || loading.value) return true
  const minSelected = Math.max(0, Number(props.minSelected ?? 0))
  const maxSelected = Number.isFinite(props.maxSelected as number)
    ? Math.max(minSelected, Number(props.maxSelected))
    : null
  if (selectedCount.value < minSelected) return true
  if (maxSelected !== null && selectedCount.value > maxSelected) return true
  return false
})

const summaryText = computed(() => {
  const total = gridRows.value.length
  const selected = selectedCount.value
  if (workspaceMissing.value) return "Workspace is not selected"
  if (loading.value) return "Refreshing allocation rows"
  return `${total} rows · ${selected} selected`
})

const confirmButtonLabel = computed(() => {
  if (!props.multiple) return props.confirmLabel
  return `${props.confirmLabel} (${selectedCount.value})`
})

watch(
  () => props.open,
  (open) => {
    if (!open) {
      selectedRowKeys.value = []
      loading.value = false
      return
    }
    if (!shouldRefreshOnOpen()) {
      return
    }
    void refreshData()
  },
)

watch(
  gridRows,
  (rows) => {
    if (selectedRowKeys.value.length === 0) return
    const allowed = new Set(rows.map(row => row.rowId))
    selectedRowKeys.value = selectedRowKeys.value.filter(key => allowed.has(key))
  },
)

function rowKey(row: Record<string, unknown>) {
  return String(row.rowId ?? "")
}

function clearSelection() {
  selectedRowKeys.value = []
  if (gridState.value) {
    gridState.value = {
      ...gridState.value,
      rowSelection: {
        focusedRow: null,
        selectedRows: [],
      },
    }
  }
}

function handleGridStateUpdate(state: UnifiedGridState) {
  gridState.value = state
}

function rowKeysFromSelectionSnapshot(snapshot: RowSelectionSnapshot | null | undefined): string[] {
  return (snapshot?.selectedRows ?? []).map(rowKey => String(rowKey))
}

function syncSelectedRowKeysFromSnapshot(snapshot: RowSelectionSnapshot | null | undefined) {
  const rowKeys = rowKeysFromSelectionSnapshot(snapshot)
  if (props.multiple) {
    selectedRowKeys.value = rowKeys
    return
  }
  selectedRowKeys.value = rowKeys.length > 0 ? [rowKeys[rowKeys.length - 1]] : []
}

function hasUnknownRowSelectionShape(snapshot: unknown): snapshot is RowSelectionSnapshot {
  return Boolean(
    snapshot
    && typeof snapshot === "object"
    && Array.isArray((snapshot as Record<string, unknown>).selectedRows),
  )
}

function handleGridSelectionChange(snapshot?: unknown) {
  if (hasUnknownRowSelectionShape(snapshot)) {
    syncSelectedRowKeysFromSnapshot(snapshot)
    return
  }

  syncSelectedRowKeysFromSnapshot(gridState.value?.rowSelection ?? null)
}

function parseSignalId(rowKey: string): number | null {
  if (!rowKey.startsWith("signal-")) return null
  const numeric = Number(rowKey.slice("signal-".length))
  return Number.isFinite(numeric) ? numeric : null
}

function sourceColumnKey(index: number): string {
  return `source_col_${index}`
}

function createGridRow(row: SignalAllocationRow, headers: readonly string[]): GridRow {
  const payload: GridRow = {
    signal_id: row.signal_id,
    rowId: `signal-${row.signal_id}`,
    signal_direction: row.signal_direction,
    unit_channel: formatUnitChannel(row),
    allocation_status: Number.isFinite(row.channel_id as number) ? "allocated" : "unallocated",
  }
  const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
  headers.forEach((header, index) => {
    payload[sourceColumnKey(index)] = sourceRow[header] ?? ""
  })
  return payload
}

function formatUnitChannel(row: SignalAllocationRow): string {
  if (!Number.isFinite(row.channel_index as number)) {
    return "—"
  }
  const suffix = `ch${Number(row.channel_index) + 1}`
  return row.unit_id?.trim() ? `${row.unit_id}/${suffix}` : suffix
}

function formatCell(value: unknown): string {
  if (value === null || value === undefined) return ""
  if (typeof value === "string") return value
  if (typeof value === "number" || typeof value === "boolean") return String(value)
  try {
    return JSON.stringify(value)
  } catch {
    return String(value)
  }
}

function isFreshTimestamp(value: number | null | undefined, maxAgeMs: number): boolean {
  if (!Number.isFinite(value as number)) return false
  const ageMs = Date.now() - Number(value)
  return ageMs >= 0 && ageMs <= maxAgeMs
}

function shouldRefreshOnOpen(): boolean {
  if (workspaceMissing.value) return false
  if (!signalSheetStore.sheet || signalSheetStore.allocationRows.length === 0) {
    return true
  }
  const sheetFresh = isFreshTimestamp(signalSheetStore.lastSheetLoadedAt, DATA_FRESHNESS_WINDOW_MS)
  const allocationsFresh = isFreshTimestamp(signalSheetStore.lastAllocationsLoadedAt, DATA_FRESHNESS_WINDOW_MS)
  return !(sheetFresh && allocationsFresh)
}

async function refreshData() {
  if (workspaceMissing.value) return
  loading.value = true
  try {
    await runStoreBootstrap(
      ["signal-selection-grid", workspaceStore.activeWorkspaceId],
      [
        () => signalSheetStore.refreshSheet(),
        () => signalSheetStore.refreshAllocations(),
      ],
      { mode: "settled" },
    )
  } finally {
    loading.value = false
  }
}

function confirmSelection() {
  if (confirmDisabled.value) return
  emit("confirm", selectedRows.value)
}
</script>
