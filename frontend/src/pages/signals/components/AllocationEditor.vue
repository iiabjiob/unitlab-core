<template>
  <div class="flex h-full min-h-0 min-w-0 flex-col gap-4 p-3 md:p-4">
    <header class="flex flex-wrap items-center justify-between gap-3 rounded-2xl border border-neutral-200 bg-white px-4 py-3 dark:border-neutral-800 dark:bg-neutral-900">
      <div class="flex flex-wrap items-center gap-3">
        <div>
          <p class="text-xs font-semibold uppercase tracking-[0.2em] text-neutral-500 dark:text-neutral-400">Live Signal Sheet</p>
          <p class="text-sm text-neutral-700 dark:text-neutral-200">
            {{ summaryText }}
          </p>
        </div>
        <UiButton variant="primary" size="sm" :disabled="workspaceMissing || loading" @click="openImportModal">
          + Import Signal List
        </UiButton>
        <UiButton
          variant="secondary"
          size="sm"
          :disabled="workspaceMissing || loading || allocatedCableRows.length === 0"
          @click="exportCableJournal"
        >
          Export Cable Journal
        </UiButton>
      </div>
      <div class="flex flex-wrap items-center gap-2">
        <div
          v-if="showTestRunProgress"
          class="min-w-[260px] rounded-lg border border-neutral-200 bg-neutral-50 px-2 py-1 dark:border-neutral-700 dark:bg-neutral-800/60"
        >
          <div class="flex items-center justify-between text-[11px] font-medium text-neutral-600 dark:text-neutral-300">
            <span>{{ testRunProgressText }}</span>
            <span>{{ testRunProgressPercent }}%</span>
          </div>
          <div class="mt-1 h-1.5 overflow-hidden rounded bg-neutral-200 dark:bg-neutral-700">
            <div
              class="h-full bg-emerald-500 transition-[width] duration-200"
              :style="{ width: `${testRunProgressPercent}%` }"
            ></div>
          </div>
        </div>
        <UiButton
          v-if="selectedUnassignedSignalIds.length > 0"
          variant="secondary"
          size="sm"
          :disabled="loading"
          @click="allocateSelectedUnassigned"
        >
          Allocate selected unassigned
        </UiButton>
        <UiButton
          v-if="selectedAllocatedSignalIds.length > 0"
          variant="ghost"
          size="sm"
          :disabled="loading"
          @click="deallocateSelected"
        >
          De-allocate selected
        </UiButton>
        <UiButton
          v-if="selectedAllocatedPhysicalRows.length > 0"
          variant="success"
          size="sm"
          :disabled="loading || testRunInProgress"
          @click="runTestVisualOnly"
        >
          {{ testRunInProgress ? "Running…" : "Run test" }}
        </UiButton>
        <UiButton
          v-if="canCreateSwitchgearFromSelection"
          variant="secondary"
          size="sm"
          :disabled="loading"
          @click="createSwitchgearVisualOnly"
        >
          {{ createSwitchgearButtonLabel }}
        </UiButton>
      </div>
    </header>

    <div
      v-if="workspaceMissing"
      class="flex flex-1 items-center justify-center rounded-2xl border border-dashed border-neutral-300 bg-white/80 p-8 text-sm text-neutral-500 dark:border-neutral-700 dark:bg-neutral-900/40 dark:text-neutral-400"
    >
      Select a workspace to manage signal allocations.
    </div>

    <div
      v-else-if="!signalSheetStore.sheet || signalSheetStore.sheet.signals_count === 0"
      class="flex flex-1 items-center justify-center rounded-2xl border border-dashed border-neutral-300 bg-white/80 p-8 text-sm text-neutral-500 dark:border-neutral-700 dark:bg-neutral-900/40 dark:text-neutral-400"
    >
      Import a signal list to start allocating channels.
    </div>

    <UiAffinoDataGrid
      v-else
      class="flex-1 min-h-0"
      :rows="gridRows"
      :columns="gridColumns"
      :row-height="34"
      :overscan-rows="10"
      :overscan-columns="2"
      :enable-filtering="true"
      :enable-column-resize="true"
      :empty-text="'No signals available.'"
      :row-key="rowKey"
      :show-controls="true"
      :table-id="'signals-live-sheet-grid'"
      :persist-state="true"
      :dataset-key="gridDatasetKey"
      @row-click="handleRowClick"
      @selection-change="handleSelectionChange"
    >
      <template #cell="{ column, row, value }">
        <AllocationChannelPicker
          v-if="column.key === 'channel_select'"
          :row="asAllocationRow(row)"
          :loading="loading"
          :value-label="allocationDisplayLabel(asAllocationRow(row))"
          :value-class="allocationValueClass(asAllocationRow(row))"
          :channel-groups="channelGroupsForRow(asAllocationRow(row))"
          :unit-status-by-id="unitStatusById"
          @allocate="channelId => handleAllocationPickerSelect(asAllocationRow(row), channelId)"
        />

        <div v-else-if="column.key === 'control'" class="flex justify-center">
          <UiMenu v-if="canControl(asAllocationRow(row))" :options="persistentControlMenuOptions">
            <UiMenuTrigger asChild>
              <button
                type="button"
                class="rounded border border-neutral-300 bg-white px-2 py-1 text-xs font-semibold text-neutral-700 hover:bg-neutral-100 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200 dark:hover:bg-neutral-800"
                @click.stop
              >
                <span class="inline-flex items-center gap-1.5 whitespace-nowrap">
                  <span
                    class="h-2 w-2 shrink-0 rounded-full"
                    :class="controlLampClass(asAllocationRow(row))"
                  ></span>
                  <span>Control</span>
                  <span
                    class="text-[10px] font-semibold uppercase tracking-[0.08em]"
                    :class="controlStatusClass(asAllocationRow(row))"
                  >
                    {{ controlStatusTag(asAllocationRow(row)) }}
                  </span>
                </span>
              </button>
            </UiMenuTrigger>
            <UiMenuContent>
              <UiMenuItem disabled>
                <span class="inline-flex items-center gap-2 text-xs font-semibold">
                  <span
                    class="h-2.5 w-2.5 shrink-0 rounded-full"
                    :class="controlLampClass(asAllocationRow(row))"
                  ></span>
                  <span>State: {{ controlStateLabel(asAllocationRow(row)) }}</span>
                </span>
              </UiMenuItem>
              <UiMenuSeparator />
              <UiMenuItem
                :disabled="controlDisabled(asAllocationRow(row))"
                @select="() => handleControlMenuSelect(asAllocationRow(row), true)"
              >
                ON
              </UiMenuItem>
              <UiMenuItem
                :disabled="controlDisabled(asAllocationRow(row))"
                @select="() => handleControlMenuSelect(asAllocationRow(row), false)"
              >
                OFF
              </UiMenuItem>
            </UiMenuContent>
          </UiMenu>
          <span v-else class="text-xs text-neutral-400">—</span>
        </div>

        <span
          v-else-if="column.key === 'last_tested_at'"
          class="text-xs text-neutral-700 dark:text-neutral-100"
        >
          {{ formatTestedAt(value) }}
        </span>

        <span v-else class="text-xs text-neutral-700 dark:text-neutral-100">{{ formatCell(value) }}</span>
      </template>
    </UiAffinoDataGrid>

    <SignalImportModal :open="importModalOpen" @close="closeImportModal" @imported="handleImported" />
  </div>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, ref, shallowRef, triggerRef, watch } from "vue"
