<template>
  <section class="rounded-2xl border border-neutral-200/70 bg-white/90 p-4 dark:border-neutral-800 dark:bg-neutral-950/40">
    <div class="flex flex-wrap items-start justify-between gap-3">
      <div class="min-w-0">
        <p class="text-[11px] uppercase tracking-[0.26em] text-neutral-500 dark:text-neutral-400">Time / NTP Sync</p>
        <p class="mt-1 text-sm text-neutral-700 dark:text-neutral-200">
          Mode:
          <span class="font-semibold uppercase">{{ ntpModeText }}</span>
          <template v-if="ntpSnapshot?.last_event"> · {{ ntpSnapshot.last_event }}</template>
        </p>
        <p class="mt-1 text-xs text-neutral-500 dark:text-neutral-400">
          Chrony:
          <span class="font-medium">{{ ntpChronyServiceLabel }}</span>
          · Service {{ ntpChronyServiceActiveLabel }}
          · Sync {{ ntpSyncLabel }}
          <template v-if="ntpTracking?.stratum !== undefined && ntpTracking?.stratum !== null">
            · Stratum {{ ntpTracking.stratum }}
          </template>
        </p>
        <p v-if="ntpTrackingSummary" class="mt-1 text-xs text-neutral-500 dark:text-neutral-400">
          {{ ntpTrackingSummary }}
        </p>
        <p v-if="ntpErrorText" class="mt-1 text-xs text-rose-500">{{ ntpErrorText }}</p>
      </div>
    </div>

    <div class="mt-3 grid gap-3 lg:grid-cols-[minmax(0,1.05fr)_minmax(0,0.95fr)]">
      <div class="rounded-xl border border-neutral-200/80 bg-neutral-50/80 p-3 dark:border-neutral-800 dark:bg-neutral-900/60">
        <div class="mb-2 flex items-center justify-between gap-2">
          <p class="text-xs font-semibold text-neutral-700 dark:text-neutral-200">Configured NTP Servers (Chrony)</p>
        </div>

        <div class="space-y-2">
          <div
            v-for="(server, idx) in draftServers"
            :key="`ntp-server-${idx}`"
            class="grid items-center gap-2 sm:grid-cols-[minmax(0,1fr)_auto]"
          >
            <input
              v-model.trim="draftServers[idx]"
              type="text"
              autocomplete="off"
              class="w-full rounded-lg border border-neutral-300 bg-white px-2 py-1.5 text-sm text-neutral-900 outline-none focus:border-neutral-500 dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
              placeholder="pool.ntp.org or 192.168.1.10"
              @input="markDirty"
            />
            <button
              type="button"
              class="rounded-md border border-rose-300 px-2 py-1 text-[11px] font-semibold text-rose-700 hover:border-rose-500 dark:border-rose-700 dark:text-rose-200 dark:hover:border-rose-500"
              :disabled="ntpBusy"
              @click="removeServer(idx)"
            >
              Remove
            </button>
          </div>
        </div>

        <div class="mt-3 flex flex-wrap items-center gap-2">
          <button
            type="button"
            class="rounded-md border border-neutral-300 px-2 py-1 text-[11px] font-semibold text-neutral-700 hover:border-neutral-500 dark:border-neutral-700 dark:text-neutral-200 dark:hover:border-neutral-500"
            :disabled="ntpBusy"
            @click="addServer"
          >
            Add server
          </button>
          <button
            type="button"
            class="rounded-lg bg-emerald-600 px-3 py-1.5 text-xs font-semibold text-white hover:bg-emerald-500 disabled:cursor-not-allowed disabled:opacity-60"
            :disabled="ntpBusy || !canApplyServers"
            @click="applyNtpServers"
          >
            Apply servers
          </button>
          <span class="text-[11px] text-neutral-500 dark:text-neutral-400">
            {{ draftServersCount }} server{{ draftServersCount === 1 ? "" : "s" }}
            <template v-if="ntpDirty"> · edited</template>
          </span>
        </div>

        <div v-if="ntpSnapshot?.request_in_flight" class="mt-2 rounded-lg border border-amber-200 bg-amber-50 px-2 py-1 text-[11px] text-amber-800 dark:border-amber-700/50 dark:bg-amber-950/40 dark:text-amber-200">
          In progress: {{ ntpSnapshot.request_in_flight.action }} ({{ ntpSnapshot.request_in_flight.request_id }})
        </div>
      </div>

      <div class="rounded-xl border border-neutral-200/80 bg-neutral-50/80 p-3 dark:border-neutral-800 dark:bg-neutral-900/60">
        <div class="mb-2 flex items-center justify-between gap-2">
          <p class="text-xs font-semibold text-neutral-700 dark:text-neutral-200">Chrony Status</p>
          <span class="text-[11px] text-neutral-500 dark:text-neutral-400">{{ ntpSnapshot?.updated_at || "—" }}</span>
        </div>

        <div class="rounded-lg border border-neutral-200 bg-white/80 p-3 dark:border-neutral-800 dark:bg-neutral-950/50">
          <div class="flex items-center justify-between gap-2">
            <p class="text-[11px] font-semibold uppercase tracking-[0.12em] text-neutral-500 dark:text-neutral-400">
              Manual system time
            </p>
            <span class="text-[11px] text-neutral-500 dark:text-neutral-400">Browser local timezone</span>
          </div>

          <div class="mt-2 grid gap-2 text-xs">
            <div class="flex items-center justify-between gap-2">
              <span class="text-neutral-500 dark:text-neutral-400">RPi local</span>
              <span class="truncate font-medium text-neutral-800 dark:text-neutral-100" :title="ntpSystemTimeLocalRaw || '—'">{{ ntpSystemTimeLocalText }}</span>
            </div>
            <div class="flex items-center justify-between gap-2">
              <span class="text-neutral-500 dark:text-neutral-400">RPi UTC</span>
              <span class="truncate font-medium text-neutral-800 dark:text-neutral-100" :title="ntpSystemTimeUtcRaw || '—'">{{ ntpSystemTimeUtcText }}</span>
            </div>
          </div>

          <div class="mt-3 grid gap-2 sm:grid-cols-[minmax(0,1fr)_auto_auto]">
            <input
              v-model="manualTimeDraft"
              type="datetime-local"
              step="1"
              class="w-full rounded-lg border border-neutral-300 bg-white px-2 py-1.5 text-sm text-neutral-900 outline-none focus:border-neutral-500 dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
              :disabled="ntpBusy"
              @pointerdown="beginManualTimeInteraction"
              @focus="beginManualTimeInteraction"
              @blur="endManualTimeInteraction"
              @input="markManualTimeDirty"
            />
            <button
              type="button"
              class="rounded-md border border-neutral-300 px-2 py-1 text-[11px] font-semibold text-neutral-700 hover:border-neutral-500 dark:border-neutral-700 dark:text-neutral-200 dark:hover:border-neutral-500"
              :disabled="ntpBusy"
              @click="setManualTimeFromBrowser"
            >
              Browser now
            </button>
            <button
              type="button"
              class="rounded-md border border-neutral-300 px-2 py-1 text-[11px] font-semibold text-neutral-700 hover:border-neutral-500 dark:border-neutral-700 dark:text-neutral-200 dark:hover:border-neutral-500"
              :disabled="ntpBusy"
              @click="resetManualTimeDraft"
            >
              Use RPi time
            </button>
          </div>

          <div class="mt-2 flex flex-wrap items-center gap-2">
            <button
              type="button"
              class="rounded-lg bg-sky-600 px-3 py-1.5 text-xs font-semibold text-white hover:bg-sky-500 disabled:cursor-not-allowed disabled:opacity-60"
              :disabled="!canApplyManualTime"
              @click="applyManualTime"
            >
              Set system time
            </button>
            <button
              type="button"
              class="rounded-md border border-neutral-300 px-2 py-1 text-[11px] font-semibold text-neutral-700 hover:border-neutral-500 dark:border-neutral-700 dark:text-neutral-200 dark:hover:border-neutral-500"
              :disabled="ntpBusy"
              @click="resumeUpstreamSync"
            >
              Resume NTP sync
            </button>
            <span class="text-[11px] text-neutral-500 dark:text-neutral-400">
              Manual set pauses upstream chrony sources so the local time does not jump back immediately.
            </span>
          </div>
        </div>

        <div class="mt-3 grid gap-2 text-xs">
          <div class="flex items-center justify-between gap-2">
            <span class="text-neutral-500 dark:text-neutral-400">Configured</span>
            <span class="font-medium text-neutral-800 dark:text-neutral-100">{{ ntpConfiguredServers.length }}</span>
          </div>
          <div class="flex items-center justify-between gap-2">
            <span class="text-neutral-500 dark:text-neutral-400">Effective sources</span>
            <span class="font-medium text-neutral-800 dark:text-neutral-100">{{ ntpEffectiveServers.length }}</span>
          </div>
          <div class="flex items-center justify-between gap-2">
            <span class="text-neutral-500 dark:text-neutral-400">Reference</span>
            <span class="truncate font-medium text-neutral-800 dark:text-neutral-100" :title="ntpTracking?.source || '—'">{{ ntpTracking?.source || "—" }}</span>
          </div>
          <div class="flex items-center justify-between gap-2">
            <span class="text-neutral-500 dark:text-neutral-400">Offset</span>
            <span class="font-medium text-neutral-800 dark:text-neutral-100">{{ ntpOffsetText }}</span>
          </div>
        </div>

        <div class="mt-3 rounded-lg border border-neutral-200 bg-white/80 p-1 dark:border-neutral-800 dark:bg-neutral-950/50">
          <p class="px-2 py-1 text-[11px] font-semibold uppercase tracking-[0.12em] text-neutral-500 dark:text-neutral-400">
            Effective sources
          </p>
          <div class="max-h-40 overflow-y-auto">
            <div
              v-for="src in ntpSources"
              :key="`${src.name}-${src.mode_mark || ''}${src.state_mark || ''}`"
              class="flex items-center justify-between gap-2 px-2 py-1 text-xs"
            >
              <span class="min-w-0 truncate text-neutral-800 dark:text-neutral-100">
                <span class="mr-1 font-mono text-[10px] text-neutral-500 dark:text-neutral-400">{{ src.mode_mark || "?" }}{{ src.state_mark || "?" }}</span>
                {{ src.name }}
              </span>
              <span class="shrink-0 text-neutral-500 dark:text-neutral-400">
                s{{ src.stratum ?? "?" }} · reach {{ src.reach ?? "?" }}
              </span>
            </div>
            <p v-if="ntpSources.length === 0" class="px-2 py-2 text-xs text-neutral-500 dark:text-neutral-400">
              No chrony sources reported yet.
            </p>
          </div>
        </div>
      </div>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from "vue"
