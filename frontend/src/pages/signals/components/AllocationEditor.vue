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
      </div>
      <div class="flex flex-wrap items-center gap-2">
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
        <div
          v-if="column.key === 'channel_select'"
          class="flex min-w-0 items-center justify-between gap-2"
        >
          <span
            class="truncate text-xs"
            :class="allocationValueClass(asAllocationRow(row))"
            :title="allocationDisplayLabel(asAllocationRow(row))"
          >
            {{ allocationDisplayLabel(asAllocationRow(row)) }}
          </span>
          <UiMenu
          >
            <UiMenuTrigger asChild>
              <button
                type="button"
                class="shrink-0 rounded border border-neutral-300 bg-white px-2 py-1 text-[11px] font-semibold text-neutral-700 hover:bg-neutral-100 disabled:cursor-default disabled:opacity-50 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200 dark:hover:bg-neutral-800"
                :disabled="loading"
              >
                {{ asAllocationRow(row).channel_id ? "Change" : "Bind" }}
              </button>
            </UiMenuTrigger>
            <UiMenuContent>
              <UiMenuItem
                v-if="asAllocationRow(row).channel_id"
                @select="() => setAllocationForRow(asAllocationRow(row), null)"
              >
                Unassign
              </UiMenuItem>
              <UiSubMenu
                v-for="group in channelGroupsForRow(asAllocationRow(row))"
                :key="`alloc-group-${asAllocationRow(row).signal_id}-${group.unitId}`"
              >
                <UiSubMenuTrigger class="flex w-full items-center justify-between px-2 py-1.5 text-left text-sm text-neutral-800 hover:bg-neutral-100 dark:text-neutral-100 dark:hover:bg-neutral-800">
                  <span>{{ group.unitId }}</span>
                  <span class="text-[11px] text-neutral-500 dark:text-neutral-400">{{ group.options.length }}</span>
                </UiSubMenuTrigger>
                <UiSubMenuContent>
                  <UiMenuItem
                    v-for="option in group.options"
                    :key="`alloc-${asAllocationRow(row).signal_id}-${option.id}`"
                    :disabled="option.disabled"
                    @select="() => handleAllocationMenuSelect(asAllocationRow(row), option)"
                  >
                    <span :class="option.disabled ? 'text-neutral-400 dark:text-neutral-500' : ''">
                      {{ channelSuffixLabel(option.label) }}
                    </span>
                  </UiMenuItem>
                </UiSubMenuContent>
              </UiSubMenu>
              <UiMenuItem
                v-if="channelGroupsForRow(asAllocationRow(row)).length === 0"
                disabled
              >
                No compatible channels
              </UiMenuItem>
            </UiMenuContent>
          </UiMenu>
        </div>

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

        <span v-else class="text-xs text-neutral-700 dark:text-neutral-100">{{ formatCell(value) }}</span>
      </template>
    </UiAffinoDataGrid>

    <SignalImportModal :open="importModalOpen" @close="closeImportModal" @imported="handleImported" />
  </div>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from "vue"
import { storeToRefs } from "pinia"
import {
  UiMenu,
  UiMenuContent,
  UiMenuItem,
  UiMenuSeparator,
  UiMenuTrigger,
  UiSubMenu,
  UiSubMenuContent,
  UiSubMenuTrigger,
} from "@affino/menu-vue"

import UiAffinoDataGrid from "@/components/ui/UiAffinoDataGrid.vue"
import UiButton from "@/components/ui/UiButton.vue"
import type { Channel, DoChannel } from "@/types/channel"
import type { SignalAllocationRow, SignalSheet } from "@/types/signal"
import SignalImportModal from "@/pages/signals/components/SignalImportModal.vue"
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

const { allocationRows, loadingAllocations, loadingSheet, allocatedCount, allocationRevision } = storeToRefs(signalSheetStore)
const { channels } = storeToRefs(channelStore)

const scopeId = "signals:allocations"
const INTERNAL_TYPE_COLUMN_KEY = "internal_type"
const importModalOpen = ref(false)
const persistentControlMenuOptions = { closeOnSelect: false }
const selectedRowKeys = ref<string[]>([])

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

