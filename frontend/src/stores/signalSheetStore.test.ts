import { beforeEach, describe, expect, it, vi } from "vitest"
import { createPinia, setActivePinia } from "pinia"
import { isReactive } from "vue"

import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import type { SignalAllocationRow } from "@/types/signal"

const signalSheetApiMock = vi.hoisted(() => ({
  streamAllocations: vi.fn(),
  listAllocations: vi.fn(),
}))

vi.mock("@/api/signal_sheet.api", () => ({
  SignalSheetAPI: signalSheetApiMock,
}))

function buildRow(overrides: Partial<SignalAllocationRow> = {}): SignalAllocationRow {
  return {
    row_id: `signal-${overrides.signal_id ?? 1}`,
    signal_id: 1,
    signal_key: "S1",
    signal_name: "Signal 1",
    signal_direction: "DI",
    signal_category: null,
    signal_metadata: {},
    allocation_id: null,
    allocation_status: "unassigned",
    allocation_health: null,
    channel_id: null,
    channel_type: null,
    channel_index: null,
    channel_label: null,
    device_id: null,
    unit_id: null,
    unit_online: null,
    unit_last_seen_at: null,
    tested_at: null,
    ...overrides,
  }
}

describe("signalSheetStore allocation patching", () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    signalSheetApiMock.streamAllocations.mockReset()
    signalSheetApiMock.listAllocations.mockReset()
    if (typeof window !== "undefined") {
      window.localStorage.clear()
    }
  })

  it("does not replace the loaded allocation projection when a normal patch references an unknown row", async () => {
    signalSheetApiMock.streamAllocations.mockResolvedValue([
      buildRow({ signal_id: 1, row_id: "signal-1" }),
      buildRow({ signal_id: 2, row_id: "signal-2" }),
    ])
    useWorkspaceStore().setActiveWorkspace(7)
    const store = useSignalSheetStore()

    await store.refreshAllocations()
    store.applyAllocationRowsPatch([
      buildRow({
        signal_id: 99,
        row_id: "signal-99",
        channel_id: 990,
        channel_index: 0,
        unit_id: "unit-z",
      }),
    ])

    expect(store.allocationRows.map(row => row.signal_id)).toEqual([1, 2])
    expect(store.allocationRows[0].channel_id).toBeNull()
    expect(store.allocationRows[1].channel_id).toBeNull()
  })

  it("patches known rows without forcing a projection reload", async () => {
    signalSheetApiMock.streamAllocations.mockResolvedValue([
      buildRow({ signal_id: 1, row_id: "signal-1" }),
      buildRow({ signal_id: 2, row_id: "signal-2" }),
    ])
    useWorkspaceStore().setActiveWorkspace(7)
    const store = useSignalSheetStore()

    await store.refreshAllocations()
    store.applyAllocationRowsPatch([
      buildRow({
        signal_id: 2,
        row_id: "signal-2",
        channel_id: 22,
        channel_index: 1,
        unit_id: "unit-b",
      }),
    ])

    expect(store.allocationRows.map(row => row.signal_id)).toEqual([1, 2])
    expect(store.allocationRows[1]).toMatchObject({
      signal_id: 2,
      channel_id: 22,
      unit_id: "unit-b",
    })
    expect(signalSheetApiMock.streamAllocations).toHaveBeenCalledTimes(1)
    expect(signalSheetApiMock.listAllocations).not.toHaveBeenCalled()
  })

  it("keeps the allocation row cache shallow so bulk patches do not deep-track every row", async () => {
    signalSheetApiMock.streamAllocations.mockResolvedValue([
      buildRow({ signal_id: 1, row_id: "signal-1" }),
      buildRow({ signal_id: 2, row_id: "signal-2" }),
    ])
    useWorkspaceStore().setActiveWorkspace(7)
    const store = useSignalSheetStore()

    await store.refreshAllocations()

    expect(isReactive(store.allocationRows)).toBe(false)
    expect(isReactive(store.allocationRows[0])).toBe(false)
  })
})
