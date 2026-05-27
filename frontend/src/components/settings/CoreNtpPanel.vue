<template>
  <section class="core-ntp-panel">
    <div class="core-ntp-panel__header">
      <div class="core-ntp-panel__heading">
        <p class="core-ntp-panel__eyebrow">Time / NTP Sync</p>
        <p class="core-ntp-panel__meta">
          Mode:
          <span class="core-ntp-panel__meta-strong">{{ ntpModeText }}</span>
          <template v-if="ntpSnapshot?.last_event"> · {{ ntpSnapshot.last_event }}</template>
        </p>
        <p class="core-ntp-panel__subtle">
          Chrony:
          <span class="core-ntp-panel__subtle-strong">{{ ntpChronyServiceLabel }}</span>
          · Service {{ ntpChronyServiceActiveLabel }}
          · Sync {{ ntpSyncLabel }}
          <template v-if="ntpTracking?.stratum !== undefined && ntpTracking?.stratum !== null">
            · Stratum {{ ntpTracking.stratum }}
          </template>
        </p>
        <p v-if="ntpTrackingSummary" class="core-ntp-panel__subtle">
          {{ ntpTrackingSummary }}
        </p>
        <p v-if="ntpErrorText" class="core-ntp-panel__error">{{ ntpErrorText }}</p>
      </div>
    </div>

    <div class="core-ntp-panel__grid">
      <div class="core-ntp-panel__card">
        <div class="core-ntp-panel__card-header">
          <p class="core-ntp-panel__card-title">Configured NTP Servers (Chrony)</p>
        </div>

        <div class="core-ntp-panel__server-list">
          <div
            v-for="(server, idx) in draftServers"
            :key="`ntp-server-${idx}`"
            class="core-ntp-panel__server-row"
          >
            <input
              v-model.trim="draftServers[idx]"
              type="text"
              autocomplete="off"
              class="core-ntp-panel__input"
              placeholder="pool.ntp.org or 192.168.1.10"
              @input="markDirty"
            />
            <button
              type="button"
              class="btn btn-base btn-danger"
              :disabled="ntpBusy"
              @click="removeServer(idx)"
            >
              Remove
            </button>
          </div>
        </div>

        <div class="core-ntp-panel__actions">
          <button
            type="button"
            class="btn btn-base btn-secondary"
            :disabled="ntpBusy"
            @click="addServer"
          >
            Add server
          </button>
          <button
            type="button"
            class="btn btn-base btn-success"
            :disabled="ntpBusy || !canApplyServers"
            @click="applyNtpServers"
          >
            Apply servers
          </button>
          <span class="core-ntp-panel__action-meta">
            {{ draftServersCount }} server{{ draftServersCount === 1 ? "" : "s" }}
            <template v-if="ntpDirty"> · edited</template>
          </span>
        </div>

        <div v-if="ntpSnapshot?.request_in_flight" class="core-ntp-panel__in-flight">
          In progress: {{ ntpSnapshot.request_in_flight.action }} ({{ ntpSnapshot.request_in_flight.request_id }})
        </div>
      </div>

      <div class="core-ntp-panel__card">
        <div class="core-ntp-panel__card-header">
          <p class="core-ntp-panel__card-title">Chrony Status</p>
          <span class="core-ntp-panel__card-meta">{{ ntpSnapshot?.updated_at || "—" }}</span>
        </div>

        <div class="core-ntp-panel__manual-card">
          <div class="core-ntp-panel__manual-header">
            <p class="core-ntp-panel__section-label">
              Manual system time
            </p>
            <span class="core-ntp-panel__card-meta">Browser local timezone</span>
          </div>

          <div class="core-ntp-panel__facts">
            <div class="core-ntp-panel__fact-row">
              <span class="core-ntp-panel__fact-label">RPi local</span>
              <span class="core-ntp-panel__fact-value core-ntp-panel__fact-value--truncate" :title="ntpSystemTimeLocalRaw || '—'">{{ ntpSystemTimeLocalText }}</span>
            </div>
            <div class="core-ntp-panel__fact-row">
              <span class="core-ntp-panel__fact-label">RPi UTC</span>
              <span class="core-ntp-panel__fact-value core-ntp-panel__fact-value--truncate" :title="ntpSystemTimeUtcRaw || '—'">{{ ntpSystemTimeUtcText }}</span>
            </div>
          </div>

          <div class="core-ntp-panel__manual-controls">
            <input
              v-model="manualTimeDraft"
              type="datetime-local"
              step="1"
              class="core-ntp-panel__input"
              :disabled="ntpBusy"
              @pointerdown="beginManualTimeInteraction"
              @focus="beginManualTimeInteraction"
              @blur="endManualTimeInteraction"
              @input="markManualTimeDirty"
            />
            <button
              type="button"
              class="btn btn-base btn-secondary"
              :disabled="ntpBusy"
              @click="setManualTimeFromBrowser"
            >
              Browser now
            </button>
            <button
              type="button"
              class="btn btn-base btn-secondary"
              :disabled="ntpBusy"
              @click="resetManualTimeDraft"
            >
              Use RPi time
            </button>
          </div>

          <div class="core-ntp-panel__actions core-ntp-panel__actions--manual">
            <button
              type="button"
              class="btn btn-base btn-primary"
              :disabled="!canApplyManualTime"
              @click="applyManualTime"
            >
              Set system time
            </button>
            <button
              type="button"
              class="btn btn-base btn-secondary"
              :disabled="ntpBusy"
              @click="resumeUpstreamSync"
            >
              Resume NTP sync
            </button>
            <span class="core-ntp-panel__action-meta">
              Manual set pauses upstream chrony sources so the local time does not jump back immediately.
            </span>
          </div>
        </div>

        <div class="core-ntp-panel__facts core-ntp-panel__facts--status">
          <div class="core-ntp-panel__fact-row">
            <span class="core-ntp-panel__fact-label">Configured</span>
            <span class="core-ntp-panel__fact-value">{{ ntpConfiguredServers.length }}</span>
          </div>
          <div class="core-ntp-panel__fact-row">
            <span class="core-ntp-panel__fact-label">Effective sources</span>
            <span class="core-ntp-panel__fact-value">{{ ntpEffectiveServers.length }}</span>
          </div>
          <div class="core-ntp-panel__fact-row">
            <span class="core-ntp-panel__fact-label">Reference</span>
            <span class="core-ntp-panel__fact-value core-ntp-panel__fact-value--truncate" :title="ntpTracking?.source || '—'">{{ ntpTracking?.source || "—" }}</span>
          </div>
          <div class="core-ntp-panel__fact-row">
            <span class="core-ntp-panel__fact-label">Offset</span>
            <span class="core-ntp-panel__fact-value">{{ ntpOffsetText }}</span>
          </div>
        </div>

        <div class="core-ntp-panel__sources">
          <p class="core-ntp-panel__sources-title">
            Effective sources
          </p>
          <div class="core-ntp-panel__sources-list">
            <div
              v-for="src in ntpSources"
              :key="`${src.name}-${src.mode_mark || ''}${src.state_mark || ''}`"
              class="core-ntp-panel__source-row"
            >
              <span class="core-ntp-panel__source-name">
                <span class="core-ntp-panel__source-mark">{{ src.mode_mark || "?" }}{{ src.state_mark || "?" }}</span>
                {{ src.name }}
              </span>
              <span class="core-ntp-panel__source-meta">
                s{{ src.stratum ?? "?" }} · reach {{ src.reach ?? "?" }}
              </span>
            </div>
            <p v-if="ntpSources.length === 0" class="core-ntp-panel__empty">
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

