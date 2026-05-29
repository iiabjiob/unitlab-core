import { describe, expect, it } from "vitest"

import type { SignalAllocationRow } from "@/types/signal"
import type { Iec61850DebugDocument } from "./iec61850DebugTree"
import { mergeIec61850SignalList } from "./iec61850SignalListMerge"

function buildSignalRow(overrides: Partial<SignalAllocationRow>): SignalAllocationRow {
  return {
    signal_id: overrides.signal_id ?? 1,
    signal_key: overrides.signal_key ?? "SIG",
    signal_name: overrides.signal_name ?? "Signal",
    signal_direction: overrides.signal_direction ?? "DI",
    signal_category: overrides.signal_category ?? null,
    signal_metadata: overrides.signal_metadata ?? {},
    channel_id: overrides.channel_id ?? null,
    channel_type: overrides.channel_type ?? null,
    channel_index: overrides.channel_index ?? null,
    channel_label: overrides.channel_label ?? null,
    device_id: overrides.device_id ?? null,
    unit_id: overrides.unit_id ?? null,
    unit_online: overrides.unit_online ?? null,
    unit_last_seen_at: overrides.unit_last_seen_at ?? null,
    tested_at: overrides.tested_at ?? null,
  }
}

const debugDocument: Iec61850DebugDocument = {
  stats: {
    sites: 0,
    voltageLevels: 0,
    bays: 0,
    switchgears: 0,
    ieds: 1,
    logicalDevices: 1,
    dataSets: 1,
    reports: 1,
    reportSignals: 1,
  },
  diagnosticSummary: {
    error: 0,
    warning: 0,
    info: 0,
    total: 0,
    rendered: 0,
    omitted: 0,
  },
  diagnostics: [],
  reportCandidates: [],
  signalInventory: {
    dataSetSignals: [
      {
        reference: "LD0/XCBR1.Pos.stVal[ST]",
        dataSetId: "ds-1",
        dataSetRef: "IED1/AP1/LD0/LLN0.dsEvents",
        iedName: "IED1",
        accessPointName: "AP1",
      },
    ],
    reportSignals: [
      {
        reference: "LD0/XCBR1.Pos.stVal[ST]",
        reportControlId: "rcb-1",
        reportControlName: "brcbEvents",
        reportKind: "BRCB",
        dataSetId: "ds-1",
        dataSetRef: "IED1/AP1/LD0/LLN0.dsEvents",
        iedName: "IED1",
        accessPointName: "AP1",
      },
    ],
  },
  treeRows: [],
}

const kintoreDebugDocument: Iec61850DebugDocument = {
  ...debugDocument,
  stats: {
    ...debugDocument.stats,
    ieds: 2,
    reportSignals: 2,
  },
  signalInventory: {
    dataSetSignals: [
      {
        reference: "CTRL1/DisconCSWI2.Pos.Oper.ctlVal[CO]",
        dataSetId: "ds-1",
        dataSetRef: "KINTE15BCU01/AP1/CTRL1/LLN0.dsCommands",
        iedName: "KINTE15BCU01",
        accessPointName: "AP1",
      },
      {
        reference: "CTRL1/DisconCSWI2.Pos.stVal[ST]",
        dataSetId: "ds-1",
        dataSetRef: "KINTE15BCU01/AP1/CTRL1/LLN0.dsCommands",
        iedName: "KINTE15BCU01",
        accessPointName: "AP1",
      },
      {
        reference: "CTRL1/DisconCSWI2.Pos.Oper.ctlVal[CO]",
        dataSetId: "ds-2",
        dataSetRef: "KINTE15BCU02/AP1/CTRL1/LLN0.dsCommands",
        iedName: "KINTE15BCU02",
        accessPointName: "AP1",
      },
    ],
    reportSignals: [
      {
        reference: "CTRL1/DisconCSWI2.Pos.Oper.ctlVal[CO]",
        reportControlId: "rcb-1",
        reportControlName: "brcbCommands",
        reportKind: "BRCB",
        dataSetId: "ds-1",
        dataSetRef: "KINTE15BCU01/AP1/CTRL1/LLN0.dsCommands",
        iedName: "KINTE15BCU01",
        accessPointName: "AP1",
      },
      {
        reference: "CTRL1/DisconCSWI2.Pos.stVal[ST]",
        reportControlId: "rcb-1",
        reportControlName: "brcbCommands",
        reportKind: "BRCB",
        dataSetId: "ds-1",
        dataSetRef: "KINTE15BCU01/AP1/CTRL1/LLN0.dsCommands",
        iedName: "KINTE15BCU01",
        accessPointName: "AP1",
      },
      {
        reference: "CTRL1/DisconCSWI2.Pos.Oper.ctlVal[CO]",
        reportControlId: "rcb-2",
        reportControlName: "brcbCommands",
        reportKind: "BRCB",
        dataSetId: "ds-2",
        dataSetRef: "KINTE15BCU02/AP1/CTRL1/LLN0.dsCommands",
        iedName: "KINTE15BCU02",
        accessPointName: "AP1",
      },
    ],
  },
}

