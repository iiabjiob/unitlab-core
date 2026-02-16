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

      <UiAffinoDataGrid
        v-else
        class="h-full min-h-0"
        :rows="gridRows"
        :columns="columns"
        :row-height="34"
        :overscan-rows="8"
        :overscan-columns="2"
        :enable-filtering="true"
        :enable-column-resize="true"
        :show-controls="true"
        :row-key="rowKey"
        :table-id="props.tableId"
        :persist-state="false"
        :empty-text="'No matching signals.'"
        @row-click="handleRowClick"
        @selection-change="handleSelectionChange"
      >
        <template #cell="{ column, value }">
          <span
            v-if="column.key === 'allocation_status'"
            class="text-xs font-semibold uppercase tracking-[0.08em]"
            :class="String(value) === 'allocated' ? 'text-emerald-600 dark:text-emerald-300' : 'text-amber-600 dark:text-amber-300'"
          >
            {{ String(value) === "allocated" ? "Allocated" : "Unallocated" }}
          </span>
          <span v-else class="text-xs text-neutral-700 dark:text-neutral-100">{{ formatCell(value) }}</span>
        </template>
      </UiAffinoDataGrid>
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
import { computed, ref, watch } from "vue"

import UiModal from "@/components/ui/UiModal.vue"
import UiButton from "@/components/ui/UiButton.vue"
import UiAffinoDataGrid from "@/components/ui/UiAffinoDataGrid.vue"

import type { SignalAllocationRow, SignalIODirection } from "@/types/signal"
import { extractSourceRowFromSignalMetadata, resolveAllSourceColumnHeaders } from "@/pages/signals/utils/sourceColumns"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useSignalSheetStore } from "@/stores/signalSheetStore"

type GridRow = Record<string, unknown> & {
  signal_id: number
  rowId: string
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

const columns = computed(() => {
  const sourceColumns = sourceColumnHeaders.value.map((header, index) => ({
    key: sourceColumnKey(index),
    label: header,
    width: Math.min(Math.max(header.length * 11, 140), 360),
    minWidth: 120,
  }))

  return [
    ...sourceColumns,
    { key: "signal_direction", label: "Type", width: 110, minWidth: 90, pin: "right" as const },
    { key: "unit_channel", label: "Unit/Channel", width: 170, minWidth: 130, pin: "right" as const },
    { key: "allocation_status", label: "Status", width: 130, minWidth: 110, pin: "right" as const },
  ]
})

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

function handleSelectionChange(payload: { rowKeys: string[] }) {
  if (props.multiple) {
    selectedRowKeys.value = payload.rowKeys
    return
  }
  selectedRowKeys.value = payload.rowKeys.length > 0 ? [payload.rowKeys[payload.rowKeys.length - 1]] : []
}

function handleRowClick(payload: { row: GridRow }) {
  const rowId = String(payload.row.rowId)
  if (props.multiple) {
    const exists = selectedRowKeys.value.includes(rowId)
    selectedRowKeys.value = exists
      ? selectedRowKeys.value.filter(item => item !== rowId)
      : [...selectedRowKeys.value, rowId]
    return
  }
  selectedRowKeys.value = [rowId]
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
    await Promise.all([
      signalSheetStore.refreshSheet(),
      signalSheetStore.refreshAllocations(),
    ])
  } finally {
    loading.value = false
  }
}

function confirmSelection() {
  if (confirmDisabled.value) return
  emit("confirm", selectedRows.value)
}
</script>
