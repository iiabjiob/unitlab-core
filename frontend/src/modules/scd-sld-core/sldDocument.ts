import type {
  ElectricalGraph,
  ElectricalGraphEdge,
  ElectricalGraphNode,
  GenerateSldOptions,
  NormalizedSclModel,
  SldConnection,
  SldDocument,
  SldElement,
} from "./types"
import { buildElectricalGraph } from "./graph"

export function createSldDocument(model: NormalizedSclModel, options: GenerateSldOptions = {}): SldDocument {
  return createSldDocumentFromGraph(buildElectricalGraph(model), options)
}

export function createSldDocumentFromGraph(graph: ElectricalGraph, options: GenerateSldOptions = {}): SldDocument {
  const elements = graph.nodes.map(mapGraphNodeToElement)

  return {
    schema: "unitlab.scd-sld.document",
    version: 1,
    sourceHash: graph.sourceHash,
    generatedAt: options.generatedAt ?? null,
    elements,
    connections: buildConnectivityNodeConnections(graph.edges),
    labels: elements.map(element => ({
      id: `${element.id}/label`,
      sourceId: element.sourceId,
      text: element.label,
      position: element.position,
    })),
    diagnostics: [...graph.diagnostics],
    layoutHints: {
      generatedFrom: "scd",
      gridSize: options.gridSize ?? 24,
    },
  }
}

function mapGraphNodeToElement(node: ElectricalGraphNode): SldElement {
  return {
    id: `sld-element:${node.sourceId}`,
    sourceId: node.sourceId,
    sourcePath: node.sourcePath,
    kind: node.kind,
    label: node.label,
    equipmentType: node.equipmentType,
    substationName: node.substationName,
    voltageLevelName: node.voltageLevelName,
    bayName: node.bayName,
    position: node.position,
  }
}

function buildConnectivityNodeConnections(edges: ElectricalGraphEdge[]): SldConnection[] {
  return edges
    .filter(edge => edge.nodeIds.length > 1)
    .map(edge => ({
      id: `connection:${sanitizeId(edge.sourceConnectivityNode)}`,
      kind: "connectivity-node",
      junctionId: edge.junctionId,
      sourceConnectivityNode: edge.sourceConnectivityNode,
      portIds: edge.portIds,
      terminalOwnerIds: edge.nodeIds,
    }))
}

function sanitizeId(value: string): string {
  return value.trim().replace(/[\s/]+/g, "_")
}