const fullPathNormalizationDocument: Iec61850DebugDocument = {
  ...debugDocument,
  signalInventory: {
    dataSetSignals: [
      {
        reference: "CTRL1/CBCSWI1.Pos.stVal[ST]",
        dataSetId: "ds-1",
        dataSetRef: "SCD_IED_A/AP1/CTRL1/LLN0.dsEvents",
        iedName: "SCD_IED_A",
        accessPointName: "AP1",
      },
      {
        reference: "CTRL1/CBCSWI1.Pos[ST]",
        dataSetId: "ds-2",
        dataSetRef: "SCD_IED_A/AP1/CTRL1/LLN0.dsPositionObject",
        iedName: "SCD_IED_A",
        accessPointName: "AP1",
      },
    ],
    reportSignals: [
      {
        reference: "CTRL1/CBCSWI1.Pos.stVal[ST]",
        reportControlId: "rcb-1",
        reportControlName: "brcbEvents",
        reportKind: "BRCB",
        dataSetId: "ds-1",
        dataSetRef: "SCD_IED_A/AP1/CTRL1/LLN0.dsEvents",
        iedName: "SCD_IED_A",
        accessPointName: "AP1",
      },
      {
        reference: "CTRL1/CBCSWI1.Pos[ST]",
        reportControlId: "rcb-2",
        reportControlName: "brcbPositionObject",
        reportKind: "BRCB",
        dataSetId: "ds-2",
        dataSetRef: "SCD_IED_A/AP1/CTRL1/LLN0.dsPositionObject",
        iedName: "SCD_IED_A",
        accessPointName: "AP1",
      },
    ],
  },
}