import { useCoreNtpStore } from "@/stores/coreNtpStore"

const coreNtpStore = useCoreNtpStore()

const ntpSnapshot = computed(() => coreNtpStore.snapshot)
const ntpModeText = computed(() => String(coreNtpStore.mode).toUpperCase())
const ntpTracking = computed(() => coreNtpStore.tracking)
const ntpSources = computed(() => coreNtpStore.sources)
const ntpConfiguredServers = computed(() => coreNtpStore.configuredServers)
const ntpEffectiveServers = computed(() => coreNtpStore.effectiveServers)
const ntpBusy = computed(() => coreNtpStore.commandPending || coreNtpStore.loading)
const ntpErrorText = computed(() => coreNtpStore.lastError || coreNtpStore.snapshot?.last_error || null)
const ntpChronyServiceLabel = computed(() => ntpSnapshot.value?.chrony_service_name || "chrony")
const ntpChronyServiceActiveLabel = computed(() => {
  const active = ntpSnapshot.value?.chrony_service_active
  if (active === true) return "ACTIVE"
  if (active === false) return "INACTIVE"
  return "UNKNOWN"
})
const ntpSyncLabel = computed(() => (coreNtpStore.isSynced ? "SYNCED" : "NOT SYNCED"))
const ntpSystemTimeUtcRaw = computed(() => ntpSnapshot.value?.system_time_utc || null)
const ntpSystemTimeLocalRaw = computed(() => ntpSnapshot.value?.system_time_local || null)
const ntpOffsetText = computed(() => {
  const value = ntpTracking.value?.system_time_offset_seconds
  if (typeof value !== "number" || !Number.isFinite(value)) return "—"
  const ms = value * 1000
  return `${ms.toFixed(Math.abs(ms) < 1 ? 3 : 1)} ms`
})
const ntpSystemTimeUtcText = computed(() => formatTimestamp(ntpSystemTimeUtcRaw.value))
const ntpSystemTimeLocalText = computed(() => formatTimestamp(ntpSystemTimeLocalRaw.value))
const ntpTrackingSummary = computed(() => {
  const tracking = ntpTracking.value
  if (!tracking) return null
  const parts: string[] = []
  if (tracking.leap_status) parts.push(`leap ${tracking.leap_status}`)
  if (typeof tracking.update_interval_seconds === "number") parts.push(`update ${tracking.update_interval_seconds}s`)
  if (typeof tracking.root_dispersion_seconds === "number") parts.push(`dispersion ${tracking.root_dispersion_seconds}s`)
  return parts.length ? parts.join(" · ") : null
})

