import { describe, expect, it } from "vitest"

import { generateSldFromScd } from "@/modules/scd-sld-core"
import { adaptSldDocumentToSwitchgearDiagram } from "./switchgearSldImportAdapter"

const genericImportScd = `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" revision="B" version="2007">
  <Substation name="SS1">
    <PowerTransformer name="TR1" type="PTR">
      <TransformerWinding name="W1" type="PTW">
        <Terminal connectivityNode="SS1/VL1/BAY1/CN_TR" name="T1"/>
      </TransformerWinding>
    </PowerTransformer>
    <VoltageLevel name="VL1">
      <Voltage multiplier="k" unit="V">110</Voltage>
      <Bay name="BAY1">
        <ConductingEquipment name="BUS1" type="BBS">
          <Terminal bayName="BAY1" cNodeName="CN_BUS" connectivityNode="SS1/VL1/BAY1/CN_BUS" name="T1" substationName="SS1" voltageLevelName="VL1"/>
        </ConductingEquipment>
        <ConductingEquipment name="Q01" type="CBR">
          <Terminal bayName="BAY1" cNodeName="CN_BUS" connectivityNode="SS1/VL1/BAY1/CN_BUS" name="T1" substationName="SS1" voltageLevelName="VL1"/>
          <Terminal bayName="BAY1" cNodeName="CN_OUT" connectivityNode="SS1/VL1/BAY1/CN_OUT" name="T2" substationName="SS1" voltageLevelName="VL1"/>
        </ConductingEquipment>
        <ConnectivityNode name="CN_BUS" pathName="SS1/VL1/BAY1/CN_BUS"/>
        <ConnectivityNode name="CN_OUT" pathName="SS1/VL1/BAY1/CN_OUT"/>
        <ConnectivityNode name="CN_TR" pathName="SS1/VL1/BAY1/CN_TR"/>
      </Bay>
    </VoltageLevel>
  </Substation>
</SCL>`

describe("switchgear SLD import adapter", () => {
  it("maps generated busbar visuals into bold editor lines without creating switchgear records", () => {
    const generated = generateSldFromScd({
      fileName: "generic-import.scd",
      contentHash: "generic-import",
      xmlText: genericImportScd,
    }, {
      gridSize: 24,
    })

    const adapted = adaptSldDocumentToSwitchgearDiagram(generated.document, {
      stagePadding: 1000,
    })

    const busbarLine = adapted.diagram.edges?.find(edge => edge.id.includes("BUS1"))
    expect(busbarLine).toMatchObject({
      x1: 1096,
      y1: 1120,
      x2: 1240,
      y2: 1120,
      kind: "line",
      weight: "bold",
      startBinding: null,
      endBinding: null,
    })
    expect(adapted.switchgearCandidates).toEqual([
      expect.objectContaining({
        label: "Q01",
        equipmentType: "CBR",
        kind: "breaker",
        switchgearType: "switchgear",
        position: { x: 1168, y: 1168 },
      }),
    ])
    expect(adapted.diagram.layoutById).toEqual({})
  })

  it("keeps transformer symbols and generated connection routes in editor diagram state", () => {
    const generated = generateSldFromScd({
      fileName: "generic-import.scd",
      contentHash: "generic-import",
      xmlText: genericImportScd,
    }, {
      gridSize: 24,
    })

    const adapted = adaptSldDocumentToSwitchgearDiagram(generated.document, {
      stagePadding: 1000,
    })

    expect(adapted.diagram.staticElements).toContainEqual(expect.objectContaining({
      kind: "transformer",
      size: "md",
      x: 1096,
      y: 1456,
      rotation: 0,
    }))
    expect(adapted.diagram.edges).toContainEqual(expect.objectContaining({
      id: expect.stringContaining("CN_BUS"),
      x1: 1168,
      y1: 1120,
      x2: 1168,
      y2: 1144,
      weight: "normal",
    }))
    expect(adapted.diagram.textElements).toEqual(expect.arrayContaining([
      expect.objectContaining({ text: "BUS1" }),
      expect.objectContaining({ text: "TR1" }),
      expect.objectContaining({ text: "Q01" }),
    ]))
    expect(adapted.diagnostics).toEqual([])
  })
})
