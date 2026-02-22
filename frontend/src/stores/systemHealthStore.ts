import { defineStore } from "pinia"
import { computed, ref } from "vue"
import { fetchSystemHealth } from "@/api/system.api"
import type { SystemHealthResponse, SystemStatus } from "@/types/health"
import { getLogger } from "@/utils/logger"
import { devPerfIncrement, devPerfMeasureStart } from "@/utils/devPerf"

const logger = getLogger("SYSTEM")
const HEALTH_REFRESH_INTERVAL_MS = 15_000
const DEFAULT_HEALTH_TTL_MS = 15_000

export const useSystemHealthStore = defineStore("systemHealth", () => {
  const snapshot = ref<SystemHealthResponse | null>(null)
  const isLoading = ref(false)
  const lastError = ref<string | null>(null)
  const lastUpdatedAtMs = ref<number | null>(null)

  let refreshInFlight: Promise<void> | null = null
  let monitorSubscribers = 0
  let healthRefreshTimer: ReturnType<typeof setInterval> | null = null

  function applySnapshot(data: SystemHealthResponse) {
    snapshot.value = data
    lastError.value = null
    lastUpdatedAtMs.value = Date.now()
  }

  async function refresh(_options?: { force?: boolean }) {
    devPerfIncrement("systemHealth.refresh.calls")
    void _options
    if (refreshInFlight) {
      devPerfIncrement("systemHealth.refresh.dedupe_waits")
      return refreshInFlight
    }

    const run = (async () => {
      const endMeasure = devPerfMeasureStart("systemHealth.refresh")
      try {
        isLoading.value = true
        const data = await fetchSystemHealth()
        applySnapshot(data)
        devPerfIncrement("systemHealth.refresh.completed")
        endMeasure({ ok: true, status: data.status })
      } catch (error) {
        lastError.value = error instanceof Error ? error.message : "Unknown error"
        logger.error("Failed to retrieve system status", error)
        endMeasure({ ok: false })
      } finally {
        isLoading.value = false
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

  async function ensureFresh(options?: { ttlMs?: number; force?: boolean }) {
    devPerfIncrement("systemHealth.ensureFresh.calls")
    if (options?.force) {
      devPerfIncrement("systemHealth.ensureFresh.forced")
      await refresh({ force: true })
      return
    }

    const ttlMs = Math.max(0, options?.ttlMs ?? DEFAULT_HEALTH_TTL_MS)
    const lastUpdated = lastUpdatedAtMs.value
    if (lastUpdated !== null && (Date.now() - lastUpdated) < ttlMs) {
      devPerfIncrement("systemHealth.ensureFresh.cache_hits")
      return
    }

    devPerfIncrement("systemHealth.ensureFresh.refreshes")
    await refresh()
  }

  function stopHealthMonitoring() {
    if (typeof document !== "undefined") {
      document.removeEventListener("visibilitychange", handleVisibilityChange)
    }
    if (healthRefreshTimer) {
      clearInterval(healthRefreshTimer)
      healthRefreshTimer = null
    }
  }

  function handleVisibilityChange() {
    if (document.visibilityState === "visible") {
      void ensureFresh({ force: true })
    }
  }

  function startMonitoring() {
    if (typeof window === "undefined") {
      return
    }

    devPerfIncrement("systemHealth.monitor.start.calls")
    monitorSubscribers += 1
    if (monitorSubscribers > 1) {
      devPerfIncrement("systemHealth.monitor.start.reused")
      return
    }
    devPerfIncrement("systemHealth.monitor.start.activations")

    void ensureFresh()
    document.addEventListener("visibilitychange", handleVisibilityChange)
    healthRefreshTimer = setInterval(() => {
      void ensureFresh({ force: true })
    }, HEALTH_REFRESH_INTERVAL_MS)
  }

  function stopMonitoring() {
    devPerfIncrement("systemHealth.monitor.stop.calls")
    monitorSubscribers = Math.max(0, monitorSubscribers - 1)
    if (monitorSubscribers > 0) {
      return
    }
    devPerfIncrement("systemHealth.monitor.stop.deactivations")
    stopHealthMonitoring()
  }

  const status = computed<SystemStatus>(() => {
    if (snapshot.value) return snapshot.value.status
    return lastError.value ? "degraded" : "online"
  })

  const issues = computed(() => snapshot.value?.issues ?? [])
  const workers = computed(() => snapshot.value?.workers ?? [])
  const checkedAt = computed(() => snapshot.value?.checked_at ?? null)

  const tooltip = computed(() => {
    const workerLines = workers.value.map((worker) => {
      const label = worker.display_name || worker.name
      const detail = worker.detail ? ` — ${worker.detail}` : ""
      return `• ${label}: ${worker.status}${detail}`
    })

    const headerByStatus: Record<SystemStatus, string> = {
      online: "System is online.",
      degraded: "System is degraded.",
      offline: "System is offline.",
    }

    const lines: string[] = [headerByStatus[status.value]]

    if (issues.value.length > 0) {
      lines.push("", "Current issues:", ...issues.value.map((issue) => `• ${issue}`))
    }

    if (workerLines.length > 0) {
      lines.push("", "Process health:", ...workerLines)
    } else if (lastError.value) {
      lines.push("", `Failed to fetch status: ${lastError.value}`)
    } else {
      lines.push("", "Process health data is not available yet.")
    }

    return lines.join("\n")
  })

  return {
    snapshot,
    isLoading,
    lastError,
    workers,
    checkedAt,
    status,
    issues,
    tooltip,
    applySnapshot,
    refresh,
    ensureFresh,
    startMonitoring,
    stopMonitoring,
  }
})
