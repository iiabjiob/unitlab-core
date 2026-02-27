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

        <div class="flex-1 min-h-0">
          <UiAffinoDataGrid
            :rows="gridRows"
            :columns="gridColumns"
            :row-height="38"
            :overscan-rows="10"
            :overscan-columns="2"
            :enable-filtering="true"
            :enable-column-resize="true"
            :empty-text="'No signals match your filters.'"
            :row-key="gridRowKey"
          >
            <template #cell="{ column, row, value }">
              <template v-if="column.key === 'direction'">
                <UiBadge :variant="resolveDirectionBadge(rowAsGridRow(row).source.signal.io_direction)">
                  {{ rowAsGridRow(row).source.signal.io_direction }}
                </UiBadge>
              </template>

              <template v-else-if="column.key === 'action'">
                <UiButton
                  v-if="canToggle(rowAsGridRow(row).source)"
                  size="sm"
                  variant="secondary"
                  :disabled="isChannelBusy(rowAsGridRow(row).source)"
                  @click.stop="toggleDigital(rowAsGridRow(row).source)"
                >
                  {{ toggleLabel(rowAsGridRow(row).source) }}
                </UiButton>
                <span v-else class="text-xs text-neutral-400">—</span>
              </template>

              <template v-else>
                <span class="truncate">{{ String(value ?? '') }}</span>
              </template>
            </template>
          </UiAffinoDataGrid>
        </div>
      </template>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, ref } from "vue"

import UiAffinoDataGrid from "@/components/ui/UiAffinoDataGrid.vue"
import UiBadge from "@/components/ui/UiBadge.vue"
import UiButton from "@/components/ui/UiButton.vue"
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
  action: string
  source: LiveSignalRow
}

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

const gridColumns = computed(() => [
  { key: "name", label: "Signal", width: 260, minWidth: 180 },
  { key: "key", label: "Key", width: 210, minWidth: 160 },
  { key: "direction", label: "Direction", width: 120, minWidth: 110 },
  { key: "channel", label: "Channel", width: 260, minWidth: 180 },
  { key: "source_link", label: "Source Link", width: 300, minWidth: 180 },
  { key: "live", label: "Live State", width: 130, minWidth: 110 },
  { key: "action", label: "Action", width: 120, minWidth: 100 },
])

const gridRows = computed<LiveSignalGridRow[]>(() =>
  filteredRows.value.map((row) => ({
    rowId: String(row.signal.id),
    name: row.signal.name,
    key: row.signal.key,
    direction: row.signal.io_direction,
    channel: describeChannel(row),
    source_link: describeMapping(row),
    live: liveStateLabel(row),
    action: canToggle(row) ? toggleLabel(row) : "",
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

function rowAsGridRow(value: unknown): LiveSignalGridRow {
  return value as LiveSignalGridRow
}

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

function canToggle(row: LiveSignalRow): boolean {
  return Boolean(row.channel && row.channel.type === CHANNEL_TYPES.DO)
}

function isChannelBusy(row: LiveSignalRow): boolean {
  if (!row.channel || row.channel.type !== CHANNEL_TYPES.DO) return false
  const stage = (row.channel as DoChannel).ui?.stage
  return stage === "pending" || stage === "debounce"
}

function toggleLabel(row: LiveSignalRow): string {
  if (!row.channel || row.channel.type !== CHANNEL_TYPES.DO) return "Toggle"
  return row.channel.state ? "Turn off" : "Turn on"
}

function toggleDigital(row: LiveSignalRow) {
  if (!row.channel || row.channel.type !== CHANNEL_TYPES.DO) return
  if (isChannelBusy(row)) return
  const next = !row.channel.state
  const unitId = channelStore.resolveUnitId(row.channel.device_id)
  channelStore.sendDoCommand(unitId, row.channel.index, next)
}
</script>
