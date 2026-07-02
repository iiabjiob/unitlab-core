import { describe, expect, it } from "vitest"

import type { SignalAllocationRow } from "@/types/signal"
import { matchOnline61850TargetSignals } from "./online61850TargetMatching"

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

describe("matchOnline61850TargetSignals", () => {
  it("matches full signal-list paths against relative discovery references", () => {
    const rows = [
      buildRow({
        signal_id: 10,
        signal_metadata: {
          row: {
            "61850 Address": "KINTE15BCU01CTRL1/CBCSWI1/Pos/Oper.ctlVal[CO]",
          },
        },
      }),
    ]

    const result = matchOnline61850TargetSignals(rows, {
      endpoint: {
        iedName: "KINTE15BCU01",
        accessPointName: "AP1",
      },
      signals: [
        {
          reference: "CTRL1/CBCSWI1.Pos.Oper.ctlVal[CO]",
        },
      ],
      dataSets: [],
    }, {
      iedName: "KINTE15BCU01",
      accessPointName: "AP1",
    })

    expect(result.matchedSignalCount).toBe(1)
    expect(result.unmatchedSignalCount).toBe(0)
    expect([...result.matchedSignalIds]).toEqual([10])
    expect(result.matchedSignalReferencesById.get(10)).toBe("ctrl1/cbcswi1.pos.oper.ctlval[co]")
  })
})
