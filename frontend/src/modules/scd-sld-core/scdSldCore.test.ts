import { describe, expect, it } from "vitest"

import { generateSldFromScd, parseScdSource } from "./index"

const fixtureScd = `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" xmlns:sxy="http://www.iec.ch/61850/2003/SCLcoordinates" revision="B" version="2007">
  <Substation sxy:x="3" sxy:y="-3" desc="Kintore" name="KINT">
    <PowerTransformer sxy:x="157" sxy:y="29" name="SGT3" type="PTR">
      <TransformerWinding name="W1" type="PTW">
        <Terminal bayName="SGT3" cNodeName="CN_PTR_1" connectivityNode="KINT/E/SGT3/CN_PTR_1" substationName="KINT" voltageLevelName="E"/>
      </TransformerWinding>
    </PowerTransformer>
    <VoltageLevel name="E">
      <Bay sxy:x="164" sxy:y="16" desc="Craigiebuckler West" name="XCW">
        <LNode iedName="KINTE15BCU01" ldInst="CTRL1" lnClass="CSWI" lnInst="1" lnType="BCU_CSWI1" prefix="BCU_"/>
        <ConductingEquipment sxy:x="3" sxy:y="4" name="1505" type="CBR">
          <LNode iedName="KINTE15BCU01" ldInst="CTRL1" lnClass="CSWI" lnInst="1" lnType="CBCSWI1" prefix="CB"/>
          <Terminal bayName="XCW" cNodeName="CN_BREAKER_TOP" connectivityNode="KINT/E/XCW/CN_BREAKER_TOP" name="T1" substationName="KINT" voltageLevelName="E"/>
          <Terminal bayName="XCW" cNodeName="CN_BREAKER_BOTTOM" connectivityNode="KINT/E/XCW/CN_BREAKER_BOTTOM" name="T2" substationName="KINT" voltageLevelName="E"/>
        </ConductingEquipment>
        <ConductingEquipment sxy:y="1" name="1504" type="DIS">
          <Terminal bayName="132MainBusbar1" cNodeName="L1" connectivityNode="KINT/E/132MainBusbar1/L1" name="T1" substationName="KINT" voltageLevelName="E"/>
          <Terminal bayName="XCW" cNodeName="CN_BREAKER_TOP" connectivityNode="KINT/E/XCW/CN_BREAKER_TOP" name="T2" substationName="KINT" voltageLevelName="E"/>
        </ConductingEquipment>
        <ConnectivityNode name="CN_BREAKER_TOP" pathName="KINT/E/XCW/CN_BREAKER_TOP"/>
      </Bay>
    </VoltageLevel>
  </Substation>
  <IED desc="Modular Substation / Bay Controller" manufacturer="GE Grid Solutions" type="DS Agile C264" name="KINTE15BCU01"/>
</SCL>`

