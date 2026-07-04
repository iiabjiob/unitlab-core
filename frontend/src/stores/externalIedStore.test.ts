import { beforeEach, describe, expect, it, vi } from "vitest"
import { createPinia, setActivePinia } from "pinia"

import { useExternalIedStore } from "@/stores/externalIedStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { WSChannel, type ExternalIedPlanningChangedEvent, type ExternalIedStatusChangedEvent, type ExternalIedStatusSnapshotEvent } from "@/types/ws/events"

const verificationApiMock = vi.hoisted(() => ({
  configureExternalIedTargets: vi.fn(),
  refreshExternalIedDiscovery: vi.fn(),
  getExternalIedDiscoveryTree: vi.fn(),
}))

vi.mock("@/api/verification.api", () => ({
  VerificationAPI: verificationApiMock,
}))

function snapshot(overrides: Partial<ExternalIedStatusSnapshotEvent> = {}): ExternalIedStatusSnapshotEvent {
  return {
    channel: WSChannel.EXTERNAL_IED_STATUS,
    event: "external_ied_status_snapshot",
    workspace_id: 7,
    devices: [],
    removed_signal_ids: [],
    emitted_at: "2026-01-01T00:00:00Z",
    ...overrides,
  }
}

function statusChanged(overrides: Partial<ExternalIedStatusChangedEvent> = {}): ExternalIedStatusChangedEvent {
  return {
    channel: WSChannel.EXTERNAL_IED_STATUS,
    event: "external_ied_status_changed",
    workspace_id: 7,
    ip: "10.10.10.20",
    port: 102,
    old_status: "expected",
    new_status: "reachable",
    signal_ids: [1],
    checked_at: "2026-01-01T00:00:01Z",
    check_kind: "tcp_connect",
    failure_code: null,
    error: null,
    ...overrides,
  }
}

function planningChanged(overrides: Partial<ExternalIedPlanningChangedEvent> = {}): ExternalIedPlanningChangedEvent {
  return {
    channel: WSChannel.EXTERNAL_IED_STATUS,
    event: "external_ied_planning_changed",
    workspace_id: 7,
    endpoint: {
      endpoint: "10.10.10.20:102",
      ip: "10.10.10.20",
      port: 102,
      state: "Ready",
      updated_at_ms: 1000,
      matched_count: 1,
      unmatched_count: 0,
      ambiguous_count: 0,
      signal_ids: [1],
    },
    signal_results: [{
      signal_id: 1,
      endpoint: "10.10.10.20:102",
      status: "matched",
      address: "IEDLD0/GGIO1.ST.stVal",
      fcda_reference: "IEDLD0/GGIO1.ST.stVal",
      dataset_reference: "IEDLD0/LLN0.ds",
      rcb_reference: "IEDLD0/LLN0.BR.brcb01",
      rcb_name: "brcb01",
    }],
    removed_signal_ids: [],
    emitted_at: "2026-01-01T00:00:02Z",
    ...overrides,
  }
}

