import { defineStore } from "pinia"
import { ref } from "vue"

import { useWorkspaceStore } from "@/stores/workspaceStore"

const testedAtByWorkspaceId = new Map<number, Map<number, string>>()
const pendingPatchByWorkspaceId = new Map<number, Map<number, string>>()

const TESTED_AT_REALTIME_MAX_CACHE_ENTRIES = 20_000

export const useTestedAtRealtimeStore = defineStore("testedAtRealtimeStore", () => {
  const workspaceStore = useWorkspaceStore()

  const activeWorkspaceRevision = ref(0)
  const activeWorkspacePatchedSignalIds = ref<number[]>([])

  let flushFrame: number | null = null
  let flushMicrotaskScheduled = false

  function normalizeWorkspaceId(workspaceId: number | null | undefined): number | null {
    const parsed = Number(workspaceId)
    if (!Number.isFinite(parsed) || parsed <= 0) {
      return null
    }
    return parsed
  }

  function getWorkspaceMap(workspaceId: number): Map<number, string> {
    let map = testedAtByWorkspaceId.get(workspaceId)
    if (!map) {
      map = new Map<number, string>()
      testedAtByWorkspaceId.set(workspaceId, map)
    }
    return map
  }

  function scheduleMicrotaskFlush() {
    if (flushMicrotaskScheduled) {
      return
    }
    flushMicrotaskScheduled = true
    const run = () => {
      flushMicrotaskScheduled = false
      flushPendingPatches()
    }
    if (typeof queueMicrotask === "function") {
      queueMicrotask(run)
      return
    }
    Promise.resolve().then(run)
  }

  function scheduleRafFlush() {
    if (flushFrame !== null) {
      return
    }
    flushFrame = requestAnimationFrame(() => {
      flushFrame = null
      flushPendingPatches()
    })
  }

  function scheduleFlush(mode: "raf" | "microtask" = "raf") {
    if (mode === "microtask") {
      // Upgrade existing RAF flush to same-tick flush when realtime UI feedback matters.
      if (flushFrame !== null) {
        cancelAnimationFrame(flushFrame)
        flushFrame = null
      }
      scheduleMicrotaskFlush()
      return
    }
    if (flushMicrotaskScheduled) {
      return
    }
    scheduleRafFlush()
  }

  function flushPendingPatches() {
    if (pendingPatchByWorkspaceId.size === 0) {
      return
    }

    const activeWorkspaceId = normalizeWorkspaceId(workspaceStore.activeWorkspaceId)
    const touchedActiveSignalIds: number[] = []

    pendingPatchByWorkspaceId.forEach((patchMap, workspaceId) => {
      const targetMap = getWorkspaceMap(workspaceId)
      patchMap.forEach((testedAtIso, signalId) => {
        targetMap.set(signalId, testedAtIso)
        if (activeWorkspaceId !== null && workspaceId === activeWorkspaceId) {
          touchedActiveSignalIds.push(signalId)
        }
      })
      patchMap.clear()
    })

    pendingPatchByWorkspaceId.clear()

    if (touchedActiveSignalIds.length === 0) {
      return
    }

    activeWorkspacePatchedSignalIds.value = Array.from(new Set(touchedActiveSignalIds))
    activeWorkspaceRevision.value += 1

    const activeMap = testedAtByWorkspaceId.get(activeWorkspaceId!)
    if (activeMap && activeMap.size > TESTED_AT_REALTIME_MAX_CACHE_ENTRIES) {
      activeMap.clear()
    }
  }

  function applyPatch(
    workspaceIdRaw: number | null | undefined,
    testedAtBySignal: Record<number, string> | Record<string, string>,
    options?: { flush?: "raf" | "microtask" },
  ) {
    const workspaceId = normalizeWorkspaceId(workspaceIdRaw)
    if (workspaceId === null) {
      return
    }

    const entries = Object.entries(testedAtBySignal ?? {})
    if (entries.length === 0) {
      return
    }

    let pendingMap = pendingPatchByWorkspaceId.get(workspaceId)
    if (!pendingMap) {
      pendingMap = new Map<number, string>()
      pendingPatchByWorkspaceId.set(workspaceId, pendingMap)
    }

    entries.forEach(([rawSignalId, testedAtValue]) => {
      const signalId = Number(rawSignalId)
      if (!Number.isFinite(signalId) || signalId <= 0) {
        return
      }
      const testedAtIso = String(testedAtValue ?? "").trim()
      if (!testedAtIso) {
        return
      }
      pendingMap.set(signalId, testedAtIso)
    })

    if (pendingMap.size === 0) {
      return
    }

    scheduleFlush(options?.flush ?? "raf")
  }

  function getTestedAt(signalIdRaw: number | null | undefined, workspaceIdRaw?: number | null): string | null {
    const signalId = Number(signalIdRaw)
    if (!Number.isFinite(signalId) || signalId <= 0) {
      return null
    }

    const workspaceId = normalizeWorkspaceId(workspaceIdRaw ?? workspaceStore.activeWorkspaceId)
    if (workspaceId === null) {
      return null
    }

    const map = testedAtByWorkspaceId.get(workspaceId)
    if (!map) {
      return null
    }

    return map.get(signalId) ?? null
  }

  function clearWorkspace(workspaceIdRaw: number | null | undefined) {
    const workspaceId = normalizeWorkspaceId(workspaceIdRaw)
    if (workspaceId === null) {
      return
    }

    testedAtByWorkspaceId.delete(workspaceId)
    pendingPatchByWorkspaceId.delete(workspaceId)

    const activeWorkspaceId = normalizeWorkspaceId(workspaceStore.activeWorkspaceId)
    if (activeWorkspaceId !== null && activeWorkspaceId === workspaceId) {
      activeWorkspacePatchedSignalIds.value = []
      activeWorkspaceRevision.value += 1
    }
  }

  function clearAll() {
    testedAtByWorkspaceId.clear()
    pendingPatchByWorkspaceId.clear()
    activeWorkspacePatchedSignalIds.value = []
    activeWorkspaceRevision.value += 1

    if (flushFrame !== null) {
      cancelAnimationFrame(flushFrame)
      flushFrame = null
    }
    flushMicrotaskScheduled = false
  }

  return {
    activeWorkspaceRevision,
    activeWorkspacePatchedSignalIds,
    applyPatch,
    getTestedAt,
    clearWorkspace,
    clearAll,
  }
})
