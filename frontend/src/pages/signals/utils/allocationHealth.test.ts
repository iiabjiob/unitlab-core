import { describe, expect, it } from "vitest"

import type { SignalAllocationRow } from "@/types/signal"

import {
  resolveSignalAllocationHealthLabel,
  resolveSignalAllocationStatus,
  resolveSignalAllocationStatusLabel,
} from "./allocationHealth"

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

describe("allocationHealth", () => {
  it("falls back to channel presence when projection status is missing", () => {
    expect(resolveSignalAllocationStatus(createRow())).toBe("unassigned")
    expect(resolveSignalAllocationStatus(createRow({ channel_id: 12 }))).toBe("assigned")
  })

  it("normalizes legacy status names", () => {
    expect(resolveSignalAllocationStatus(createRow({ allocation_status: "allocated" }))).toBe("assigned")
    expect(resolveSignalAllocationStatus(createRow({ allocation_status: "unallocated" }))).toBe("unassigned")
  })

  it("labels status and health fields for grid display", () => {
    const conflict = createRow({
      allocation_status: "conflict",
      allocation_health: { conflict: true },
    })
    const invalid = createRow({
      allocation_status: "invalid",
      allocation_health: { invalid_type: true },
    })
    const offline = createRow({
      allocation_status: "assigned",
      allocation_health: { offline_device: true },
    })

    expect(resolveSignalAllocationStatusLabel(conflict)).toBe("Conflict")
    expect(resolveSignalAllocationHealthLabel(conflict)).toBe("Conflict")
    expect(resolveSignalAllocationHealthLabel(invalid)).toBe("Invalid type")
    expect(resolveSignalAllocationHealthLabel(offline)).toBe("Offline")
  })

})
