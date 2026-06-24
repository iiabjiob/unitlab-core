import { describe, expect, it } from "vitest"

import type { SignalAllocationRow } from "@/types/signal"

import { resolveVerificationSelection } from "./verificationSelection"

function buildRow(overrides: Partial<SignalAllocationRow>): SignalAllocationRow {
  return {
    signal_id: 1,
    signal_key: "signal-1",
    signal_name: "Signal 1",
    signal_direction: "DO",
    signal_category: null,
    signal_metadata: {},
    allocation_id: 1,
    allocation_status: "assigned",
    allocation_health: {},
    channel_id: 3,
    channel_type: "DO",
    channel_index: 1,
    channel_label: "CH3",
    device_id: 10,
    unit_id: "IED-A/P1",
    unit_online: true,
    unit_last_seen_at: null,
    tested_at: null,
    ...overrides,
  }
}

describe("resolveVerificationSelection", () => {
  it("allows multiple selected rows on the same IED", () => {
    const result = resolveVerificationSelection([
      buildRow({ signal_id: 101 }),
      buildRow({ signal_id: 102, signal_key: "signal-2", signal_name: "Signal 2", channel_id: 4 }),
    ])

    expect(result.canRun).toBe(true)
    expect(result.unitId).toBe("IED-A/P1")
    expect(result.signalIds).toEqual([101, 102])
    expect(result.label).toBe("2 selected signals on IED-A/P1")
  })

  it("allows mixed IED selections", () => {
    const result = resolveVerificationSelection([
      buildRow({ signal_id: 101 }),
      buildRow({ signal_id: 202, signal_key: "signal-2", signal_name: "Signal 2", unit_id: "IED-B/P1" }),
    ])

    expect(result.canRun).toBe(true)
    expect(result.unitId).toBeNull()
    expect(result.signalIds).toEqual([101, 202])
    expect(result.label).toBe("2 selected signals across 2 IEDs")
  })
})
