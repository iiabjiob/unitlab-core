import { describe, expect, it } from "vitest"

import type { SignalAllocationRow } from "@/types/signal"
import {
  buildExternalIedAvailabilityTargets,
  resolveOnline61850SignalReference,
} from "./online61850Targets"

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

  it("builds external IED watcher targets only from mapped verification rows", () => {
    const result = buildExternalIedAvailabilityTargets([
      buildRow({
        signal_id: 20,
        signal_metadata: {
          verification: {
            enabled: true,
            transport_host: "10.10.10.20:12447",
            iec61850_address: "IED-A/P1/LLN0.brA",
          },
        },
      }),
      buildRow({
        signal_id: 21,
        signal_metadata: {
          verification: {
            enabled: true,
            transport_host: "10.10.10.20",
            iec61850_address: "IED-A/P1/LLN0.brB",
          },
        },
      }),
      buildRow({
        signal_id: 22,
        signal_metadata: {
          verification: {
            enabled: true,
            transport_host: "10.10.10.21",
          },
        },
      }),
      buildRow({
        signal_id: 23,
        signal_metadata: {
          row: { host: "10.10.10.22", iec61850_address: "IED-A/P1/LLN0.brC" },
        },
      }),
    ])

    expect(result).toEqual([
      { ip: "10.10.10.20", port: 102, signalIds: [21] },
      { ip: "10.10.10.20", port: 12447, signalIds: [20] },
    ])
  })
})
