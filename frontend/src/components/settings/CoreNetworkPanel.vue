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
      <div class="flex flex-wrap items-center gap-2">
        <button
          type="button"
          class="rounded-lg border border-neutral-300 px-3 py-1.5 text-xs font-semibold text-neutral-700 hover:border-neutral-500 dark:border-neutral-700 dark:text-neutral-200 dark:hover:border-neutral-500"
          :disabled="coreNetBusy"
          @click="refreshCoreNetworkStatus"
        >
          Refresh
        </button>
        <button
          type="button"
          class="rounded-lg border border-neutral-300 px-3 py-1.5 text-xs font-semibold text-neutral-700 hover:border-neutral-500 dark:border-neutral-700 dark:text-neutral-200 dark:hover:border-neutral-500"
          :disabled="coreNetBusy"
          @click="scanCoreNetworks"
        >
          Scan Wi‑Fi
        </button>
        <button
          type="button"
          class="rounded-lg border border-neutral-300 px-3 py-1.5 text-xs font-semibold text-neutral-700 hover:border-neutral-500 dark:border-neutral-700 dark:text-neutral-200 dark:hover:border-neutral-500"
          :disabled="coreNetBusy"
          @click="restartCoreAp"
        >
          Restart AP
        </button>
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

      <div class="rounded-xl border border-neutral-200/80 bg-neutral-50/80 p-3 dark:border-neutral-800 dark:bg-neutral-900/60">
        <div class="mb-2 flex items-center justify-between gap-2">
          <p class="text-xs font-semibold text-neutral-700 dark:text-neutral-200">Connect to Plant Wi‑Fi (STA)</p>
          <button
            type="button"
            class="rounded-md border border-neutral-300 px-2 py-1 text-[11px] font-semibold text-neutral-700 hover:border-neutral-500 dark:border-neutral-700 dark:text-neutral-200 dark:hover:border-neutral-500"
            :disabled="coreNetBusy"
            @click="disconnectCoreSta"
          >
            Disconnect STA
          </button>
        </div>

        <div class="grid gap-2 sm:grid-cols-[minmax(0,1fr)_auto]">
          <label class="text-xs text-neutral-500 dark:text-neutral-400 sm:col-span-2">
            SSID
            <input
              v-model.trim="coreNetStaSsid"
              type="text"
              class="mt-1 w-full rounded-lg border border-neutral-300 bg-white px-2 py-1.5 text-sm text-neutral-900 outline-none focus:border-neutral-500 dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
              placeholder="Select from scan or type manually"
            />
          </label>
          <label class="text-xs text-neutral-500 dark:text-neutral-400 sm:col-span-2">
            Password
            <input
              v-model="coreNetStaPassword"
              type="password"
              class="mt-1 w-full rounded-lg border border-neutral-300 bg-white px-2 py-1.5 text-sm text-neutral-900 outline-none focus:border-neutral-500 dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
              placeholder="Optional for open networks"
            />
          </label>
          <label class="inline-flex items-center gap-2 text-xs text-neutral-600 dark:text-neutral-300">
            <input v-model="coreNetStaHidden" type="checkbox" class="h-4 w-4 rounded border-neutral-300 dark:border-neutral-700" />
            Hidden SSID
          </label>
          <button
            type="button"
            class="rounded-lg bg-emerald-600 px-3 py-1.5 text-xs font-semibold text-white hover:bg-emerald-500 disabled:cursor-not-allowed disabled:opacity-60"
            :disabled="coreNetBusy || !coreNetStaSsid"
            @click="connectCoreSta"
          >
            Connect STA
          </button>
        </div>

        <div class="mt-3 max-h-44 overflow-y-auto rounded-lg border border-neutral-200 bg-white/80 p-1 dark:border-neutral-800 dark:bg-neutral-950/50">
          <button
            v-for="network in coreNetNetworks"
            :key="`${network.ssid}-${network.security || 'open'}`"
            type="button"
            class="flex w-full items-center justify-between rounded-md px-2 py-1.5 text-left text-xs hover:bg-neutral-100 dark:hover:bg-neutral-900"
            @click="selectCoreNetwork(network.ssid)"
          >
            <span class="min-w-0 truncate text-neutral-800 dark:text-neutral-100">
              {{ network.ssid }}
              <span v-if="network.in_use" class="ml-1 text-[10px] text-emerald-600 dark:text-emerald-300">(in use)</span>
            </span>
            <span class="shrink-0 text-neutral-500 dark:text-neutral-400">
              {{ network.signal ?? "?" }}%
              <template v-if="network.security"> · {{ network.security }}</template>
            </span>
          </button>
          <p v-if="coreNetNetworks.length === 0" class="px-2 py-2 text-xs text-neutral-500 dark:text-neutral-400">
            No scanned networks yet. Click “Scan Wi‑Fi”.
          </p>
        </div>
      </div>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref } from "vue"
import { useCoreNetworkStore } from "@/stores/coreNetworkStore"

const coreNetworkStore = useCoreNetworkStore()

const coreNetSnapshot = computed(() => coreNetworkStore.snapshot)
const coreNetMode = computed(() => String(coreNetworkStore.mode).toUpperCase())
const coreNetStaState = computed(() => String(coreNetworkStore.sta?.state ?? "disconnected").toUpperCase())
const coreNetApIp = computed(() => coreNetworkStore.ap?.ip ?? "10.42.0.1")
const coreNetWebUrl = computed(() => coreNetworkStore.webUiUrl)
const coreNetNetworks = computed(() => coreNetworkStore.networks)
const coreNetBusy = computed(() => coreNetworkStore.commandPending || coreNetworkStore.loading)
const coreNetErrorText = computed(() => coreNetworkStore.lastError || coreNetworkStore.snapshot?.last_error || null)

const coreNetStaSsid = ref("")
const coreNetStaPassword = ref("")
const coreNetStaHidden = ref(false)

function selectCoreNetwork(ssid: string) {
  coreNetStaSsid.value = ssid
  coreNetStaHidden.value = false
}

async function refreshCoreNetworkStatus() {
  await coreNetworkStore.ensureFresh({ force: true })
  await coreNetworkStore.requestStatus().catch(() => undefined)
}

async function scanCoreNetworks() {
  await coreNetworkStore.scan().catch(() => undefined)
}

async function connectCoreSta() {
  const ssid = coreNetStaSsid.value.trim()
  if (!ssid) return
  await coreNetworkStore.connectSta({
    ssid,
    password: coreNetStaPassword.value || undefined,
    hidden: coreNetStaHidden.value,
  }).catch(() => undefined)
}

async function disconnectCoreSta() {
  await coreNetworkStore.disconnectSta().catch(() => undefined)
}

async function restartCoreAp() {
  await coreNetworkStore.restartAp().catch(() => undefined)
}

onMounted(() => {
  coreNetworkStore.startMonitoring()
})

onUnmounted(() => {
  coreNetworkStore.stopMonitoring()
})
</script>

