<template>
  <section class="core-diagnostics-panel">
    <div class="core-diagnostics-panel__header">
      <div class="core-diagnostics-panel__heading">
        <p class="core-diagnostics-panel__eyebrow">Core Diagnostics</p>
        <p class="core-diagnostics-panel__meta">
          Mode: <span class="core-diagnostics-panel__meta-strong">{{ modeText }}</span>
          <template v-if="snapshot?.last_event"> · {{ snapshot.last_event }}</template>
        </p>
        <p class="core-diagnostics-panel__subtle">
          {{ snapshot?.hostname || "—" }}
          <template v-if="snapshot?.model"> · {{ snapshot.model }}</template>
          <template v-if="snapshot?.kernel"> · kernel {{ snapshot.kernel }}</template>
        </p>
        <p v-if="snapshot?.os_pretty_name" class="core-diagnostics-panel__subtle">
          {{ snapshot.os_pretty_name }}
        </p>
        <p v-if="errorText" class="core-diagnostics-panel__error">{{ errorText }}</p>
      </div>
    </div>

    <div class="core-diagnostics-panel__grid">
      <div class="core-diagnostics-panel__card core-diagnostics-panel__card--wide">
        <div class="core-diagnostics-panel__card-header">
          <p class="core-diagnostics-panel__card-title">Backend Health</p>
          <span class="core-diagnostics-panel__card-meta">{{ healthCheckedAtText }}</span>
        </div>
        <div class="core-diagnostics-panel__facts">
          <div class="core-diagnostics-panel__fact-row">
            <span class="core-diagnostics-panel__fact-label">Overall status</span>
            <span
              class="core-diagnostics-panel__pill"
              :class="systemStatusPillClass"
            >
              {{ systemStatusText }}
            </span>
          </div>
          <div class="core-diagnostics-panel__fact-row">
            <span class="core-diagnostics-panel__fact-label">Workers</span>
            <span class="core-diagnostics-panel__fact-value">{{ healthyWorkersCount }} / {{ healthWorkers.length }} online</span>
          </div>
        </div>
        <div v-if="healthIssues.length > 0" class="core-diagnostics-panel__issues">
          <p class="core-diagnostics-panel__issues-title">Issues</p>
          <ul class="core-diagnostics-panel__issues-list">
            <li v-for="issue in healthIssues" :key="issue" class="core-diagnostics-panel__issue">
              {{ issue }}
            </li>
          </ul>
        </div>
      </div>

      <div class="core-diagnostics-panel__card">
        <p class="core-diagnostics-panel__card-title core-diagnostics-panel__card-title--spaced">System</p>
        <div class="core-diagnostics-panel__facts">
          <Row label="Time (UTC)" :value="snapshot?.time_utc || '—'" />
          <Row label="Uptime" :value="uptimeText" />
          <Row label="CPU temp" :value="cpuTempText" />
          <Row
            label="Load (1m/5m/15m)"
            tooltip="Average system load over 1, 5, and 15 minutes. This is not CPU percent: 1.00 is roughly equal to one fully utilized CPU core."
            :value="loadText"
          />
          <Row label="Memory" :value="memoryText" />
          <Row
            label="Disk (root fs)"
            tooltip="Usage of the Linux root filesystem (mount point /), where system files and applications are stored."
            :value="diskText"
          />
        </div>
      </div>

      <div class="core-diagnostics-panel__card">
        <p class="core-diagnostics-panel__card-title core-diagnostics-panel__card-title--spaced">Host Services</p>
        <div class="core-diagnostics-panel__service-list">
          <div
            v-for="svc in services"
            :key="svc.name"
            class="core-diagnostics-panel__service-row"
          >
            <span class="core-diagnostics-panel__service-name">{{ svc.name }}</span>
            <span
              class="core-diagnostics-panel__pill"
              :class="statusPillClass(svc.active)"
            >
              {{ statusText(svc.active) }}
            </span>
          </div>
          <p v-if="services.length === 0" class="core-diagnostics-panel__empty">
            No service diagnostics reported yet.
          </p>
        </div>
      </div>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, defineComponent, h, onMounted, onUnmounted } from "vue"
