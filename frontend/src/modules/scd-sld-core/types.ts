export type ScdDiagnosticSeverity = "info" | "warning" | "error"
export type ScdDiagnosticStage = "xml" | "parser" | "normalizer" | "graph" | "layout" | "adapter"

export type ScdSourceLocation = {
  line: number
  column: number
  offset: number
}

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

export type SldElementRepresentation = "symbol" | "busbar"

export type SldElementStrokeWeight = "normal" | "bold"

export type SldElementOrientation = "horizontal" | "vertical" | null

export type SldElementDimensions = {
  width: number
  height: number
}

export type SldElementVisual = {
  representation: SldElementRepresentation
  orientation: SldElementOrientation
  strokeWeight: SldElementStrokeWeight
  dimensions: SldElementDimensions | null
}

export type SldCellNodeRole =
  | "busbar"
  | "switchgear"
  | "transformer"
  | "feeder"
  | "measurement"
  | "ground"
  | "unknown"

export type SldBayCellType =
  | "busbar"
  | "bus-coupler"
  | "transformer"
  | "feeder"
  | "reactor"
  | "protection"
  | "switchgear"
  | "unknown"

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
  sourceLocation?: ScdSourceLocation
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
  sourceLocation?: ScdSourceLocation
}

export type SclTerminal = {
  id: string
  name: string | null
  connectivityNode: string | null
  resolvedConnectivityNodeId: string | null
  resolvedConnectivityNodePath: string | null
  cNodeName: string | null
  substationName: string | null
  voltageLevelName: string | null
  bayName: string | null
  sourcePath: string
  sourceLocation?: ScdSourceLocation
}

export type SclConnectivityNode = {
  id: string
  name: string | null
  pathName: string | null
  normalizedPath: string
  substationName: string | null
  voltageLevelName: string | null
  bayName: string | null
  sourcePath: string
  sourceLocation?: ScdSourceLocation
}

export type SclVoltage = {
  value: string | null
  multiplier: string | null
  unit: string | null
  sourcePath: string
  sourceLocation?: ScdSourceLocation
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
  sourceLocation?: ScdSourceLocation
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
  sourceLocation?: ScdSourceLocation
}

export type SclVoltageLevel = {
  id: string
  name: string
  coordinates: SldCoordinate
  voltage: SclVoltage | null
  lNodes: SclLogicalNodeRef[]
  connectivityNodes: SclConnectivityNode[]
  bays: SclBay[]
  substationName: string
  sourcePath: string
  sourceLocation?: ScdSourceLocation
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
  sourceLocation?: ScdSourceLocation
}

export type SclIed = {
  id: string
  name: string
  desc: string | null
  manufacturer: string | null
  type: string | null
  configVersion: string | null
  sourcePath: string
  sourceLocation?: ScdSourceLocation
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
  coordinates: SldCoordinate
  sourcePath: string
  sourceLocation?: ScdSourceLocation
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
  generated: boolean
  grounded: boolean
  sourceLocation?: ScdSourceLocation
}

export type ElectricalGraphPort = {
  id: string
  nodeId: string
  sourceTerminalId: string
  name: string | null
  connectivityNode: string | null
  junctionId: string | null
  sourcePath: string
  sourceLocation?: ScdSourceLocation
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
  sourceLocation?: ScdSourceLocation
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

export type SldCellNode = {
  id: string
  graphNodeId: string
  sourceId: string
  sourcePath: string
  label: string
  kind: SclEquipmentKind
  equipmentType: string
  role: SldCellNodeRole
  orderIndex: number
  position: SldCoordinate
  generated: boolean
  grounded: boolean
  busbarConnected: boolean
  sourceLocation?: ScdSourceLocation
}

export type SldBayCell = {
  id: string
  groupId: string
  voltageLevelGroupId: string
  name: string
  label: string
  cellType: SldBayCellType
  orderIndex: number
  position: SldCoordinate
  nodes: SldCellNode[]
  nodeIds: string[]
  busbarNodeIds: string[]
  switchgearNodeIds: string[]
  feederNodeIds: string[]
  transformerNodeIds: string[]
  measurementNodeIds: string[]
  groundNodeIds: string[]
  unknownNodeIds: string[]
}

export type SldVoltageLevelLane = {
  id: string
  groupId: string
  name: string
  label: string
  orderIndex: number
  position: SldCoordinate
  bayCells: SldBayCell[]
  ungroupedNodes: SldCellNode[]
}

export type SldCellModel = {
  schema: "unitlab.scd-sld.cell-model"
  version: 1
  sourceHash: string
  voltageLevels: SldVoltageLevelLane[]
  orphanNodes: SldCellNode[]
  diagnostics: ScdDiagnostic[]
  layoutPolicy: {
    orientation: "horizontal-voltage-levels"
    bayOrder: "name-then-id"
    nodeOrder: "role-then-label"
  }
}

export type SldElement = {
  id: string
  sourceId: string
  sourcePath: string
  kind: SldElementKind
  label: string
  equipmentType: string
  visual: SldElementVisual
  substationName: string | null
  voltageLevelName: string | null
  bayName: string | null
  position: SldCoordinate
  grounded: boolean
  sourceLocation?: ScdSourceLocation
}

export type SldRoutePoint = {
  x: number
  y: number
}

export type SldConnectionRouteSegment = {
  terminalOwnerId: string
  points: SldRoutePoint[]
}

export type SldConnectionRoute = {
  kind: "orthogonal-star"
  anchor: SldRoutePoint
  segments: SldConnectionRouteSegment[]
}

export type SldConnection = {
  id: string
  kind: SldConnectionKind
  junctionId: string
  sourceConnectivityNode: string
  portIds: string[]
  terminalOwnerIds: string[]
  route: SldConnectionRoute | null
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
    generatedFrom: "scd" | "scd-flat-debug"
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
  cellModel: SldCellModel
  document: SldDocument
  diagnostics: ScdDiagnostic[]
}
