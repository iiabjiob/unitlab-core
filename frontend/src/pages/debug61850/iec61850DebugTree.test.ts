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
})