import { useCoreDiagnosticsStore } from "@/stores/coreDiagnosticsStore"
import { useSystemHealthStore } from "@/stores/systemHealthStore"
import InlineInfoTooltip from "@/components/ui/InlineInfoTooltip.vue"

const store = useCoreDiagnosticsStore()
const systemHealthStore = useSystemHealthStore()

const snapshot = computed(() => store.snapshot)
const services = computed(() => store.services)
const busy = computed(() => store.loading || store.commandPending)
const errorText = computed(() => store.lastError || snapshot.value?.last_error || null)
const modeText = computed(() => String(store.mode).toUpperCase())
const systemStatusText = computed(() => String(systemHealthStore.status).toUpperCase())
const healthIssues = computed(() => systemHealthStore.issues)
const healthWorkers = computed(() => systemHealthStore.workers)
const healthyWorkersCount = computed(() => healthWorkers.value.filter(worker => worker.status === "online").length)
const healthCheckedAtText = computed(() => {
  const raw = systemHealthStore.checkedAt
  return raw ? String(raw) : "—"
})

const systemStatusPillClass = computed(() => {
  const status = String(systemHealthStore.status).toLowerCase()
  if (status === "online") {
    return "core-diagnostics-panel__pill--success"
  }
  if (status === "offline") {
    return "core-diagnostics-panel__pill--danger"
  }
  return "core-diagnostics-panel__pill--warning"
})

const cpuTempText = computed(() => {
  const t = store.cpu?.temperature_c
  if (typeof t !== "number" || !Number.isFinite(t)) return "—"
  return `${t.toFixed(1)} °C`
})

const loadText = computed(() => {
  const cpu = store.cpu
  if (!cpu) return "—"
  const values = [cpu.load_1m, cpu.load_5m, cpu.load_15m].map(v =>
    typeof v === "number" && Number.isFinite(v) ? v.toFixed(2) : "—",
  )
  return values.join(" / ")
})

const uptimeText = computed(() => {
  const seconds = snapshot.value?.uptime_seconds
  if (typeof seconds !== "number" || !Number.isFinite(seconds)) return "—"
  const s = Math.max(0, Math.floor(seconds))
  const d = Math.floor(s / 86400)
  const h = Math.floor((s % 86400) / 3600)
  const m = Math.floor((s % 3600) / 60)
  if (d > 0) return `${d}d ${h}h ${m}m`
  if (h > 0) return `${h}h ${m}m`
  return `${m}m`
})

const memoryText = computed(() => {
  const mem = store.memory
  if (!mem) return "—"
  return `${formatBytes(mem.used_bytes)} / ${formatBytes(mem.total_bytes)} (${formatPercent(mem.used_percent)})`
})

const diskText = computed(() => {
  const disk = store.diskRoot
  if (!disk) return "—"
  return `${formatBytes(disk.used_bytes)} / ${formatBytes(disk.total_bytes)} (${formatPercent(disk.used_percent)})`
})

function formatBytes(value: number | null | undefined): string {
  if (typeof value !== "number" || !Number.isFinite(value)) return "—"
  const units = ["B", "KB", "MB", "GB", "TB"]
  let n = value
  let idx = 0
  while (n >= 1024 && idx < units.length - 1) {
    n /= 1024
    idx += 1
  }
  return `${n.toFixed(idx === 0 ? 0 : 1)} ${units[idx]}`
}

function formatPercent(value: number | null | undefined): string {
  if (typeof value !== "number" || !Number.isFinite(value)) return "—"
  return `${value.toFixed(1)}%`
}

function statusText(active: boolean | null): string {
  if (active === true) return "ACTIVE"
  if (active === false) return "INACTIVE"
  return "UNKNOWN"
}

