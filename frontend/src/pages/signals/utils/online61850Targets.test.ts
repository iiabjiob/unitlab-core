import { describe, expect, it } from "vitest"

import type { SignalAllocationRow } from "@/types/signal"
import { buildOnline61850PreparationTargets, resolveOnline61850SignalReference } from "./online61850Targets"

function buildRow(overrides: Partial<SignalAllocationRow>): SignalAllocationRow {
  return {
    signal_id: overrides.signal_id ?? 1,
    signal_key: overrides.signal_key ?? "SIG",
    signal_name: overrides.signal_name ?? "Signal",
    signal_direction: overrides.signal_direction ?? "DO",
    signal_category: null,
    signal_metadata: overrides.signal_metadata ?? {},
    channel_id: null,
    channel_type: null,
    channel_index: null,
    channel_label: null,
    device_id: null,
    unit_id: null,
    unit_online: null,
    unit_last_seen_at: null,
    tested_at: null,
  }
}

describe("online61850Targets", () => {
  it("resolves the IEC 61850 address from nested verification metadata", () => {
    const row = buildRow({
      signal_metadata: {
        verification: {
          iec61850_address: "KINTE15BCU01CTRL1/CBCSWI1/Pos/stVal[ST]",
        },
      },
    })

    expect(resolveOnline61850SignalReference(row)).toBe("KINTE15BCU01CTRL1/CBCSWI1/Pos/stVal[ST]")
  })

  it("keeps rows grouped by host and port when building preparation targets", () => {
    const row = buildRow({
      signal_id: 10,
      signal_metadata: {
        row: {
          host: "172.16.40.128:12447",
          verification: {
            iec61850_address: "KINTE15BCU01CTRL1/CBCSWI1/Pos/stVal[ST]",
          },
        },
      },
    })

    const result = buildOnline61850PreparationTargets([row])

    expect(result.targets).toHaveLength(1)
    expect(result.targets[0]?.host).toBe("172.16.40.128")
    expect(result.targets[0]?.port).toBe(12447)
    expect(result.targets[0]?.signalIds).toEqual([10])
  })
})
