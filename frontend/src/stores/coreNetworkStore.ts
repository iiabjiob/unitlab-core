import { computed, ref } from "vue"
import { defineStore } from "pinia"
import {
  enqueueCoreNetworkConnect,
  enqueueCoreNetworkDisconnect,
  enqueueCoreNetworkRestartAp,
  enqueueCoreNetworkScan,
  enqueueCoreNetworkStatus,
  fetchCoreNetworkState,
} from "@/api/core_network.api"
import type { CoreNetworkCommandAccepted, CoreNetworkSnapshot } from "@/types/coreNetwork"
import { getLogger } from "@/utils/logger"

const logger = getLogger("CORE-NET")
const DEFAULT_TTL_MS = 10_000
const POLL_MS = 10_000

export const useCoreNetworkStore = defineStore("coreNetworkStore", () => {
  const snapshot = ref<CoreNetworkSnapshot | null>(null)
  const loading = ref(false)
  const commandPending = ref(false)
  const lastError = ref<string | null>(null)
  const lastLoadedAtMs = ref<number | null>(null)
  const lastCommandAccepted = ref<CoreNetworkCommandAccepted | null>(null)

  let refreshInFlight: Promise<void> | null = null
  let monitorSubscribers = 0
  let pollTimer: ReturnType<typeof setInterval> | null = null

  function applySnapshot(next: CoreNetworkSnapshot) {
    snapshot.value = next
    lastLoadedAtMs.value = Date.now()
    lastError.value = null
  }

  async function refreshState(options?: { force?: boolean }) {
    void options
    if (refreshInFlight) return refreshInFlight

    const run = (async () => {
      try {
        loading.value = true
        const data = await fetchCoreNetworkState()
        applySnapshot(data.state)
      } catch (error) {
        const message = error instanceof Error ? error.message : "Failed to load core network state"
        lastError.value = message
        logger.warn("Failed to load core network state", error)
      } finally {
        loading.value = false
      }
    })()

    refreshInFlight = run
    try {
      await run
    } finally {
      if (refreshInFlight === run) {
        refreshInFlight = null
      }
    }
  }

  async function ensureFresh(options?: { force?: boolean; ttlMs?: number }) {
    if (options?.force) {
      await refreshState({ force: true })
      return
    }
    const ttlMs = Math.max(0, options?.ttlMs ?? DEFAULT_TTL_MS)
    if (lastLoadedAtMs.value !== null && (Date.now() - lastLoadedAtMs.value) < ttlMs) {
      return
    }
    await refreshState()
  }

  async function _runCommand(
    runner: () => Promise<CoreNetworkCommandAccepted>,
    successLog: string,
  ) {
    try {
      commandPending.value = true
      const accepted = await runner()
      lastCommandAccepted.value = accepted
      logger.info(successLog, accepted)
      return accepted
    } finally {
      commandPending.value = false
      void enqueueCoreNetworkStatus().catch(() => undefined)
    }
  }

  async function requestStatus() {
    return _runCommand(() => enqueueCoreNetworkStatus(), "Queued core-network status")
  }

  async function scan(timeoutSec?: number | null) {
    return _runCommand(() => enqueueCoreNetworkScan(timeoutSec), "Queued core-network scan")
  }

  async function connectSta(payload: { ssid: string; password?: string | null; hidden?: boolean; timeout_sec?: number | null }) {
    return _runCommand(() => enqueueCoreNetworkConnect(payload), "Queued core-network connect_sta")
  }

  async function disconnectSta() {
    return _runCommand(() => enqueueCoreNetworkDisconnect(), "Queued core-network disconnect_sta")
  }

  async function restartAp() {
    return _runCommand(() => enqueueCoreNetworkRestartAp(), "Queued core-network restart_ap")
  }

  function startMonitoring() {
    if (typeof window === "undefined") return
    monitorSubscribers += 1
    if (monitorSubscribers > 1) return
    void ensureFresh({ force: true })
    pollTimer = setInterval(() => {
      void ensureFresh({ force: true })
    }, POLL_MS)
    document.addEventListener("visibilitychange", handleVisibilityChange)
  }

  function stopMonitoring() {
    monitorSubscribers = Math.max(0, monitorSubscribers - 1)
    if (monitorSubscribers > 0) return
    if (pollTimer) {
      clearInterval(pollTimer)
      pollTimer = null
    }
    if (typeof document !== "undefined") {
      document.removeEventListener("visibilitychange", handleVisibilityChange)
    }
  }

  function handleVisibilityChange() {
    if (document.visibilityState === "visible") {
      void ensureFresh({ force: true })
    }
  }

  const mode = computed(() => snapshot.value?.mode ?? "unknown")
  const ap = computed(() => snapshot.value?.ap ?? null)
  const sta = computed(() => snapshot.value?.sta ?? null)
  const networks = computed(() => snapshot.value?.available_networks ?? [])
  const isApActive = computed(() => snapshot.value?.mode === "ap" && snapshot.value.ap?.active)
  const isStaConnected = computed(() => snapshot.value?.sta?.state === "connected")
  const webUiUrl = computed(() => {
    const ip = snapshot.value?.ap?.ip
    return ip ? `http://${ip}` : "http://10.42.0.1"
  })

  return {
    snapshot,
    loading,
    commandPending,
    lastError,
    lastCommandAccepted,
    mode,
    ap,
    sta,
    networks,
    isApActive,
    isStaConnected,
    webUiUrl,
    applySnapshot,
    refreshState,
    ensureFresh,
    startMonitoring,
    stopMonitoring,
    requestStatus,
    scan,
    connectSta,
    disconnectSta,
    restartAp,
  }
})

