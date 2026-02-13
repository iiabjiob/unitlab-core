<template>
  <div class="flex h-full min-h-0 min-w-0 flex-col gap-4">
    <div
      v-if="!selectedSnapshotId"
      class="flex flex-1 items-center justify-center rounded-2xl border border-dashed border-neutral-300 bg-white/80 p-8 text-sm text-neutral-500 dark:border-neutral-700 dark:bg-neutral-900/40 dark:text-neutral-400"
    >
      Select a snapshot to configure allocation
    </div>

    <div v-else class="flex flex-1 min-h-0 min-w-0 flex-col gap-4">
      
        <div class="flex min-h-0 min-w-0 flex-col overflow-hidden rounded-2xl bg-white shadow-sm dark:bg-neutral-900">
          <SignalEditorHeader
            v-if="selectedSnapshot"
            :snapshot="selectedSnapshot"
            @lock="handleLock"
            @delete="requestDelete"
          />


          <div class="flex flex-1 min-h-0 min-w-0 overflow-hidden">
            <div
              v-if="snapshotRows.length === 0"
              class="flex h-full w-full items-center justify-center rounded-2xl border border-dashed border-neutral-200 bg-neutral-50 text-sm text-neutral-500 dark:border-neutral-700 dark:bg-neutral-900"
            >
              No data detected for the selected sheet.
            </div>

            <UiAffinoDataGrid
              v-else
              class="flex-1 min-h-0"
              :rows="snapshotRows"
              :columns="snapshotColumns"
              :row-height="34"
              :overscan-rows="10"
              :overscan-columns="2"
              :enable-filtering="true"
              :enable-column-resize="true"
              :empty-text="'Selected worksheet has no rows.'"
              :row-key="snapshotRowKey"
            >
              <template #cell="{ column, value }">
                <span
                  v-if="column.key === SNAPSHOT_ROW_INDEX_COLUMN_KEY"
                  class="font-mono text-xs text-neutral-500 dark:text-neutral-400"
                >
                  #{{ String(value ?? "") }}
                </span>
                <span v-else class="text-xs text-neutral-700 dark:text-neutral-100">{{ formatSnapshotCellValue(value) }}</span>
              </template>
            </UiAffinoDataGrid>
          </div>
        </div>

      
    </div>

    <SlideOver
      :open="livePanelOpen"
      title="Live Signals"
      placement="right"
      :widthPx="820"
      @close="livePanelOpen = false"
    >
      <div class="h-full p-4">
        <LiveSignalsPanel
          :signals="signals"
          :channels="channels"
          :mapping="mapping"
          :workspace-ready="workspaceReady"
          :loading="livePanelLoading"
          @refresh-signals="refreshLiveSignals"
          @refresh-channels="refreshLiveChannelStates"
        />
      </div>
    </SlideOver>

    <ConfirmModal
      v-if="selectedSnapshot"
      :open="deleteModalOpen"
      title="Delete snapshot"
      :message="deleteMessage"
      confirm-label="Delete"
      cancel-label="Cancel"
      @confirm="confirmDelete"
      @cancel="cancelDelete"
    />
  </div>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from "vue"
import { useRouter } from "vue-router"
import { storeToRefs } from "pinia"

import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import SlideOver from "@/components/ui/SlideOver.vue"
import UiButton from "@/components/ui/UiButton.vue"
import UiAffinoDataGrid from "@/components/ui/UiAffinoDataGrid.vue"
import { useSignalSnapshotStore } from "@/stores/signalSnapshotStore"
import { useSignalsStore } from "@/stores/signalStore"
import { useChannelStore } from "@/stores/channelStore"
import { useToastStore } from "@/stores/toastStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { useRealtimeScopeStore } from "@/stores/realtimeScopeStore"
import type { AllocationMappingItem, AllocationMappingMeta, SignalSnapshot, SnapshotSheet } from "@/types/signal"
import SignalEditorHeader from "./SignalEditorHeader.vue"
import LiveSignalsPanel from "./LiveSignalsPanel.vue"

