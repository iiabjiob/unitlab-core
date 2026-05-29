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

export type SldBayEquipmentRole =
  | "busbar"
  | "circuitBreaker"
  | "busDisconnector"
  | "lineDisconnector"
  | "earthSwitch"
  | "transformer"
  | "feederTerminal"
  | "measurement"
  | "ground"
  | "genericSwitchgear"
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

export type SldBayInterpretation =
  | "busbar"
  | "bus-coupler"
  | "single-bus-feeder"
  | "double-bus-feeder"
  | "transformer-feeder"
  | "busbar-earth"
  | "switchgear"
  | "unknown"

export type SldBayLayoutOrientation = "up" | "down"

export type SldBayOutgoingSide = "top" | "bottom" | "none"

export type SldBayEarthSwitchPlacement = "line-side" | "bus-side" | "both" | "none"

export type SldBayLayoutVariant = {
  templateId: string
  orientation: SldBayLayoutOrientation
  busbarCount: number
  outgoingSide: SldBayOutgoingSide
  earthSwitchPlacement: SldBayEarthSwitchPlacement
  confidence: "high" | "medium" | "low"
}

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
  accessPoints: SclAccessPoint[]
  sourcePath: string
  sourceLocation?: ScdSourceLocation
}

export type SclAccessPoint = {
  id: string
  name: string
  desc: string | null
  router: boolean | null
  clock: boolean | null
  server: SclServer | null
  sourcePath: string
  sourceLocation?: ScdSourceLocation
}

export type SclServer = {
  id: string
  logicalDevices: SclLogicalDevice[]
  sourcePath: string
  sourceLocation?: ScdSourceLocation
}

export type SclLogicalDevice = {
  id: string
  inst: string
  desc: string | null
  ldName: string | null
  logicalNodes: SclLogicalNode[]
  sourcePath: string
  sourceLocation?: ScdSourceLocation
}

export type SclLogicalNode = {
  id: string
  tagName: "LN0" | "LN"
  logicalNodeName: string
  prefix: string | null
  lnClass: string
  lnInst: string | null
  lnType: string | null
  desc: string | null
  iedName: string
  accessPointName: string
  logicalDeviceInst: string
  dataSets: SclDataSet[]
  reportControls: SclReportControl[]
  sourcePath: string
  sourceLocation?: ScdSourceLocation
}

export type SclDataSetMemberKind = "FCDA" | "FCD"

export type SclDataSetMember = {
  id: string
  kind: SclDataSetMemberKind
  ldInst: string | null
  prefix: string | null
  lnClass: string | null
  lnInst: string | null
  doName: string | null
  daName: string | null
  fc: string | null
  ix: string | null
  reference: string
  sourcePath: string
  sourceLocation?: ScdSourceLocation
}

export type SclDataSet = {
  id: string
  name: string
  desc: string | null
  iedName: string
  accessPointName: string
  logicalDeviceInst: string
  logicalNodeName: string
  members: SclDataSetMember[]
  sourcePath: string
  sourceLocation?: ScdSourceLocation
}

export type SclReportTriggerOptions = {
  dataChange: boolean | null
  qualityChange: boolean | null
  dataUpdate: boolean | null
  periodic: boolean | null
  generalInterrogation: boolean | null
}

export type SclReportOptionalFields = {
  sequenceNumber: boolean | null
  timestamp: boolean | null
  reasonCode: boolean | null
  dataSetName: boolean | null
  dataReference: boolean | null
  entryId: boolean | null
  configRevision: boolean | null
  bufferOverflow: boolean | null
}

export type SclReportClient = {
  iedName: string | null
  accessPointRef: string | null
  logicalDeviceInst: string | null
  prefix: string | null
  lnClass: string | null
  lnInst: string | null
  desc: string | null
  sourcePath: string
  sourceLocation?: ScdSourceLocation
}

export type SclReportEnabled = {
  max: number | null
  desc: string | null
  clients: SclReportClient[]
  sourcePath: string
  sourceLocation?: ScdSourceLocation
}

export type SclReportControl = {
  id: string
  name: string
  desc: string | null
  rptId: string | null
  dataSetName: string | null
  dataSetId: string | null
  dataSetRef: string | null
  confRev: string | null
  buffered: boolean
  indexed: boolean | null
  bufferTimeMs: number | null
  integrityPeriodMs: number | null
  triggerOptions: SclReportTriggerOptions
  optionalFields: SclReportOptionalFields
  rptEnabled: SclReportEnabled | null
  iedName: string
  accessPointName: string
  logicalDeviceInst: string
  logicalNodeName: string
  sourcePath: string
  sourceLocation?: ScdSourceLocation
}

export type Iec61850ReportSubscriptionCandidate = {
  id: string
  iedName: string
  accessPointName: string
  logicalDeviceInst: string
  logicalNodeName: string
  reportControlId: string
  reportControlName: string
  reportKind: "buffered" | "unbuffered"
  rptId: string | null
  dataSetId: string | null
  dataSetRef: string | null
  confRev: string | null
  indexed: boolean | null
  signalCount: number
  signals: SclDataSetMember[]
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
  reportSubscriptions: Iec61850ReportSubscriptionCandidate[]
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
  equipmentRole: SldBayEquipmentRole
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
  interpretation: SldBayInterpretation
  layoutVariant: SldBayLayoutVariant
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
