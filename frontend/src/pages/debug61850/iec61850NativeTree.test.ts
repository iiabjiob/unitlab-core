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
        network: {
          connectedAccessPoints: [{
            iedName: "IED1",
            accessPointName: "AP1",
            subNetworkName: "StationBus",
            subNetworkType: "8-MMS",
            ipAddress: "192.168.14.50",
            ipSubnet: "255.255.255.0",
            ipGateway: "192.168.14.1",
            osiApTitle: "1,3,9999,23",
            osiAeQualifier: "23",
            osiPSelector: "00000001",
            osiSSelector: "0001",
            osiTSelector: "0001",
            address: { IP: "192.168.14.50", "IP-SUBNET": "255.255.255.0", "IP-GATEWAY": "192.168.14.1" },
            addressParameters: [{ type: "IP", value: "192.168.14.50" }],
          }],
        },
        logicalDevices: [{ inst: "IED1LD0" }],
        logicalNodes: [{ logicalDeviceInst: "IED1LD0", name: "LLN0" }, { logicalDeviceInst: "IED1LD0", name: "PGGIO1" }],
        dataSets: [{ name: "dsEvents", reference: "IED1/AP1/LD0/LLN0.dsEvents", logicalDeviceInst: "IED1LD0", logicalNodeName: "LLN0", firstSignalIndex: 0, memberCount: 1 }],
        reports: [{
          name: "brcbEvents",
          logicalDeviceInst: "IED1LD0",
          logicalNodeName: "LLN0",
          reportKind: "buffered",
          isBuffered: true,
          rptId: "IED1LD0/LLN0.BR.Events",
          dataSetRef: "IED1/AP1/LD0/LLN0.dsEvents",
          dataSetIndex: 0,
          confRev: 7,
          triggerOptionsMask: 3,
          optionalFieldsMask: 159,
          triggerOptions: { dataChange: "true", qualityChange: "true", dataUpdate: "false", periodic: "false", generalInterrogation: "true" },
          optionalFields: { sequenceNumber: "true", timestamp: "true", reasonCode: "true", dataSetName: "true", dataReference: "true", entryId: "true", configRevision: "true", bufferOverflow: "false" },
        }],
        signals: [{ reference: "LD0/PGGIO1.Ind1.stVal[ST]", dataSetEntryVariable: "IED1LD0/PGGIO1$ST$Ind1$stVal", objectReference: "IED1LD0.PGGIO1.Ind1.stVal", logicalDeviceInst: "IED1LD0", logicalNodeName: "PGGIO1", dataSetIndex: 0, dataObjectName: "Ind1", dataAttributePath: "stVal", fc: "ST", initialValueKind: 3, initialValue: "0" }],
      },
      diagnostics: [{ severity: "warning", code: "SCL_REPORT_DATASET_EMPTY", message: "empty", iedName: "IED1", accessPointName: "AP1", logicalDeviceInst: "LD0", logicalNodeName: "LLN0", dataSetName: "", reportControlName: "urcbC", memberReference: "" }],
    }

    const document = buildIec61850NativeTreeDocument(response)

    expect(document.stats).toMatchObject({ logicalDevices: 1, logicalNodes: 2, dataSets: 1, reports: 1, signals: 1, warnings: 1 })
    expect(document.rows.some(row => row.kind === "dataset-member" && row.detail.subtitle === "IED1LD0/PGGIO1$ST$Ind1$stVal")).toBe(true)
    expect(document.rows.some(row => row.kind === "report-control" && row.label === "brcbEvents" && row.meta === "buffered · dsEvents")).toBe(true)
    expect(document.rows.some(row => row.kind === "dataset-member" && row.runtimeSignal?.valueKind === "integer" && row.runtimeSignal.objectReference === "IED1LD0.PGGIO1.Ind1.stVal")).toBe(true)
    expect(document.rows.some(row => row.kind === "connected-access-point" && row.label === "AP1" && row.meta === "192.168.14.50")).toBe(true)
    expect(document.rows.some(row => row.detail.rows.some(detail => detail.label === "IP-GATEWAY" && detail.value === "192.168.14.1"))).toBe(true)
    expect(document.rows.some(row => row.detail.rows.some(detail => detail.label === "OSI-AP-Title" && detail.value === "1,3,9999,23"))).toBe(true)
    expect(document.rows.some(row => row.detail.rows.some(detail => detail.label === "TrgOps dchg" && detail.value === "true"))).toBe(true)
    expect(document.rows.some(row => row.detail.rows.some(detail => detail.label === "OptFlds dataRef" && detail.value === "true"))).toBe(true)
    expect(document.rows.some(row => row.label === "SCL_REPORT_DATASET_EMPTY")).toBe(false)
  })
})
