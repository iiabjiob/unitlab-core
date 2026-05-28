import { describe, expect, it } from "vitest"

import { generateSldFromScd, parseScdSource } from "./index"

const genericFeederBayScd = `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" xmlns:sxy="http://www.iec.ch/61850/2003/SCLcoordinates" revision="B" version="2007">
  <Substation sxy:x="3" sxy:y="-3" desc="Generic substation" name="SS1">
    <PowerTransformer sxy:x="157" sxy:y="29" name="TR1" type="PTR">
      <TransformerWinding name="W1" type="PTW">
        <Terminal bayName="TR1" cNodeName="CN_TR_1" connectivityNode="SS1/VL1/TR1/CN_TR_1" substationName="SS1" voltageLevelName="VL1"/>
      </TransformerWinding>
    </PowerTransformer>
    <VoltageLevel name="VL1">
      <Bay sxy:x="164" sxy:y="16" desc="Generic feeder bay" name="BAY1">
        <LNode iedName="IED1" ldInst="CTRL1" lnClass="CSWI" lnInst="1" lnType="GENERIC_CSWI1" prefix="BCU_"/>
        <ConductingEquipment sxy:x="3" sxy:y="4" name="Q01" type="CBR">
          <LNode iedName="IED1" ldInst="CTRL1" lnClass="CSWI" lnInst="1" lnType="GENERIC_CBR_CSWI1" prefix="CB"/>
          <Terminal bayName="BAY1" cNodeName="CN_Q01_TOP" connectivityNode="SS1/VL1/BAY1/CN_Q01_TOP" name="T1" substationName="SS1" voltageLevelName="VL1"/>
          <Terminal bayName="BAY1" cNodeName="CN_Q01_BOTTOM" connectivityNode="SS1/VL1/BAY1/CN_Q01_BOTTOM" name="T2" substationName="SS1" voltageLevelName="VL1"/>
        </ConductingEquipment>
        <ConductingEquipment sxy:y="1" name="QB1" type="DIS">
          <Terminal bayName="BUS1" cNodeName="L1" connectivityNode="SS1/VL1/BUS1/L1" name="T1" substationName="SS1" voltageLevelName="VL1"/>
          <Terminal bayName="BAY1" cNodeName="CN_Q01_TOP" connectivityNode="SS1/VL1/BAY1/CN_Q01_TOP" name="T2" substationName="SS1" voltageLevelName="VL1"/>
        </ConductingEquipment>
        <ConnectivityNode name="CN_Q01_TOP" pathName="SS1/VL1/BAY1/CN_Q01_TOP"/>
      </Bay>
    </VoltageLevel>
  </Substation>
  <IED desc="Generic bay controller" manufacturer="Generic Vendor" type="Generic IED" name="IED1"/>
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
      xmlText: genericFeederBayScd,
    })

    expect(model.scl).toEqual({ version: "2007", revision: "B" })
    expect(model.substations).toHaveLength(1)
    expect(model.substations[0]).toMatchObject({
      id: "substation/SS1",
      name: "SS1",
      coordinates: { x: 3, y: -3 },
    })

    const voltageLevel = model.substations[0]?.voltageLevels[0]
    expect(voltageLevel).toMatchObject({
      id: "substation/SS1/voltageLevel/VL1",
      name: "VL1",
    })

    const bay = voltageLevel?.bays[0]
    expect(bay).toMatchObject({
      id: "substation/SS1/voltageLevel/VL1/bay/BAY1",
      name: "BAY1",
      desc: "Generic feeder bay",
    })

    expect(bay?.equipments.map(item => [item.name, item.type, item.kind])).toEqual([
      ["Q01", "CBR", "breaker"],
      ["QB1", "DIS", "disconnector"],
    ])
    expect(bay?.equipments[0]?.terminals).toHaveLength(2)
    expect(model.substations[0]?.powerTransformers[0]).toMatchObject({
      name: "TR1",
      type: "PTR",
      kind: "transformer",
    })
    expect(model.ieds).toEqual([])
  })

  it("builds an electrical graph from parsed topology", () => {
    const result = generateSldFromScd({
      fileName: "fixture.scd",
      contentHash: "fixture",
      xmlText: genericFeederBayScd,
    }, {
      generatedAt: "2026-05-28T00:00:00.000Z",
      gridSize: 24,
    })

    expect(result.graph).toMatchObject({
      schema: "unitlab.scd-sld.electrical-graph",
      sourceHash: "fixture",
    })
    expect(result.graph.nodes.map(item => [item.label, item.kind])).toEqual([
      ["TR1", "transformer"],
      ["Q01", "breaker"],
      ["QB1", "disconnector"],
    ])

    const breakerTopEdge = result.graph.edges.find(edge => edge.sourceConnectivityNode === "SS1/VL1/BAY1/CN_Q01_TOP")
    expect(breakerTopEdge).toMatchObject({
      kind: "connectivity-node",
      nodeIds: [
        "substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/Q01",
        "substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/QB1",
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
      xmlText: genericFeederBayScd,
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
      ["TR1", "transformer"],
      ["Q01", "breaker"],
      ["QB1", "disconnector"],
    ])
    expect(result.document.connections).toContainEqual(expect.objectContaining({
      sourceConnectivityNode: "SS1/VL1/BAY1/CN_Q01_TOP",
      portIds: expect.arrayContaining([
        "port/substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/Q01/terminal/T1_1",
        "port/substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/QB1/terminal/T2_2",
      ]),
      terminalOwnerIds: [
        "substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/Q01",
        "substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/QB1",
      ],
    }))
    expect(JSON.parse(JSON.stringify(result.document))).toEqual(result.document)
  })

  it("stops after substation topology and leaves later IED payloads to a future metadata slice", () => {
    const model = parseScdSource({
      fileName: "fixture.scd",
      contentHash: "fixture",
      xmlText: genericFeederBayScd,
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