describe("scd-sld-core", () => {
  it("returns a structured error for empty SCD input", () => {
    const result = generateSldFromScd({
      fileName: "empty.scd",
      contentHash: "empty",
      xmlText: "",
    })

    expect(result.diagnostics).toContainEqual(expect.objectContaining({
      severity: "error",
      code: "xml.empty-source",
    }))
    expect(result.graph.nodes).toEqual([])
    expect(result.document.elements).toEqual([])
  })

  it("parses the initial SCD subset into a normalized model", () => {
    const model = parseScdSource({
      fileName: "fixture.scd",
      contentHash: "fixture",
      xmlText: fixtureScd,
    })

    expect(model.scl).toEqual({ version: "2007", revision: "B" })
    expect(model.substations).toHaveLength(1)
    expect(model.substations[0]).toMatchObject({
      id: "substation/KINT",
      name: "KINT",
      coordinates: { x: 3, y: -3 },
    })

    const voltageLevel = model.substations[0]?.voltageLevels[0]
    expect(voltageLevel).toMatchObject({
      id: "substation/KINT/voltageLevel/E",
      name: "E",
    })

    const bay = voltageLevel?.bays[0]
    expect(bay).toMatchObject({
      id: "substation/KINT/voltageLevel/E/bay/XCW",
      name: "XCW",
      desc: "Craigiebuckler West",
    })

    expect(bay?.equipments.map(item => [item.name, item.type, item.kind])).toEqual([
      ["1505", "CBR", "breaker"],
      ["1504", "DIS", "disconnector"],
    ])
    expect(bay?.equipments[0]?.terminals).toHaveLength(2)
    expect(model.substations[0]?.powerTransformers[0]).toMatchObject({
      name: "SGT3",
      type: "PTR",
      kind: "transformer",
    })
    expect(model.ieds).toEqual([])
  })

  it("builds an electrical graph from parsed topology", () => {
    const result = generateSldFromScd({
      fileName: "fixture.scd",
      contentHash: "fixture",
      xmlText: fixtureScd,
    }, {
      generatedAt: "2026-05-28T00:00:00.000Z",
      gridSize: 24,
    })

    expect(result.graph).toMatchObject({
      schema: "unitlab.scd-sld.electrical-graph",
      sourceHash: "fixture",
    })
    expect(result.graph.nodes.map(item => [item.label, item.kind])).toEqual([
      ["SGT3", "transformer"],
      ["1504", "disconnector"],
      ["1505", "breaker"],
    ])

    const breakerTopEdge = result.graph.edges.find(edge => edge.sourceConnectivityNode === "KINT/E/XCW/CN_BREAKER_TOP")
    expect(breakerTopEdge).toMatchObject({
      kind: "connectivity-node",
      nodeIds: [
        "substation/KINT/voltageLevel/E/bay/XCW/equipment/1504",
        "substation/KINT/voltageLevel/E/bay/XCW/equipment/1505",
      ],
    })
    expect(breakerTopEdge?.portIds).toHaveLength(2)

    const graphNodeIds = new Set(result.graph.nodes.map(node => node.id))
    const graphPortIds = new Set(result.graph.ports.map(port => port.id))
    for (const edge of result.graph.edges) {
      expect(edge.nodeIds.every(nodeId => graphNodeIds.has(nodeId))).toBe(true)
      expect(edge.portIds.every(portId => graphPortIds.has(portId))).toBe(true)
    }
  })

  it("creates a renderer-neutral SLD document from the electrical graph", () => {
    const result = generateSldFromScd({
      fileName: "fixture.scd",
      contentHash: "fixture",
      xmlText: fixtureScd,
    }, {
      generatedAt: "2026-05-28T00:00:00.000Z",
      gridSize: 24,
    })

    expect(result.document).toMatchObject({
      schema: "unitlab.scd-sld.document",
      sourceHash: "fixture",
      generatedAt: "2026-05-28T00:00:00.000Z",
      layoutHints: {
        generatedFrom: "scd",
        gridSize: 24,
      },
    })
    expect(result.document.elements.map(item => [item.label, item.kind])).toEqual([
      ["SGT3", "transformer"],
      ["1504", "disconnector"],
      ["1505", "breaker"],
    ])
    expect(result.document.connections).toContainEqual(expect.objectContaining({
      sourceConnectivityNode: "KINT/E/XCW/CN_BREAKER_TOP",
      portIds: expect.arrayContaining([
        "port/substation/KINT/voltageLevel/E/bay/XCW/equipment/1504/terminal/T2_2",
        "port/substation/KINT/voltageLevel/E/bay/XCW/equipment/1505/terminal/T1_1",
      ]),
      terminalOwnerIds: [
        "substation/KINT/voltageLevel/E/bay/XCW/equipment/1504",
        "substation/KINT/voltageLevel/E/bay/XCW/equipment/1505",
      ],
    }))
    expect(JSON.parse(JSON.stringify(result.document))).toEqual(result.document)
  })

  it("stops after substation topology and leaves later IED payloads to a future metadata slice", () => {
    const model = parseScdSource({
      fileName: "fixture.scd",
      contentHash: "fixture",
      xmlText: fixtureScd,
    })

    expect(model.substations).toHaveLength(1)
    expect(model.ieds).toHaveLength(0)
  })

  it("keeps unresolved terminal topology visible as graph diagnostics", () => {
    const result = generateSldFromScd({
      fileName: "diagnostics.scd",
      contentHash: "diagnostics",
      xmlText: `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <Substation name="SS1">
    <VoltageLevel name="VL1">
      <Bay name="B1">
        <ConductingEquipment name="X1" type="VENDOR_SPECIAL">
          <Terminal name="T1"/>
        </ConductingEquipment>
      </Bay>
    </VoltageLevel>
  </Substation>
</SCL>`,
    })

    expect(result.graph.nodes).toContainEqual(expect.objectContaining({
      label: "X1",
      kind: "unknown",
    }))
    expect(result.diagnostics).toContainEqual(expect.objectContaining({
      stage: "graph",
      code: "graph.unsupported-equipment-kind",
    }))
    expect(result.diagnostics).toContainEqual(expect.objectContaining({
      stage: "graph",
      code: "graph.terminal-missing-connectivity-node",
    }))
  })
})