const channelOwnerById = computed(() => {
  const map = new Map<number, number>()
  allocationRows.value.forEach((row) => {
    if (!Number.isFinite(row.channel_id as number)) return
    map.set(Number(row.channel_id), row.signal_id)
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

const allocationRowByRowKey = computed(() => {
  const map = new Map<string, SignalAllocationRow>()
  allocationRows.value.forEach((row) => {
    map.set(`signal-${row.signal_id}`, row)
  })
  return map
})

const selectedAllocationRows = computed(() => (
  selectedRowKeys.value
    .map(rowKey => allocationRowByRowKey.value.get(rowKey))
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

const sourceColumnHeaders = computed(() => resolveSourceColumnHeaders(signalSheetStore.sheet))

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
    { key: "control", label: "Control", width: 120, minWidth: 96, pin: "right" as const, meta: { filterable: false } },
  ]
})

const gridRows = computed(() =>
  allocationRows.value.map((row) => {
    const sourceRow = extractSourceRow(row.signal_metadata)
    const payload: Record<string, unknown> = {
      rowId: `signal-${row.signal_id}`,
      ...row,
      // Filtering/sorting must use the same human-readable text as in the cell.
      channel_select: allocationDisplayLabel(row),
      control: "",
    }
    sourceColumnHeaders.value.forEach((header, index) => {
      payload[sourceColumnKey(index)] = sourceRow[header] ?? ""
    })
    return payload
  }),
)

type GridRow = Record<string, unknown>
type ChannelOption = { id: number; label: string; disabled: boolean }
type ChannelOptionGroup = { unitId: string; options: ChannelOption[] }

function sourceColumnKey(index: number): string {
  return `source_col_${index}`
}

function normalizeHeaderList(raw: unknown): string[] {
  if (!Array.isArray(raw)) return []
  const seen = new Set<string>()
  const headers: string[] = []
  raw.forEach((item) => {
    const header = String(item ?? "").trim()
    if (!header || seen.has(header)) return
    seen.add(header)
    headers.push(header)
  })
  return headers
}

function resolveSourceColumnHeaders(sheet: SignalSheet | null): string[] {
  if (!sheet) return []

  const explicit = normalizeHeaderList(sheet.import_meta?.selected_columns ?? [])
  if (explicit.length > 0) {
    return explicit
  }

  const data = sheet.data
  if (!data || typeof data !== "object") {
    return []
  }

  const typedData = data as {
    default_sheet_index?: number
    sheets?: Array<{ index?: number; headers?: unknown[] }>
  }
  const sheets = Array.isArray(typedData.sheets) ? typedData.sheets : []
  if (!sheets.length) return []

  const defaultSheetIndex = Number(typedData.default_sheet_index)
  const byDefaultIndex = Number.isFinite(defaultSheetIndex)
    ? sheets.find(item => Number(item?.index) === defaultSheetIndex) ?? null
    : null
  const targetSheet = byDefaultIndex ?? sheets[0]
  const headers = normalizeHeaderList(targetSheet?.headers ?? [])
  if (!headers.length) return []

  const internalTypeColumn = String(
    sheet.import_meta?.internal_type_column || INTERNAL_TYPE_COLUMN_KEY,
  ).trim().toLowerCase()
  return headers.filter(header => header.trim().toLowerCase() !== internalTypeColumn)
}

function extractSourceRow(signalMetadata: Record<string, unknown>): Record<string, unknown> {
  const source = signalMetadata?.row
  if (!source || typeof source !== "object" || Array.isArray(source)) {
    return {}
  }
  return source as Record<string, unknown>
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
      disabled: deviceStatusById.value.get(channel.device_id) !== "online",
    })
  })
  return grouped
})

function channelOptionsForRow(row: SignalAllocationRow): ChannelOption[] {
  const ownerByChannel = channelOwnerById.value
  return channelOptionsBySignal(row.signal_direction).filter((option) => {
    const ownerSignalId = ownerByChannel.get(option.id)
    return ownerSignalId === undefined || ownerSignalId === row.signal_id
  })
}

function channelGroupsForRow(row: SignalAllocationRow): ChannelOptionGroup[] {
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

  return Array.from(groups.entries()).map(([unitId, options]) => ({ unitId, options }))
}

function channelSuffixLabel(label: string): string {
  const slashIndex = label.indexOf("/")
  if (slashIndex === -1 || slashIndex + 1 >= label.length) {
    return label
  }
  return label.slice(slashIndex + 1)
}

function setAllocationForRow(row: SignalAllocationRow, nextChannelId: number | null) {
  void signalSheetStore
    .setAllocation(row.signal_id, Number.isFinite(nextChannelId as number) ? nextChannelId : null)
    .catch((err) => {
      toastStore.error(err instanceof Error ? err.message : String(err))
    })
}

function handleAllocationMenuSelect(row: SignalAllocationRow, option: ChannelOption) {
  if (option.disabled) return
  setAllocationForRow(row, option.id)
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

async function sendControl(row: SignalAllocationRow, state: boolean) {
  const target = resolveControlTarget(row)
  if (!target) {
    toastStore.error("Channel not found")
    return
  }
  if (!target.online) {
    toastStore.error("Device is offline")
    return
  }
  if (controlBusy(row) && target.channel.ui?.target === state) {
    return
  }

  try {
    channelStore.sendDoCommand(target.unitId, target.channelIndex, state)
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
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
    const units = new Set<string>()
    allocationRows.value.forEach((row) => {
      if (!row.channel_id) return
      const channel = channelMap.value.get(row.channel_id)
      if (!channel) return
      const unitId = channelStore.resolveUnitId(channel.device_id)
      if (unitId) units.add(unitId)
    })
    realtimeScopeStore.setRealtimeUnitScope(scopeId, [...units])
  },
  { immediate: true },
)

watch(
  allocationRowByRowKey,
  (rowMap) => {
    if (!selectedRowKeys.value.length) return
    selectedRowKeys.value = selectedRowKeys.value.filter(rowKey => rowMap.has(rowKey))
  },
)

onBeforeUnmount(() => {
  realtimeScopeStore.clearRealtimeUnitScope(scopeId)
})
</script>
