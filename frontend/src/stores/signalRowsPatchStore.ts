import { defineStore } from "pinia"
import { markRaw, ref } from "vue"

import { useWorkspaceStore } from "@/stores/workspaceStore"
import type { SignalRowsPatchedEvent } from "@/types/ws/events"

type SignalRowsPatchApplyResult = {
  applied: boolean
  ignored: "none" | "invalid" | "workspace_mismatch" | "stale"
  gap: boolean
  duplicateOrOld: boolean
  requiresFullReload: boolean
}

function ignoredResult(ignored: SignalRowsPatchApplyResult["ignored"]): SignalRowsPatchApplyResult {
  return {
    applied: false,
    ignored,
    gap: false,
    duplicateOrOld: ignored === "stale",
    requiresFullReload: false,
  }
}

function normalizePositiveInteger(value: unknown): number | null {
  const parsed = Number(value)
  return Number.isInteger(parsed) && parsed > 0 ? parsed : null
}

export const useSignalRowsPatchStore = defineStore("signalRowsPatchStore", () => {
  const workspaceStore = useWorkspaceStore()
  const activeWorkspacePatchEvent = ref<SignalRowsPatchedEvent | null>(null)
  const activeWorkspacePatchRevision = ref(0)
  const lastSequenceByWorkspaceId = new Map<number, number>()

  function applyEvent(event: SignalRowsPatchedEvent): SignalRowsPatchApplyResult {
    const workspaceId = normalizePositiveInteger(event.workspace_id)
    const sequence = normalizePositiveInteger(event.sequence)
    if (workspaceId === null || sequence === null) {
      return ignoredResult("invalid")
    }

    const activeWorkspaceId = workspaceStore.activeWorkspaceId
    if (activeWorkspaceId !== workspaceId) {
      return ignoredResult("workspace_mismatch")
    }

    const lastSequence = lastSequenceByWorkspaceId.get(workspaceId)
    if (lastSequence !== undefined && sequence <= lastSequence) {
      return ignoredResult("stale")
    }

    const gap = lastSequence !== undefined && sequence !== lastSequence + 1
    const requiresFullReload = event.requires_full_reload === true || gap
    const patches = Array.isArray(event.patches) ? event.patches : []
    const normalizedEvent = markRaw({
      ...event,
      workspace_id: workspaceId,
      sequence,
      patches,
      requires_full_reload: requiresFullReload,
    })

    lastSequenceByWorkspaceId.set(workspaceId, sequence)
    activeWorkspacePatchEvent.value = normalizedEvent
    activeWorkspacePatchRevision.value += 1

    return {
      applied: true,
      ignored: "none",
      gap,
      duplicateOrOld: false,
      requiresFullReload,
    }
  }

  return {
    activeWorkspacePatchEvent,
    activeWorkspacePatchRevision,
    applyEvent,
  }
})
