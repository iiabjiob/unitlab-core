import { describe, expect, it } from "vitest"

import {
  buildElectricalGraph,
  buildSldCellModel,
  createFlatSldDocument,
  createFlatSldDocumentFromGraph,
  generateSldFromScd,
  layoutSldDocument,
  parseScdSource,
} from "./index"
import type { ScdDiagnostic } from "./types"
import { scanXmlElements } from "./xmlScanner"

const genericFeederBayScd = `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" xmlns:sxy="http://www.iec.ch/61850/2003/SCLcoordinates" revision="B" version="2007">
  <Substation sxy:x="3" sxy:y="-3" desc="Generic substation" name="SS1">
    <PowerTransformer sxy:x="157" sxy:y="29" name="TR1" type="PTR">
      <TransformerWinding name="W1" type="PTW">
        <Terminal bayName="TR1" cNodeName="CN_TR_1" connectivityNode="SS1/VL1/TR1/CN_TR_1" substationName="SS1" voltageLevelName="VL1"/>
      </TransformerWinding>
    </PowerTransformer>
    <VoltageLevel name="VL1">
      <Voltage multiplier="k" unit="V">110</Voltage>
      <ConnectivityNode name="CN_TR_1" pathName="SS1/VL1/TR1/CN_TR_1"/>
      <ConnectivityNode name="L1" pathName="SS1/VL1/BUS1/L1"/>
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
        <ConnectivityNode name="CN_Q01_BOTTOM" pathName="SS1/VL1/BAY1/CN_Q01_BOTTOM"/>
      </Bay>
    </VoltageLevel>
  </Substation>
  <IED desc="Generic bay controller" manufacturer="Generic Vendor" type="Generic IED" name="IED1"/>
</SCL>`

const genericBusbarBayScd = `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" revision="B" version="2007">
  <Substation name="SS1">
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
      </Bay>
    </VoltageLevel>
  </Substation>
</SCL>`

