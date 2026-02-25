<template>
  <section class="rounded-2xl border border-neutral-200/70 bg-white/90 p-4 dark:border-neutral-800 dark:bg-neutral-950/40">
    <div class="flex flex-wrap items-start justify-between gap-3">
      <div class="min-w-0">
        <p class="text-[11px] uppercase tracking-[0.26em] text-neutral-500 dark:text-neutral-400">Core Network</p>
        <p class="mt-1 text-sm text-neutral-700 dark:text-neutral-200">
          Mode: <span class="font-semibold uppercase">{{ coreNetMode }}</span>
          <template v-if="coreNetSnapshot?.last_event">
            · {{ coreNetSnapshot.last_event }}
          </template>
        </p>
        <p class="mt-1 text-xs text-neutral-500 dark:text-neutral-400">
          AP: <span class="font-medium">{{ coreNetSnapshot?.ap?.ssid || "—" }}</span>
          · {{ coreNetApIp }}
          · UI {{ coreNetWebUrl }}
        </p>
        <p class="mt-1 text-xs text-neutral-500 dark:text-neutral-400">
          STA:
          <span class="font-medium uppercase">{{ coreNetStaState }}</span>
          <template v-if="coreNetSnapshot?.sta?.ssid"> · {{ coreNetSnapshot?.sta?.ssid }}</template>
          <template v-if="coreNetSnapshot?.sta?.ip"> · {{ coreNetSnapshot?.sta?.ip }}</template>
        </p>
        <p v-if="coreNetErrorText" class="mt-1 text-xs text-rose-500">{{ coreNetErrorText }}</p>
      </div>
    </div>

    <div class="mt-3 grid gap-3 lg:grid-cols-[minmax(0,1fr)_minmax(0,1.1fr)]">
      <div class="rounded-xl border border-neutral-200/80 bg-neutral-50/80 p-3 dark:border-neutral-800 dark:bg-neutral-900/60">
        <div class="mb-2 flex items-center justify-between gap-2">
          <p class="text-xs font-semibold text-neutral-700 dark:text-neutral-200">Access Point</p>
          <span class="text-[11px] text-neutral-500 dark:text-neutral-400">{{ coreNetSnapshot?.wifi_iface || "wlan0" }}</span>
        </div>
        <div class="grid gap-2 text-xs">
          <div class="flex items-center justify-between gap-2">
            <span class="text-neutral-500 dark:text-neutral-400">SSID</span>
            <code class="rounded bg-neutral-100 px-1.5 py-0.5 text-neutral-800 dark:bg-neutral-800 dark:text-neutral-100">{{ coreNetSnapshot?.ap?.ssid || "unitlab-core-ABCD" }}</code>
          </div>
          <div class="flex items-center justify-between gap-2">
            <span class="text-neutral-500 dark:text-neutral-400">Password</span>
            <code class="rounded bg-neutral-100 px-1.5 py-0.5 text-neutral-800 dark:bg-neutral-800 dark:text-neutral-100">{{ coreNetSnapshot?.ap?.password || "pwd!ABCD" }}</code>
          </div>
          <div class="flex items-center justify-between gap-2">
            <span class="text-neutral-500 dark:text-neutral-400">Web UI</span>
            <code class="rounded bg-neutral-100 px-1.5 py-0.5 text-neutral-800 dark:bg-neutral-800 dark:text-neutral-100">{{ coreNetWebUrl }}</code>
          </div>
          <div v-if="coreNetSnapshot?.request_in_flight" class="mt-1 rounded-lg border border-amber-200 bg-amber-50 px-2 py-1 text-[11px] text-amber-800 dark:border-amber-700/50 dark:bg-amber-950/40 dark:text-amber-200">
            In progress: {{ coreNetSnapshot.request_in_flight.action }} ({{ coreNetSnapshot.request_in_flight.request_id }})
          </div>
        </div>
      </div>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, onMounted, onUnmounted } from "vue"
import { useCoreNetworkStore } from "@/stores/coreNetworkStore"

const coreNetworkStore = useCoreNetworkStore()

const coreNetSnapshot = computed(() => coreNetworkStore.snapshot)
const coreNetMode = computed(() => {
  const rawMode = String(coreNetworkStore.mode || "unknown").toLowerCase()
  if (rawMode !== "unknown") return rawMode.toUpperCase()
  const snap = coreNetworkStore.snapshot
  if (snap?.ap?.active) return "AP"
  if (snap?.sta?.state === "connected") return "STA"
  if ((snap?.ap?.ip || snap?.ap?.ssid) && snap?.sta?.state !== "connected") return "AP"
  return "OFFLINE"
})
const coreNetStaState = computed(() => String(coreNetworkStore.sta?.state ?? "disconnected").toUpperCase())
const coreNetApIp = computed(() => coreNetworkStore.ap?.ip ?? "10.42.0.1")
const coreNetWebUrl = computed(() => coreNetworkStore.webUiUrl)
const coreNetBusy = computed(() => coreNetworkStore.commandPending || coreNetworkStore.loading)
const coreNetErrorText = computed(() => coreNetworkStore.lastError || coreNetworkStore.snapshot?.last_error || null)

onMounted(() => {
  coreNetworkStore.startMonitoring()
})

onUnmounted(() => {
  coreNetworkStore.stopMonitoring()
})
</script>

