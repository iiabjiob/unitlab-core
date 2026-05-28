export type ScdDiagnosticSeverity = "info" | "warning" | "error"
export type ScdDiagnosticStage = "xml" | "parser" | "normalizer" | "graph" | "layout" | "adapter"

export type SclEquipmentKind =
  | "breaker"
  | "disconnector"
  | "busbar"
  | "transformer"
  | "measurement"
  | "feeder"
  | "ground"
  | "unknown"

export type SldElementKind = SclEquipmentKind | "placeholder"

export type SldConnectionKind = "connectivity-node"

export type ElectricalGraphGroupKind = "substation" | "voltage-level" | "bay"

export type ElectricalGraphEdgeKind = SldConnectionKind

export type SldCoordinate = {
  x: number | null
  y: number | null
}

export type ScdDiagnostic = {
  severity: ScdDiagnosticSeverity
  stage: ScdDiagnosticStage
  code: string
  message: string
  sourcePath?: string
  sourceId?: string
}

export type ScdSource = {
  fileName: string
  contentHash: string
  xmlText: string
  workspaceId?: number | string | null
}

export type SclLogicalNodeRef = {
  iedName: string | null
  ldInst: string | null
  lnClass: string | null
  lnInst: string | null
  lnType: string | null
  prefix: string | null
  sourcePath: string
}

export type SclTerminal = {
  id: string
  name: string | null
  connectivityNode: string | null
  cNodeName: string | null
  substationName: string | null
  voltageLevelName: string | null
  bayName: string | null
  sourcePath: string
}

export type SclConnectivityNode = {
  id: string
  name: string | null
  pathName: string | null
  substationName: string | null
  voltageLevelName: string | null
  bayName: string | null
  sourcePath: string
}

export type SclEquipment = {
  id: string
  name: string
  desc: string | null
  type: string
  kind: SclEquipmentKind
  tagName: "ConductingEquipment" | "PowerTransformer"
  coordinates: SldCoordinate
  terminals: SclTerminal[]
  lNodes: SclLogicalNodeRef[]
  substationName: string | null
  voltageLevelName: string | null
  bayName: string | null
  sourcePath: string
}

export type SclBay = {
  id: string
  name: string
  desc: string | null
  coordinates: SldCoordinate
  lNodes: SclLogicalNodeRef[]
  connectivityNodes: SclConnectivityNode[]
  equipments: SclEquipment[]
  substationName: string
  voltageLevelName: string
  sourcePath: string
}

export type SclVoltageLevel = {
  id: string
  name: string
  voltage: string | null
  lNodes: SclLogicalNodeRef[]
  connectivityNodes: SclConnectivityNode[]
  bays: SclBay[]
  substationName: string
  sourcePath: string
}

export type SclSubstation = {
  id: string
  name: string
  desc: string | null
  coordinates: SldCoordinate
  lNodes: SclLogicalNodeRef[]
  connectivityNodes: SclConnectivityNode[]
  voltageLevels: SclVoltageLevel[]
  powerTransformers: SclEquipment[]
  sourcePath: string
}

export type SclIed = {
  id: string
  name: string
  desc: string | null
  manufacturer: string | null
  type: string | null
  configVersion: string | null
  sourcePath: string
}

export type NormalizedSclModel = {
  schema: "unitlab.scd-sld.normalized-scl"
  version: 1
  source: {
    fileName: string
    contentHash: string
  }
  scl: {
    version: string | null
    revision: string | null
  }
  substations: SclSubstation[]
  ieds: SclIed[]
  diagnostics: ScdDiagnostic[]
}

export type ElectricalGraphGroup = {
  id: string
  kind: ElectricalGraphGroupKind
  name: string
  label: string
  parentId: string | null
  sourcePath: string
}

export type ElectricalGraphNode = {
  id: string
  sourceId: string
  sourcePath: string
  kind: SclEquipmentKind
  label: string
  equipmentType: string
  groupId: string | null
  substationName: string | null
  voltageLevelName: string | null
  bayName: string | null
  position: SldCoordinate
}

export type ElectricalGraphPort = {
  id: string
  nodeId: string
  sourceTerminalId: string
  name: string | null
  connectivityNode: string | null
  junctionId: string | null
  sourcePath: string
}

export type ElectricalGraphJunction = {
  id: string
  sourceId: string
  sourcePath: string | null
  name: string | null
  pathName: string
  substationName: string | null
  voltageLevelName: string | null
  bayName: string | null
  position: SldCoordinate
  portIds: string[]
}

export type ElectricalGraphEdge = {
  id: string
  kind: ElectricalGraphEdgeKind
  junctionId: string
  sourceConnectivityNode: string
  portIds: string[]
  nodeIds: string[]
}

export type ElectricalGraph = {
  schema: "unitlab.scd-sld.electrical-graph"
  version: 1
  sourceHash: string
  nodes: ElectricalGraphNode[]
  ports: ElectricalGraphPort[]
  junctions: ElectricalGraphJunction[]
  edges: ElectricalGraphEdge[]
  groups: ElectricalGraphGroup[]
  diagnostics: ScdDiagnostic[]
}

export type SldElement = {
  id: string
  sourceId: string
  sourcePath: string
  kind: SldElementKind
  label: string
  equipmentType: string
  substationName: string | null
  voltageLevelName: string | null
  bayName: string | null
  position: SldCoordinate
}

export type SldConnection = {
  id: string
  kind: SldConnectionKind
  junctionId: string
  sourceConnectivityNode: string
  portIds: string[]
  terminalOwnerIds: string[]
}

export type SldLabel = {
  id: string
  sourceId: string
  text: string
  position: SldCoordinate
}

export type SldDocument = {
  schema: "unitlab.scd-sld.document"
  version: 1
  sourceHash: string
  generatedAt: string | null
  elements: SldElement[]
  connections: SldConnection[]
  labels: SldLabel[]
  diagnostics: ScdDiagnostic[]
  layoutHints: {
    generatedFrom: "scd"
    gridSize: number
  }
}

export type GenerateSldOptions = {
  generatedAt?: string | null
  gridSize?: number
}

export type GenerateSldResult = {
  model: NormalizedSclModel
  graph: ElectricalGraph
  document: SldDocument
  diagnostics: ScdDiagnostic[]
}