import { storeToRefs } from "pinia"
import { useRoute, useRouter } from "vue-router"
import {
  UiMenu,
  UiMenuContent,
  UiMenuItem,
  UiMenuSeparator,
  UiMenuTrigger,
} from "@affino/menu-vue"

import UiAffinoDataGrid from "@/components/ui/UiAffinoDataGrid.vue"
import UiButton from "@/components/ui/UiButton.vue"
import type { Channel, DoChannel } from "@/types/channel"
import type { SignalAllocationRow } from "@/types/signal"
import AllocationChannelPicker from "@/pages/signals/components/AllocationChannelPicker.vue"
import SignalImportModal from "@/pages/signals/components/SignalImportModal.vue"
import { extractSourceRowFromSignalMetadata, resolveAllSourceColumnHeaders } from "@/pages/signals/utils/sourceColumns"
import { useChannelStore } from "@/stores/channelStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { useRealtimeScopeStore } from "@/stores/realtimeScopeStore"
import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { useToastStore } from "@/stores/toastStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"

const signalSheetStore = useSignalSheetStore()
const workspaceStore = useWorkspaceStore()
const channelStore = useChannelStore()
const deviceStore = useDeviceStore()
const realtimeScopeStore = useRealtimeScopeStore()
const toastStore = useToastStore()
const route = useRoute()
const router = useRouter()

const { allocationRows, loadingAllocations, loadingSheet, allocatedCount, allocationRevision, recentlyChangedSignalIds } = storeToRefs(signalSheetStore)
const { channels } = storeToRefs(channelStore)

const scopeId = "signals:allocations"
const importModalOpen = ref(false)
const persistentControlMenuOptions = { closeOnSelect: false }
const selectedRowKeys = ref<string[]>([])
const testRunInProgress = ref(false)
const testRunTotal = ref(0)
const testRunProcessed = ref(0)
const testRunSucceeded = ref(0)
const testRunSkipped = ref(0)
let realtimeScopeSyncFrame: number | null = null
const TEST_TOGGLE_STEP_MS = 1000

