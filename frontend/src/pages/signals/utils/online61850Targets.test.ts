import { describe, expect, it } from "vitest"

import type { SignalAllocationRow } from "@/types/signal"
import {
  buildExternalIedAvailabilityTargets,
  buildOnline61850PreparationTargets,
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

  it("keeps rows grouped by host and port when building preparation targets", () => {
    const row = buildRow({
      signal_id: 10,
      signal_metadata: {
        verification: {
          enabled: true,
          transport_host: "172.16.40.128:12447",
          iec61850_address: "KINTE15BCU01CTRL1/CBCSWI1/Pos/stVal[ST]",
        },
        row: {
          host: "172.16.40.128:12447",
        },
      },
    })

    const result = buildOnline61850PreparationTargets([row])

    expect(result.targets).toHaveLength(1)
    expect(result.targets[0]?.host).toBe("172.16.40.128")
    expect(result.targets[0]?.port).toBe(12447)
    expect(result.targets[0]?.signalIds).toEqual([10])
  })

  it("does not build targets from IP-like row data without wizard-confirmed verification", () => {
    const row = buildRow({
      signal_id: 11,
      signal_metadata: {
        row: {
          host: "172.16.40.129",
          iec61850_address: "KINTE15BCU01CTRL1/CBCSWI1/Pos/stVal[ST]",
        },
      },
    })

    const result = buildOnline61850PreparationTargets([row])

    expect(result.targets).toHaveLength(0)
    expect(result.skippedRows).toHaveLength(1)
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
