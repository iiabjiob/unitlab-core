import { getLogger } from "@/utils/logger"

const perfLogger = getLogger("PERF")
const PERF_ENABLED = import.meta.env.DEV

type PerfState = {
  counters: Record<string, number>
  lastDurationsMs: Record<string, number>
}

const state: PerfState = {
  counters: Object.create(null) as Record<string, number>,
  lastDurationsMs: Object.create(null) as Record<string, number>,
}

declare global {
  interface Window {
    __UNITLAB_DEV_PERF__?: PerfState
  }
}

function publishState() {
  if (!PERF_ENABLED || typeof window === "undefined") return
  window.__UNITLAB_DEV_PERF__ = state
}

export function devPerfIncrement(counter: string, delta = 1) {
  if (!PERF_ENABLED) return
  state.counters[counter] = (state.counters[counter] ?? 0) + delta
  publishState()
}

export function devPerfMeasureStart(name: string) {
  if (!PERF_ENABLED) {
    return () => undefined
  }
  const startedAt = typeof performance !== "undefined" ? performance.now() : Date.now()
  return (extra?: Record<string, unknown>) => {
    const endedAt = typeof performance !== "undefined" ? performance.now() : Date.now()
    const durationMs = Math.max(0, endedAt - startedAt)
    state.lastDurationsMs[name] = durationMs
    publishState()
    perfLogger.debug(`⏱️ ${name}: ${durationMs.toFixed(1)}ms`, extra ?? {})
  }
}

export function devPerfSnapshot() {
  if (!PERF_ENABLED) return null
  return {
    counters: { ...state.counters },
    lastDurationsMs: { ...state.lastDurationsMs },
  }
}
