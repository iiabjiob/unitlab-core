import type {
  ElectricalGraph,
  ElectricalGraphGroup,
  ElectricalGraphNode,
  SclEquipmentKind,
  ScdDiagnostic,
  SldBayCell,
  SldBayCellType,
  SldCellModel,
  SldCellNode,
  SldCellNodeRole,
  SldVoltageLevelLane,
} from "./types"

export function buildSldCellModel(graph: ElectricalGraph): SldCellModel {
  const diagnostics: ScdDiagnostic[] = [...graph.diagnostics]
  const nodesByGroupId = groupNodesByGroupId(graph.nodes)
  const voltageLevelGroups = graph.groups
    .filter(group => group.kind === "voltage-level")
    .sort(compareGroupsByYThenX)

  const assignedNodeIds = new Set<string>()
  const voltageLevels = voltageLevelGroups.map((voltageLevelGroup, orderIndex) => {
    const lane = buildVoltageLevelLane({
      voltageLevelGroup,
      orderIndex,
      groups: graph.groups,
      nodesByGroupId,
      assignedNodeIds,
    })
    return lane
  })

  const orphanNodes = graph.nodes
    .filter(node => !assignedNodeIds.has(node.id))
    .map((node, orderIndex) => mapNodeToCellNode(node, orderIndex))

  return {
    schema: "unitlab.scd-sld.cell-model",
    version: 1,
    sourceHash: graph.sourceHash,
    voltageLevels,
    orphanNodes,
    diagnostics,
    layoutPolicy: {
      orientation: "horizontal-voltage-levels",
      bayOrder: "name-then-id",
      nodeOrder: "role-then-label",
    },
  }
}

function buildVoltageLevelLane(input: {
  voltageLevelGroup: ElectricalGraphGroup
  orderIndex: number
  groups: ElectricalGraphGroup[]
  nodesByGroupId: Map<string, ElectricalGraphNode[]>
  assignedNodeIds: Set<string>
}): SldVoltageLevelLane {
  const bayGroups = input.groups
    .filter(group => group.kind === "bay" && group.parentId === input.voltageLevelGroup.id)
    .sort(compareGroupsByXThenY)

  const bayCells = bayGroups.map((bayGroup, orderIndex) => {
    const nodes = buildCellNodes(input.nodesByGroupId.get(bayGroup.id) ?? [])
    nodes.forEach(node => input.assignedNodeIds.add(node.graphNodeId))
    return buildBayCell({
      bayGroup,
      voltageLevelGroupId: input.voltageLevelGroup.id,
      orderIndex,
      nodes,
    })
  })

  const ungroupedNodes = buildCellNodes(input.nodesByGroupId.get(input.voltageLevelGroup.id) ?? [])
  ungroupedNodes.forEach(node => input.assignedNodeIds.add(node.graphNodeId))

  return {
    id: `lane:${input.voltageLevelGroup.id}`,
    groupId: input.voltageLevelGroup.id,
    name: input.voltageLevelGroup.name,
    label: input.voltageLevelGroup.label,
    orderIndex: input.orderIndex,
    position: input.voltageLevelGroup.coordinates,
    bayCells,
    ungroupedNodes,
  }
}

function buildBayCell(input: {
  bayGroup: ElectricalGraphGroup
  voltageLevelGroupId: string
  orderIndex: number
  nodes: SldCellNode[]
}): SldBayCell {
  return {
    id: `cell:${input.bayGroup.id}`,
    groupId: input.bayGroup.id,
    voltageLevelGroupId: input.voltageLevelGroupId,
    name: input.bayGroup.name,
    label: input.bayGroup.label,
    cellType: inferBayCellType(input.bayGroup, input.nodes),
    orderIndex: input.orderIndex,
    position: input.bayGroup.coordinates,
    nodes: input.nodes,
    nodeIds: input.nodes.map(node => node.graphNodeId),
    busbarNodeIds: nodeIdsByRole(input.nodes, "busbar"),
    switchgearNodeIds: nodeIdsByRole(input.nodes, "switchgear"),
    feederNodeIds: nodeIdsByRole(input.nodes, "feeder"),
    transformerNodeIds: nodeIdsByRole(input.nodes, "transformer"),
    measurementNodeIds: nodeIdsByRole(input.nodes, "measurement"),
    groundNodeIds: nodeIdsByRole(input.nodes, "ground"),
    unknownNodeIds: nodeIdsByRole(input.nodes, "unknown"),
  }
}

function buildCellNodes(nodes: ElectricalGraphNode[]): SldCellNode[] {
  return [...nodes]
    .sort(compareNodesForCell)
    .map((node, orderIndex) => mapNodeToCellNode(node, orderIndex))
}

