import { describe, expect, it } from "vitest"

import { parseScdSource } from "@/modules/scd-sld-core"
import { buildIec61850DebugTreeRows } from "./iec61850DebugTree"

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
  <Substation name="SS1">
    <VoltageLevel name="VL1">
      <Bay name="BAY1"/>
    </VoltageLevel>
  </Substation>
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
  })
})
