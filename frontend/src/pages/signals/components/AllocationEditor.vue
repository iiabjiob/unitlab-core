<template>
  <div class="flex h-full flex-col gap-4 p-4">
    <div
      v-if="!selectedSnapshotId"
      class="flex flex-1 items-center justify-center rounded-2xl border border-dashed border-neutral-300 bg-white/80 p-8 text-sm text-neutral-500 dark:border-neutral-700 dark:bg-neutral-900/40 dark:text-neutral-400"
    >
      Select a snapshot to configure allocation
    </div>

    <div
      v-else
      class="flex flex-1 min-h-0 flex-col overflow-hidden rounded-2xl bg-white shadow-sm dark:bg-neutral-900"
    >
      <div class="border-b border-neutral-100 px-5 py-4 dark:border-neutral-800">
        <div class="flex flex-col gap-3 lg:flex-row lg:items-center lg:justify-between">
          <div>
            <p class="text-sm font-semibold text-neutral-900 dark:text-neutral-50">Snapshot rows</p>
            <p v-if="currentSheet" class="text-xs text-neutral-500 dark:text-neutral-400">
              Sheet "{{ currentSheet.name }}" · {{ currentSheet.rows_count }} rows
            </p>
            <p v-else class="text-xs text-neutral-500 dark:text-neutral-400">No sheet detected</p>
          </div>
          <select
            v-if="sheetOptions.length > 1"
            v-model.number="selectedSheetIndex"
            class="w-full rounded-xl border border-neutral-300 bg-white px-3 py-2 text-xs font-medium text-neutral-700 shadow-sm focus:outline-none dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100 lg:w-48"
          >
            <option
              v-for="sheet in sheetOptions"
              :key="sheet.index"
              :value="sheet.index"
              :disabled="sheet.rows_count === 0"
            >
              {{ sheet.name }} ({{ sheet.rows_count }})
            </option>
          </select>
          <span
            v-else-if="currentSheet"
            class="inline-flex items-center rounded-xl border border-neutral-200 px-3 py-1 text-xs font-medium text-neutral-500 dark:border-neutral-700 dark:text-neutral-300"
          >
            {{ currentSheet.name }}
          </span>
        </div>
      </div>
      <div class="flex-1 min-h-0">
        <div
          v-if="snapshotRows.length === 0"
          class="flex h-full items-center justify-center rounded-2xl border border-dashed border-neutral-200 bg-neutral-50 text-sm text-neutral-500 dark:border-neutral-700 dark:bg-neutral-900"
        >
          No data detected for the selected sheet.
        </div>
        <div v-else class="h-full min-h-0">
          <UiTableLight
            class="h-full"
            :columns="snapshotColumns"
            :rows="snapshotRows"
            :row-key="rowKeyForSnapshot"
          >
            <template #cell="{ column, value, rowIndex }">
              <template v-if="column.key === SNAPSHOT_ROW_INDEX_COLUMN_KEY">
                <span class="font-mono text-xs text-neutral-500 dark:text-neutral-400">#{{ rowIndex + 1 }}</span>
              </template>
              <template v-else>
                <span class="text-xs text-neutral-700 dark:text-neutral-100">{{ formatSnapshotCellValue(value) }}</span>
              </template>
            </template>
            <template #empty>
              Selected worksheet has no rows.
            </template>
          </UiTableLight>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, watch } from "vue"

import UiTableLight from "@/components/ui/UiTableLight.vue"
import { useSignalSnapshotStore } from "@/stores/signalSnapshotStore"
import { useToastStore } from "@/stores/toastStore"
import type { AllocationMappingItem, AllocationMappingMeta, SignalSnapshot, SnapshotSheet } from "@/types/signal"

const props = defineProps<{ snapshotId: number | null }>()

const snapshotStore = useSignalSnapshotStore()
const toastStore = useToastStore()

const selectedSnapshotId = ref<number | null>(props.snapshotId ?? null)
type SnapshotRow = Record<string, unknown>

const mapping = ref<AllocationMappingItem[]>([])
const snapshotRows = ref<SnapshotRow[]>([])
const sheetOptions = ref<SnapshotSheet[]>([])
const selectedSheetIndex = ref<number | null>(null)
const SNAPSHOT_ROW_INDEX_COLUMN_KEY = "__snapshotIndex__"

const currentSheet = computed(() =>
  sheetOptions.value.find(sheet => sheet.index === selectedSheetIndex.value) ?? null,
)

const snapshotColumns = computed(() => {
  const headers = currentSheet.value?.headers ?? []
  const uniqueHeaders = headers.filter((header, index) => header && headers.indexOf(header) === index)
  return [
    { key: SNAPSHOT_ROW_INDEX_COLUMN_KEY, label: "#" },
    ...uniqueHeaders.map(header => ({ key: header, label: header })),
  ]
})

const snapshots = computed(() => snapshotStore.snapshots)

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
      mapping.value = [createMappingRow(null)]
      return
    }
    initialize(next)
  },
  { immediate: true },
)