const workspaceMissing = computed(() => !workspaceStore.activeWorkspaceId)
const loading = computed(() => loadingAllocations.value || loadingSheet.value)

const summaryText = computed(() => {
  const sheet = signalSheetStore.sheet
  if (!sheet) return "No active sheet"
  return `${sheet.signals_count} signals · ${allocatedCount.value} allocated · ${sheet.rows_count} source rows`
})

const gridDatasetKey = computed(() => {
  const sheet = signalSheetStore.sheet
  if (!sheet) return "no-sheet"
  const selectedColumns = Array.isArray(sheet.import_meta?.selected_columns)
    ? sheet.import_meta?.selected_columns.map(item => String(item)).join("|")
    : ""
  return [
    sheet.source_hash ?? "",
    String(sheet.schema_version),
    String(sheet.rows_count),
    String(sheet.signals_count),
    selectedColumns,
  ].join("::")
})

const channelMap = computed(() => {
  const map = new Map<number, Channel>()
  channels.value.forEach(channel => map.set(channel.id, channel))
  return map
})

const deviceStatusById = computed(() => {
  const map = new Map<number, string>()
  deviceStore.devices.forEach((device) => {
    map.set(device.id, device.status)
  })
  return map
})

const unitStatusById = computed<Record<string, string>>(() => {
  const map: Record<string, string> = {}
  deviceStore.devices.forEach((device) => {
    const unitId = String(device.unit_id ?? "").trim()
    if (!unitId) return
    map[unitId] = device.status
  })
  return map
})

const channelUnitById = computed(() => {
  const map = new Map<number, string>()
  channels.value.forEach((channel) => {
    const unitId = channelStore.resolveUnitId(channel.device_id)
    map.set(channel.id, unitId || `Device ${channel.device_id}`)
  })
  return map
})

function signalIdFromRowKey(rowKey: string): number | null {
  if (!rowKey.startsWith("signal-")) return null
  const parsed = Number(rowKey.slice("signal-".length))
  if (!Number.isFinite(parsed)) return null
  return parsed
}

const selectedAllocationRows = computed(() => (
  selectedRowKeys.value
    .map((rowKey) => {
      const signalId = signalIdFromRowKey(rowKey)
      if (signalId === null) return null
      return findAllocationRowBySignalId(signalId)
    })
    .filter((row): row is SignalAllocationRow => Boolean(row))
))

const selectedUnassignedSignalIds = computed(() => (
  selectedAllocationRows.value
    .filter(row => !Number.isFinite(row.channel_id as number))
    .map(row => row.signal_id)
))

const selectedAllocatedSignalIds = computed(() => (
  selectedAllocationRows.value
    .filter(row => Number.isFinite(row.channel_id as number))
    .map(row => row.signal_id)
))

const selectedAllocatedPhysicalRows = computed(() => (
  selectedAllocationRows.value.filter((row) => (
    Number.isFinite(row.channel_id as number)
    && Number.isFinite(row.device_id as number)
    && Boolean(row.unit_id)
  ))
))

const selectedAllocatedPhysicalDiCount = computed(() => (
  selectedAllocatedPhysicalRows.value.filter(row => row.signal_direction === "DI").length
))

const selectedAllocatedPhysicalDoCount = computed(() => (
  selectedAllocatedPhysicalRows.value.filter(row => row.signal_direction === "DO").length
))

const selectedAllocatedPhysicalOnlyDiDo = computed(() => (
  selectedAllocatedPhysicalRows.value.every(row => row.signal_direction === "DI" || row.signal_direction === "DO")
))

const switchgearCreatableCount = computed(() => {
  if (!selectedAllocatedPhysicalOnlyDiDo.value) return 0
  const diCount = selectedAllocatedPhysicalDiCount.value
  const doCount = selectedAllocatedPhysicalDoCount.value
  if (diCount < 2 || doCount < 2) return 0
  if (diCount !== doCount) return 0
  if (diCount % 2 !== 0) return 0
  return diCount / 2
})

const canCreateSwitchgearFromSelection = computed(() => switchgearCreatableCount.value > 0)

const createSwitchgearButtonLabel = computed(() => (
  switchgearCreatableCount.value === 1
    ? "Create switchgear"
    : `Create ${switchgearCreatableCount.value} switchgears`
))

const allocatedCableRows = computed(() => (
  allocationRows.value.filter((row) => (
    Number.isFinite(row.channel_id as number)
    && Number.isFinite(row.channel_index as number)
    && Boolean(String(row.unit_id ?? "").trim())
  ))
))

const showTestRunProgress = computed(() => testRunInProgress.value && testRunTotal.value > 0)

const testRunProgressPercent = computed(() => {
  const total = testRunTotal.value
  if (!total) return 0
  return Math.max(0, Math.min(100, Math.round((testRunProcessed.value / total) * 100)))
})

