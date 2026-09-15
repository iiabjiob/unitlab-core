<template>
  <UiModal :open="props.open" :title="props.title" max-width="6xl" @close="emit('close')">
    <template #header>
      <div class="signal-selection-grid-modal__header">
        <div>
          <div class="signal-selection-grid-modal__title">{{ props.title }}</div>
          <div class="signal-selection-grid-modal__summary">
            {{ summaryText }}
          </div>
        </div>
      </div>
    </template>

    <div class="signal-selection-grid-modal__viewport">
      <div
        v-if="workspaceMissing"
        class="signal-selection-grid-modal__empty"
      >
        Select a workspace first.
      </div>

      <div
        v-else-if="loading"
        class="signal-selection-grid-modal__empty"
      >
        Loading signal rows…
      </div>

      <div
        v-else-if="gridRows.length === 0"
        class="signal-selection-grid-modal__empty"
      >
        No matching signals found.
      </div>

      <div v-else class="affino-native-data-grid signal-selection-grid-modal__grid-wrapper">
        <div class="affino-native-data-grid__toolbar signal-selection-grid-modal__toolbar">
          <div class="affino-native-data-grid__toolbar-meta">
            <span class="affino-native-data-grid__stat">Selected: {{ selectedCount }}</span>
            <button
              v-if="selectedCount > 0"
              type="button"
              class="affino-native-data-grid__button signal-selection-grid-modal__clear-button"
              @click="clearSelection"
            >
              Clear selection
            </button>
          </div>
        </div>

        <div class="affino-native-data-grid__shell">
          <DataGrid
            ref="selectionGridRef"
            class="affino-native-data-grid__grid"
            :rows="gridRows"
            :columns="resolvedColumns"
            :theme="theme"
            :grid-lines="gridLines"
            :row-selection-state="rowSelectionState"
            :client-row-model-options="clientRowModelOptions"
            :virtualization="virtualizationOptions"
            :base-row-height="34"
            :row-selection="SIGNAL_GRID_ROW_SELECTION"
            column-menu
            column-layout
            render-mode="virtualization"
            layout-mode="fill"
            :advanced-filter="true"
            row-hover
            striped-rows
            @update:rowSelectionState="handleGridRowSelectionStateUpdate"
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
import { computed, h, nextTick, onBeforeUnmount, ref, watch } from "vue"
import { defineDataGridComponent, parseDataGridSavedView, type DataGridAppCellRendererContext, type DataGridAppColumnInput, type DataGridProps, useDataGridRef, writeDataGridSavedViewToStorage } from "@affino/datagrid-vue-app"

import UiModal from "@/components/ui/UiModal.vue"
import UiButton from "@/components/ui/UiButton.vue"
import { useAffinoDataGridTheme } from "@/components/ui/affinoDataGridTheme"
import "@/components/ui/affinoDataGridNative.css"
import { createLocalSettingsStringStorage, localSettingsKeys } from "@/services/localSettingsStorage"

import type { SignalAllocationRow, SignalIODirection } from "@/types/signal"
import { extractSourceRowFromSignalMetadata, resolveAllSourceColumnHeaders, resolveSourceColumnInitialWidth, resolveSourceColumnMinWidth } from "@/pages/signals/utils/sourceColumns"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"

type GridRow = Record<string, unknown> & {
  signal_id: number
  rowId: string
}

type RowSelectionSnapshot = NonNullable<DataGridProps<GridRow>["rowSelectionState"]>

const DataGrid = defineDataGridComponent<GridRow>()
const SIGNAL_GRID_ROW_SELECTION = { enabled: true, columnWidth: 44 } satisfies NonNullable<DataGridProps<GridRow>["rowSelection"]>

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
  (e: "selection-cleared"): void
}>()

const workspaceStore = useWorkspaceStore()
const signalSheetStore = useSignalSheetStore()
const DATA_FRESHNESS_WINDOW_MS = 20_000

const loading = ref(false)
const selectedRowKeys = ref<string[]>([])
const rowSelectionState = ref<RowSelectionSnapshot | null>(null)
const selectionGridRef = useDataGridRef<GridRow>()
const restoringGridState = ref(false)
const gridStatePersistenceReady = ref(false)
let gridStatePersistTimer: ReturnType<typeof setTimeout> | null = null
const { gridLines, theme } = useAffinoDataGridTheme()
const selectionGridSavedViewStorage = createLocalSettingsStringStorage()

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
  return h("span", { class: "signal-selection-grid-modal__cell" }, formatCell(context.value))
}

const resolvedColumns = computed<DataGridAppColumnInput<GridRow>[]>(() => {
  const sourceColumns: DataGridAppColumnInput<GridRow>[] = sourceColumnHeaders.value.map((header, index) => ({
    key: sourceColumnKey(index),
    label: header,
    minWidth: resolveSourceColumnMinWidth(header),
    initialState: { width: resolveSourceColumnInitialWidth(header) },
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
            "signal-selection-grid-modal__status",
            String(context.value) === "allocated" ? "signal-selection-grid-modal__status--allocated" : "signal-selection-grid-modal__status--unallocated",
          ],
        },
        String(context.value) === "allocated" ? "Allocated" : "Unallocated",
      ),
    },
  ]
})

const clientRowModelOptions: NonNullable<DataGridProps<GridRow>["clientRowModelOptions"]> = {
  resolveRowId: row => rowKey(row),
}

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
      persistGridState()
      selectedRowKeys.value = []
      loading.value = false
      gridStatePersistenceReady.value = false
      return
    }
    if (!shouldRefreshOnOpen()) {
      void restoreGridState()
      return
    }
    void refreshData().then(() => restoreGridState())
  },
)