const draftServers = ref<string[]>([])
const ntpDirty = ref(false)
const manualTimeDraft = ref("")
const manualTimeDirty = ref(false)
const manualTimeInteracting = ref(false)

function formatTimestamp(value: string | null | undefined) {
  if (!value) return "—"
  const parsed = new Date(value)
  if (Number.isNaN(parsed.getTime())) return value
  return parsed.toLocaleString()
}

function toDateTimeLocalValue(value: string | null | undefined) {
  if (!value) return ""
  const parsed = new Date(value)
  if (Number.isNaN(parsed.getTime())) return ""
  const parts = [
    parsed.getFullYear(),
    String(parsed.getMonth() + 1).padStart(2, "0"),
    String(parsed.getDate()).padStart(2, "0"),
  ]
  const time = [
    String(parsed.getHours()).padStart(2, "0"),
    String(parsed.getMinutes()).padStart(2, "0"),
    String(parsed.getSeconds()).padStart(2, "0"),
  ]
  return `${parts[0]}-${parts[1]}-${parts[2]}T${time[0]}:${time[1]}:${time[2]}`
}

function syncManualTimeDraftFromSnapshot() {
  manualTimeDraft.value = toDateTimeLocalValue(
    ntpSnapshot.value?.system_time_local || ntpSnapshot.value?.system_time_utc || ntpSnapshot.value?.updated_at || null,
  )
  manualTimeDirty.value = false
}