const genericStandardFeederCellScd = `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" xmlns:sxy="http://www.iec.ch/61850/2003/SCLcoordinates" revision="B" version="2007">
  <Substation name="SS1">
    <VoltageLevel name="VL1">
      <Voltage multiplier="k" unit="V">110</Voltage>
      <Bay name="BUS1">
        <ConnectivityNode name="L1" pathName="SS1/VL1/BUS1/L1"/>
      </Bay>
      <Bay sxy:x="10" name="BUS2">
        <ConnectivityNode name="L1" pathName="SS1/VL1/BUS2/L1"/>
      </Bay>
      <Bay name="BAY1">
        <ConductingEquipment sxy:y="1" name="QB1" type="DIS">
          <Terminal bayName="BUS1" cNodeName="L1" connectivityNode="SS1/VL1/BUS1/L1" name="T1" substationName="SS1" voltageLevelName="VL1"/>
          <Terminal bayName="BAY1" cNodeName="CN_BUS" connectivityNode="SS1/VL1/BAY1/CN_BUS" name="T2" substationName="SS1" voltageLevelName="VL1"/>
        </ConductingEquipment>
        <ConductingEquipment sxy:x="5" sxy:y="1" name="QB2" type="DIS">
          <Terminal bayName="BUS2" cNodeName="L1" connectivityNode="SS1/VL1/BUS2/L1" name="T1" substationName="SS1" voltageLevelName="VL1"/>
          <Terminal bayName="BAY1" cNodeName="CN_BUS" connectivityNode="SS1/VL1/BAY1/CN_BUS" name="T2" substationName="SS1" voltageLevelName="VL1"/>
        </ConductingEquipment>
        <ConductingEquipment sxy:x="2" name="QBE1" type="DIS">
          <Terminal bayName="BAY1" cNodeName="grounded" connectivityNode="SS1/VL1/BAY1/grounded" name="T1" substationName="SS1" voltageLevelName="VL1"/>
          <Terminal bayName="BUS1" cNodeName="L1" connectivityNode="SS1/VL1/BUS1/L1" name="T2" substationName="SS1" voltageLevelName="VL1"/>
        </ConductingEquipment>
        <ConductingEquipment sxy:x="7" name="QBE2" type="DIS">
          <Terminal bayName="BAY1" cNodeName="grounded" connectivityNode="SS1/VL1/BAY1/grounded" name="T1" substationName="SS1" voltageLevelName="VL1"/>
          <Terminal bayName="BUS2" cNodeName="L1" connectivityNode="SS1/VL1/BUS2/L1" name="T2" substationName="SS1" voltageLevelName="VL1"/>
        </ConductingEquipment>
        <ConductingEquipment sxy:x="3" sxy:y="4" name="Q01" type="CBR">
          <Terminal bayName="BAY1" cNodeName="CN_TOP" connectivityNode="SS1/VL1/BAY1/CN_TOP" name="T1" substationName="SS1" voltageLevelName="VL1"/>
          <Terminal bayName="BAY1" cNodeName="CN_BUS" connectivityNode="SS1/VL1/BAY1/CN_BUS" name="T2" substationName="SS1" voltageLevelName="VL1"/>
        </ConductingEquipment>
        <ConductingEquipment sxy:x="3" sxy:y="7" name="QS1" type="DIS">
          <Terminal bayName="BAY1" cNodeName="CN_FEEDER" connectivityNode="SS1/VL1/BAY1/CN_FEEDER" name="T1" substationName="SS1" voltageLevelName="VL1"/>
          <Terminal bayName="BAY1" cNodeName="CN_TOP" connectivityNode="SS1/VL1/BAY1/CN_TOP" name="T2" substationName="SS1" voltageLevelName="VL1"/>
        </ConductingEquipment>
        <ConductingEquipment sxy:x="3" sxy:y="12" name="FEEDER" type="IFL">
          <Terminal bayName="BAY1" cNodeName="CN_FEEDER" connectivityNode="SS1/VL1/BAY1/CN_FEEDER" name="T1" substationName="SS1" voltageLevelName="VL1"/>
        </ConductingEquipment>
        <ConnectivityNode name="grounded" pathName="SS1/VL1/BAY1/grounded"/>
        <ConnectivityNode name="CN_BUS" pathName="SS1/VL1/BAY1/CN_BUS"/>
        <ConnectivityNode name="CN_TOP" pathName="SS1/VL1/BAY1/CN_TOP"/>
        <ConnectivityNode name="CN_FEEDER" pathName="SS1/VL1/BAY1/CN_FEEDER"/>
      </Bay>
    </VoltageLevel>
  </Substation>
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
    expect(result.cellModel.voltageLevels).toEqual([])
    expect(result.cellModel.orphanNodes).toEqual([])
    expect(result.document.elements).toEqual([])
  })

  it("preserves diagnostics through pipeline stages and deduplicates the public result list", () => {
    const result = generateSldFromScd({
      fileName: "empty.scd",
      contentHash: "empty",
      xmlText: "",
    })

    for (const diagnostics of [
      result.model.diagnostics,
      result.graph.diagnostics,
      result.cellModel.diagnostics,
      result.document.diagnostics,
    ]) {
      expect(diagnostics).toContainEqual(expect.objectContaining({
        severity: "error",
        stage: "xml",
        code: "xml.empty-source",
      }))
    }

    expect(result.diagnostics.filter(diagnostic => diagnostic.code === "xml.empty-source")).toHaveLength(1)
  })

  it("uses the full graph to cell model to layout pipeline for production generation", () => {
    const source = {
      fileName: "standard-feeder.scd",
      contentHash: "standard-feeder",
      xmlText: genericStandardFeederCellScd,
    }
    const options = {
      generatedAt: "2026-05-28T00:00:00.000Z",
      gridSize: 24,
    }

    const result = generateSldFromScd(source, options)
    const model = parseScdSource(source)
    const graph = buildElectricalGraph(model)
    const cellModel = buildSldCellModel(graph)
    const document = layoutSldDocument(cellModel, graph, options)

    expect(result.model).toEqual(model)
    expect(result.graph).toEqual(graph)
    expect(result.cellModel).toEqual(cellModel)
    expect(result.document).toEqual(document)
    expect(result.document.layoutHints.generatedFrom).toBe("scd")

    const flatDocument = createFlatSldDocumentFromGraph(graph, options)
    expect(createFlatSldDocument(model, options)).toEqual(flatDocument)
    expect(flatDocument.layoutHints.generatedFrom).toBe("scd-flat-debug")
    expect(flatDocument.diagnostics).toEqual(graph.diagnostics)

    const productionBreaker = result.document.elements.find(element => element.label === "Q01")
    const flatBreaker = flatDocument.elements.find(element => element.label === "Q01")
    expect(result.cellModel.voltageLevels[0]?.bayCells.find(cell => cell.name === "BAY1")?.cellType).toBe("feeder")
    expect(productionBreaker?.position).toEqual({ x: 216, y: 216 })
    expect(flatBreaker?.position).toEqual({ x: 3, y: 4 })
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
      voltage: {
        value: "110",
        multiplier: "k",
        unit: "V",
      },
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
    expect(bay?.equipments[0]?.terminals[0]).toMatchObject({
      connectivityNode: "SS1/VL1/BAY1/CN_Q01_TOP",
      resolvedConnectivityNodeId: "substation/SS1/voltageLevel/VL1/bay/BAY1/connectivityNode/CN_Q01_TOP",
      resolvedConnectivityNodePath: "SS1/VL1/BAY1/CN_Q01_TOP",
    })
    expect(model.substations[0]?.powerTransformers[0]).toMatchObject({
      name: "TR1",
      type: "PTR",
      kind: "transformer",
    })
    expect(model.ieds).toEqual([])
  })

  it("scans SCD topology tags through the lightweight XML boundary", () => {
    const diagnostics: ScdDiagnostic[] = []
    const events = [...scanXmlElements(`<?xml version="1.0"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL" xmlns:sxy="http://www.iec.ch/61850/2003/SCLcoordinates">
  <!-- ignored by the SCD topology scanner -->
  <Substation name="SS1" sxy:x="3">
    <VoltageLevel name="VL1"><Voltage multiplier="k" unit="V">110</Voltage></VoltageLevel>
  </Substation>