<style scoped>
.core-ntp-panel {
  padding: 1rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 70%, transparent);
  border-radius: 1rem;
  background: color-mix(in srgb, var(--color-white) 90%, transparent);
}

.core-ntp-panel__header {
  display: flex;
  flex-wrap: wrap;
  align-items: flex-start;
  justify-content: space-between;
  gap: 0.75rem;
}

.core-ntp-panel__heading {
  min-width: 0;
}

.core-ntp-panel__eyebrow {
  color: var(--color-neutral-500);
  font-size: 11px;
  letter-spacing: 0;
  text-transform: uppercase;
}

.core-ntp-panel__meta {
  margin-top: 0.25rem;
  color: var(--color-neutral-700);
  font-size: var(--text-sm);
}

.core-ntp-panel__meta-strong {
  font-weight: 600;
  text-transform: uppercase;
}

.core-ntp-panel__subtle {
  margin-top: 0.25rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.core-ntp-panel__subtle-strong {
  font-weight: 500;
}

.core-ntp-panel__error {
  margin-top: 0.25rem;
  color: var(--color-rose-500);
  font-size: var(--text-xs);
}

.core-ntp-panel__grid {
  display: grid;
  gap: 0.75rem;
  margin-top: 0.75rem;
}

.core-ntp-panel__card {
  padding: 0.75rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 80%, transparent);
  border-radius: 0.75rem;
  background: color-mix(in srgb, var(--color-neutral-50) 80%, transparent);
}

.core-ntp-panel__card-header,
.core-ntp-panel__manual-header,
.core-ntp-panel__fact-row,
.core-ntp-panel__source-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.5rem;
}

.core-ntp-panel__card-header {
  margin-bottom: 0.5rem;
}

.core-ntp-panel__card-title {
  color: var(--color-neutral-700);
  font-size: var(--text-xs);
  font-weight: 600;
}

.core-ntp-panel__card-meta,
.core-ntp-panel__fact-label,
.core-ntp-panel__action-meta,
.core-ntp-panel__source-meta {
  color: var(--color-neutral-500);
  font-size: 11px;
}

.core-ntp-panel__server-list {
  display: grid;
  gap: 0.5rem;
}

