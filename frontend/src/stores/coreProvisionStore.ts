import { computed, ref } from "vue"
import { defineStore } from "pinia"
import {
  enqueueCoreProvisionInstallDiagAgent,
  enqueueCoreProvisionInstallNetAgent,
  enqueueCoreProvisionInstallNtpAgent,
  enqueueCoreProvisionSmokeCheck,
  enqueueCoreProvisionStatus,
  fetchCoreProvisionState,
} from "@/api/core_provision.api"
import type { CoreProvisionCommandAccepted, CoreProvisionSnapshot } from "@/types/coreProvision"
import { getLogger } from "@/utils/logger"

const logger = getLogger("CORE-PROVISION")
const DEFAULT_TTL_MS = 8000
const POLL_MS = 8000

export const useCoreProvisionStore = defineStore("coreProvisionStore", () => {
  const snapshot = ref<CoreProvisionSnapshot | null>(null)
  const loading = ref(false)
  const commandPending = ref(false)
  const lastError = ref<string | null>(null)
  const lastLoadedAtMs = ref<number | null>(null)
  const lastCommandAccepted = ref<CoreProvisionCommandAccepted | null>(null)

  let refreshInFlight: Promise<void> | null = null
  let monitorSubscribers = 0
  let pollTimer: ReturnType<typeof setInterval> | null = null

  function applySnapshot(next: CoreProvisionSnapshot) {
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
        const data = await fetchCoreProvisionState()
        applySnapshot(data.state)
      } catch (error) {
        const message = error instanceof Error ? error.message : "Failed to load core provisioning state"
        lastError.value = message
        logger.warn("Failed to load core provisioning state", error)
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

  async function runCommand(runner: () => Promise<CoreProvisionCommandAccepted>, label: string) {
    try {
      commandPending.value = true
      const accepted = await runner()
      lastCommandAccepted.value = accepted
      logger.info(label, accepted)
      return accepted
    } finally {
      commandPending.value = false
    }
  }

  async function requestStatus() {
    return runCommand(() => enqueueCoreProvisionStatus(), "Queued core-provision status")
  }
  async function runSmokeCheck() {
    return runCommand(() => enqueueCoreProvisionSmokeCheck(), "Queued core-provision smoke_check")
  }
  async function installNetAgent() {
    return runCommand(() => enqueueCoreProvisionInstallNetAgent(), "Queued core-provision install_net_agent")
  }
  async function installNtpAgent() {
    return runCommand(() => enqueueCoreProvisionInstallNtpAgent(), "Queued core-provision install_ntp_agent")
  }
  async function installDiagAgent() {
    return runCommand(() => enqueueCoreProvisionInstallDiagAgent(), "Queued core-provision install_diag_agent")
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
    pollTimer = setInterval(() => { void ensureFresh({ force: true }) }, POLL_MS)
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
  const checks = computed(() => snapshot.value?.checks ?? [])
  const smokeChecks = computed(() => snapshot.value?.smoke_checks ?? [])
  const lastAction = computed(() => snapshot.value?.last_action_result ?? null)

  return {
    snapshot,
    loading,
    commandPending,
    lastError,
    lastCommandAccepted,
    mode,
    checks,
    smokeChecks,
    lastAction,
    applySnapshot,
    refreshState,
    ensureFresh,
    requestStatus,
    runSmokeCheck,
    installNetAgent,
    installNtpAgent,
    installDiagAgent,
    startMonitoring,
    stopMonitoring,
  }
})

