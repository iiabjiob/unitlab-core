import { describe, expect, it } from "vitest"

import { generateSldFromScd } from "@/modules/scd-sld-core"
import {
  adaptSldDocumentToSwitchgearDiagram,
  buildSwitchgearCandidateDecisions,
  mergeGeneratedSldDiagramOverlay,
} from "./switchgearSldImportAdapter"

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
    ]))
    expect(adapted.diagram.textElements).not.toContainEqual(expect.objectContaining({ text: "Q01" }))
    expect(adapted.diagnostics).toEqual([])
  })

  it("replaces only previous generated import overlay when applying a new preview", () => {
    const merged = mergeGeneratedSldDiagramOverlay({
      edges: [
        { id: "manual-line", x1: 1, y1: 2, x2: 3, y2: 4, kind: "line", weight: "normal" },
        { id: "sld-import-connection:old", x1: 10, y1: 20, x2: 30, y2: 40, kind: "line", weight: "normal" },
      ],
      staticElements: [
        { id: "manual-static", kind: "ground", size: "md", x: 1, y: 1, rotation: 0 },
        { id: "sld-import-static:old", kind: "transformer", size: "md", x: 2, y: 2, rotation: 0 },
      ],
      textElements: [
        { id: "manual-text", text: "MANUAL", size: "md", x: 1, y: 1 },
        { id: "sld-import-label:old", text: "OLD", size: "md", x: 2, y: 2 },
      ],
      snapEnabled: true,
    }, {
      edges: [
        { id: "sld-import-connection:new", x1: 100, y1: 200, x2: 300, y2: 400, kind: "line", weight: "bold" },
      ],
      staticElements: [
        { id: "sld-import-static:new", kind: "transformer", size: "md", x: 12, y: 12, rotation: 0 },
      ],
      textElements: [
        { id: "sld-import-label:new", text: "NEW", size: "md", x: 24, y: 24 },
      ],
    })

    expect(merged.edges?.map(edge => edge.id)).toEqual(["manual-line", "sld-import-connection:new"])
    expect(merged.staticElements?.map(element => element.id)).toEqual(["manual-static", "sld-import-static:new"])
    expect(merged.textElements?.map(element => element.id)).toEqual(["manual-text", "sld-import-label:new"])
  })

  it("marks existing switchgear candidates for reuse and creates unique names for missing records", () => {
    const decisions = buildSwitchgearCandidateDecisions([
      {
        id: "candidate:existing",
        sourceId: "source/existing",
        sourcePath: "SCL/Substation/SS1/VoltageLevel/VL1/Bay/BAY1/ConductingEquipment/Q01",
        label: "Q01",
        equipmentType: "CBR",
        kind: "breaker",
        switchgearType: "switchgear",
        position: { x: 10, y: 20 },
      },
      {
        id: "candidate:new",
        sourceId: "source/new",
        sourcePath: "SCL/Substation/SS1/VoltageLevel/VL1/Bay/BAY1/ConductingEquipment/Q02",
        label: "Q01",
        equipmentType: "CBR",
        kind: "breaker",
        switchgearType: "switchgear",
        position: { x: 30, y: 40 },
      },
      {
        id: "candidate:disconnector",
        sourceId: "source/disconnector",
        sourcePath: "SCL/Substation/SS1/VoltageLevel/VL1/Bay/BAY1/ConductingEquipment/QB1",
        label: "QB1",
        equipmentType: "DIS",
        kind: "disconnector",
        switchgearType: "disconnector",
        position: { x: 50, y: 60 },
      },
    ], [
      {
        id: 101,
        workspace_ids: [1],
        switchgear_type: "switchgear",
        name: "Q01",
        bindings: [],
      },
    ])

    expect(decisions.map(decision => ({
      label: decision.candidate.label,
      action: decision.action,
      existingSwitchgearId: decision.existingSwitchgearId,
      createName: decision.createName,
    }))).toEqual([
      { label: "Q01", action: "reuse-existing", existingSwitchgearId: 101, createName: "Q01" },
      { label: "Q01", action: "create", existingSwitchgearId: null, createName: "Q01 2" },
      { label: "QB1", action: "create", existingSwitchgearId: null, createName: "QB1" },
    ])
  })
})
