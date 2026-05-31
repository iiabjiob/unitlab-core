import { describe, expect, it } from "vitest"

import { parseScdSource } from "@/modules/scd-sld-core"
import {
  buildIec61850DebugDocument,
  buildIec61850DebugTreeRows,
} from "./iec61850DebugTree"

const fixtureScd = `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <Substation name="SS1" desc="Generic site">
    <VoltageLevel name="VL1">
      <Voltage multiplier="k" unit="V">110</Voltage>
      <Bay name="BAY1" desc="Generic feeder">
        <ConductingEquipment name="Q01" type="CBR">
          <Terminal connectivityNode="SS1/VL1/BAY1/CN_TOP" name="T1"/>
        </ConductingEquipment>
        <ConductingEquipment name="QB1" type="DIS"/>
        <ConductingEquipment name="M1" type="VTR"/>
        <ConnectivityNode name="CN_TOP" pathName="SS1/VL1/BAY1/CN_TOP"/>
      </Bay>
    </VoltageLevel>
  </Substation>
</SCL>`

const runtimeScd = `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <DataTypeTemplates>
    <LNodeType id="LLN0_TYPE" lnClass="LLN0">
      <DO name="Beh" type="SPS_DO"/>
    </LNodeType>
    <LNodeType id="XCBR_TYPE" lnClass="XCBR">
      <DO name="Pos" type="SPS_DO"/>
    </LNodeType>
    <DOType id="SPS_DO" cdc="SPS">
      <DA name="stVal" bType="BOOLEAN" fc="ST"/>
    </DOType>
  </DataTypeTemplates>
  <Substation name="SS1">
    <VoltageLevel name="VL1">
      <Bay name="BAY1"/>
    </VoltageLevel>
  </Substation>
  <Communication>
    <SubNetwork name="StationBus" type="8-MMS">
      <ConnectedAP iedName="IED1" apName="AP1">
        <Address>
          <P type="IP">192.168.10.21</P>
        </Address>
      </ConnectedAP>
    </SubNetwork>
  </Communication>
  <IED name="IED1" manufacturer="Generic Vendor" type="Generic IED">
    <AccessPoint name="AP1">
      <Server>
        <LDevice inst="LD0">
          <LN0 lnType="LLN0_TYPE">
            <DataSet name="dsEvents">
              <FCDA ldInst="LD0" lnClass="XCBR" lnInst="1" doName="Pos" daName="stVal" fc="ST"/>
            </DataSet>
            <ReportControl name="brcbEvents" rptID="IED1LD0/LLN0.BR.Events" datSet="dsEvents" confRev="3" buffered="true"/>
          </LN0>
          <LN lnClass="XCBR" inst="1" lnType="XCBR_TYPE"/>
        </LDevice>
      </Server>
    </AccessPoint>
  </IED>
</SCL>`