function normalizeServerList(values: readonly string[]): string[] {
  const result: string[] = []
  const seen = new Set<string>()
  values.forEach((value) => {
    const trimmed = String(value ?? "").trim()
    if (!trimmed) return
    if (seen.has(trimmed)) return
    seen.add(trimmed)
    result.push(trimmed)
  })
  return result
}

function syncDraftFromSnapshot() {
  const next = normalizeServerList(ntpConfiguredServers.value)
  draftServers.value = next.length > 0 ? next : [""]
  ntpDirty.value = false
}

watch(
  () => ntpSnapshot.value?.configured_servers,
  () => {
    if (!ntpDirty.value) {
      syncDraftFromSnapshot()
    }
  },
  { immediate: true },
)

watch(
  () => [ntpSnapshot.value?.system_time_local, ntpSnapshot.value?.system_time_utc, ntpSnapshot.value?.updated_at],
  () => {
    if (!manualTimeDirty.value && !manualTimeInteracting.value) {
      syncManualTimeDraftFromSnapshot()
    }
  },
  { immediate: true },
)

function markDirty() {
  ntpDirty.value = true
}

function markManualTimeDirty() {
  manualTimeDirty.value = true
}

function beginManualTimeInteraction() {
  manualTimeInteracting.value = true
}

function endManualTimeInteraction() {
  manualTimeInteracting.value = false
}

function addServer() {
  draftServers.value = [...draftServers.value, ""]
  ntpDirty.value = true
}

function removeServer(index: number) {
  draftServers.value = draftServers.value.filter((_, idx) => idx !== index)
  ntpDirty.value = true
}

function resetDraftServers() {
  syncDraftFromSnapshot()
}

function resetManualTimeDraft() {
  syncManualTimeDraftFromSnapshot()
  manualTimeInteracting.value = false
}

function setManualTimeFromBrowser() {
  manualTimeDraft.value = toDateTimeLocalValue(new Date().toISOString())
  manualTimeDirty.value = true
}

const normalizedDraftServers = computed(() => normalizeServerList(draftServers.value))
const draftServersCount = computed(() => normalizedDraftServers.value.length)
const canApplyServers = computed(() => ntpDirty.value && !ntpBusy.value)
const manualTimeIso = computed(() => {
  if (!manualTimeDraft.value) return null
  const parsed = new Date(manualTimeDraft.value)
  if (Number.isNaN(parsed.getTime())) return null
  return parsed.toISOString()
})
const canApplyManualTime = computed(() => manualTimeDirty.value && !ntpBusy.value && Boolean(manualTimeIso.value))

async function applyNtpServers() {
  const servers = normalizedDraftServers.value
  await coreNtpStore.applyServers(servers).catch(() => undefined)
  ntpDirty.value = false
}

async function applyManualTime() {
  if (!manualTimeIso.value) return
  await coreNtpStore.setTime(manualTimeIso.value).catch(() => undefined)
  manualTimeDirty.value = false
  manualTimeInteracting.value = false
}

async function resumeUpstreamSync() {
  await coreNtpStore.reloadSources().catch(() => undefined)
}

onMounted(() => {
  coreNtpStore.startMonitoring()
})

onUnmounted(() => {
  coreNtpStore.stopMonitoring()
})
</script>

