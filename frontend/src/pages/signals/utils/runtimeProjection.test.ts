import { describe, expect, it } from "vitest"

import type { SignalAllocationRow } from "@/types/signal"

import { applyRuntimeTestedAt, applyRuntimeTestedAtToRows, resolveRuntimeTestedAt } from "./runtimeProjection"

function createRow(overrides: Partial<SignalAllocationRow> = {}): SignalAllocationRow {
  return {
    signal_id: 1,
    signal_key: "SIG-1",
    signal_name: "Signal 1",
    signal_direction: "DI",
    signal_category: null,
    signal_metadata: {},
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

describe("runtimeProjection", () => {
  it("keeps the static row reference when no runtime value exists", () => {
    const row = createRow({ tested_at: "2026-01-01T00:00:00Z" })

    expect(resolveRuntimeTestedAt(row, 10, () => null)).toBe("2026-01-01T00:00:00Z")
    expect(applyRuntimeTestedAt(row, 10, () => null)).toBe(row)
  })

  it("overlays runtime tested_at without mutating the static row", () => {
    const row = createRow({ tested_at: "2026-01-01T00:00:00Z" })
    const patched = applyRuntimeTestedAt(row, 10, () => "2026-01-01T00:01:00Z")

    expect(patched).not.toBe(row)
    expect(patched.tested_at).toBe("2026-01-01T00:01:00Z")
    expect(row.tested_at).toBe("2026-01-01T00:00:00Z")
  })

  it("only copies rows with a changed runtime tested_at value", () => {
    const rows = [
      createRow({ signal_id: 1, tested_at: null }),
      createRow({ signal_id: 2, tested_at: "2026-01-01T00:00:00Z" }),
    ]
    const patched = applyRuntimeTestedAtToRows(rows, 10, signalId => (
      signalId === 1 ? "2026-01-01T00:01:00Z" : null
    ))

    expect(patched[0]).not.toBe(rows[0])
    expect(patched[0].tested_at).toBe("2026-01-01T00:01:00Z")
    expect(patched[1]).toBe(rows[1])
  })
})
