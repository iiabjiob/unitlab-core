import { defineStore } from "pinia"
import { computed, ref } from "vue"
import { fetchSystemHealth } from "@/api/system.api"
import type { SystemHealthResponse, SystemStatus } from "@/types/health"
import { getLogger } from "@/utils/logger"

const logger = getLogger("SYSTEM")

export const useSystemHealthStore = defineStore("systemHealth", () => {
  const snapshot = ref<SystemHealthResponse | null>(null)
  const isLoading = ref(false)
  const lastError = ref<string | null>(null)

  function applySnapshot(data: SystemHealthResponse) {
    snapshot.value = data
    lastError.value = null
  }

  async function refresh() {
    try {
      isLoading.value = true
      const data = await fetchSystemHealth()
      applySnapshot(data)
    } catch (error) {
      lastError.value = error instanceof Error ? error.message : "Unknown error"
      logger.error("Failed to retrieve system status", error)
    } finally {
      isLoading.value = false
    }
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
  }
})