</SCL>`, diagnostics)]

    expect(diagnostics).toEqual([])
    expect(events.find(event => event.localName === "Substation")).toMatchObject({
      name: "Substation",
      sourceLocation: {
        line: 4,
        column: 3,
      },
      attributes: {
        name: "SS1",
        "sxy:x": "3",
      },
    })
    expect(events.find(event => event.localName === "Voltage")).toMatchObject({
      textContent: "110",
      sourcePath: "SCL:#1/Substation:SS1/VoltageLevel:VL1/Voltage:#1",
    })
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
      ["BUS1", "busbar"],
    ])
    expect(result.graph.groups).toContainEqual(expect.objectContaining({
      id: "group/substation/SS1/voltageLevel/VL1",
      kind: "voltage-level",
      label: "110 kV",
    }))

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

  it("builds a deterministic SLD cell model from graph groups", () => {
    const result = generateSldFromScd({
      fileName: "fixture.scd",
      contentHash: "fixture",
      xmlText: genericFeederBayScd,
    })

    expect(result.cellModel).toMatchObject({
      schema: "unitlab.scd-sld.cell-model",
      sourceHash: "fixture",
      layoutPolicy: {
        orientation: "horizontal-voltage-levels",
        bayOrder: "name-then-id",
        nodeOrder: "role-then-label",
      },
    })
    expect(result.cellModel.voltageLevels).toHaveLength(1)
    expect(result.cellModel.voltageLevels[0]).toMatchObject({
      groupId: "group/substation/SS1/voltageLevel/VL1",
      label: "110 kV",
      orderIndex: 0,
    })
    expect(result.cellModel.voltageLevels[0]?.bayCells).toHaveLength(1)
    expect(result.cellModel.voltageLevels[0]?.bayCells[0]).toMatchObject({
      groupId: "group/substation/SS1/voltageLevel/VL1/bay/BAY1",
      nodeIds: [
        "substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/QB1",
        "substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/Q01",
      ],
      switchgearNodeIds: [
        "substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/QB1",
        "substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/Q01",
      ],
    })
    expect(result.cellModel.voltageLevels[0]?.bayCells[0]?.nodes.map(node => [node.label, node.role, node.orderIndex])).toEqual([
      ["QB1", "switchgear", 0],
      ["Q01", "switchgear", 1],
    ])
    expect(result.cellModel.voltageLevels[0]?.ungroupedNodes.map(node => [node.label, node.role, node.generated])).toEqual([
      ["BUS1", "busbar", true],
    ])
    expect(result.cellModel.orphanNodes.map(node => [node.label, node.role])).toEqual([
      ["TR1", "transformer"],
    ])
    expect(JSON.parse(JSON.stringify(result.cellModel))).toEqual(result.cellModel)
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
      ["BUS1", "busbar"],
    ])
    expect(result.document.elements.map(item => [item.label, item.position])).toEqual([
      ["TR1", { x: 7632, y: 1464 }],
      ["Q01", { x: 8112, y: 1032 }],
      ["QB1", { x: 7968, y: 888 }],
      ["BUS1", { x: 4200, y: 72 }],
    ])
    for (const element of result.document.elements) {
      expect(element.position.x).not.toBeNull()
      expect(element.position.y).not.toBeNull()
      expect((element.position.x ?? 0) % 24).toBe(0)
      expect((element.position.y ?? 0) % 24).toBe(0)
    }
    const breakerTopConnection = result.document.connections.find(connection => (
      connection.sourceConnectivityNode === "SS1/VL1/BAY1/CN_Q01_TOP"
    ))
    expect(breakerTopConnection).toEqual(expect.objectContaining({
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
    expect(breakerTopConnection?.route).toEqual({
      kind: "orthogonal-star",
      anchor: { x: 8040, y: 960 },
      segments: [
        {
          terminalOwnerId: "substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/Q01",
          points: [
            { x: 8112, y: 1032 },
            { x: 8040, y: 1032 },
            { x: 8040, y: 960 },
          ],
        },
        {
          terminalOwnerId: "substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/QB1",
          points: [
            { x: 7968, y: 888 },
            { x: 8040, y: 888 },
            { x: 8040, y: 960 },
          ],
        },
      ],
    })
    const busbarConnection = result.document.connections.find(connection => (
      connection.sourceConnectivityNode === "SS1/VL1/BUS1/L1"
    ))
    expect(busbarConnection?.terminalOwnerIds).toEqual([
      "substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/QB1",
      "substation/SS1/voltageLevel/VL1/connectivityNode/L1/busbar",
    ])
    expect(busbarConnection?.route).toEqual({
      kind: "orthogonal-star",
      anchor: { x: 4200, y: 72 },
      segments: [
        {
          terminalOwnerId: "substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/QB1",
          points: [
            { x: 7968, y: 888 },
            { x: 7968, y: 72 },
          ],
        },
      ],
    })
    for (const connection of result.document.connections) {
      for (const segment of connection.route?.segments ?? []) {
        for (const point of segment.points) {
          expect(point.x % 24).toBe(0)
          expect(point.y % 24).toBe(0)
        }
      }
    }
    expect(JSON.parse(JSON.stringify(result.document))).toEqual(result.document)
  })

  it("marks BBS equipment with renderer-neutral busbar visual metadata", () => {
    const result = generateSldFromScd({
      fileName: "busbar.scd",
      contentHash: "busbar",
      xmlText: genericBusbarBayScd,
    })

    const busbar = result.document.elements.find(element => element.label === "BUS1")
    const breaker = result.document.elements.find(element => element.label === "Q01")

    expect(busbar).toMatchObject({
      kind: "busbar",
      equipmentType: "BBS",
      visual: {
        representation: "busbar",
        orientation: "horizontal",
        strokeWeight: "bold",
        dimensions: {
          width: 144,
          height: 24,
        },
      },
    })
    expect(breaker?.visual).toEqual({
      representation: "symbol",
      orientation: null,
      strokeWeight: "normal",
      dimensions: null,
    })
    expect(result.cellModel.voltageLevels[0]?.bayCells[0]?.busbarNodeIds).toEqual([
      "substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/BUS1",
    ])
    expect(JSON.parse(JSON.stringify(result.document))).toEqual(result.document)
  })

  it("lays out a feeder bay as a reusable standard cell template", () => {
    const result = generateSldFromScd({
      fileName: "standard-feeder.scd",
      contentHash: "standard-feeder",
      xmlText: genericStandardFeederCellScd,
    }, {
      gridSize: 24,
    })

    const feederCell = result.cellModel.voltageLevels[0]?.bayCells.find(cell => cell.name === "BAY1")
    expect(feederCell).toMatchObject({
      cellType: "feeder",
    })
    expect(feederCell?.nodes.filter(node => node.grounded).map(node => node.label)).toEqual([
      "QBE1",
      "QBE2",
    ])

    const elementsByLabel = new Map(result.document.elements.map(element => [element.label, element]))
    expect(elementsByLabel.get("FEEDER")?.position).toEqual({ x: 216, y: 72 })
    expect(elementsByLabel.get("QS1")?.position).toEqual({ x: 216, y: 144 })
    expect(elementsByLabel.get("Q01")?.position).toEqual({ x: 216, y: 216 })
    expect(elementsByLabel.get("QB1")?.position).toEqual({ x: 96, y: 312 })
    expect(elementsByLabel.get("QB2")?.position).toEqual({ x: 336, y: 312 })
    expect(elementsByLabel.get("QBE1")?.position).toEqual({ x: 144, y: 360 })
    expect(elementsByLabel.get("QBE2")?.position).toEqual({ x: 384, y: 360 })

    const feederConnection = result.document.connections.find(connection => (
      connection.sourceConnectivityNode === "SS1/VL1/BAY1/CN_FEEDER"
    ))
    expect(feederConnection?.route?.segments.find(segment => (
      segment.terminalOwnerId === "substation/SS1/voltageLevel/VL1/bay/BAY1/equipment/FEEDER"
    ))?.points).toEqual([
      { x: 216, y: 120 },
      { x: 216, y: 72 },
    ])
    expect(result.document.connections.some(connection => connection.sourceConnectivityNode.includes("ground"))).toBe(false)
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

  it("resolves terminal connectivity from standard cNodeName hierarchy attributes", () => {
    const result = generateSldFromScd({
      fileName: "resolved-cnode.scd",
      contentHash: "resolved-cnode",
      xmlText: `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <Substation name="SS1">
    <VoltageLevel name="VL1">
      <Bay name="B1">
        <ConnectivityNode name="CN_A"/>
        <ConductingEquipment name="Q01" type="CBR">
          <Terminal cNodeName="CN_A" name="T1" substationName="SS1" voltageLevelName="VL1" bayName="B1"/>
        </ConductingEquipment>
      </Bay>
    </VoltageLevel>
  </Substation>
