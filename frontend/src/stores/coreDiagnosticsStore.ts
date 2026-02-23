import { computed, ref } from "vue"
import { defineStore } from "pinia"
import { enqueueCoreDiagnosticsStatus, fetchCoreDiagnosticsState } from "@/api/core_diagnostics.api"
import type { CoreDiagnosticsCommandAccepted, CoreDiagnosticsSnapshot } from "@/types/coreDiagnostics"
import { getLogger } from "@/utils/logger"

const logger = getLogger("CORE-DIAG")
const DEFAULT_TTL_MS = 8_000
const POLL_MS = 8_000

export const useCoreDiagnosticsStore = defineStore("coreDiagnosticsStore", () => {
  const snapshot = ref<CoreDiagnosticsSnapshot | null>(null)
  const loading = ref(false)
  const commandPending = ref(false)
  const lastError = ref<string | null>(null)
  const lastLoadedAtMs = ref<number | null>(null)
  const lastCommandAccepted = ref<CoreDiagnosticsCommandAccepted | null>(null)

  let refreshInFlight: Promise<void> | null = null
  let monitorSubscribers = 0
  let pollTimer: ReturnType<typeof setInterval> | null = null

  function applySnapshot(next: CoreDiagnosticsSnapshot) {
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
        const data = await fetchCoreDiagnosticsState()
        applySnapshot(data.state)
      } catch (error) {
        const message = error instanceof Error ? error.message : "Failed to load core diagnostics state"
        lastError.value = message
        logger.warn("Failed to load core diagnostics state", error)
      } finally {
        loading.value = false
      }
    })()
    refreshInFlight = run
    try {
      await run
    } finally {
      if (refreshInFlight === run) refreshInFlight = null
    }
  }

  async function ensureFresh(options?: { force?: boolean; ttlMs?: number }) {
    if (options?.force) {
      await refreshState({ force: true })
      return
    }
    const ttlMs = Math.max(0, options?.ttlMs ?? DEFAULT_TTL_MS)
    if (lastLoadedAtMs.value !== null && Date.now() - lastLoadedAtMs.value < ttlMs) return
    await refreshState()
  }

  async function requestStatus() {
    try {
      commandPending.value = true
      const accepted = await enqueueCoreDiagnosticsStatus()
      lastCommandAccepted.value = accepted
      logger.info("Queued core-diagnostics status", accepted)
      return accepted
    } finally {
      commandPending.value = false
    }
  }

  function handleVisibilityChange() {
    if (document.visibilityState === "visible") {
      void ensureFresh({ force: true })
    }
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

  const mode = computed(() => snapshot.value?.mode ?? "unknown")
  const cpu = computed(() => snapshot.value?.cpu ?? null)
  const memory = computed(() => snapshot.value?.memory ?? null)
  const diskRoot = computed(() => snapshot.value?.disk_root ?? null)
  const services = computed(() => snapshot.value?.services ?? [])

  return {
    snapshot,
    loading,
    commandPending,
    lastError,
    lastCommandAccepted,
    mode,
    cpu,
    memory,
    diskRoot,
    services,
    applySnapshot,
    refreshState,
    ensureFresh,
    requestStatus,
    startMonitoring,
    stopMonitoring,
  }
})

