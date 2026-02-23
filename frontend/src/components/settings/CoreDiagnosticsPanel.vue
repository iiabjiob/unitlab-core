<template>
  <section class="rounded-2xl border border-neutral-200/70 bg-white/90 p-4 dark:border-neutral-800 dark:bg-neutral-950/40">
    <div class="flex flex-wrap items-start justify-between gap-3">
      <div class="min-w-0">
        <p class="text-[11px] uppercase tracking-[0.26em] text-neutral-500 dark:text-neutral-400">Core Diagnostics</p>
        <p class="mt-1 text-sm text-neutral-700 dark:text-neutral-200">
          Mode: <span class="font-semibold uppercase">{{ modeText }}</span>
          <template v-if="snapshot?.last_event"> · {{ snapshot.last_event }}</template>
        </p>
        <p class="mt-1 text-xs text-neutral-500 dark:text-neutral-400">
          {{ snapshot?.hostname || "—" }}
          <template v-if="snapshot?.model"> · {{ snapshot.model }}</template>
          <template v-if="snapshot?.kernel"> · kernel {{ snapshot.kernel }}</template>
        </p>
        <p v-if="snapshot?.os_pretty_name" class="mt-1 text-xs text-neutral-500 dark:text-neutral-400">
          {{ snapshot.os_pretty_name }}
        </p>
        <p v-if="errorText" class="mt-1 text-xs text-rose-500">{{ errorText }}</p>
      </div>
      <div class="flex items-center gap-2">
        <button
          type="button"
          class="rounded-lg border border-neutral-300 px-3 py-1.5 text-xs font-semibold text-neutral-700 hover:border-neutral-500 dark:border-neutral-700 dark:text-neutral-200 dark:hover:border-neutral-500"
          :disabled="busy"
          @click="refreshStatus"
        >
          Refresh
        </button>
      </div>
    </div>

    <div class="mt-3 grid gap-3 xl:grid-cols-2">
      <div class="rounded-xl border border-neutral-200/80 bg-neutral-50/80 p-3 dark:border-neutral-800 dark:bg-neutral-900/60">
        <p class="mb-2 text-xs font-semibold text-neutral-700 dark:text-neutral-200">System</p>
        <div class="grid gap-2 text-xs">
          <Row label="Time (UTC)" :value="snapshot?.time_utc || '—'" />
          <Row label="Uptime" :value="uptimeText" />
          <Row label="CPU temp" :value="cpuTempText" />
          <Row label="Load" :value="loadText" />
          <Row label="Memory" :value="memoryText" />
          <Row label="Disk /" :value="diskText" />
        </div>
      </div>

      <div class="rounded-xl border border-neutral-200/80 bg-neutral-50/80 p-3 dark:border-neutral-800 dark:bg-neutral-900/60">
        <p class="mb-2 text-xs font-semibold text-neutral-700 dark:text-neutral-200">Host Services</p>
        <div class="max-h-56 overflow-y-auto rounded-lg border border-neutral-200 bg-white/80 p-1 dark:border-neutral-800 dark:bg-neutral-950/50">
          <div
            v-for="svc in services"
            :key="svc.name"
            class="flex items-center justify-between gap-2 px-2 py-1 text-xs"
          >
            <span class="truncate text-neutral-800 dark:text-neutral-100">{{ svc.name }}</span>
            <span
              class="rounded px-1.5 py-0.5 text-[10px] font-semibold"
              :class="statusPillClass(svc.active)"
            >
              {{ statusText(svc.active) }}
            </span>
          </div>
          <p v-if="services.length === 0" class="px-2 py-2 text-xs text-neutral-500 dark:text-neutral-400">
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

const store = useCoreDiagnosticsStore()

const snapshot = computed(() => store.snapshot)
const services = computed(() => store.services)
const busy = computed(() => store.loading || store.commandPending)
const errorText = computed(() => store.lastError || snapshot.value?.last_error || null)
const modeText = computed(() => String(store.mode).toUpperCase())

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
    return "bg-emerald-100 text-emerald-800 dark:bg-emerald-900/40 dark:text-emerald-200"
  }
  if (active === false) {
    return "bg-rose-100 text-rose-800 dark:bg-rose-900/40 dark:text-rose-200"
  }
  return "bg-neutral-200 text-neutral-700 dark:bg-neutral-800 dark:text-neutral-200"
}

async function refreshStatus() {
  await store.ensureFresh({ force: true })
  await store.requestStatus().catch(() => undefined)
}

onMounted(() => {
  store.startMonitoring()
})

onUnmounted(() => {
  store.stopMonitoring()
})

const Row = defineComponent({
  name: "CoreDiagRow",
  props: {
    label: { type: String, required: true },
    value: { type: String, required: true },
  },
  setup(props) {
    return () => h("div", { class: "flex items-center justify-between gap-2" }, [
      h("span", { class: "text-neutral-500 dark:text-neutral-400" }, props.label),
      h("span", { class: "text-right font-medium text-neutral-800 dark:text-neutral-100" }, props.value),
    ])
  },
})
</script>