</SCL>`,
    })

    const terminal = result.model.substations[0]?.voltageLevels[0]?.bays[0]?.equipments[0]?.terminals[0]
    expect(terminal).toMatchObject({
      resolvedConnectivityNodeId: "substation/SS1/voltageLevel/VL1/bay/B1/connectivityNode/CN_A",
      resolvedConnectivityNodePath: "SS1/VL1/B1/CN_A",
    })
    expect(result.graph.edges).toContainEqual(expect.objectContaining({
      sourceConnectivityNode: "SS1/VL1/B1/CN_A",
      portIds: [
        "port/substation/SS1/voltageLevel/VL1/bay/B1/equipment/Q01/terminal/T1_1",
      ],
    }))
    expect(result.diagnostics).not.toContainEqual(expect.objectContaining({
      code: "normalizer.unresolved-connectivity-node",
    }))
    expect(result.diagnostics).not.toContainEqual(expect.objectContaining({
      code: "graph.terminal-missing-connectivity-node",
    }))
  })

  it("falls back to scoped cNodeName when a terminal path differs from the declared node pathName", () => {
    const result = generateSldFromScd({
      fileName: "scoped-cnode-fallback.scd",
      contentHash: "scoped-cnode-fallback",
      xmlText: `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <Substation name="SS1">
    <VoltageLevel name="VL1">
      <Bay name="TR1">
        <ConductingEquipment name="Q01" type="CBR">
          <Terminal bayName="TR1" cNodeName="CN_REMOTE" connectivityNode="SS1/VL1/TR1/CN_REMOTE" name="T1" substationName="SS1" voltageLevelName="VL1"/>
        </ConductingEquipment>
        <ConnectivityNode name="CN_REMOTE" pathName="SS1/REMOTE/CN_REMOTE"/>
      </Bay>
    </VoltageLevel>
  </Substation>
