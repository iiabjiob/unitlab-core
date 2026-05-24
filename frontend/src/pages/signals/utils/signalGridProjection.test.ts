import { describe, expect, it } from "vitest"

import {
  createSignalGridRowPatch,
  createSignalGridRows,
  resolveSignalAllocationDisplayLabel,
  signalGridSourceColumnKey,
} from "./signalGridProjection"
import type { SignalAllocationRow } from "@/types/signal"

function buildRow(overrides: Partial<SignalAllocationRow> = {}): SignalAllocationRow {
  return {
    row_id: "signal-1",
    signal_id: 1,
    signal_key: "S1",
    signal_name: "Signal 1",
    signal_direction: "DO",
    signal_category: null,
    signal_metadata: {
      row: {
        Cabinet: "A1",
        Terminal: "X1:1",
      },
    },
    allocation_id: 10,
    allocation_status: "assigned",
    allocation_health: {
      offline_device: false,
    },
    channel_id: 20,
    channel_type: "do",
    channel_index: 2,
    channel_label: "fallback-label",
    device_id: 30,
    unit_id: "unit-a",
    unit_online: true,
    unit_last_seen_at: null,
    tested_at: null,
    ...overrides,
  }
}

describe("signalGridProjection", () => {
  it("maps a projection row into the grid row shape", () => {
    const [row] = createSignalGridRows([buildRow()], ["Cabinet", "Terminal", "Missing"])

    expect(row).toMatchObject({
      rowId: "signal-1",
      signal_id: 1,
      internal_signal_type: "di",
      channel_select: "unit-a/ch3",
      allocation_status: "assigned",
      allocation_health: "OK",
      tested_at: null,
      [signalGridSourceColumnKey(0)]: "A1",
      [signalGridSourceColumnKey(1)]: "X1:1",
      [signalGridSourceColumnKey(2)]: "",
    })
  })

  it("uses stable fallback row ids and channel labels", () => {
    const [row] = createSignalGridRows([
      buildRow({
        row_id: undefined,
        channel_index: null,
        unit_id: null,
        channel_label: "unit-b/DO1",
      }),
    ], [])

    expect(row.rowId).toBe("signal-1")
    expect(row.channel_select).toBe("unit-b/DO1")
  })

  it("applies runtime tested-at overlay without mutating the source row", () => {
    const source = buildRow({ tested_at: "2026-01-01T00:00:00Z" })

    const [row] = createSignalGridRows([source], [], {
      workspaceId: 7,
      getTestedAt: (signalId, workspaceId) => (
        signalId === 1 && workspaceId === 7 ? "2026-01-01T00:00:01Z" : null
      ),
    })

    expect(row.tested_at).toBe("2026-01-01T00:00:01Z")
    expect(source.tested_at).toBe("2026-01-01T00:00:00Z")
  })

  it("creates row patches through the same mapper boundary", () => {
    const patch = createSignalGridRowPatch(buildRow(), ["Cabinet"], ["channel_select"])

    expect(patch).toEqual({
      rowId: "signal-1",
      changes: {
        channel_select: "unit-a/ch3",
      },
      columns: ["channel_select"],
    })
  })

  it("only includes requested source columns in row patches", () => {
    const patch = createSignalGridRowPatch(buildRow(), ["Cabinet", "Terminal"], [signalGridSourceColumnKey(1)])

    expect(patch).toEqual({
      rowId: "signal-1",
      changes: {
        [signalGridSourceColumnKey(1)]: "X1:1",
      },
      columns: [signalGridSourceColumnKey(1)],
    })
  })

  it("maps row arrays without sharing array identity", () => {
    const rows = [buildRow({ signal_id: 1, row_id: "signal-1" })]
    const mapped = createSignalGridRows(rows, [])

    expect(mapped).toHaveLength(1)
    expect(mapped).not.toBe(rows)
  })

  it("formats unassigned rows as a dash", () => {
    const row = buildRow({
      allocation_status: "unassigned",
      channel_id: null,
      channel_index: null,
      channel_label: null,
      unit_id: null,
    })

    expect(resolveSignalAllocationDisplayLabel(row)).toBe("-")
    expect(createSignalGridRows([row], [])[0]).toMatchObject({
      allocation_status: "unassigned",
      allocation_health: "Unassigned",
    })
  })

  it("keeps invalid and offline health visible in the grid row", () => {
    expect(createSignalGridRows([buildRow({
      allocation_status: "invalid",
      allocation_health: { invalid_type: true },
    })], [])[0]).toMatchObject({
      allocation_status: "invalid",
      allocation_health: "Invalid type",
    })
    expect(createSignalGridRows([buildRow({
      allocation_health: { offline_device: true },
    })], [])[0]).toMatchObject({
      allocation_health: "Offline",
    })
  })
})
