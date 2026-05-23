<template>
  <section class="flex min-h-0 flex-col rounded-2xl border border-neutral-200 bg-white shadow-sm dark:border-neutral-800 dark:bg-neutral-900">
    <header class="flex flex-col gap-2 border-b border-neutral-100 px-5 py-4 dark:border-neutral-800">
      <div class="flex items-start justify-between gap-3">
        <div>
          <p class="text-base font-semibold text-neutral-900 dark:text-neutral-50">Live Signals</p>
          <p class="text-xs text-neutral-500 dark:text-neutral-400">{{ summaryLine }}</p>
        </div>
        <div class="flex gap-2">
          <UiButton size="xs" variant="ghost" @click="emit('refresh-channels')">Sync channels</UiButton>
          <UiButton size="xs" variant="ghost" @click="emit('refresh-signals')">Reload signals</UiButton>
        </div>
      </div>
      <div class="flex items-center gap-2 text-[11px] uppercase tracking-[0.35em] text-neutral-400 dark:text-neutral-500">
        <span>Active {{ activeCount }}</span>
        <span>·</span>
        <span>Bound {{ boundCount }}</span>
      </div>
    </header>

    <div class="flex flex-1 min-h-0 flex-col gap-3 px-5 py-4">
      <div
        v-if="!workspaceReady"
        class="flex flex-1 items-center justify-center rounded-xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400"
      >
        Choose a workspace to load live signals.
      </div>
      <div
        v-else-if="loading"
        class="flex flex-1 items-center justify-center text-sm text-neutral-500 dark:text-neutral-400"
      >
        Loading live signals…
      </div>
      <template v-else>
        <div class="flex flex-col gap-2 md:flex-row md:items-center md:justify-between">
          <input
            v-model="query"
            type="text"
            id="live-signals-query"
            name="live-signals-query"
            autocomplete="off"
            placeholder="Search name or key…"
            class="w-full rounded-xl border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 placeholder-neutral-500 focus:outline-none dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100 md:max-w-sm"
          />
          <label class="flex items-center gap-2 text-xs font-medium text-neutral-600 dark:text-neutral-300">
            <input
              v-model="showOnlyBound"
              id="live-signals-show-only-bound"
              name="live-signals-show-only-bound"
              type="checkbox"
              autocomplete="off"
              class="accent-neutral-900"
            />
            Bound only
          </label>
        </div>

        <div class="affino-native-data-grid flex-1 min-h-0">
          <div class="affino-native-data-grid__shell">
            <DataGrid
              class="affino-native-data-grid__grid"
              :rows="gridRows"
              :columns="resolvedColumns"
              :theme="theme"
              :client-row-model-options="clientRowModelOptions"
              :virtualization="virtualizationOptions"
              :base-row-height="38"
              render-mode="virtualization"
              layout-mode="fill"
              row-hover
              striped-rows
            />
          </div>
        </div>
      </template>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, h, ref } from "vue"
import { defineDataGridComponent, type DataGridAppCellRendererContext, type DataGridAppColumnInput, type DataGridProps } from "@affino/datagrid-vue-app"

import UiBadge from "@/components/ui/UiBadge.vue"
import UiButton from "@/components/ui/UiButton.vue"
import { useAffinoDataGridTheme } from "@/components/ui/affinoDataGridTheme"
import "@/components/ui/affinoDataGridNative.css"
import { useChannelStore } from "@/stores/channelStore"
import { CHANNEL_TYPES, type Channel, type DoChannel } from "@/types/channel"
import type { AllocationMappingItem, Signal } from "@/types/signal"

const channelStore = useChannelStore()

const props = defineProps<{
  signals: Signal[]
  mapping: AllocationMappingItem[]
  channels: Channel[]
  workspaceReady: boolean
  loading: boolean
}>()

const emit = defineEmits<{ (e: "refresh-signals"): void; (e: "refresh-channels"): void }>()

const query = ref("")
const showOnlyBound = ref(false)
const { theme } = useAffinoDataGridTheme()

interface LiveSignalRow {
  signal: Signal
  mapping: AllocationMappingItem | null
  channel: Channel | null
}

interface LiveSignalGridRow extends Record<string, unknown> {
  rowId: string
  name: string
  key: string
  direction: string
  channel: string
  source_link: string
  live: string
  source: LiveSignalRow
}

const DataGrid = defineDataGridComponent<LiveSignalGridRow>()

const rows = computed<LiveSignalRow[]>(() => {
  const mappingBySignal = new Map<string, AllocationMappingItem>()
  props.mapping?.forEach(item => {
    if (item.signal_key) {
      mappingBySignal.set(item.signal_key, item)
    }
  })

  const channelById = new Map<number, Channel>()
  props.channels?.forEach(channel => {
    channelById.set(channel.id, channel)
  })

  return (props.signals ?? []).map((signal) => {
    const mapped = mappingBySignal.get(signal.key) ?? null
    const parsedChannelId = normalizeChannelId(mapped?.channel_id)
    const channel = parsedChannelId !== null ? (channelById.get(parsedChannelId) ?? null) : null
    return {
      signal,
      mapping: mapped,
      channel,
    }
  })
})

const filteredRows = computed(() => {
  const q = query.value.trim().toLowerCase()
  return rows.value.filter((row) => {
    if (showOnlyBound.value && !row.channel) return false
    if (!q) return true
    return row.signal.name.toLowerCase().includes(q) || row.signal.key.toLowerCase().includes(q)
  })
})