</SCL>`,
    })

    const terminal = result.model.substations[0]?.voltageLevels[0]?.bays[0]?.equipments[0]?.terminals[0]
    expect(terminal).toMatchObject({
      resolvedConnectivityNodeId: "substation/SS1/voltageLevel/VL1/bay/TR1/connectivityNode/CN_REMOTE",
      resolvedConnectivityNodePath: "SS1/REMOTE/CN_REMOTE",
    })
    expect(result.graph.edges).toContainEqual(expect.objectContaining({
      sourceConnectivityNode: "SS1/REMOTE/CN_REMOTE",
    }))
    expect(result.diagnostics).not.toContainEqual(expect.objectContaining({
      code: "normalizer.unresolved-connectivity-node",
    }))
  })

  it("resolves copied bay terminal aliases when the target connectivity node is unambiguous", () => {
    const result = generateSldFromScd({
      fileName: "copied-bay-alias.scd",
      contentHash: "copied-bay-alias",
      xmlText: `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <Substation name="SS1">
    <PowerTransformer name="TR1" type="PTR">
      <Terminal connectivityNode="SS1/Copy_1_TR1/CN_REMOTE" name="TW"/>
    </PowerTransformer>
    <VoltageLevel name="VL1">
      <Bay name="TR1">
        <ConnectivityNode name="CN_REMOTE" pathName="SS1/ALT/CN_REMOTE"/>
      </Bay>
    </VoltageLevel>
  </Substation>