const props = defineProps<{ snapshotId: number | null }>()

const snapshotStore = useSignalSnapshotStore()
const signalsStore = useSignalsStore()
const channelStore = useChannelStore()
const workspaceStore = useWorkspaceStore()
const deviceStore = useDeviceStore()
const realtimeScopeStore = useRealtimeScopeStore()
const toastStore = useToastStore()
const router = useRouter()
const liveScopeId = "signals:live-panel"

const { signals, loading: liveSignalsLoading } = storeToRefs(signalsStore)
const { channels, isLoading: channelsLoading } = storeToRefs(channelStore)
const { devices } = storeToRefs(deviceStore)

const selectedSnapshotId = ref<number | null>(props.snapshotId ?? null)
const livePanelOpen = ref(false)
const livePanelDataReady = ref(false)

type SnapshotRow = Record<string, unknown>

type SnapshotGridRow = SnapshotRow & {
  rowId: string
  __snapshotIndex__: number
}

type AllocationGridRow = {
  rowId: string
  index: number
  entry: AllocationMappingItem
  channel_id: string
  signal_key: string
  sheet: string
  column_key: string
  signal_row_index: number
  selection: string
  actions: string
}

const mapping = ref<AllocationMappingItem[]>([])
const snapshotRows = ref<SnapshotGridRow[]>([])
const sheetOptions = ref<SnapshotSheet[]>([])
const selectedSheetIndex = ref<number | null>(null)
const SNAPSHOT_ROW_INDEX_COLUMN_KEY = "__snapshotIndex__"
const workspaceReady = computed(() => Boolean(workspaceStore.activeWorkspaceId))
const livePanelLoading = computed(() => liveSignalsLoading.value || channelsLoading.value)

const currentSheet = computed(() =>
  sheetOptions.value.find(sheet => sheet.index === selectedSheetIndex.value) ?? null,
)

const snapshotColumns = computed(() => {
  const headers = currentSheet.value?.headers ?? []
  const uniqueHeaders = headers.filter((header, index) => header && headers.indexOf(header) === index)
  return [
    {
      key: SNAPSHOT_ROW_INDEX_COLUMN_KEY,
      label: "#",
      width: 74,
      minWidth: 60,
    },
    ...uniqueHeaders.map(header => ({
      key: header,
      label: header,
      width: 180,
      minWidth: 120,
    })),
  ]
})

const snapshots = computed(() => snapshotStore.snapshots)
const selectedSnapshot = computed(() => {
  if (!selectedSnapshotId.value) return null
  return snapshots.value.find(s => s.id === selectedSnapshotId.value)
    ?? snapshotStore.snapshotDetails[selectedSnapshotId.value]
    ?? null
})
const deleteModalOpen = ref(false)
const deleteMessage = computed(() => {
  if (!selectedSnapshot.value) return ""
  const name = selectedSnapshot.value.source_filename ?? `Snapshot #${selectedSnapshot.value.id}`
  return `Snapshot "${name}" will be deleted.`
})

watch(
  () => props.snapshotId,
  (next) => {
    selectedSnapshotId.value = next ?? null
  },
)

watch(
  selectedSnapshotId,
  (next) => {
    if (!next) {
      sheetOptions.value = []
      selectedSheetIndex.value = null
      snapshotRows.value = []
      return
    }
    initialize(next)
  },
  { immediate: true },
)

watch(
  () => sheetOptions.value,
  (next) => {
    if (!next.length) {
      selectedSheetIndex.value = null
      snapshotRows.value = []
      return
    }
    const currentIndex = selectedSheetIndex.value
    const hasMatch = next.some(sheet => sheet.index === currentIndex)
    if (!hasMatch) {
      const fallback = next.find(sheet => sheet.rows_count > 0) ?? next[0]
      selectedSheetIndex.value = fallback.index
    } else {
      syncRowsWithSelection()
    }
    hydrateMappingMetaWithSheets(getSheetByIndex(selectedSheetIndex.value))
  },
)