.core-ntp-panel__server-row {
  display: grid;
  align-items: center;
  gap: 0.5rem;
}

.core-ntp-panel__input {
  width: 100%;
  padding: 0.375rem 0.5rem;
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-md);
  background: var(--color-white);
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  outline: none;
}

.core-ntp-panel__input:focus {
  border-color: var(--color-neutral-500);
}

.core-ntp-panel__input:disabled {
  opacity: 0.65;
}

.core-ntp-panel__actions {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.5rem;
  margin-top: 0.75rem;
}

.core-ntp-panel__actions--manual {
  margin-top: 0.5rem;
}

.core-ntp-panel__in-flight {
  margin-top: 0.5rem;
  padding: 0.25rem 0.5rem;
  border: 1px solid color-mix(in srgb, var(--color-amber-300) 70%, transparent);
  border-radius: var(--radius-md);
  background: var(--color-amber-50);
  color: var(--color-yellow-800);
  font-size: 11px;
}

.core-ntp-panel__manual-card,
.core-ntp-panel__sources {
  padding: 0.75rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
}

.core-ntp-panel__section-label,
.core-ntp-panel__sources-title {
  color: var(--color-neutral-500);
  font-size: 11px;
  font-weight: 600;
  letter-spacing: 0;
  text-transform: uppercase;
}

.core-ntp-panel__facts {
  display: grid;
  gap: 0.5rem;
  margin-top: 0.5rem;
  font-size: var(--text-xs);
}

.core-ntp-panel__facts--status,
.core-ntp-panel__sources {
  margin-top: 0.75rem;
}

.core-ntp-panel__fact-label {
  font-size: var(--text-xs);
}

.core-ntp-panel__fact-value {
  color: var(--color-neutral-800);
  font-size: var(--text-xs);
  font-weight: 500;
}

.core-ntp-panel__fact-value--truncate {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.core-ntp-panel__manual-controls {
  display: grid;
  gap: 0.5rem;
  margin-top: 0.75rem;
}

.core-ntp-panel__sources {
  padding: 0.25rem;
}

.core-ntp-panel__sources-title {
  padding: 0.25rem 0.5rem;
}

.core-ntp-panel__sources-list {
  max-height: 10rem;
  overflow-y: auto;
}

.core-ntp-panel__source-row {
  padding: 0.25rem 0.5rem;
  font-size: var(--text-xs);
}

.core-ntp-panel__source-name {
  min-width: 0;
  overflow: hidden;
  color: var(--color-neutral-800);
  text-overflow: ellipsis;
  white-space: nowrap;
}

.core-ntp-panel__source-mark {
  margin-right: 0.25rem;
  color: var(--color-neutral-500);
  font-family: var(--font-mono);
  font-size: 10px;
}

.core-ntp-panel__source-meta {
  flex-shrink: 0;
  font-size: var(--text-xs);
}

.core-ntp-panel__empty {
  padding: 0.5rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

@media (min-width: 640px) {
  .core-ntp-panel__server-row {
    grid-template-columns: minmax(0, 1fr) auto;
  }

  .core-ntp-panel__manual-controls {
    grid-template-columns: minmax(0, 1fr) auto auto;
  }
}

@media (min-width: 1024px) {
  .core-ntp-panel__grid {
    grid-template-columns: minmax(0, 1.05fr) minmax(0, 0.95fr);
  }
}

:global(.dark .core-ntp-panel) {
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-950) 40%, transparent);
}

:global(.dark .core-ntp-panel__meta),
:global(.dark .core-ntp-panel__card-title),
:global(.dark .core-ntp-panel__fact-value),
:global(.dark .core-ntp-panel__source-name) {
  color: var(--color-neutral-100);
}

:global(.dark .core-ntp-panel__eyebrow),
:global(.dark .core-ntp-panel__subtle),
:global(.dark .core-ntp-panel__card-meta),
:global(.dark .core-ntp-panel__fact-label),
:global(.dark .core-ntp-panel__action-meta),
:global(.dark .core-ntp-panel__source-meta),
:global(.dark .core-ntp-panel__source-mark),
:global(.dark .core-ntp-panel__empty),
:global(.dark .core-ntp-panel__section-label),
:global(.dark .core-ntp-panel__sources-title) {
  color: var(--color-neutral-400);
}

:global(.dark .core-ntp-panel__card) {
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-900) 60%, transparent);
}

:global(.dark .core-ntp-panel__input) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-950);
  color: var(--color-neutral-100);
}

:global(.dark .core-ntp-panel__in-flight) {
  border-color: color-mix(in srgb, var(--color-amber-700) 50%, transparent);
  background: color-mix(in srgb, var(--color-amber-900) 40%, transparent);
  color: var(--color-amber-300);
}

:global(.dark .core-ntp-panel__manual-card),
:global(.dark .core-ntp-panel__sources) {
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-950) 50%, transparent);
}
</style>
