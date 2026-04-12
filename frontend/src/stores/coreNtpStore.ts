import { computed, ref } from "vue"
import { defineStore } from "pinia"
import {
  enqueueCoreNtpApplyServers,
  enqueueCoreNtpReload,
  enqueueCoreNtpRestoreDefaults,
  enqueueCoreNtpSetTime,
  enqueueCoreNtpStatus,
  fetchCoreNtpState,
} from "@/api/core_ntp.api"
import type { CoreNtpCommandAccepted, CoreNtpSnapshot } from "@/types/coreNtp"
import { getLogger } from "@/utils/logger"

const logger = getLogger("CORE-NTP")
const DEFAULT_TTL_MS = 10_000
const POLL_MS = 10_000

export const useCoreNtpStore = defineStore("coreNtpStore", () => {
  const snapshot = ref<CoreNtpSnapshot | null>(null)
  const loading = ref(false)
  const commandPending = ref(false)
  const lastError = ref<string | null>(null)
  const lastLoadedAtMs = ref<number | null>(null)
  const lastCommandAccepted = ref<CoreNtpCommandAccepted | null>(null)

  let refreshInFlight: Promise<void> | null = null
  let monitorSubscribers = 0
  let pollTimer: ReturnType<typeof setInterval> | null = null

  function applySnapshot(next: CoreNtpSnapshot) {
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
        const data = await fetchCoreNtpState()
        applySnapshot(data.state)
      } catch (error) {
        const message = error instanceof Error ? error.message : "Failed to load core NTP state"
        lastError.value = message
        logger.warn("Failed to load core NTP state", error)
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
    if (lastLoadedAtMs.value !== null && (Date.now() - lastLoadedAtMs.value) < ttlMs) return
    await refreshState()
  }

  async function runCommand(
    runner: () => Promise<CoreNtpCommandAccepted>,
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
      void enqueueCoreNtpStatus().catch(() => undefined)
    }
  }

  async function requestStatus() {
    return runCommand(() => enqueueCoreNtpStatus(), "Queued core-ntp status")
  }

  async function applyServers(servers: string[]) {
    return runCommand(() => enqueueCoreNtpApplyServers({ servers }), "Queued core-ntp apply_servers")
  }

  async function restoreDefaults() {
    return runCommand(() => enqueueCoreNtpRestoreDefaults(), "Queued core-ntp restore_defaults")
  }

  async function reloadSources() {
    return runCommand(() => enqueueCoreNtpReload(), "Queued core-ntp reload")
  }

  async function setTime(timestamp: string) {
    return runCommand(() => enqueueCoreNtpSetTime({ timestamp }), "Queued core-ntp set_time")
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
  const configuredServers = computed(() => snapshot.value?.configured_servers ?? [])
  const effectiveServers = computed(() => snapshot.value?.effective_servers ?? [])
  const tracking = computed(() => snapshot.value?.tracking ?? null)
  const sources = computed(() => snapshot.value?.sources ?? [])
  const selectedUpstreamSource = computed(() =>
    sources.value.find((source) => source.mode_mark === "^" && source.state_mark === "*") ?? null,
  )
  const isSynced = computed(() => snapshot.value?.tracking?.synced === true && selectedUpstreamSource.value !== null)

  return {
    snapshot,
    loading,
    commandPending,
    lastError,
    lastCommandAccepted,
    mode,
    configuredServers,
    effectiveServers,
    tracking,
    sources,
    selectedUpstreamSource,
    isSynced,
    applySnapshot,
    refreshState,
    ensureFresh,
    startMonitoring,
    stopMonitoring,
    requestStatus,
    applyServers,
    restoreDefaults,
    reloadSources,
    setTime,
  }
})

