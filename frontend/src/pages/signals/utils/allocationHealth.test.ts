import { describe, expect, it } from "vitest"

import type { SignalAllocationRow } from "@/types/signal"

import {
  countSignalAllocationQuickFilters,
  hasSignalAllocationIssue,
  matchesSignalAllocationQuickFilter,
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

  it("matches quick filters by status and health flags", () => {
    const assigned = createRow({ channel_id: 10, allocation_status: "assigned" })
    const unassigned = createRow({ allocation_status: "unassigned" })
    const conflict = createRow({ allocation_status: "conflict", allocation_health: { conflict: true } })
    const stale = createRow({ allocation_status: "assigned", allocation_health: { stale_device: true } })

    expect(matchesSignalAllocationQuickFilter(assigned, "assigned")).toBe(true)
    expect(matchesSignalAllocationQuickFilter(unassigned, "unassigned")).toBe(true)
    expect(matchesSignalAllocationQuickFilter(conflict, "issues")).toBe(true)
    expect(matchesSignalAllocationQuickFilter(conflict, "conflicts")).toBe(true)
    expect(matchesSignalAllocationQuickFilter(stale, "offline_missing")).toBe(true)
    expect(hasSignalAllocationIssue(stale)).toBe(true)
  })

  it("counts quick filters from the full allocation projection", () => {
    const counts = countSignalAllocationQuickFilters([
      createRow({ signal_id: 1, allocation_status: "unassigned" }),
      createRow({ signal_id: 2, channel_id: 10, allocation_status: "assigned" }),
      createRow({ signal_id: 3, allocation_status: "conflict", allocation_health: { conflict: true } }),
      createRow({ signal_id: 4, allocation_status: "invalid", allocation_health: { invalid_type: true } }),
      createRow({ signal_id: 5, allocation_status: "assigned", allocation_health: { offline_device: true } }),
    ])

    expect(counts).toEqual({
      all: 5,
      unassigned: 1,
      assigned: 2,
      issues: 3,
      conflicts: 1,
      invalid: 1,
      offline_missing: 1,
    })
  })
})