async function initialize(snapshotId: number) {
  try {
    const allocation = await snapshotStore.getAllocation(snapshotId)
    const snapshot = await snapshotStore.getSnapshot(snapshotId)
    const normalized = normalizeSnapshotData(snapshot)
    sheetOptions.value = normalized.sheets
    selectedSheetIndex.value = normalized.defaultSheetIndex ?? normalized.sheets[0]?.index ?? null
    const fallbackSheet = getSheetByIndex(selectedSheetIndex.value) ?? normalized.sheets[0] ?? null

    mapping.value = allocation.mapping.length
      ? allocation.mapping.map((item) => ({
          ...item,
          meta: item.meta ? { ...item.meta } : null,
        }))
      : [createMappingRow(fallbackSheet)]

    hydrateMappingMetaWithSheets(fallbackSheet)
    autoFillMissingValues()
    syncRowsWithSelection()
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

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
  { deep: true },
)

watch(
  selectedSheetIndex,
  () => {
    syncRowsWithSelection()
  },
)

function createMappingRow(defaultSheet: SnapshotSheet | null): AllocationMappingItem {
  return {
    channel_id: "",
    signal_key: "",
    signal_row_index: 0,
    meta: defaultSheet
      ? {
          sheet_index: defaultSheet.index,
          sheet_name: defaultSheet.name,
          column_key: defaultSheet.headers[0] ?? null,
        }
      : {
          sheet_index: null,
          sheet_name: null,
          column_key: null,
        },
  }
}

function addMapping() {
  mapping.value.push(createMappingRow(currentSheet.value ?? sheetOptions.value[0] ?? null))
}

function removeMapping(index: number) {
  if (mapping.value.length <= 1) return
  mapping.value.splice(index, 1)
}

async function save() {
  if (!selectedSnapshotId.value) return
  try {
    await snapshotStore.saveAllocation(selectedSnapshotId.value, mapping.value)
    toastStore.success("Allocation saved")
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

function handleSheetSelect(item: AllocationMappingItem, event: Event) {
  const select = event.target as HTMLSelectElement
  const value = select.value
  const sheetIndex = value === "" ? null : Number(value)
  applySheetSelection(item, sheetIndex)
}

function handleColumnSelect(item: AllocationMappingItem, event: Event) {
  const select = event.target as HTMLSelectElement
  const value = select.value
  const meta = ensureMeta(item)
  meta.column_key = value || null
  autoFillSignalKeyFromSelection(item)
}

function handleRowIndexChange(item: AllocationMappingItem) {
  if (!Number.isFinite(item.signal_row_index) || item.signal_row_index < 0) {
    item.signal_row_index = 0
  }
  autoFillSignalKeyFromSelection(item)
}

function getSheetByIndex(index: number | null): SnapshotSheet | null {
  if (index === null || index === undefined) return null
  return sheetOptions.value.find(sheet => sheet.index === index) ?? null
}

function getHeadersForSheet(index: number | null): string[] {
  const sheet = getSheetByIndex(index)
  return sheet?.headers ?? []
}

function applySheetSelection(item: AllocationMappingItem, sheetIndex: number | null) {
  const meta = ensureMeta(item)
  const sheet = getSheetByIndex(sheetIndex)
  meta.sheet_index = sheet?.index ?? null
  meta.sheet_name = sheet?.name ?? null
  const headers = sheet?.headers ?? []
  if (!headers.length) {
    meta.column_key = null
  } else if (!meta.column_key || !headers.includes(meta.column_key)) {
    meta.column_key = headers[0]
  }
  autoFillSignalKeyFromSelection(item)
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

function pullValueFromSelection(item: AllocationMappingItem) {
  autoFillSignalKeyFromSelection(item, { force: true })
}

function pullValuesForAll() {
  mapping.value.forEach(item => autoFillSignalKeyFromSelection(item, { force: true }))
}

function describeSelection(item: AllocationMappingItem): string {
  const meta = item.meta
  if (!meta || meta.sheet_index === null) {
    return "Select sheet, column, and row to pull a value."
  }
  const sheetLabel = meta.sheet_name ?? `Sheet ${meta.sheet_index + 1}`
  const columnLabel = meta.column_key ?? "column"
  const rowLabel = Number.isFinite(item.signal_row_index)
    ? `row ${item.signal_row_index + 1}`
    : "row ?"
  const value = resolveCellValue(item)
  if (value === null) {
    return `${sheetLabel} · ${columnLabel} @ ${rowLabel} — empty`
  }
  return `${sheetLabel} · ${columnLabel} @ ${rowLabel} → ${value}`
}

function syncRowsWithSelection() {
  if (selectedSheetIndex.value === null) {
    snapshotRows.value = []
    return
  }
  const selected = getSheetByIndex(selectedSheetIndex.value)
  snapshotRows.value = selected?.rows ?? []
}

function rowKeyForSnapshot(row: SnapshotRow, index: number): string {
  const candidates: (string | number | undefined)[] = [
    row.id as string | number | undefined,
    row.ID as string | number | undefined,
    row.key as string | number | undefined,
    row.Key as string | number | undefined,
  ]
  const match = candidates.find(value => typeof value === "string" || typeof value === "number")
  if (match !== undefined) {
    return String(match)
  }
  return `row-${index}`
}

function formatSnapshotCellValue(value: unknown): string {
  if (value === null || value === undefined) return ""
  if (typeof value === "string") return value
  if (value instanceof Date) return value.toISOString()
  if (typeof value === "number" || typeof value === "boolean") return String(value)
  if (typeof value === "object") {
    try {
      return JSON.stringify(value)
    } catch (err) {
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