function statusPillClass(active: boolean | null): string {
  if (active === true) {
    return "core-diagnostics-panel__pill--success"
  }
  if (active === false) {
    return "core-diagnostics-panel__pill--danger"
  }
  return "core-diagnostics-panel__pill--neutral"
}

onMounted(() => {
  store.startMonitoring()
  systemHealthStore.startMonitoring()
})

onUnmounted(() => {
  store.stopMonitoring()
  systemHealthStore.stopMonitoring()
})

const Row = defineComponent({
  name: "CoreDiagRow",
  props: {
    label: { type: String, required: true },
    tooltip: { type: String, default: null },
    value: { type: String, required: true },
  },
  setup(props) {
    return () => h("div", { class: "core-diagnostics-row" }, [
      h("span", { class: "core-diagnostics-row__label" }, [
        h("span", props.label),
        props.tooltip
          ? h(InlineInfoTooltip, {
            text: props.tooltip,
            placement: "top",
            align: "start",
          })
          : null,
      ]),
      h("span", { class: "core-diagnostics-row__value" }, props.value),
    ])
  },
})
</script>

<style scoped>
.core-diagnostics-panel {
  padding: 1rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 70%, transparent);
  border-radius: 1rem;
  background: color-mix(in srgb, var(--color-white) 90%, transparent);
}

.core-diagnostics-panel__header {
  display: flex;
  flex-wrap: wrap;
  align-items: flex-start;
  justify-content: space-between;
  gap: 0.75rem;
}

.core-diagnostics-panel__heading {
  min-width: 0;
}

.core-diagnostics-panel__eyebrow {
  color: var(--color-neutral-500);
  font-size: 11px;
  letter-spacing: 0;
  text-transform: uppercase;
}

.core-diagnostics-panel__meta {
  margin-top: 0.25rem;
  color: var(--color-neutral-700);
  font-size: var(--text-sm);
}

.core-diagnostics-panel__meta-strong {
  font-weight: 600;
  text-transform: uppercase;
}

.core-diagnostics-panel__subtle {
  margin-top: 0.25rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.core-diagnostics-panel__error {
  margin-top: 0.25rem;
  color: var(--color-rose-500);
  font-size: var(--text-xs);
}

.core-diagnostics-panel__grid {
  display: grid;
  gap: 0.75rem;
  margin-top: 0.75rem;
}

.core-diagnostics-panel__card {
  padding: 0.75rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 80%, transparent);
  border-radius: 0.75rem;
  background: color-mix(in srgb, var(--color-neutral-50) 80%, transparent);
}

.core-diagnostics-panel__card-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.5rem;
  margin-bottom: 0.5rem;
}

.core-diagnostics-panel__card-title {
  color: var(--color-neutral-700);
  font-size: var(--text-xs);
  font-weight: 600;
}

.core-diagnostics-panel__card-title--spaced {
  margin-bottom: 0.5rem;
}

.core-diagnostics-panel__card-meta {
  color: var(--color-neutral-500);
  font-size: 11px;
}

.core-diagnostics-panel__facts {
  display: grid;
  gap: 0.5rem;
  font-size: var(--text-xs);
}

.core-diagnostics-panel__fact-row,
.core-diagnostics-panel__service-row,
:global(.core-diagnostics-row) {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.5rem;
}

.core-diagnostics-panel__fact-label,
:global(.core-diagnostics-row__label) {
  color: var(--color-neutral-500);
}

:global(.core-diagnostics-row__label) {
  display: inline-flex;
  align-items: center;
  gap: 0.25rem;
}

.core-diagnostics-panel__fact-value,
:global(.core-diagnostics-row__value) {
  color: var(--color-neutral-800);
  font-weight: 500;
}

:global(.core-diagnostics-row__value) {
  text-align: right;
}