function mapNodeToCellNode(node: ElectricalGraphNode, orderIndex: number): SldCellNode {
  return {
    id: `cell-node:${node.id}`,
    graphNodeId: node.id,
    sourceId: node.sourceId,
    sourcePath: node.sourcePath,
    label: node.label,
    kind: node.kind,
    equipmentType: node.equipmentType,
    role: roleForKind(node.kind),
    orderIndex,
    position: node.position,
    generated: node.generated,
  }
}

function groupNodesByGroupId(nodes: ElectricalGraphNode[]): Map<string, ElectricalGraphNode[]> {
  const groups = new Map<string, ElectricalGraphNode[]>()

  for (const node of nodes) {
    if (!node.groupId) {
      continue
    }
    const groupNodes = groups.get(node.groupId) ?? []
    groupNodes.push(node)
    groups.set(node.groupId, groupNodes)
  }

  return groups
}

function roleForKind(kind: SclEquipmentKind): SldCellNodeRole {
  switch (kind) {
    case "busbar":
      return "busbar"
    case "breaker":
    case "disconnector":
      return "switchgear"
    case "transformer":
      return "transformer"
    case "feeder":
      return "feeder"
    case "measurement":
      return "measurement"
    case "ground":
      return "ground"
    case "unknown":
    default:
      return "unknown"
  }
}

function nodeIdsByRole(nodes: SldCellNode[], role: SldCellNodeRole): string[] {
  return nodes
    .filter(node => node.role === role)
    .map(node => node.graphNodeId)
}

function compareNodesForCell(left: ElectricalGraphNode, right: ElectricalGraphNode): number {
  return compareCoordinateValue(left.position.y, right.position.y)
    || compareCoordinateValue(left.position.x, right.position.x)
    || compareRole(roleForKind(left.kind), roleForKind(right.kind))
    || left.label.localeCompare(right.label, undefined, { numeric: true })
    || left.id.localeCompare(right.id)
}

function compareGroupsByXThenY(left: ElectricalGraphGroup, right: ElectricalGraphGroup): number {
  return compareCoordinateValue(left.coordinates.x, right.coordinates.x)
    || compareCoordinateValue(left.coordinates.y, right.coordinates.y)
    || left.sourcePath.localeCompare(right.sourcePath, undefined, { numeric: true })
    || left.name.localeCompare(right.name, undefined, { numeric: true })
    || left.id.localeCompare(right.id)
}

function compareGroupsByYThenX(left: ElectricalGraphGroup, right: ElectricalGraphGroup): number {
  return compareCoordinateValue(left.coordinates.y, right.coordinates.y)
    || compareCoordinateValue(left.coordinates.x, right.coordinates.x)
    || left.sourcePath.localeCompare(right.sourcePath, undefined, { numeric: true })
    || left.name.localeCompare(right.name, undefined, { numeric: true })
    || left.id.localeCompare(right.id)
}

function compareRole(left: SldCellNodeRole, right: SldCellNodeRole): number {
  return roleOrder(left) - roleOrder(right)
}

function roleOrder(role: SldCellNodeRole): number {
  switch (role) {
    case "busbar":
      return 10
    case "switchgear":
      return 20
    case "transformer":
      return 30
    case "feeder":
      return 40
    case "measurement":
      return 50
    case "ground":
      return 60
    case "unknown":
    default:
      return 90
  }
}

function inferBayCellType(group: ElectricalGraphGroup, nodes: SldCellNode[]): SldBayCellType {
  const name = `${group.name} ${group.label}`.toLowerCase()
  if (nodes.some(node => node.role === "busbar") || isBusbarName(name)) {
    return "busbar"
  }
  if (name.includes("coupler")) {
    return "bus-coupler"
  }
  if (nodes.some(node => node.role === "transformer") || /\b(sgt|gt|tr|transformer)\b/i.test(name)) {
    return "transformer"
  }
  if (name.includes("reactor") || nodes.some(node => node.equipmentType.trim().toUpperCase() === "REA")) {
    return "reactor"
  }
  if (name.includes("protection")) {
    return "protection"
  }
  if (nodes.some(node => node.role === "feeder")) {
    return "feeder"
  }
  if (nodes.some(node => node.role === "switchgear")) {
    return "switchgear"
  }
  return "unknown"
}

function isBusbarName(value: string): boolean {
  const normalized = value.trim().toLowerCase()
  return normalized.includes("busbar")
    || /^bus\w*$/i.test(value.trim())
    || normalized.includes("mainbus")
    || normalized.includes("reservebus")
    || /^\d+(\.\d+)?\s*kv(\s+bus)?$/i.test(value.trim())
}

function compareCoordinateValue(left: number | null, right: number | null): number {
  return coordinateValue(left) - coordinateValue(right)
}

function coordinateValue(value: number | null): number {
  return Number.isFinite(value) ? (value as number) : 0
}