watch(
  gridRows,
  (rows) => {
    if (selectedRowKeys.value.length === 0) return
    const allowed = new Set(rows.map(row => row.rowId))
    setControlledRowSelection(selectedRowKeys.value.filter(key => allowed.has(key)))
  },
)

function rowKey(row: Record<string, unknown>) {
  return String(row.rowId ?? "")
}

function clearSelection() {
  setControlledRowSelection([])
  emit("selection-cleared")
}

function rowKeysFromSelectionSnapshot(snapshot: RowSelectionSnapshot | null | undefined): string[] {
  const excludedRows = new Set((snapshot?.excludedRows ?? []).map(value => String(value)))
  if (snapshot?.mode === "all") {
    return gridRows.value
      .map(row => row.rowId)
      .filter(key => !excludedRows.has(key))
  }
  return (snapshot?.selectedRows ?? []).map(rowKey => String(rowKey))
}

function setControlledRowSelection(rowKeys: string[], focusedRow: string | number | null = null) {
  selectedRowKeys.value = rowKeys
  rowSelectionState.value = {
    focusedRow: focusedRow !== null && rowKeys.includes(String(focusedRow))
      ? focusedRow
      : (rowKeys[rowKeys.length - 1] ?? null),
    selectedRows: [...rowKeys],
  }
}

function syncSelectedRowKeysFromSnapshot(snapshot: RowSelectionSnapshot | null | undefined) {
  const rowKeys = rowKeysFromSelectionSnapshot(snapshot)
  const nextRowKeys = props.multiple ? rowKeys : rowKeys.slice(-1)
  setControlledRowSelection(nextRowKeys, snapshot?.focusedRow ?? null)
}

function handleGridRowSelectionStateUpdate(snapshot: RowSelectionSnapshot | null) {
  syncSelectedRowKeysFromSnapshot(snapshot)
}

function getSavedViewStorageKey(): string | null {
  const workspaceId = workspaceStore.activeWorkspaceId
  if (!workspaceId) return null
  return localSettingsKeys.signalSelectionGridSavedView(workspaceId, props.tableId)
}

function persistGridState() {
  if (!gridStatePersistenceReady.value || restoringGridState.value) return
  const key = getSavedViewStorageKey()
  const savedView = selectionGridRef.value?.getSavedView?.()
  if (key && savedView) {
    writeDataGridSavedViewToStorage(selectionGridSavedViewStorage, key, savedView)
  }
}

function scheduleGridStatePersist() {
  if (gridStatePersistTimer !== null) clearTimeout(gridStatePersistTimer)
  gridStatePersistTimer = setTimeout(() => {
    gridStatePersistTimer = null
    persistGridState()
  }, 120)
}

function handleGridStateUpdate() {
  if (!restoringGridState.value) scheduleGridStatePersist()
}

async function restoreGridState() {
  restoringGridState.value = true
  gridStatePersistenceReady.value = false
  await nextTick()
  const key = getSavedViewStorageKey()
  const raw = key ? selectionGridSavedViewStorage.getItem(key) : null
  const grid = selectionGridRef.value
  if (raw && grid) {
    const savedView = parseDataGridSavedView<GridRow>(raw, grid.migrateState)
    if (savedView) grid.applySavedView(savedView)
  }
  await nextTick()
  restoringGridState.value = false
  gridStatePersistenceReady.value = true
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
    rowId: row.row_id || `signal-${row.signal_id}`,
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

onBeforeUnmount(() => {
  if (gridStatePersistTimer !== null) {
    clearTimeout(gridStatePersistTimer)
    gridStatePersistTimer = null
  }
})
</script>

<style>
.signal-selection-grid-modal__header {
  align-items: center;
  display: flex;
  gap: 1rem;
  justify-content: space-between;
}

.signal-selection-grid-modal__title {
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  font-weight: 600;
}

.signal-selection-grid-modal__summary {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.signal-selection-grid-modal__viewport {
  height: 62vh;
  min-height: 420px;
}

.signal-selection-grid-modal__empty {
  align-items: center;
  background: var(--color-neutral-50);
  border: 1px dashed var(--color-neutral-300);
  border-radius: var(--radius-lg);
  color: var(--color-neutral-500);
  display: flex;
  font-size: var(--text-sm);
  height: 100%;
  justify-content: center;
}

.signal-selection-grid-modal__grid-wrapper {
  height: 100%;
}

.signal-selection-grid-modal__toolbar {
  min-height: 2rem;
}

.signal-selection-grid-modal__clear-button {
  min-height: 1.5rem;
  padding: 0.25rem 0.5rem;
  font-size: var(--text-2xs);
}

.signal-selection-grid-modal__cell {
  color: var(--color-neutral-700);
  font-size: var(--text-xs);
}

.signal-selection-grid-modal__status {
  font-size: var(--text-xs);
  font-weight: 600;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.signal-selection-grid-modal__status--allocated {
  color: var(--color-emerald-600);
}

.signal-selection-grid-modal__status--unallocated {
  color: var(--color-amber-600);
}

:where(.dark) .signal-selection-grid-modal__title,
:where(.dark) .signal-selection-grid-modal__cell {
  color: var(--color-neutral-100);
}

:where(.dark) .signal-selection-grid-modal__summary,
:where(.dark) .signal-selection-grid-modal__empty {
  color: var(--color-neutral-400);
}

:where(.dark) .signal-selection-grid-modal__empty {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-700);
}

:where(.dark) .signal-selection-grid-modal__status--allocated {
  color: var(--color-emerald-300);
}

:where(.dark) .signal-selection-grid-modal__status--unallocated {
  color: var(--color-amber-300);
}
</style>
