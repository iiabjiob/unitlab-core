import { watch, type ComputedRef } from "vue"
import type { RouteLocationNormalizedLoaded } from "vue-router"

type RefreshOptions = { force?: boolean }
type AllocationLoadOptions = { force?: boolean; ttlMs?: number; initialPageSize?: number }

type Params = {
  workspaceId: ComputedRef<number | null>
  workspaceMissing: ComputedRef<boolean>
  loading: ComputedRef<boolean>
  route: RouteLocationNormalizedLoaded
  ensureRuntimeCatalogLoaded: () => Promise<void>
  ensureSignalSheetLoaded: (options?: { force?: boolean; ttlMs?: number }) => Promise<unknown>
  ensureAllocationsLoaded: (options?: AllocationLoadOptions) => Promise<unknown>
  clearRealtimeTestedAtWorkspace: (workspaceId: number) => void
  resetSignalSheetState: () => void
  restoreSelectedRowKeysFromStorage: () => void
  syncRealtimeUnitScope: () => void
  openImportModal: () => void
  isImportQueryRequested: (raw: unknown) => boolean
  clearImportQueryFlag: () => void
  onError: (error: unknown) => void
}

export function useSignalsPageLifecycle(params: Params) {
  let refreshCycleId = 0
  let refreshAllInFlight: Promise<void> | null = null
  let refreshAllInFlightWorkspaceId: number | null = null

  async function refreshAll(options?: RefreshOptions) {
    const activeWorkspaceId = params.workspaceId.value ?? null
    if (refreshAllInFlight && refreshAllInFlightWorkspaceId === activeWorkspaceId) {
      await refreshAllInFlight
      return
    }

    const force = options?.force ?? false
    const task = (async () => {
      const cycleId = ++refreshCycleId
      try {
        await params.ensureRuntimeCatalogLoaded()
        await Promise.all([
          params.ensureSignalSheetLoaded({ force, ttlMs: 1_500 }),
          params.ensureAllocationsLoaded({ force, ttlMs: 1_500 }),
        ])
        if (cycleId !== refreshCycleId) {
          return
        }
        params.syncRealtimeUnitScope()
      } catch (error) {
        if (cycleId !== refreshCycleId) {
          return
        }
        params.onError(error)
      }
    })()

    refreshAllInFlight = task
    refreshAllInFlightWorkspaceId = activeWorkspaceId
    try {
      await task
    } finally {
      if (refreshAllInFlight === task) {
        refreshAllInFlight = null
        refreshAllInFlightWorkspaceId = null
      }
    }
  }

  watch(
    params.workspaceId,
    async (workspaceId) => {
      refreshCycleId += 1
      if (!workspaceId) return
      params.clearRealtimeTestedAtWorkspace(workspaceId)
      params.resetSignalSheetState()
      params.restoreSelectedRowKeysFromStorage()
      await refreshAll({ force: true })
    },
    { immediate: true },
  )

  watch(
    () => [params.route.query.import, params.workspaceMissing.value, params.loading.value] as const,
    ([importFlag, missingWorkspace, isLoading]) => {
      if (!params.isImportQueryRequested(importFlag)) return
      if (missingWorkspace || isLoading) return
      params.openImportModal()
      params.clearImportQueryFlag()
    },
    { immediate: true },
  )

  return {
    refreshAll,
  }
}