.core-diagnostics-panel__pill {
  padding: 0.125rem 0.375rem;
  border-radius: var(--radius-sm);
  font-size: 10px;
  font-weight: 600;
}

.core-diagnostics-panel__pill--success {
  background: var(--color-green-100);
  color: var(--color-green-800);
}

.core-diagnostics-panel__pill--danger {
  background: var(--color-red-100);
  color: var(--color-red-800);
}

.core-diagnostics-panel__pill--warning {
  background: var(--color-yellow-100);
  color: var(--color-yellow-800);
}

.core-diagnostics-panel__pill--neutral {
  background: var(--color-neutral-200);
  color: var(--color-neutral-700);
}

.core-diagnostics-panel__issues {
  max-height: 8rem;
  margin-top: 0.75rem;
  padding: 0.5rem;
  overflow-y: auto;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
}

.core-diagnostics-panel__issues-title {
  margin-bottom: 0.25rem;
  color: var(--color-neutral-500);
  font-size: 11px;
  font-weight: 600;
  letter-spacing: 0;
  text-transform: uppercase;
}

.core-diagnostics-panel__issues-list {
  display: grid;
  gap: 0.25rem;
  padding: 0;
  margin: 0;
  list-style-position: inside;
}

.core-diagnostics-panel__issue {
  color: var(--color-rose-600);
  font-size: var(--text-xs);
}

.core-diagnostics-panel__service-list {
  max-height: 14rem;
  padding: 0.25rem;
  overflow-y: auto;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-md);
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
}

.core-diagnostics-panel__service-row {
  padding: 0.25rem 0.5rem;
  font-size: var(--text-xs);
}

.core-diagnostics-panel__service-name {
  overflow: hidden;
  color: var(--color-neutral-800);
  text-overflow: ellipsis;
  white-space: nowrap;
}

.core-diagnostics-panel__empty {
  padding: 0.5rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

@media (min-width: 1280px) {
  .core-diagnostics-panel__grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }

  .core-diagnostics-panel__card--wide {
    grid-column: span 2;
  }
}

:global(.dark .core-diagnostics-panel){
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-950) 40%, transparent);
}

:global(.dark .core-diagnostics-panel__meta),
:global(.dark .core-diagnostics-panel__card-title){
  color: var(--color-neutral-200);
}

:global(.dark .core-diagnostics-panel__eyebrow),
:global(.dark .core-diagnostics-panel__subtle),
:global(.dark .core-diagnostics-panel__card-meta),
:global(.dark .core-diagnostics-panel__fact-label),
:global(.dark .core-diagnostics-panel__empty),
:global(.dark .core-diagnostics-row__label) {
  color: var(--color-neutral-400);
}

:global(.dark .core-diagnostics-panel__card){
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-900) 60%, transparent);
}

:global(.dark .core-diagnostics-panel__fact-value),
:global(.dark .core-diagnostics-panel__service-name),
:global(.dark .core-diagnostics-row__value) {
  color: var(--color-neutral-100);
}

:global(.dark .core-diagnostics-panel__pill--success){
  background: color-mix(in srgb, var(--color-emerald-900) 40%, transparent);
  color: var(--color-emerald-300);
}

:global(.dark .core-diagnostics-panel__pill--danger){
  background: color-mix(in srgb, var(--color-red-900) 40%, transparent);
  color: var(--color-red-300);
}

:global(.dark .core-diagnostics-panel__pill--warning){
  background: color-mix(in srgb, var(--color-yellow-900) 40%, transparent);
  color: var(--color-yellow-300);
}

:global(.dark .core-diagnostics-panel__pill--neutral){
  background: var(--color-neutral-800);
  color: var(--color-neutral-200);
}

:global(.dark .core-diagnostics-panel__issues),
:global(.dark .core-diagnostics-panel__service-list){
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-950) 50%, transparent);
}

:global(.dark .core-diagnostics-panel__issue){
  color: var(--color-rose-300);
}
</style>
