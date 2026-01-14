<template>
  <section class="flex min-h-0 flex-col rounded-2xl border border-neutral-200 bg-white shadow-sm dark:border-neutral-800 dark:bg-neutral-900">
    <header class="flex flex-col gap-2 border-b border-neutral-100 px-5 py-4 dark:border-neutral-800">
      <div class="flex items-start justify-between gap-3">
        <div>
          <p class="text-base font-semibold text-neutral-900 dark:text-neutral-50">Live Signals</p>
          <p class="text-xs text-neutral-500 dark:text-neutral-400">
            {{ summaryLine }}
          </p>
        </div>
        <div class="flex gap-2">
          <UiButton size="xs" variant="ghost" @click="emit('refresh-channels')">
            Sync channels
          </UiButton>
          <UiButton size="xs" variant="ghost" @click="emit('refresh-signals')">
            Reload signals
          </UiButton>
        </div>
      </div>
      <div class="flex items-center gap-2 text-[11px] uppercase tracking-[0.35em] text-neutral-400 dark:text-neutral-500">
        <span>Active {{ activeCount }}</span>
        <span>·</span>
        <span>Bound {{ boundCount }}</span>
      </div>
    </header>

    <div class="flex flex-1 min-h-0 flex-col px-5 py-4">
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
      <div v-else class="flex flex-1 min-h-0 flex-col gap-3">
        <div class="flex flex-col gap-2">
          <input
            v-model="query"
            type="text"
            autocomplete="off"
            placeholder="Search name or key…"
            class="w-full rounded-xl border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 placeholder-neutral-500 focus:outline-none dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
          />
          <label class="flex items-center gap-2 text-xs font-medium text-neutral-600 dark:text-neutral-300">
            <input v-model="showOnlyBound" type="checkbox" class="accent-neutral-900" />
            Bound only
          </label>
        </div>

        <div class="flex-1 min-h-0 space-y-3 overflow-y-auto pr-1">
          <article
            v-for="row in filteredRows"
            :key="row.signal.id"
            class="rounded-xl border border-neutral-200 px-4 py-3 dark:border-neutral-800"
          >
            <div class="flex items-start justify-between gap-2">
              <div>
                <p class="text-sm font-semibold text-neutral-900 dark:text-neutral-50">{{ row.signal.name }}</p>
                <p class="text-xs font-mono text-neutral-500 dark:text-neutral-400">{{ row.signal.key }}</p>
              </div>
              <UiBadge :variant="row.signal.is_active ? 'success' : 'neutral'">
                {{ row.signal.io_direction }}
              </UiBadge>
            </div>

            <div class="mt-3 text-xs text-neutral-500 dark:text-neutral-400">
              <p>
                <span class="font-medium text-neutral-700 dark:text-neutral-200">Channel:</span>
                <span class="ml-1">{{ describeChannel(row) }}</span>
              </p>
              <p v-if="row.mapping">
                <span class="font-medium text-neutral-700 dark:text-neutral-200">Snapshot:</span>
                <span class="ml-1">{{ describeMapping(row) }}</span>
              </p>
              <p v-else class="italic">Not bound in this snapshot</p>
            </div>

            <div class="mt-3 flex items-center justify-between gap-3 rounded-lg bg-neutral-50 px-3 py-2 text-xs dark:bg-neutral-900/60">
              <div>
                <p class="text-[11px] uppercase tracking-[0.35em] text-neutral-500 dark:text-neutral-400">Live state</p>
                <p class="text-sm font-semibold text-neutral-900 dark:text-neutral-50">{{ liveStateLabel(row) }}</p>
              </div>
              <UiButton
                v-if="canToggle(row)"
                size="sm"
                variant="secondary"
                :disabled="isChannelBusy(row)"
                @click="toggleDigital(row)"
              >
                {{ toggleLabel(row) }}
              </UiButton>
            </div>
          </article>

          <div
            v-if="!filteredRows.length"
            class="rounded-xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400"
          >
            No signals match your filters.
          </div>
        </div>
      </div>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, ref } from "vue"

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
    const mapping = mappingBySignal.get(signal.key) ?? null
    const parsedChannelId = normalizeChannelId(mapping?.channel_id)
    const channel = parsedChannelId !== null ? (channelById.get(parsedChannelId) ?? null) : null
    return {
      signal,
      mapping,
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

const boundCount = computed(() => rows.value.filter(row => Boolean(row.channel)).length)
const activeCount = computed(() => rows.value.filter(row => row.signal.is_active).length)

const summaryLine = computed(() => {
  if (!rows.value.length) return "No live signals yet"
  return `${rows.value.length} signals · ${boundCount.value} bound`
})

const workspaceReady = computed(() => props.workspaceReady)
const loading = computed(() => props.loading)

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
  if (!row.mapping) return "—"
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