describe("externalIedStore", () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    useWorkspaceStore().setActiveWorkspace(7)
    verificationApiMock.configureExternalIedTargets.mockReset()
    verificationApiMock.configureExternalIedTargets.mockResolvedValue({ data: {} })
    verificationApiMock.refreshExternalIedDiscovery.mockReset()
    verificationApiMock.refreshExternalIedDiscovery.mockResolvedValue({ data: {} })
    verificationApiMock.getExternalIedDiscoveryTree.mockReset()
  })

  it("does not configure backend when no workspace is active", async () => {
    useWorkspaceStore().setActiveWorkspace(null)
    const store = useExternalIedStore()

    await store.configureExpectedDevices(null, [{ ip: "10.10.10.20", signalIds: [1] }])

    expect(store.devices).toEqual([])
    expect(verificationApiMock.configureExternalIedTargets).not.toHaveBeenCalled()
  })

  it("configures backend target set without starting frontend polling", async () => {
    const store = useExternalIedStore()

    await store.configureExpectedDevices(7, [{ ip: "10.10.10.20", signalIds: [1, 1] }])
    await store.configureExpectedDevices(7, [{ ip: "10.10.10.20", signalIds: [1] }])

    expect(verificationApiMock.configureExternalIedTargets).toHaveBeenCalledTimes(1)
    expect(verificationApiMock.configureExternalIedTargets).toHaveBeenCalledWith(7, {
      targets: [{ ip: "10.10.10.20", port: 102, signal_ids: [1] }],
    })
    expect(store.getStatus("10.10.10.20")).toBe("expected")
  })

  it("keeps endpoint status scoped by custom MMS port", async () => {
    const store = useExternalIedStore()

    await store.configureExpectedDevices(7, [{ ip: "10.10.10.20", port: 12447, signalIds: [1] }])
    store.applyStatusChanged(statusChanged({ port: 12447 }))

    expect(verificationApiMock.configureExternalIedTargets).toHaveBeenCalledWith(7, {
      targets: [{ ip: "10.10.10.20", port: 12447, signal_ids: [1] }],
    })
    expect(store.getStatus("10.10.10.20", 102)).toBe("not_applicable")
    expect(store.getStatus("10.10.10.20", 12447)).toBe("reachable")
  })

  it("applies backend snapshots and ignores other workspaces", () => {
    const store = useExternalIedStore()

    store.applySnapshot(snapshot({
      devices: [{
        ip: "10.10.10.20",
        port: 102,
        status: "offline",
        signal_ids: [1],
        last_checked_at: "2026-01-01T00:00:00Z",
        last_error: "timeout",
        check_kind: "tcp_connect",
        failure_code: "unreachable",
      }],
    }))
    store.applySnapshot(snapshot({ workspace_id: 8, devices: [] }))

    expect(store.getStatus("10.10.10.20")).toBe("offline")
    expect(store.lastChangedSignalIds).toEqual([1])
  })

  it("loads discovery model tree lazily from backend details endpoint", async () => {
    const store = useExternalIedStore()
    verificationApiMock.getExternalIedDiscoveryTree.mockResolvedValue({
      data: {
        endpoint: "10.10.10.20:102",
        model_fingerprint: "model-1",
        reports: [{
          reference: "IEDLD0/LLN0.BR.brcb01",
          name: "brcb01",
          kind: "buffered",
          dataset_reference: "IEDLD0/LLN0.ds",
          dataset: {
            reference: "IEDLD0/LLN0.ds",
            signals: [{ reference: "IEDLD0/GGIO1.ST.stVal", fc: "ST" }],
          },
        }],
      },
    })

    const tree = await store.loadDiscoveryTree("10.10.10.20", 102)

    expect(verificationApiMock.getExternalIedDiscoveryTree).toHaveBeenCalledWith(7, "10.10.10.20", 102)
    expect(tree?.reports[0]?.name).toBe("brcb01")
    expect(tree?.reports[0]?.dataset?.signals[0]).toEqual({
      reference: "IEDLD0/GGIO1.ST.stVal",
      fc: "ST",
    })
    expect(store.getDiscoveryTree("10.10.10.20", 102)?.model_fingerprint).toBe("model-1")
  })

  it("applies backend transition events as IP-level diffs", () => {
    const store = useExternalIedStore()

    store.applyStatusChanged(statusChanged())

    expect(store.getStatus("10.10.10.20")).toBe("reachable")
    expect(store.lastChangedIps).toEqual(["10.10.10.20"])
    expect(store.lastChangedSignalIds).toEqual([1])
  })

  it("keeps configured signal ids and counts reachable endpoints as ready", async () => {
    const store = useExternalIedStore()

    await store.configureExpectedDevices(7, [{ ip: "10.10.10.20", port: 12447, signalIds: [11, 12] }])
    store.applyStatusChanged(statusChanged({ port: 12447, signal_ids: [] }))

    expect(store.getRecord("10.10.10.20", 12447)?.signalIds).toEqual([11, 12])
    expect(store.summary).toMatchObject({
      ready: 1,
      discovering: 0,
      offline: 0,
      failed: 0,
    })
  })

  it("clears local statuses when backend disables context", () => {
    const store = useExternalIedStore()
    store.applyStatusChanged(statusChanged())

    store.applySnapshot(snapshot({ devices: [], removed_signal_ids: [1] }))

    expect(store.getStatus("10.10.10.20")).toBe("not_applicable")
    expect(store.lastChangedSignalIds).toEqual([1])
  })

  it("applies planning coverage as signal-level diffs", () => {
    const store = useExternalIedStore()
    store.applyStatusChanged(statusChanged())

    store.applyPlanningChanged(planningChanged())

    expect(store.getRecord("10.10.10.20")?.planningState).toBe("Ready")
    expect(store.getPlanningCoverage(1)).toMatchObject({
      signalId: 1,
      status: "matched",
      rcbName: "brcb01",
    })
    expect(store.lastChangedSignalIds).toEqual([1])
  })
})
