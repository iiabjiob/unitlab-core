import { describe, expect, it } from "vitest"

import type { SignalAllocationJob, SignalAllocationRow } from "@/types/signal"
import {
  normalizeSignalAllocationJobChangedRows,
  resolveSignalAllocationJobSkippedCount,
} from "./signalAllocationJobResult"

function buildBaseRow(): SignalAllocationRow {
  return {
    row_id: "signal-1",
    signal_id: 1,
    signal_key: "SIG-1",
    signal_name: "Signal 1",
    signal_direction: "DO",
    signal_category: null,
    signal_metadata: {},
    allocation_id: 10,
    allocation_status: "assigned",
    allocation_health: { offline_device: true },
    channel_id: 20,
    channel_type: "DO",
    channel_index: 1,
    channel_label: "CH-2",
    device_id: 30,
    unit_id: "UNIT-1",
    unit_online: false,
    unit_last_seen_at: "2026-05-24T20:00:00Z",
    tested_at: "2026-05-24T20:01:00Z",
  }
}

function buildJob(result: Record<string, unknown>): SignalAllocationJob {
  return {
    job_id: "job-1",
    workspace_id: 2,
    operation: "bulk_update",
    status: "succeeded",
    progress_total: 1,
    progress_done: 1,
    message: null,
    error: null,
    result,
    created_at: "2026-05-24T20:00:00Z",
    updated_at: "2026-05-24T20:00:01Z",
  }
}

describe("normalizeSignalAllocationJobChangedRows", () => {
  it("preserves explicit null fields from unassign job patches", () => {
    const baseRow = buildBaseRow()
    const rows = normalizeSignalAllocationJobChangedRows(
      buildJob({
        changed_row_patches: [
          {
            row_id: "signal-1",
            signal_id: 1,
            allocation_id: null,
            allocation_status: "unassigned",
            allocation_health: {},
            channel_id: null,
            channel_type: null,
            channel_index: null,
            channel_label: null,
            device_id: null,
            unit_id: null,
            unit_online: null,
            unit_last_seen_at: null,
          },
        ],
      }),
      signalId => signalId === 1 ? baseRow : null,
    )

    expect(rows).toHaveLength(1)
    expect(rows[0]).toMatchObject({
      signal_id: 1,
      signal_key: "SIG-1",
      signal_name: "Signal 1",
      allocation_id: null,
      allocation_status: "unassigned",
      allocation_health: {},
      channel_id: null,
      channel_type: null,
      channel_index: null,
      channel_label: null,
      device_id: null,
      unit_id: null,
      unit_online: null,
      unit_last_seen_at: null,
    })
  })
})

describe("resolveSignalAllocationJobSkippedCount", () => {
  it("uses auto-allocation skipped and rejected result counts", () => {
    const job = buildJob({
      skipped_items: [{ signal_id: 1 }, { signal_id: 2 }],
      rejected: [{ signal_id: 3 }],
    })
    job.operation = "auto_allocate"

    expect(resolveSignalAllocationJobSkippedCount(job, 10, 7)).toBe(3)
  })

  it("falls back to requested minus changed for non-auto jobs", () => {
    expect(resolveSignalAllocationJobSkippedCount(buildJob({}), 10, 7)).toBe(3)
  })
})
