import { beforeEach, describe, expect, it } from "vitest"
import { createPinia, setActivePinia } from "pinia"

import { useSignalRowsPatchStore } from "@/stores/signalRowsPatchStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { WSChannel, type SignalRowsPatchedEvent } from "@/types/ws/events"

function buildEvent(overrides: Partial<SignalRowsPatchedEvent> = {}): SignalRowsPatchedEvent {
  return {
    channel: WSChannel.SYSTEM_INFO,
    event: "signal_rows_patched",
    workspace_id: 7,
    sequence: 1,
    source: "allocation",
    patches: [
      {
        row_id: "signal-1",
        signal_id: 1,
        changes: { allocation_status: "assigned" },
        columns: ["allocation_status"],
      },
    ],
    emitted_at: "2026-01-01T12:30:00Z",
    ...overrides,
  }
}

describe("signalRowsPatchStore", () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    if (typeof window !== "undefined") {
      window.localStorage.clear()
    }
  })

  it("accepts the next patch event for the active workspace", () => {
    useWorkspaceStore().setActiveWorkspace(7)
    const store = useSignalRowsPatchStore()

    const result = store.applyEvent(buildEvent())

    expect(result).toEqual({
      applied: true,
      ignored: "none",
      gap: false,
      duplicateOrOld: false,
      requiresFullReload: false,
    })
    expect(store.activeWorkspacePatchRevision).toBe(1)
    expect(store.activeWorkspacePatchEvent?.sequence).toBe(1)
    expect(store.getLastSequence(7)).toBe(1)
  })

  it("marks sequence gaps as full reload recovery events", () => {
    useWorkspaceStore().setActiveWorkspace(7)
    const store = useSignalRowsPatchStore()
    store.applyEvent(buildEvent({ sequence: 1 }))

    const result = store.applyEvent(buildEvent({ sequence: 3 }))

    expect(result).toMatchObject({
      applied: true,
      gap: true,
      requiresFullReload: true,
    })
    expect(store.activeWorkspacePatchEvent?.requires_full_reload).toBe(true)
    expect(store.getLastSequence(7)).toBe(3)
  })

  it("ignores duplicate or older patch events", () => {
    useWorkspaceStore().setActiveWorkspace(7)
    const store = useSignalRowsPatchStore()
    store.applyEvent(buildEvent({ sequence: 4 }))
    const revision = store.activeWorkspacePatchRevision

    const duplicate = store.applyEvent(buildEvent({ sequence: 4 }))
    const older = store.applyEvent(buildEvent({ sequence: 3 }))

    expect(duplicate).toMatchObject({
      applied: false,
      ignored: "stale",
      duplicateOrOld: true,
    })
    expect(older).toMatchObject({
      applied: false,
      ignored: "stale",
      duplicateOrOld: true,
    })
    expect(store.activeWorkspacePatchRevision).toBe(revision)
    expect(store.getLastSequence(7)).toBe(4)
  })

  it("ignores events for another workspace", () => {
    useWorkspaceStore().setActiveWorkspace(7)
    const store = useSignalRowsPatchStore()

    const result = store.applyEvent(buildEvent({ workspace_id: 8 }))

    expect(result).toMatchObject({
      applied: false,
      ignored: "workspace_mismatch",
    })
    expect(store.activeWorkspacePatchEvent).toBeNull()
    expect(store.getLastSequence(8)).toBeNull()
  })
})
