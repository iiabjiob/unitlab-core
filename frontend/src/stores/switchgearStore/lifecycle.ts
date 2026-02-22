import { watch, type Ref } from "vue"

import type { Switchgear } from "@/types/switchgear"

type DiEdgeMemory = Record<number, { diOpen: boolean | null; diClose: boolean | null }>

type Params = {
  switchgears: Ref<Switchgear[]>
  loadedOnce: Ref<boolean>
  diEdgeMemory: Ref<DiEdgeMemory>
  updateQueueById: Map<number, Promise<void>>
  getActiveWorkspaceId: () => number | null
  getChannelStateRevision: () => number
  clearAllDiDelayTimers: () => void
  fetchAll: () => Promise<void>
  pruneDiEdgeMemory: (memory: Ref<DiEdgeMemory>) => void
  syncDiDrivenSwitching: () => void
}

export function createSwitchgearLifecycle(params: Params) {
  function resetForWorkspaceChange() {
    params.switchgears.value = []
    params.loadedOnce.value = false
    params.diEdgeMemory.value = {}
    params.updateQueueById.clear()
    params.clearAllDiDelayTimers()
  }

  watch(
    () => params.getActiveWorkspaceId(),
    (workspaceId) => {
      resetForWorkspaceChange()
      if (workspaceId) {
        void params.fetchAll()
      }
    },
  )

  watch(
    () => params.switchgears.value.map(sw => sw.id).join(","),
    () => {
      params.pruneDiEdgeMemory(params.diEdgeMemory)
    },
    { immediate: true },
  )

  watch(
    () => params.getChannelStateRevision(),
    () => {
      params.syncDiDrivenSwitching()
    },
  )

  return {
    resetForWorkspaceChange,
  }
}