describe("iec61850DebugTree", () => {
  it("builds a Site to VoltageLevel to Bay to Switchgears tree from the normalized SCL model", () => {
    const model = parseScdSource({
      fileName: "fixture.scd",
      contentHash: "fixture",
      xmlText: fixtureScd,
    })

    const rows = buildIec61850DebugTreeRows(model)
    const site = rows.find(row => row.kind === "site")
    const voltageLevel = rows.find(row => row.kind === "voltage-level")
    const bay = rows.find(row => row.kind === "bay")
    const switchgearGroup = rows.find(row => row.kind === "switchgears-group")
    const switchgears = rows.filter(row => row.kind === "switchgear")

    expect(site).toMatchObject({ label: "SS1", parent: null, valueLabel: "1 VL" })
    expect(voltageLevel).toMatchObject({ label: "VL1", parent: site?.value, valueLabel: "110 kV" })
    expect(bay).toMatchObject({ label: "BAY1", parent: voltageLevel?.value, valueLabel: "2 SG" })
    expect(switchgearGroup).toMatchObject({ label: "Switchgears", parent: bay?.value, valueLabel: "2" })
    expect(switchgears.map(row => [row.label, row.valueLabel])).toEqual([
      ["Q01", "CBR"],
      ["QB1", "DIS"],
    ])
  })

  it("exposes useful standard properties in the detail model", () => {
    const model = parseScdSource({
      fileName: "fixture.scd",
      contentHash: "fixture",
      xmlText: fixtureScd,
    })

    const rows = buildIec61850DebugTreeRows(model)
    const breaker = rows.find(row => row.kind === "switchgear" && row.label === "Q01")

    expect(breaker?.detail.subtitle).toBe("SCL ConductingEquipment · breaker")
    expect(breaker?.detail.sections).toContainEqual(expect.objectContaining({
      title: "Identity",
      rows: expect.arrayContaining([
        { label: "Standard element", value: "ConductingEquipment" },
        { label: "type", value: "CBR" },
      ]),
    }))
    expect(breaker?.detail.sections).toContainEqual(expect.objectContaining({
      title: "Terminals",
      rows: expect.arrayContaining([
        { label: "T1 connectivityNode", value: "SS1/VL1/BAY1/CN_TOP" },
      ]),
    }))
  })

  it("adds IED, DataSet, ReportControl and report signal rows", () => {
    const model = parseScdSource({
      fileName: "runtime.scd",
      contentHash: "runtime",
      xmlText: runtimeScd,
    })

    const rows = buildIec61850DebugTreeRows(model)
    const iedsGroup = rows.find(row => row.kind === "ieds-group")
    const ied = rows.find(row => row.kind === "ied")
    const accessPoint = rows.find(row => row.kind === "access-point")
    const logicalDevice = rows.find(row => row.kind === "logical-device")
    const lln0 = rows.find(row => row.kind === "logical-node" && row.label === "LLN0")
    const dataSet = rows.find(row => row.kind === "dataset")
    const reportControl = rows.find(row => row.kind === "report-control")
    const reportSignal = rows.find(row => row.kind === "report-signal")

    expect(iedsGroup).toMatchObject({ label: "IEDs", parent: null, valueLabel: "1" })
    expect(ied).toMatchObject({ label: "IED1", parent: iedsGroup?.value, valueLabel: "1 AP" })
    expect(accessPoint).toMatchObject({ label: "AP1", parent: ied?.value, valueLabel: "Server" })
    expect(logicalDevice).toMatchObject({ label: "LD0", parent: expect.any(String), valueLabel: "2 LN" })
    expect(lln0).toMatchObject({ label: "LLN0", parent: logicalDevice?.value, valueLabel: "1 DS · 1 RCB" })
    expect(dataSet).toMatchObject({ label: "dsEvents", valueLabel: "1 signals" })
    expect(reportControl).toMatchObject({ label: "brcbEvents", valueLabel: "BRCB" })
    expect(reportSignal).toMatchObject({ label: "LD0/XCBR1.Pos.stVal[ST]", valueLabel: "ST" })
    expect(reportControl?.detail.sections).toContainEqual(expect.objectContaining({
      title: "DataSet",
      rows: expect.arrayContaining([
        { label: "DataSet ref", value: "IED1/AP1/LD0/LLN0.dsEvents" },
        { label: "signal count", value: "1" },
      ]),
    }))
    expect(iedsGroup?.detail.sections).toContainEqual(expect.objectContaining({
      title: "Report candidates",
      rows: expect.arrayContaining([
        expect.objectContaining({
          label: "brcbEvents",
          signalCount: 1,
          reportSignalsAction: {
            kind: "report-signals-dialog",
            reportControlValue: "report-control:ied/IED1/accessPoint/AP1/server/lDevice/LD0/ln/LLN0/reportControl/brcbEvents",
            reportKind: "buffered",
            dataSetRef: "IED1/AP1/LD0/LLN0.dsEvents",
            signalCount: 1,
          },
        }),
      ]),
    }))

    expect(accessPoint?.detail.sections).toContainEqual(expect.objectContaining({
      title: "Identity",
      rows: expect.arrayContaining([
        { label: "IP address", value: "192.168.10.21" },
      ]),
    }))

    expect(lln0?.detail.sections).toContainEqual(expect.objectContaining({
      title: "Runtime inventory",
      rows: expect.arrayContaining([
        expect.objectContaining({
          label: "DataSets",
          value: "1",
          action: expect.objectContaining({
            title: "LLN0 DataSets",
            items: [
              expect.objectContaining({
                title: "dsEvents",
                rows: expect.arrayContaining([
                  { label: "signals", value: "1" },
                  { label: "signal 1", value: "LD0/XCBR1.Pos.stVal[ST]" },
                ]),
              }),
            ],
          }),
        }),
        expect.objectContaining({
          label: "ReportControls",
          value: "1",
          action: expect.objectContaining({
            title: "LLN0 ReportControls",
            items: [
              expect.objectContaining({
                title: "brcbEvents",
              }),
            ],
          }),
        }),
      ]),
    }))
  })

  it("builds a bounded debug document for worker transfer", () => {
    const model = parseScdSource({
      fileName: "runtime.scd",
      contentHash: "runtime",
      xmlText: runtimeScd,
    })

    const document = buildIec61850DebugDocument(model, {
      maxSignalRowsPerCollection: 0,
      maxDetailRowsPerSection: 0,
    })

    expect(document.stats).toMatchObject({
      sites: 1,
      voltageLevels: 1,
      bays: 1,
      ieds: 1,
      logicalDevices: 1,
      dataSets: 1,
      reports: 1,
      reportSignals: 1,
    })
    expect(document.diagnostics).toBe(model.diagnostics)
    expect(document.signalInventory.dataSetSignals).toHaveLength(1)
    expect(document.signalInventory.reportSignals).toHaveLength(1)
    expect(document.treeRows.some(row => row.kind === "dataset-member" && row.valueLabel === "capped")).toBe(true)
    expect(document.treeRows.some(row => row.kind === "report-signal" && row.valueLabel === "capped")).toBe(true)
  })

  it("caps debug diagnostics while keeping full severity counts", () => {
    const model = parseScdSource({
      fileName: "runtime.scd",
      contentHash: "runtime",
      xmlText: runtimeScd,
    })
    model.diagnostics.push(
      {
        severity: "warning",
        stage: "parser",
        code: "test.warning",
        message: "warning",
        sourceLocation: { line: 1, column: 1, offset: 0 },
      },
      {
        severity: "error",
        stage: "parser",
        code: "test.error",
        message: "error",
        sourceLocation: { line: 1, column: 1, offset: 0 },
      },
    )

    const document = buildIec61850DebugDocument(model, {
      maxDiagnostics: 1,
    })

    expect(document.diagnostics).toHaveLength(1)
    expect(document.diagnosticSummary).toMatchObject({
      error: 1,
      warning: 1,
      info: 0,
      total: 2,
      rendered: 1,
      omitted: 1,
    })
  })

  it("caps rendered DataSet and report signal rows for large debug views", () => {
    const model = parseScdSource({
      fileName: "runtime.scd",
      contentHash: "runtime",
      xmlText: runtimeScd,
    })

    const rows = buildIec61850DebugTreeRows(model, {
      maxSignalRowsPerCollection: 0,
      maxDetailRowsPerSection: 0,
    })

    expect(rows).toContainEqual(expect.objectContaining({
      kind: "dataset-member",
      label: "1 more signals not rendered",
      valueLabel: "capped",
    }))
    expect(rows).toContainEqual(expect.objectContaining({
      kind: "report-signal",
      label: "1 more signals not rendered",
      valueLabel: "capped",
    }))

    const dataSet = rows.find(row => row.kind === "dataset")
    expect(dataSet?.detail.sections).toContainEqual(expect.objectContaining({
      title: "Signals",
      rows: [
        { label: "omitted", value: "1 rows not rendered in debug view" },
      ],
    }))
  })

  it("caps rendered signal rows globally for worker-sized debug documents", () => {
    const model = parseScdSource({
      fileName: "runtime.scd",
      contentHash: "runtime",
      xmlText: runtimeScd,
    })

    const rows = buildIec61850DebugTreeRows(model, {
      maxTotalSignalRows: 1,
    })

    expect(rows.filter(row => row.kind === "dataset-member" && row.valueLabel !== "capped")).toHaveLength(1)
    expect(rows.filter(row => row.kind === "report-signal" && row.valueLabel !== "capped")).toHaveLength(0)
    expect(rows).toContainEqual(expect.objectContaining({
      kind: "report-signal",
      label: "1 more signals not rendered",
      valueLabel: "capped",
    }))
  })

})