describe("mergeIec61850SignalList", () => {
  it("detects the IEC 61850 address column by content and maps signal list rows to reports", () => {
    const rows = [
      buildSignalRow({
        signal_id: 1,
        signal_key: "Q01_POS",
        signal_name: "Q01 position",
        signal_metadata: {
          row: {
            Description: "Q01 position",
            "Imported Column 17": "LD0/XCBR1.Pos.stVal[ST]",
          },
        },
      }),
      buildSignalRow({
        signal_id: 2,
        signal_key: "Q02_POS",
        signal_name: "Q02 position",
        signal_metadata: {
          row: {
            Description: "Q02 position",
            "Imported Column 17": "LD0/XCBR2.Pos.stVal[ST]",
          },
        },
      }),
    ]

    const result = mergeIec61850SignalList(debugDocument, rows, null)

    expect(result.addressColumn).toBe("Imported Column 17")
    expect(result.columnConfidence).toBe("high")
    expect(result.matchedRows).toBe(1)
    expect(result.unmatchedRows).toBe(1)
    expect(result.matches[0]).toMatchObject({
      signalId: 1,
      address: "LD0/XCBR1.Pos.stVal[ST]",
      modelReference: "LD0/XCBR1.Pos.stVal[ST]",
      dataSets: ["IED1/AP1/LD0/LLN0.dsEvents"],
      reports: [
        {
          name: "brcbEvents",
          kind: "BRCB",
          dataSetRef: "IED1/AP1/LD0/LLN0.dsEvents",
          iedName: "IED1",
        },
      ],
      ieds: ["IED1"],
    })
    expect(result.matchedReports).toHaveLength(1)
  })

  it("matches addresses even when the signal list omits functional constraint", () => {
    const result = mergeIec61850SignalList(debugDocument, [
      buildSignalRow({
        signal_metadata: {
          row: {
            Unknown: "XCBR1.Pos.stVal",
          },
        },
      }),
    ], null)

    expect(result.addressColumn).toBe("Unknown")
    expect(result.matchedRows).toBe(1)
  })

  it("does not depend on rendered tree rows for matching", () => {
    const result = mergeIec61850SignalList({
      ...debugDocument,
      treeRows: [
        {
          value: "report-signal:omitted",
          parent: null,
          kind: "report-signal",
          label: "1 more signals not rendered",
          valueLabel: "capped",
          isLeaf: true,
          detail: {
            title: "omitted",
            subtitle: "fixture",
            sections: [],
          },
        },
      ],
    }, [
      buildSignalRow({
        signal_metadata: {
          row: {
            Address: "LD0/XCBR1.Pos.stVal[ST]",
          },
        },
      }),
    ], null)

    expect(result.matchedRows).toBe(1)
    expect(result.matchedReports).toHaveLength(1)
  })

  it("matches full IED-prefixed slash references from imported signal lists", () => {
    const result = mergeIec61850SignalList(kintoreDebugDocument, [
      buildSignalRow({
        signal_id: 10,
        signal_key: "KINT.132KV.15.BCU01.1504.CMD",
        signal_name: "1504 Open",
        signal_metadata: {
          row: {
            "Short IEC address": "CTRL1/DisconCSWI2.Pos.Oper.ctlVal[CO]",
            "Full 61850 Path": "KINTE15BCU01CTRL1/DisconCSWI2/Pos/Oper.ctlVal[CO]",
          },
        },
      }),
      buildSignalRow({
        signal_id: 11,
        signal_key: "KINT.132KV.15.BCU01.1504.POS",
        signal_name: "1504 Position",
        signal_metadata: {
          row: {
            "Short IEC address": "CTRL1/DisconCSWI2.Pos.stVal[ST]",
            "Full 61850 Path": "KINTE15BCU01CTRL1/DisconCSWI2/Pos/stVal[ST]",
          },
        },
      }),
    ], null)

    expect(result.addressColumn).toBe("Full 61850 Path")
    expect(result.matchedRows).toBe(2)
    expect(result.unmatchedRows).toBe(0)
    expect(result.matchedIeds).toEqual(["KINTE15BCU01"])
    expect(result.matchedReports).toEqual([
      {
        name: "brcbCommands",
        kind: "BRCB",
        dataSetRef: "KINTE15BCU01/AP1/CTRL1/LLN0.dsCommands",
        iedName: "KINTE15BCU01",
      },
    ])
  })

  it("uses row IED context when the detected address column contains short references", () => {
    const result = mergeIec61850SignalList(kintoreDebugDocument, [
      buildSignalRow({
        signal_id: 10,
        signal_key: "KINT.132KV.15.BCU01.1504.CMD",
        signal_name: "1504 Open",
        signal_metadata: {
          row: {
            Device: "KINTE15BCU01",
            "61850 Address with Functional Constraint": "CTRL1/DisconCSWI2.Pos.Oper.ctlVal[CO]",
          },
        },
      }),
      buildSignalRow({
        signal_id: 11,
        signal_key: "KINT.132KV.15.BCU01.1504.POS",
        signal_name: "1504 Position",
        signal_metadata: {
          row: {
            Device: "KINTE15BCU01",
            "61850 Address with Functional Constraint": "CTRL1/DisconCSWI2.Pos.stVal[ST]",
          },
        },
      }),
    ], null)

    expect(result.addressColumn).toBe("61850 Address with Functional Constraint")
    expect(result.matchedRows).toBe(2)
    expect(result.unmatchedRows).toBe(0)
    expect(result.matchedIeds).toEqual(["KINTE15BCU01"])
    expect(result.matchedReports).toHaveLength(1)
  })

  it("keeps ambiguous short references matched by aggregating possible SCD candidates", () => {
    const result = mergeIec61850SignalList(kintoreDebugDocument, [
      buildSignalRow({
        signal_id: 10,
        signal_key: "KINT",
        signal_name: "KINT",
        signal_metadata: {
          row: {
            Site: "KINT",
            "61850 Address with Functional Constraint": "CTRL1/DisconCSWI2.Pos.Oper.ctlVal[CO]",
          },
        },
      }),
    ], null)

    expect(result.addressColumn).toBe("61850 Address with Functional Constraint")
    expect(result.matchedRows).toBe(1)
    expect(result.unmatchedRows).toBe(0)
    expect(result.matches[0]?.modelReference).toBe(
      "2 possible SCD signals for CTRL1/DisconCSWI2.Pos.Oper.ctlVal[CO]",
    )
    expect(result.matches[0]?.ieds).toEqual(["KINTE15BCU01", "KINTE15BCU02"])
    expect(result.matchedReports).toHaveLength(2)
  })

  it("matches signal list references that include the IED bang prefix", () => {
    const result = mergeIec61850SignalList(kintoreDebugDocument, [
      buildSignalRow({
        signal_metadata: {
          row: {
            Address: "KINTE15BCU01!KINTE15BCU01CTRL1/DisconCSWI2/Pos/Oper.ctlVal[CO]",
          },
        },
      }),
    ], null)

    expect(result.addressColumn).toBe("Address")
    expect(result.matchedRows).toBe(1)
    expect(result.matchedIeds).toEqual(["KINTE15BCU01"])
  })

  it("normalizes Full 61850 Path by stripping the network device prefix before known logical device", () => {
    const result = mergeIec61850SignalList(fullPathNormalizationDocument, [
      buildSignalRow({
        signal_metadata: {
          row: {
            "Full 61850 Path": "KINTE15BCU01CTRL1/CBCSWI1/Pos/stVal[ST]",
          },
        },
      }),
    ], null)

    expect(result.addressColumn).toBe("Full 61850 Path")
    expect(result.matchedRows).toBe(1)
    expect(result.matches[0]?.modelReference).toBe("CTRL1/CBCSWI1.Pos.stVal[ST]")
    expect(result.matchedReports).toEqual([
      {
        name: "brcbEvents",
        kind: "BRCB",
        dataSetRef: "SCD_IED_A/AP1/CTRL1/LLN0.dsEvents",
        iedName: "SCD_IED_A",
      },
    ])
  })

  it("matches a Full 61850 Path leaf signal to an SCD FCD parent member", () => {
    const result = mergeIec61850SignalList({
      ...fullPathNormalizationDocument,
      signalInventory: {
        dataSetSignals: [fullPathNormalizationDocument.signalInventory.dataSetSignals[1]],
        reportSignals: [fullPathNormalizationDocument.signalInventory.reportSignals[1]],
      },
    }, [
      buildSignalRow({
        signal_metadata: {
          row: {
            "Full 61850 Path": "KINTE15BCU01CTRL1/CBCSWI1/Pos/stVal[ST]",
          },
        },
      }),
    ], null)

    expect(result.matchedRows).toBe(1)
    expect(result.matches[0]?.modelReference).toBe("CTRL1/CBCSWI1.Pos[ST]")
    expect(result.matchedReports).toHaveLength(1)
  })

  it("does not match a CO full path to ST report data through functional-constraint stripping", () => {
    const result = mergeIec61850SignalList(fullPathNormalizationDocument, [
      buildSignalRow({
        signal_metadata: {
          row: {
            "Full 61850 Path": "KINTE15BCU01CTRL1/CBCSWI1/Pos/Oper.ctlVal[CO]",
          },
        },
      }),
    ], null)

    expect(result.addressColumn).toBe("Full 61850 Path")
    expect(result.matchedRows).toBe(0)
    expect(result.unmatchedRows).toBe(1)
    expect(result.matchedReports).toHaveLength(0)
  })
})