</SCL>`,
    })

    const terminal = result.model.substations[0]?.powerTransformers[0]?.terminals[0]
    expect(terminal).toMatchObject({
      resolvedConnectivityNodeId: "substation/SS1/voltageLevel/VL1/bay/TR1/connectivityNode/CN_REMOTE",
      resolvedConnectivityNodePath: "SS1/ALT/CN_REMOTE",
    })
    expect(result.diagnostics).not.toContainEqual(expect.objectContaining({
      code: "normalizer.unresolved-connectivity-node",
    }))
  })

  it("reports terminal references to undeclared connectivity nodes", () => {
    const result = generateSldFromScd({
      fileName: "undeclared-cnode.scd",
      contentHash: "undeclared-cnode",
      xmlText: `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <Substation name="SS1">
    <VoltageLevel name="VL1">
      <Bay name="B1">
        <ConductingEquipment name="Q01" type="CBR">
          <Terminal connectivityNode="SS1/VL1/B1/CN_MISSING" name="T1"/>
        </ConductingEquipment>
      </Bay>
    </VoltageLevel>
  </Substation>
</SCL>`,
    })

    expect(result.model.substations[0]?.voltageLevels[0]?.bays[0]?.equipments[0]?.terminals[0]).toMatchObject({
      resolvedConnectivityNodeId: null,
      resolvedConnectivityNodePath: "SS1/VL1/B1/CN_MISSING",
    })
    expect(result.diagnostics).toContainEqual(expect.objectContaining({
      severity: "warning",
      stage: "normalizer",
      code: "normalizer.unresolved-connectivity-node",
      sourceLocation: expect.objectContaining({
        line: 7,
      }),
    }))
    expect(result.graph.junctions).toContainEqual(expect.objectContaining({
      pathName: "SS1/VL1/B1/CN_MISSING",
    }))
  })

  it("disambiguates duplicate normalized ids within the same parent scope", () => {
    const result = generateSldFromScd({
      fileName: "duplicate-ids.scd",
      contentHash: "duplicate-ids",
      xmlText: `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <Substation name="SS1">
    <VoltageLevel name="VL1">
      <Bay name="B1">
        <ConductingEquipment name="Q01" type="CBR"/>
        <ConductingEquipment name="Q01" type="DIS"/>
      </Bay>
      <Bay name="B1"/>
    </VoltageLevel>
  </Substation>