const testRunEtaSeconds = computed(() => {
  if (!testRunInProgress.value) return 0
  const remaining = Math.max(0, testRunTotal.value - testRunProcessed.value)
  return remaining * (TEST_TOGGLE_STEP_MS / 1000)
})

const testRunProgressText = computed(() => {
  const base = `${testRunProcessed.value}/${testRunTotal.value} · ok ${testRunSucceeded.value} · skip ${testRunSkipped.value}`
  if (!testRunInProgress.value) return base
  return `${base} · ETA ${formatDurationShort(testRunEtaSeconds.value)}`
})

const sourceColumnHeaders = computed(() => (
  resolveAllSourceColumnHeaders(signalSheetStore.sheet, allocationRows.value)
))

const gridColumns = computed(() => {
  const sourceColumns = sourceColumnHeaders.value.map((header, index) => ({
    key: sourceColumnKey(index),
    label: header,
    width: Math.min(Math.max(header.length * 11, 140), 360),
    minWidth: 120,
  }))

  return [
    ...sourceColumns,
    { key: "channel_select", label: "Unit/Channel", width: 150, minWidth: 120, pin: "right" as const },
    { key: "last_tested_at", label: "Last tested", width: 180, minWidth: 150, pin: "right" as const },
    { key: "control", label: "Control", width: 120, minWidth: 96, pin: "right" as const, meta: { filterable: false } },
  ]
})

const gridRows = shallowRef<GridRow[]>([])
const gridRowBySignalId = new Map<number, GridRow>()
const gridRowIndexBySignalId = new Map<number, number>()

type GridRow = Record<string, unknown>
type ChannelOption = { id: number; label: string; disabled: boolean }
type ChannelOptionGroup = { unitId: string; options: ChannelOption[] }

function sourceColumnKey(index: number): string {
  return `source_col_${index}`
}

function assignDynamicGridFields(payload: GridRow, row: SignalAllocationRow) {
  Object.assign(payload, row)
  payload.rowId = `signal-${row.signal_id}`
  // Filtering/sorting must use the same human-readable text as in the cell.
  payload.channel_select = allocationDisplayLabel(row)
  payload.last_tested_at = row.tested_at
  payload.control = ""
}

function createGridRow(row: SignalAllocationRow, headers: readonly string[]): GridRow {
  const payload: GridRow = {}
  assignDynamicGridFields(payload, row)
  const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
  headers.forEach((header, index) => {
    payload[sourceColumnKey(index)] = sourceRow[header] ?? ""
  })
  return payload
}

function rebuildGridRows() {
  const headers = sourceColumnHeaders.value
  const nextRows: GridRow[] = []
  gridRowBySignalId.clear()
  gridRowIndexBySignalId.clear()

  allocationRows.value.forEach((row, index) => {
    const signalId = row.signal_id
    const payload = createGridRow(row, headers)
    gridRowBySignalId.set(signalId, payload)
    gridRowIndexBySignalId.set(signalId, index)
    nextRows.push(payload)
  })

  if (selectedRowKeys.value.length > 0) {
    const allowed = new Set(nextRows.map(item => String(item.rowId)))
    selectedRowKeys.value = selectedRowKeys.value.filter(rowKey => allowed.has(rowKey))
  }

  gridRows.value = nextRows
}

function findAllocationRowBySignalId(signalId: number): SignalAllocationRow | null {
  const row = allocationRows.value.find(item => item.signal_id === signalId)
  return row ?? null
}

function syncGridRowsBySignalIds(signalIds: readonly number[]) {
  if (!signalIds.length) return
  if (signalIds.length > 128) {
    rebuildGridRows()
    return
  }

  const headers = sourceColumnHeaders.value
  let structuralChange = false

  signalIds.forEach((signalId) => {
    const row = findAllocationRowBySignalId(signalId)
    if (!row) {
      const existingIndex = gridRowIndexBySignalId.get(signalId)
      if (existingIndex === undefined) return
      const nextRows = [...gridRows.value]
      nextRows.splice(existingIndex, 1)
      gridRows.value = nextRows
      structuralChange = true
      return
    }

    const existingPayload = gridRowBySignalId.get(signalId)
    if (!existingPayload) {
      structuralChange = true
      return
    }
    assignDynamicGridFields(existingPayload, row)
  })

  if (structuralChange) {
    rebuildGridRows()
    return
  }

  // shallowRef: notify grid about in-place patched row objects.
  triggerRef(gridRows)
}

function asAllocationRow(row: GridRow): SignalAllocationRow {
  return row as unknown as SignalAllocationRow
}

function rowKey(row: Record<string, unknown>) {
  return String(row.rowId)
}