watch(
  selectedSheetIndex,
  () => {
    syncRowsWithSelection()
  },
)

watch(
  livePanelOpen,
  (open) => {
    realtimeScopeStore.setGlobalRealtimeScope(liveScopeId, open)
  },
)

onBeforeUnmount(() => {
  realtimeScopeStore.setGlobalRealtimeScope(liveScopeId, false)
})

function requestDelete() {
  deleteModalOpen.value = true
}

function cancelDelete() {
  deleteModalOpen.value = false
}

async function confirmDelete() {
  if (!selectedSnapshot.value) return
  try {
    await snapshotStore.deleteSnapshot(selectedSnapshot.value.id)
    deleteModalOpen.value = false
    await router.push({ name: "signals.home" })
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

async function refreshLiveSignals() {
  if (!workspaceReady.value) return
  try {
    await signalsStore.refreshSignals(true)
    livePanelDataReady.value = true
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

async function ensureChannelCatalogLoaded() {
  try {
    await Promise.all([deviceStore.ensureLoaded(), channelStore.ensureLoaded()])
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

async function refreshLiveChannelStates() {
  try {
    await ensureChannelCatalogLoaded()
    const deviceIds = new Set<number>()
    channels.value.forEach(channel => deviceIds.add(channel.device_id))
    if (!deviceIds.size) {
      devices.value.forEach(device => deviceIds.add(device.id))
    }
    deviceIds.forEach(deviceId => channelStore.requestStates(deviceId))
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

async function handleLock() {
  if (!selectedSnapshot.value) return
  if (selectedSnapshot.value.status === "locked") return
  try {
    await snapshotStore.lockSnapshot(selectedSnapshot.value.id)
    toastStore.success("Snapshot locked")
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

async function initialize(snapshotId: number) {
  try {
    await ensureChannelCatalogLoaded()
    
    const snapshot = await snapshotStore.getSnapshot(snapshotId)
    const normalized = normalizeSnapshotData(snapshot)
    sheetOptions.value = normalized.sheets
    selectedSheetIndex.value = normalized.defaultSheetIndex ?? normalized.sheets[0]?.index ?? null
    const fallbackSheet = getSheetByIndex(selectedSheetIndex.value) ?? normalized.sheets[0] ?? null

    
    hydrateMappingMetaWithSheets(fallbackSheet)
    autoFillMissingValues()
    syncRowsWithSelection()
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}


function snapshotRowKey(row: Record<string, unknown>): string {
  return String(row.rowId ?? "")
}

function getSheetByIndex(index: number | null): SnapshotSheet | null {
  if (index === null || index === undefined) return null
  return sheetOptions.value.find(sheet => sheet.index === index) ?? null
}

function getHeadersForSheet(index: number | null): string[] {
  const sheet = getSheetByIndex(index)
  return sheet?.headers ?? []
}

function ensureMeta(item: AllocationMappingItem): AllocationMappingMeta {
  if (!item.meta) {
    item.meta = {
      sheet_index: currentSheet.value?.index ?? null,
      sheet_name: currentSheet.value?.name ?? null,
      column_key: currentSheet.value?.headers[0] ?? null,
    }
  }
  return item.meta
}

function hydrateMappingMetaWithSheets(fallbackSheet: SnapshotSheet | null = null) {
  const fallback = fallbackSheet ?? currentSheet.value ?? sheetOptions.value[0] ?? null
  mapping.value.forEach((item) => {
    const meta = ensureMeta(item)
    const sheet = getSheetByIndex(meta.sheet_index) ?? fallback
    meta.sheet_index = sheet?.index ?? null
    meta.sheet_name = sheet?.name ?? null
    if (sheet) {
      if (!meta.column_key || !sheet.headers.includes(meta.column_key)) {
        meta.column_key = sheet.headers[0] ?? null
      }
    } else {
      meta.column_key = null
    }
  })
}

function autoFillMissingValues() {
  mapping.value.forEach((item) => autoFillSignalKeyFromSelection(item))
}

function autoFillSignalKeyFromSelection(
  item: AllocationMappingItem,
  options: { force?: boolean } = {},
) {
  const { force = false } = options
  if (!force && item.signal_key) return
  const value = resolveCellValue(item)
  if (value === null) return
  item.signal_key = value
}

function resolveCellValue(item: AllocationMappingItem): string | null {
  const meta = item.meta
  if (!meta) return null
  const sheet = getSheetByIndex(meta.sheet_index)
  if (!sheet) return null
  const columnKey = meta.column_key
  if (!columnKey) return null
  const row = sheet.rows[item.signal_row_index]
  if (!row) return null
  const rawValue = row[columnKey]
  if (rawValue === undefined || rawValue === null) return null
  if (typeof rawValue === "string") return rawValue
  if (typeof rawValue === "number") return String(rawValue)
  if (typeof rawValue === "boolean") return rawValue ? "true" : "false"
  return String(rawValue)
}

function syncRowsWithSelection() {
  if (selectedSheetIndex.value === null) {
    snapshotRows.value = []
    return
  }
  const selected = getSheetByIndex(selectedSheetIndex.value)
  const rows = selected?.rows ?? []
  snapshotRows.value = rows.map((row, index) => ({
    rowId: `sheet-row-${index}`,
    [SNAPSHOT_ROW_INDEX_COLUMN_KEY]: index + 1,
    ...row,
  }))
}

function formatSnapshotCellValue(value: unknown): string {
  if (value === null || value === undefined) return ""
  if (typeof value === "string") return value
  if (value instanceof Date) return value.toISOString()
  if (typeof value === "number" || typeof value === "boolean") return String(value)
  if (typeof value === "object") {
    try {
      return JSON.stringify(value)
    } catch {
      return "[object]"
    }
  }
  return String(value)
}

function normalizeSnapshotData(snapshot: SignalSnapshot): {
  sheets: SnapshotSheet[]
  defaultSheetIndex: number | null
} {
  const payload = snapshot.data
  if (Array.isArray(payload)) {
    return {
      sheets: [buildLegacySheet(payload)],
      defaultSheetIndex: 0,
    }
  }

  if (payload && typeof payload === "object" && Array.isArray((payload as any).sheets)) {
    const rawSheets = (payload as any).sheets as SnapshotSheet[]
    const sheets = rawSheets.map((sheet, idx) => ({
      name: sheet?.name ?? `Sheet ${idx + 1}`,
      index: typeof sheet?.index === "number" ? sheet.index : idx,
      headers: Array.isArray(sheet?.headers) ? sheet.headers.filter(isString) : [],
      rows: Array.isArray(sheet?.rows) ? sheet.rows : [],
      rows_count:
        typeof sheet?.rows_count === "number"
          ? sheet.rows_count
          : Array.isArray(sheet?.rows)
            ? sheet.rows.length
            : 0,
    }))

    const preferredIndex = typeof (payload as any).default_sheet_index === "number"
      ? (payload as any).default_sheet_index
      : null
    const preferredSheet = sheets.find(sheet => sheet.index === preferredIndex)
    const fallback = preferredSheet ?? sheets.find(sheet => sheet.rows_count > 0) ?? sheets[0] ?? null

    return {
      sheets,
      defaultSheetIndex: fallback ? fallback.index : null,
    }
  }

  return { sheets: [], defaultSheetIndex: null }
}

function buildLegacySheet(rows: SnapshotRow[]): SnapshotSheet {
  return {
    name: "Sheet 1",
    index: 0,
    headers: extractHeaders(rows),
    rows,
    rows_count: rows.length,
  }
}

function extractHeaders(rows: SnapshotRow[]): string[] {
  if (!rows.length) return []
  const firstRow = rows.find(row => Object.keys(row).length > 0)
  return firstRow ? Object.keys(firstRow) : []
}

function isString(value: unknown): value is string {
  return typeof value === "string"
}
</script>
