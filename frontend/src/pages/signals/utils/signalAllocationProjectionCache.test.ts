import { describe, expect, it } from "vitest"

import { createSignalAllocationProjectionCache } from "./signalAllocationProjectionCache"
import type { SignalAllocationRow } from "@/types/signal"

function buildRow(overrides: Partial<SignalAllocationRow> = {}): SignalAllocationRow {
  return {
    row_id: `signal-${overrides.signal_id ?? 1}`,
    signal_id: 1,
    signal_key: "S1",
    signal_name: "Signal 1",
    signal_direction: "DI",
    signal_category: null,
    signal_metadata: { row: { Cabinet: "A1" } },
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

describe("createSignalAllocationProjectionCache", () => {
  it("indexes rows by signal id and channel owner", () => {
    const cache = createSignalAllocationProjectionCache()

    cache.replaceRows([
      buildRow({ signal_id: 1, row_id: "signal-1", channel_id: 10 }),
      buildRow({ signal_id: 2, row_id: "signal-2", channel_id: null }),
    ])

    expect(cache.rowCount).toBe(2)
    expect(cache.allocatedCount).toBe(1)
    expect(cache.getRowBySignalId(1)?.row_id).toBe("signal-1")
    expect(cache.getOwnerSignalIdByChannelId(10)).toBe(1)
    expect(cache.getSignalIds()).toEqual([1, 2])
  })

  it("patches existing rows without replacing the full cache", () => {
    const cache = createSignalAllocationProjectionCache()
    cache.replaceRows([
      buildRow({ signal_id: 1, row_id: "signal-1", channel_id: null }),
      buildRow({ signal_id: 2, row_id: "signal-2", channel_id: 20 }),
    ])
    const initialVersion = cache.version

    const result = cache.patchRows([
      buildRow({
        signal_id: 1,
        row_id: "signal-1",
        channel_id: 30,
        unit_id: "unit-a",
      }),
    ])

    expect(result).toEqual({ changed: 1, missingSignalIds: [] })
    expect(cache.version).toBe(initialVersion + 1)
    expect(cache.allocatedCount).toBe(2)
    expect(cache.getOwnerSignalIdByChannelId(30)).toBe(1)
    expect(cache.getRowBySignalId(1)).toMatchObject({
      channel_id: 30,
      unit_id: "unit-a",
    })
  })

  it("updates channel ownership when rows are unassigned", () => {
    const cache = createSignalAllocationProjectionCache()
    cache.replaceRows([buildRow({ signal_id: 1, row_id: "signal-1", channel_id: 10 })])

    cache.patchRows([buildRow({ signal_id: 1, row_id: "signal-1", channel_id: null })])

    expect(cache.allocatedCount).toBe(0)
    expect(cache.getOwnerSignalIdByChannelId(10)).toBeNull()
    expect(cache.getRowBySignalId(1)?.channel_id).toBeNull()
  })

  it("reports missing patch rows without changing version", () => {
    const cache = createSignalAllocationProjectionCache()
    cache.replaceRows([buildRow({ signal_id: 1, row_id: "signal-1" })])
    const initialVersion = cache.version

    const result = cache.patchRows([buildRow({ signal_id: 9, row_id: "signal-9" })])

    expect(result).toEqual({ changed: 0, missingSignalIds: [9] })
    expect(cache.version).toBe(initialVersion)
  })

  it("clones rows so external mutations do not rewrite cached source metadata", () => {
    const cache = createSignalAllocationProjectionCache()
    const source = buildRow({ signal_id: 1, row_id: "signal-1" })
    cache.replaceRows([source])

    source.signal_metadata.row = { Cabinet: "B2" }
    source.channel_id = 99

    expect(cache.getRowBySignalId(1)?.channel_id).toBeNull()
    expect(cache.getRowBySignalId(1)?.signal_metadata).toEqual({ row: { Cabinet: "A1" } })
  })
})