function formatCell(value: unknown) {
  if (value === null || value === undefined) return ""
  if (typeof value === "string") return value
  if (typeof value === "number" || typeof value === "boolean") return String(value)
  try {
    return JSON.stringify(value)
  } catch {
    return String(value)
  }
}

function formatTestedAt(value: unknown): string {
  const raw = String(value ?? "").trim()
  if (!raw) return "—"
  const parsed = new Date(raw)
  if (Number.isNaN(parsed.getTime())) {
    return raw
  }
  return parsed.toLocaleString()
}

function formatDurationShort(seconds: number): string {
  const normalized = Math.max(0, Math.round(seconds))
  const minutes = Math.floor(normalized / 60)
  const remSeconds = normalized % 60
  if (minutes <= 0) {
    return `${remSeconds}s`
  }
  return `${minutes}m ${remSeconds}s`
}

function csvEscape(value: unknown): string {
  const text = String(value ?? "")
  if (/[",\n\r]/.test(text)) {
    return `"${text.replaceAll("\"", "\"\"")}"`
  }
  return text
}

function downloadTextFile(content: string, filename: string, mimeType = "text/csv;charset=utf-8;") {
  const blob = new Blob([`\uFEFF${content}`], { type: mimeType })
  const url = URL.createObjectURL(blob)
  const link = document.createElement("a")
  link.href = url
  link.download = filename
  document.body.appendChild(link)
  link.click()
  link.remove()
  URL.revokeObjectURL(url)
}

function buildCableJournalRows(): string[][] {
  const headers = sourceColumnHeaders.value
  const fallbackHeaders = headers.length > 0 ? headers : ["signal_name", "signal_key"]

  const rows: string[][] = []
  allocatedCableRows.value.forEach((row) => {
    const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
    const sourceCells = fallbackHeaders.map((header) => {
      if (header === "signal_name") return row.signal_name
      if (header === "signal_key") return row.signal_key
      return sourceRow[header] ?? ""
    })
    const channelNumber = Number.isFinite(row.channel_index as number) ? Number(row.channel_index) + 1 : ""
    rows.push([
      ...sourceCells.map(item => String(item ?? "")),
      String(row.unit_id ?? ""),
      String(channelNumber),
    ])
  })

  return rows
}

function exportCableJournal() {
  if (!allocatedCableRows.value.length) {
    toastStore.info("No allocated rows to export.")
    return
  }

  const headers = sourceColumnHeaders.value
  const fallbackHeaders = headers.length > 0 ? headers : ["signal_name", "signal_key"]
  const csvHeaders = [...fallbackHeaders, "unit_id", "channel_index"]
  const rows = buildCableJournalRows()
  const csvContent = [
    csvHeaders.map(csvEscape).join(","),
    ...rows.map(row => row.map(csvEscape).join(",")),
  ].join("\n")

  const workspaceId = workspaceStore.activeWorkspaceId ?? "workspace"
  const dateSuffix = new Date().toISOString().slice(0, 19).replaceAll(":", "-")
  const filename = `cable-journal-ws-${workspaceId}-${dateSuffix}.csv`
  downloadTextFile(csvContent, filename)
  toastStore.success(`Cable journal exported: ${rows.length} rows`)
}

function requiredChannelType(signalDirection: string): "di" | "do" | "ai" | "ao" | null {
  const normalized = signalDirection.trim().toUpperCase()
  if (normalized === "DI") return "do"
  if (normalized === "DO") return "di"
  if (normalized === "AI") return "ao"
  if (normalized === "AO") return "ai"
  return null
}

function normalizedChannelType(raw: string | null | undefined): "di" | "do" | "ai" | "ao" | null {
  const value = String(raw || "").trim().toLowerCase()
  if (value.startsWith("di")) return "di"
  if (value.startsWith("do")) return "do"
  if (value.startsWith("ai")) return "ai"
  if (value.startsWith("ao")) return "ao"
  return null
}

function channelOptionsBySignal(signalDirection: string): ChannelOption[] {
  const needed = requiredChannelType(signalDirection)
  if (!needed) return []
  return channelOptionsByType.value[needed] ?? []
}

const channelOptionsByType = computed<Record<"di" | "do" | "ai" | "ao", ChannelOption[]>>(() => {
  const grouped: Record<"di" | "do" | "ai" | "ao", ChannelOption[]> = {
    di: [],
    do: [],
    ai: [],
    ao: [],
  }
  const deviceMap = new Map(deviceStore.devices.map(device => [device.id, device] as const))
  const sortedChannels = [...channels.value].sort((a, b) => {
    const onlineA = deviceMap.get(a.device_id)?.status === "online" ? 0 : 1
    const onlineB = deviceMap.get(b.device_id)?.status === "online" ? 0 : 1
    if (onlineA !== onlineB) return onlineA - onlineB
    if (a.device_id !== b.device_id) return a.device_id - b.device_id
    return a.index - b.index
  })

  sortedChannels.forEach((channel) => {
    const type = normalizedChannelType(channel.type)
    if (!type) return
    grouped[type].push({
      id: channel.id,
      label: `${channelStore.resolveUnitId(channel.device_id)}/ch${channel.index + 1}`,
      disabled: false,
    })
  })
  return grouped
})

function channelOptionsForRow(row: SignalAllocationRow): ChannelOption[] {
  return channelOptionsBySignal(row.signal_direction).map((option) => {
    const ownerSignalId = signalSheetStore.getAllocationOwnerSignalId(option.id)
    const allocatedToAnotherSignal = ownerSignalId !== null && ownerSignalId !== row.signal_id
    return {
      ...option,
      disabled: option.disabled || allocatedToAnotherSignal,
    }
  })
}

function buildChannelGroupsForRow(row: SignalAllocationRow): ChannelOptionGroup[] {
  const groups = new Map<string, ChannelOption[]>()
  channelOptionsForRow(row).forEach((option) => {
    const unitId = channelUnitById.value.get(option.id) ?? "Unassigned unit"
    const group = groups.get(unitId)
    if (group) {
      group.push(option)
      return
    }
    groups.set(unitId, [option])
  })

  return Array.from(groups.entries())
    .sort((a, b) => a[0].localeCompare(b[0]))
    .map(([unitId, options]) => ({ unitId, options }))
}

const channelGroupsBySignalId = computed(() => {
  const map = new Map<number, ChannelOptionGroup[]>()
  allocationRows.value.forEach((row) => {
    map.set(row.signal_id, buildChannelGroupsForRow(row))
  })
  return map
})

function channelGroupsForRow(row: SignalAllocationRow): ChannelOptionGroup[] {
  return channelGroupsBySignalId.value.get(row.signal_id) ?? []
}

function setAllocationForRow(row: SignalAllocationRow, nextChannelId: number | null) {
  // Defer heavy reactive updates out of the click task to keep menu interactions responsive.
  setTimeout(() => {
    void signalSheetStore
      .setAllocation(row.signal_id, Number.isFinite(nextChannelId as number) ? nextChannelId : null)
      .catch((err) => {
        toastStore.error(err instanceof Error ? err.message : String(err))
      })
  }, 0)
}

function handleAllocationPickerSelect(row: SignalAllocationRow, channelId: number | null) {
  setAllocationForRow(row, channelId)
}

function allocationDisplayLabel(row: SignalAllocationRow): string {
  if (Number.isFinite(row.channel_index as number)) {
    const channelSuffix = `ch${Number(row.channel_index) + 1}`
    return row.unit_id?.trim() ? `${row.unit_id}/${channelSuffix}` : channelSuffix
  }
  if (row.channel_label && row.channel_label.trim().length > 0) {
    return row.channel_label
  }
  return "—"
}

function allocationValueClass(row: SignalAllocationRow) {
  if (!row.channel_id) return "text-neutral-500 dark:text-neutral-400"
  if (row.unit_online) return "font-semibold text-neutral-900 dark:text-neutral-100"
  return "font-semibold text-neutral-400 dark:text-neutral-500"
}

type ControlTarget = {
  deviceId: number
  unitId: string
  channelIndex: number
  channel: DoChannel
  online: boolean
}

type SendControlOptions = {
  quiet?: boolean
}

function resolveControlTarget(row: SignalAllocationRow): ControlTarget | null {
  if (!Number.isFinite(row.channel_id as number)) {
    return null
  }

  const linkedChannel = channelMap.value.get(Number(row.channel_id))
  if (!linkedChannel || normalizedChannelType(linkedChannel.type) !== "do") {
    return null
  }

  const device = deviceStore.devices.find((item) => item.id === linkedChannel.device_id)
  if (!device) {
    return null
  }

  return {
    deviceId: device.id,
    unitId: device.unit_id,
    channelIndex: linkedChannel.index,
    channel: linkedChannel as DoChannel,
    online: device.status === "online",
  }
}

function canControl(row: SignalAllocationRow) {
  return resolveControlTarget(row) !== null
}

function controlBusy(row: SignalAllocationRow): boolean {
  const target = resolveControlTarget(row)
  if (!target) return false
  const stage = target.channel.ui?.stage ?? "idle"
  return stage === "pending" || stage === "debounce"
}

function controlStateLabel(row: SignalAllocationRow): string {
  const target = resolveControlTarget(row)
  if (!target) return "UNKNOWN"
  const stableLabel = target.channel.state ? "ON" : "OFF"
  const stage = target.channel.ui?.stage ?? "idle"
  if (!target.online) return `${stableLabel} (OFFLINE)`
  if (stage === "pending" || stage === "debounce") return `${stableLabel} (PENDING)`
  if (stage === "error") return `${stableLabel} (ERROR)`
  return stableLabel
}

function controlLampClass(row: SignalAllocationRow): string {
  const target = resolveControlTarget(row)
  if (!target) return "bg-neutral-400 dark:bg-neutral-600"
  const stage = target.channel.ui?.stage ?? "idle"
  if (!target.online) return "bg-neutral-500 dark:bg-neutral-700"
  if (stage === "pending" || stage === "debounce") return "bg-amber-400 animate-pulse"
  if (stage === "error") return "bg-red-500 animate-pulse"
  return target.channel.state ? "bg-emerald-500" : "bg-neutral-400 dark:bg-neutral-600"
}

function controlStatusTag(row: SignalAllocationRow): string {
  const target = resolveControlTarget(row)
  if (!target) return "N/A"
  const stage = target.channel.ui?.stage ?? "idle"
  if (!target.online) return "OFFL"
  if (stage === "pending" || stage === "debounce") return "PEND"
  if (stage === "error") return "ERR"
  return target.channel.state ? "ON" : "OFF"
}

function controlStatusClass(row: SignalAllocationRow): string {
  const target = resolveControlTarget(row)
  if (!target || !target.online) return "text-neutral-500 dark:text-neutral-400"
  const stage = target.channel.ui?.stage ?? "idle"
  if (stage === "pending" || stage === "debounce") return "text-amber-600 dark:text-amber-300"
  if (stage === "error") return "text-red-600 dark:text-red-300"
  return target.channel.state
    ? "text-emerald-600 dark:text-emerald-300"
    : "text-neutral-500 dark:text-neutral-300"
}

function controlDisabled(row: SignalAllocationRow): boolean {
  const target = resolveControlTarget(row)
  if (!target) return true
  return !target.online
}

function handleControlMenuSelect(row: SignalAllocationRow, state: boolean) {
  void sendControl(row, state)
}

function waitForControlResult(target: ControlTarget, expectedState: boolean, timeoutMs = 2600): Promise<boolean> {
  const startedAt = Date.now()
  return new Promise((resolve) => {
    const poll = () => {
      const stage = target.channel.ui?.stage ?? "idle"
      if (stage === "error") {
        resolve(false)
        return
      }
      if (stage === "idle" && Boolean(target.channel.state) === expectedState) {
        resolve(true)
        return
      }
      if (Date.now() - startedAt >= timeoutMs) {
        resolve(false)
        return
      }
      setTimeout(poll, 60)
    }
    poll()
  })
}

async function sendControl(row: SignalAllocationRow, state: boolean, options: SendControlOptions = {}): Promise<boolean> {
  const target = resolveControlTarget(row)
  if (!target) {
    toastStore.error("Channel not found")
    return false
  }
  if (!target.online) {
    toastStore.error("Device is offline")
    return false
  }
  if (controlBusy(row) && target.channel.ui?.target === state) {
    return false
  }

  try {
    channelStore.sendDoCommand(target.unitId, target.channelIndex, state)
    const succeeded = await waitForControlResult(target, state)
    if (!succeeded) {
      if (!options.quiet) {
        toastStore.warning("Command not confirmed by device")
      }
      return false
    }
    void signalSheetStore.markSignalsTested([row.signal_id]).catch(() => {
      return
    })
    return true
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
    return false
  }
}

function handleRowClick() {
  return
}

function handleSelectionChange(payload: { rowKeys: string[] }) {
  selectedRowKeys.value = payload.rowKeys
}

function openImportModal() {
  if (workspaceMissing.value) return
  importModalOpen.value = true
}

function closeImportModal() {
  importModalOpen.value = false
}

async function handleImported() {
  importModalOpen.value = false
  await refreshAll()
  await signalSheetStore.refreshPresets()
}

function isImportQueryRequested(raw: unknown): boolean {
  const values = Array.isArray(raw) ? raw : [raw]
  return values.some((value) => {
    const normalized = String(value ?? "").trim().toLowerCase()
    return normalized === "1" || normalized === "true" || normalized === "yes" || normalized === "open"
  })
}

function clearImportQueryFlag() {
  if (!("import" in route.query)) return
  const nextQuery = { ...route.query }
  delete nextQuery.import
  void router.replace({ query: nextQuery }).catch(() => {
    return
  })
}

async function allocateSelectedUnassigned() {
  if (!selectedUnassignedSignalIds.value.length) return
  try {
    const response = await signalSheetStore.autoAllocate({
      signal_ids: [...selectedUnassignedSignalIds.value],
      prefer_online: true,
      overwrite_existing: false,
    })
    const assigned = response.result.assigned
    const rest = response.result.unassigned_signal_ids.length
    toastStore.success(`Allocation complete: ${assigned} assigned${rest ? `, ${rest} left unassigned` : ""}`)
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

async function deallocateSelected() {
  if (!selectedAllocatedSignalIds.value.length) return
  try {
    await signalSheetStore.bulkSetAllocations(
      selectedAllocatedSignalIds.value.map(signalId => ({ signal_id: signalId, channel_id: null })),
    )
    toastStore.success(`De-allocated ${selectedAllocatedSignalIds.value.length} selected signal(s)`)
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

function wait(ms: number) {
  return new Promise<void>(resolve => {
    setTimeout(resolve, ms)
  })
}

async function runTestVisualOnly() {
  if (testRunInProgress.value) return

  const queue = selectedAllocatedPhysicalRows.value.filter(row => canControl(row))
  if (!queue.length) {
    toastStore.info("Selected rows have no controllable DO channels.")
    return
  }

  testRunInProgress.value = true
  testRunTotal.value = queue.length
  testRunProcessed.value = 0
  testRunSucceeded.value = 0
  testRunSkipped.value = 0

  try {
    for (let index = 0; index < queue.length; index += 1) {
      const row = queue[index]
      const target = resolveControlTarget(row)
      if (!target || !target.online) {
        testRunSkipped.value += 1
      } else {
        const nextState = !Boolean(target.channel.state)
        const ok = await sendControl(row, nextState, { quiet: true })
        if (ok) {
          testRunSucceeded.value += 1
        } else {
          testRunSkipped.value += 1
        }
      }

      testRunProcessed.value += 1

      if (index < queue.length - 1) {
        await wait(TEST_TOGGLE_STEP_MS)
      }
    }

    toastStore.success(
      `Run test complete: ${testRunSucceeded.value} toggled${testRunSkipped.value ? `, ${testRunSkipped.value} skipped` : ""}.`,
    )
  } finally {
    testRunInProgress.value = false
  }
}

function createSwitchgearVisualOnly() {
  if (!switchgearCreatableCount.value) return
  const noun = switchgearCreatableCount.value === 1 ? "switchgear" : "switchgears"
  toastStore.info(`Create ${switchgearCreatableCount.value} ${noun} is UI-only for now.`)
}

async function ensureRuntimeCatalogLoaded() {
  await deviceStore.ensureLoaded()
  await channelStore.ensureLoaded()

  if (channels.value.length === 0 && deviceStore.devices.length > 0) {
    await Promise.allSettled(
      deviceStore.devices.map(device => channelStore.ensureDeviceChannelsLoaded(device.id)),
    )
  }
}

async function refreshAll() {
  try {
    await Promise.all([
      signalSheetStore.refreshSheet(),
      signalSheetStore.refreshAllocations(),
      ensureRuntimeCatalogLoaded(),
    ])
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

watch(
  sourceColumnHeaders,
  () => {
    rebuildGridRows()
  },
  { immediate: true },
)

watch(
  () => allocationRows.value.length,
  () => {
    rebuildGridRows()
  },
  { immediate: true, flush: "post" },
)

watch(
  recentlyChangedSignalIds,
  (signalIds) => {
    syncGridRowsBySignalIds(signalIds)
  },
  { flush: "post" },
)

function syncRealtimeUnitScope() {
  const units = new Set<string>()
  allocationRows.value.forEach((row) => {
    if (!row.channel_id) return
    const channel = channelMap.value.get(row.channel_id)
    if (!channel) return
    const unitId = channelUnitById.value.get(channel.id) ?? null
    if (unitId) units.add(unitId)
  })
  realtimeScopeStore.setRealtimeUnitScope(scopeId, [...units])
}

function scheduleRealtimeUnitScopeSync() {
  if (realtimeScopeSyncFrame !== null) {
    return
  }
  realtimeScopeSyncFrame = requestAnimationFrame(() => {
    realtimeScopeSyncFrame = null
    syncRealtimeUnitScope()
  })
}

watch(
  () => workspaceStore.activeWorkspaceId,
  async (workspaceId) => {
    if (!workspaceId) return
    await refreshAll()
  },
  { immediate: true },
)

watch(
  () => [allocationRevision.value, channels.value.length, deviceStore.devices.length],
  () => {
    scheduleRealtimeUnitScopeSync()
  },
  { immediate: true, flush: "post" },
)

watch(
  () => [route.query.import, workspaceMissing.value, loading.value] as const,
  ([importFlag, missingWorkspace, isLoading]) => {
    if (!isImportQueryRequested(importFlag)) return
    if (missingWorkspace || isLoading) return
    openImportModal()
    clearImportQueryFlag()
  },
  { immediate: true },
)

onBeforeUnmount(() => {
  if (realtimeScopeSyncFrame !== null) {
    cancelAnimationFrame(realtimeScopeSyncFrame)
    realtimeScopeSyncFrame = null
  }
  realtimeScopeStore.clearRealtimeUnitScope(scopeId)
})
</script>
