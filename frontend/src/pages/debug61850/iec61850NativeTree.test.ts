import { describe, expect, it } from "vitest"

import type { Iec61850SclImportResponse } from "@/api/iec61850Client.api"
import { buildIec61850NativeTreeDocument } from "./iec61850NativeTree"

describe("buildIec61850NativeTreeDocument", () => {
  it("builds a separate backend compiler tree from native normalized output", () => {
    const response: Iec61850SclImportResponse = {
      import_id: "7",
      workspace_id: 3,
      source_filename: "station.scd",
      source_hash: "abcdef1234567890",
      source_size: 42,
      selected_ied: "IED1",
      normalized_schema: "unitlab.iec61850.scl.normalized.v1",
      normalized_model: {
        logicalDevices: [{ inst: "IED1LD0" }],
        logicalNodes: [{ logicalDeviceInst: "IED1LD0", name: "LLN0" }, { logicalDeviceInst: "IED1LD0", name: "PGGIO1" }],
        dataSets: [{ name: "dsEvents", reference: "IED1/AP1/LD0/LLN0.dsEvents", logicalDeviceInst: "IED1LD0", logicalNodeName: "LLN0", firstSignalIndex: 0, memberCount: 1 }],
        reports: [{ name: "brcbEvents", reportKind: "buffered", isBuffered: true, dataSetRef: "IED1/AP1/LD0/LLN0.dsEvents", dataSetIndex: 0, confRev: 7, triggerOptionsMask: 3, optionalFieldsMask: 159 }],
        signals: [{ reference: "LD0/PGGIO1.Ind1.stVal[ST]", dataSetEntryVariable: "IED1LD0/PGGIO1$ST$Ind1$stVal", objectReference: "IED1LD0.PGGIO1.Ind1.stVal", logicalDeviceInst: "IED1LD0", logicalNodeName: "PGGIO1", dataSetIndex: 0, dataObjectName: "Ind1", dataAttributePath: "stVal", fc: "ST", initialValue: "0" }],
      },
      diagnostics: [{ severity: "warning", code: "SCL_REPORT_DATASET_EMPTY", message: "empty", iedName: "IED1", accessPointName: "AP1", logicalDeviceInst: "LD0", logicalNodeName: "LLN0", dataSetName: "", reportControlName: "urcbC", memberReference: "" }],
    }

    const document = buildIec61850NativeTreeDocument(response)

    expect(document.stats).toMatchObject({ logicalDevices: 1, logicalNodes: 2, dataSets: 1, reports: 1, signals: 1, warnings: 1 })
    expect(document.rows.some(row => row.kind === "dataset-member" && row.detail.subtitle === "IED1LD0/PGGIO1$ST$Ind1$stVal")).toBe(true)
    expect(document.rows.some(row => row.kind === "report-control" && row.label === "brcbEvents" && row.meta === "buffered · 1 leaves")).toBe(true)
    expect(document.rows.some(row => row.kind === "diagnostic" && row.label === "SCL_REPORT_DATASET_EMPTY")).toBe(true)
  })
})