</SCL>`,
    })

    const voltageLevel = result.model.substations[0]?.voltageLevels[0]
    expect(voltageLevel?.bays.map(bay => bay.id)).toEqual([
      "substation/SS1/voltageLevel/VL1/bay/B1",
      "substation/SS1/voltageLevel/VL1/bay/B1__2",
    ])
    expect(voltageLevel?.bays[0]?.equipments.map(item => item.id)).toEqual([
      "substation/SS1/voltageLevel/VL1/bay/B1/equipment/Q01",
      "substation/SS1/voltageLevel/VL1/bay/B1/equipment/Q01__2",
    ])
    expect(result.graph.nodes.map(node => node.id)).toEqual([
      "substation/SS1/voltageLevel/VL1/bay/B1/equipment/Q01",
      "substation/SS1/voltageLevel/VL1/bay/B1/equipment/Q01__2",
    ])
    expect(result.diagnostics).toContainEqual(expect.objectContaining({
      severity: "warning",
      stage: "normalizer",
      code: "normalizer.duplicate-normalized-id",
      sourceId: "substation/SS1/voltageLevel/VL1/bay/B1/equipment/Q01__2",
    }))
    expect(result.diagnostics).toContainEqual(expect.objectContaining({
      severity: "warning",
      stage: "normalizer",
      code: "normalizer.duplicate-normalized-id",
      sourceId: "substation/SS1/voltageLevel/VL1/bay/B1__2",
    }))
  })

  it("keeps the first standard Voltage element and reports duplicates", () => {
    const result = generateSldFromScd({
      fileName: "duplicate-voltage.scd",
      contentHash: "duplicate-voltage",
      xmlText: `<?xml version="1.0" encoding="UTF-8"?>
<SCL xmlns="http://www.iec.ch/61850/2003/SCL">
  <Substation name="SS1">
    <VoltageLevel name="VL1">
      <Voltage multiplier="k" unit="V">110</Voltage>
      <Voltage multiplier="k" unit="V">220</Voltage>
      <Bay name="B1"/>
    </VoltageLevel>
  </Substation>
</SCL>`,
    })

    expect(result.model.substations[0]?.voltageLevels[0]?.voltage).toMatchObject({
      value: "110",
      multiplier: "k",
      unit: "V",
    })
    expect(result.diagnostics).toContainEqual(expect.objectContaining({
      severity: "warning",
      stage: "parser",
      code: "parser.duplicate-voltage",
      sourceLocation: expect.objectContaining({
        line: 6,
      }),
    }))
  })
})