function renderLiveSignalsDefaultCell(context: DataGridAppCellRendererContext<LiveSignalGridRow>) {
  return h("span", { class: "truncate" }, String(context.value ?? ""))
}

const resolvedColumns = computed<DataGridAppColumnInput<LiveSignalGridRow>[]>(() => [
  {
    key: "name",
    label: "Signal",
    minWidth: 180,
    initialState: { width: 260 },
    presentation: { align: "left", headerAlign: "left" },
    cellRenderer: renderLiveSignalsDefaultCell,
  },
  {
    key: "key",
    label: "Key",
    minWidth: 160,
    initialState: { width: 210 },
    presentation: { align: "left", headerAlign: "left" },
    cellRenderer: renderLiveSignalsDefaultCell,
  },
  {
    key: "direction",
    label: "Direction",
    minWidth: 110,
    initialState: { width: 120 },
    presentation: { align: "left", headerAlign: "left" },
    cellRenderer: (context: DataGridAppCellRendererContext<LiveSignalGridRow>) => h(
      UiBadge,
      { variant: resolveDirectionBadge(context.row?.source.signal.io_direction ?? "DI") },
      () => context.row?.source.signal.io_direction ?? "—",
    ),
  },
  {
    key: "channel",
    label: "Channel",
    minWidth: 180,
    initialState: { width: 260 },
    presentation: { align: "left", headerAlign: "left" },
    cellRenderer: renderLiveSignalsDefaultCell,
  },
  {
    key: "source_link",
    label: "Source Link",
    minWidth: 180,
    initialState: { width: 300 },
    presentation: { align: "left", headerAlign: "left" },
    cellRenderer: renderLiveSignalsDefaultCell,
  },
  {
    key: "live",
    label: "Live State",
    minWidth: 110,
    initialState: { width: 130 },
    presentation: { align: "left", headerAlign: "left" },
    cellRenderer: renderLiveSignalsDefaultCell,
  },
])

const clientRowModelOptions: NonNullable<DataGridProps<LiveSignalGridRow>["clientRowModelOptions"]> = {
  resolveRowId: row => gridRowKey(row),
}

const virtualizationOptions = computed(() => ({
  rows: true,
  columns: true,
  rowOverscan: 10,
  columnOverscan: 2,
}))

const gridRows = computed<LiveSignalGridRow[]>(() =>
  filteredRows.value.map((row) => ({
    rowId: String(row.signal.id),
    name: row.signal.name,
    key: row.signal.key,
    direction: row.signal.io_direction,
    channel: describeChannel(row),
    source_link: describeMapping(row),
    live: liveStateLabel(row),
    source: row,
  })),
)

const boundCount = computed(() => rows.value.filter(row => Boolean(row.channel)).length)
const activeCount = computed(() => rows.value.filter(row => row.signal.is_active).length)

const summaryLine = computed(() => {
  if (!rows.value.length) return "No live signals yet"
  return `${rows.value.length} signals · ${boundCount.value} bound`
})

const workspaceReady = computed(() => props.workspaceReady)
const loading = computed(() => props.loading)

function gridRowKey(row: Record<string, unknown>): string {
  return String(row.rowId ?? "")
}

function normalizeChannelId(raw: AllocationMappingItem["channel_id"] | null | undefined): number | null {
  if (raw === null || raw === undefined) return null
  if (typeof raw === "number" && Number.isFinite(raw)) return raw
  const parsed = Number(raw)
  return Number.isFinite(parsed) ? parsed : null
}

function describeChannel(row: LiveSignalRow): string {
  const channel = row.channel
  if (!channel) return "Unassigned"
  return channelStore.resolveChannelFullLabel(channel)
}

function describeMapping(row: LiveSignalRow): string {
  if (!row.mapping) return "Not bound"
  const meta = row.mapping.meta
  const sheet = meta?.sheet_name ?? (typeof meta?.sheet_index === "number" ? `Sheet ${meta.sheet_index + 1}` : "Sheet ?")
  const column = meta?.column_key ?? "col"
  const rowIndex = Number.isFinite(row.mapping.signal_row_index)
    ? `row ${Number(row.mapping.signal_row_index) + 1}`
    : "row ?"
  return `${sheet} · ${column} @ ${rowIndex}`
}

function liveStateLabel(row: LiveSignalRow): string {
  if (!row.channel) return "—"
  if (row.channel.type === CHANNEL_TYPES.DO) {
    const doChannel = row.channel as DoChannel
    const stage = doChannel.ui?.stage
    if (stage === "pending" || stage === "debounce") return "Switching…"
    if (stage === "error") return "Error"
    return doChannel.state ? "ON" : "OFF"
  }
  if (row.channel.type === CHANNEL_TYPES.DI) {
    return row.channel.state ? "HIGH" : "LOW"
  }
  if (row.channel.type === CHANNEL_TYPES.AO) {
    if (typeof row.channel.state === "number") {
      return `${row.channel.state.toFixed(1)} mA`
    }
    return "—"
  }
  return "—"
}

function resolveDirectionBadge(direction: Signal["io_direction"]) {
  if (direction === "DO" || direction === "AO") return "info"
  if (direction === "DI" || direction === "AI") return "success"
  return "neutral"
}
</script>
