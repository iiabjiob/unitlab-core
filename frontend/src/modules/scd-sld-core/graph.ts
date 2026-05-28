import type {
  ElectricalGraph,
  ElectricalGraphEdge,
  ElectricalGraphGroup,
  ElectricalGraphJunction,
  ElectricalGraphNode,
  ElectricalGraphPort,
  NormalizedSclModel,
  ScdDiagnostic,
  SclConnectivityNode,
  SclEquipment,
  SclBay,
  SclSubstation,
  SclVoltageLevel,
} from "./types"

export function buildElectricalGraph(model: NormalizedSclModel): ElectricalGraph {
  const diagnostics: ScdDiagnostic[] = [...model.diagnostics]
  const groups = collectGroups(model)
  const nodes: ElectricalGraphNode[] = []
  const ports: ElectricalGraphPort[] = []
  const junctions = dedupeJunctions(collectDeclaredJunctions(model), diagnostics)
  const junctionsByPath = new Map(junctions.map(junction => [junction.pathName, junction]))

  for (const equipment of collectEquipment(model)) {
    const node = mapEquipmentToNode(equipment)
    nodes.push(node)

    if (equipment.kind === "unknown") {
      diagnostics.push({
        severity: "warning",
        stage: "graph",
        code: "graph.unsupported-equipment-kind",
        message: `Unsupported conducting equipment type "${equipment.type}" is preserved as an unknown SLD node.`,
        sourcePath: equipment.sourcePath,
        sourceId: equipment.id,
      })
    }

    equipment.terminals.forEach((terminal) => {
      const connectivityNode = terminal.resolvedConnectivityNodePath ?? normalizePath(terminal.connectivityNode)
      const junction = connectivityNode
        ? ensureJunction(junctions, junctionsByPath, diagnostics, {
          pathName: connectivityNode,
          substationName: terminal.substationName ?? equipment.substationName,
          voltageLevelName: terminal.voltageLevelName ?? equipment.voltageLevelName,
          bayName: terminal.bayName ?? equipment.bayName,
          sourcePath: terminal.sourcePath,
          sourceId: terminal.id,
        })
        : null

      if (!connectivityNode) {
        diagnostics.push({
          severity: "warning",
          stage: "graph",
          code: "graph.terminal-missing-connectivity-node",
          message: "Terminal has no connectivity node reference and will be rendered as dangling topology.",
          sourcePath: terminal.sourcePath,
          sourceId: terminal.id,
        })
      }

      const port: ElectricalGraphPort = {
        id: `port/${terminal.id}`,
        nodeId: node.id,
        sourceTerminalId: terminal.id,
        name: terminal.name,
        connectivityNode,
        junctionId: junction?.id ?? null,
        sourcePath: terminal.sourcePath,
      }
      ports.push(port)
      junction?.portIds.push(port.id)
    })
  }

  for (const busbarNode of collectSyntheticBusbarNodes(model)) {
    const junction = ensureJunction(junctions, junctionsByPath, diagnostics, {
      pathName: busbarNode.connectivityNode.normalizedPath,
      substationName: busbarNode.connectivityNode.substationName,
      voltageLevelName: busbarNode.connectivityNode.voltageLevelName,
      bayName: busbarNode.connectivityNode.bayName,
      sourcePath: busbarNode.connectivityNode.sourcePath,
      sourceId: busbarNode.connectivityNode.id,
    })
    const node = mapSyntheticBusbarToNode(busbarNode.connectivityNode, busbarNode.label)
    nodes.push(node)
    const port: ElectricalGraphPort = {
      id: `port/${node.id}/terminal/${sanitizeId(busbarNode.connectivityNode.name ?? "busbar")}`,
      nodeId: node.id,
      sourceTerminalId: `${node.id}/terminal/${sanitizeId(busbarNode.connectivityNode.name ?? "busbar")}`,
      name: busbarNode.connectivityNode.name,
      connectivityNode: busbarNode.connectivityNode.normalizedPath,
      junctionId: junction.id,
      sourcePath: busbarNode.connectivityNode.sourcePath,
    }
    ports.push(port)
    junction.portIds.push(port.id)
  }

  const edges = buildEdges(junctions, ports, diagnostics)

  return {
    schema: "unitlab.scd-sld.electrical-graph",
    version: 1,
    sourceHash: model.source.contentHash,
    nodes: sortById(nodes),
    ports: sortById(ports),
    junctions: sortById(junctions.map(junction => ({
      ...junction,
      portIds: sortStrings(uniqueStrings(junction.portIds)),
    }))),
    edges,
    groups: sortById(groups),
    diagnostics,
  }
}

function collectGroups(model: NormalizedSclModel): ElectricalGraphGroup[] {
  return model.substations.flatMap((substation) => {
    const substationGroup = createGroup({
      id: groupId(["substation", substation.name]),
      kind: "substation",
      name: substation.name,
      label: substation.desc ?? substation.name,
      parentId: null,
      coordinates: substation.coordinates,
      sourcePath: substation.sourcePath,
    })

    const voltageGroups = substation.voltageLevels.flatMap(voltageLevel => collectVoltageLevelGroups(substation, voltageLevel))

    return [substationGroup, ...voltageGroups]
  })
}

function collectVoltageLevelGroups(
  substation: SclSubstation,
  voltageLevel: SclVoltageLevel,
): ElectricalGraphGroup[] {
  const voltageLevelGroup = createGroup({
    id: groupId(["substation", substation.name, "voltageLevel", voltageLevel.name]),
    kind: "voltage-level",
    name: voltageLevel.name,
    label: formatVoltageLevelLabel(voltageLevel),
    parentId: groupId(["substation", substation.name]),
    coordinates: voltageLevel.coordinates,
    sourcePath: voltageLevel.sourcePath,
  })

  const bayGroups = voltageLevel.bays.map(bay => createGroup({
    id: groupId(["substation", substation.name, "voltageLevel", voltageLevel.name, "bay", bay.name]),
    kind: "bay",
    name: bay.name,
    label: bay.desc ?? bay.name,
    parentId: voltageLevelGroup.id,
    coordinates: bay.coordinates,
    sourcePath: bay.sourcePath,
  }))

  return [voltageLevelGroup, ...bayGroups]
}

function createGroup(group: ElectricalGraphGroup): ElectricalGraphGroup {
  return group
}

function collectDeclaredJunctions(model: NormalizedSclModel): ElectricalGraphJunction[] {
  return model.substations.flatMap(substation => [
    ...substation.connectivityNodes.map(node => mapConnectivityNodeToJunction(node)),
    ...substation.voltageLevels.flatMap(voltageLevel => [
      ...voltageLevel.connectivityNodes.map(node => mapConnectivityNodeToJunction(node)),
      ...voltageLevel.bays.flatMap(bay => bay.connectivityNodes.map(node => mapConnectivityNodeToJunction(node))),
    ]),
  ])
}

function dedupeJunctions(
  junctions: ElectricalGraphJunction[],
  diagnostics: ScdDiagnostic[],
): ElectricalGraphJunction[] {
  const junctionsByPath = new Map<string, ElectricalGraphJunction>()

  for (const junction of junctions) {
    if (!junctionsByPath.has(junction.pathName)) {
      junctionsByPath.set(junction.pathName, junction)
      continue
    }

    diagnostics.push({
      severity: "info",
      stage: "graph",
      code: "graph.duplicate-connectivity-node",
      message: `Duplicate connectivity node "${junction.pathName}" was collapsed into one graph junction.`,
      sourcePath: junction.sourcePath ?? undefined,
      sourceId: junction.sourceId,
    })
  }

  return Array.from(junctionsByPath.values())
}

function collectEquipment(model: NormalizedSclModel): SclEquipment[] {
  return model.substations.flatMap(substation => [
    ...substation.powerTransformers,
    ...substation.voltageLevels.flatMap(voltageLevel => (
      voltageLevel.bays.flatMap(bay => bay.equipments)
    )),
  ])
}

function mapEquipmentToNode(equipment: SclEquipment): ElectricalGraphNode {
  return {
    id: equipment.id,
    sourceId: equipment.id,
    sourcePath: equipment.sourcePath,
    kind: equipment.kind,
    label: equipment.name,
    equipmentType: equipment.type,
    groupId: resolveEquipmentGroupId(equipment),
    substationName: equipment.substationName,
    voltageLevelName: equipment.voltageLevelName,
    bayName: equipment.bayName,
    position: equipment.coordinates,
    generated: false,
    grounded: equipment.terminals.some(terminal => isGroundReference(
      terminal.resolvedConnectivityNodePath
      ?? terminal.connectivityNode
      ?? terminal.cNodeName,
    )),
  }
}

function mapSyntheticBusbarToNode(node: SclConnectivityNode, label: string): ElectricalGraphNode {
  return {
    id: `${node.id}/busbar`,
    sourceId: `${node.id}/busbar`,
    sourcePath: node.sourcePath,
    kind: "busbar",
    label,
    equipmentType: "BBS",
    groupId: resolveConnectivityNodeGroupId(node),
    substationName: node.substationName,
    voltageLevelName: node.voltageLevelName,
    bayName: node.bayName,
    position: { x: 0, y: 0 },
    generated: true,
    grounded: false,
  }
}

function mapConnectivityNodeToJunction(node: SclConnectivityNode): ElectricalGraphJunction {
  const pathName = node.normalizedPath

  return {
    id: junctionId(pathName),
    sourceId: node.id,
    sourcePath: node.sourcePath,
    name: node.name,
    pathName,
    substationName: node.substationName,
    voltageLevelName: node.voltageLevelName,
    bayName: node.bayName,
    position: { x: null, y: null },
    portIds: [],
  }
}

function ensureJunction(
  junctions: ElectricalGraphJunction[],
  junctionsByPath: Map<string, ElectricalGraphJunction>,
  diagnostics: ScdDiagnostic[],
  input: {
    pathName: string
    substationName: string | null
    voltageLevelName: string | null
    bayName: string | null
    sourcePath: string
    sourceId: string
  },
): ElectricalGraphJunction {
  const existing = junctionsByPath.get(input.pathName)
  if (existing) {
    return existing
  }

  const junction: ElectricalGraphJunction = {
    id: junctionId(input.pathName),
    sourceId: input.pathName,
    sourcePath: null,
    name: lastPathSegment(input.pathName),
    pathName: input.pathName,
    substationName: input.substationName,
    voltageLevelName: input.voltageLevelName,
    bayName: input.bayName,
    position: { x: null, y: null },
    portIds: [],
  }
  junctions.push(junction)
  junctionsByPath.set(input.pathName, junction)
  diagnostics.push({
    severity: "info",
    stage: "graph",
    code: "graph.implicit-connectivity-node",
    message: `Terminal references connectivity node "${input.pathName}" that is not declared in the parsed topology; an implicit junction was created.`,
    sourcePath: input.sourcePath,
    sourceId: input.sourceId,
  })

  return junction
}

function buildEdges(
  junctions: ElectricalGraphJunction[],
  ports: ElectricalGraphPort[],
  diagnostics: ScdDiagnostic[],
): ElectricalGraphEdge[] {
  const portsById = new Map(ports.map(port => [port.id, port]))

  for (const junction of junctions) {
    if (junction.portIds.length === 0) {
      diagnostics.push({
        severity: "info",
        stage: "graph",
        code: "graph.unused-connectivity-node",
        message: `Connectivity node "${junction.pathName}" has no terminal references in the parsed topology.`,
        sourcePath: junction.sourcePath ?? undefined,
        sourceId: junction.sourceId,
      })
      continue
    }

    if (uniqueStrings(junction.portIds.map(portId => portsById.get(portId)?.nodeId).filter(isPresent)).length === 1) {
      diagnostics.push({
        severity: "info",
        stage: "graph",
        code: "graph.dangling-connectivity-node",
        message: `Connectivity node "${junction.pathName}" is attached to a single equipment node in the parsed topology.`,
        sourcePath: junction.sourcePath ?? undefined,
        sourceId: junction.sourceId,
      })
    }
  }

  return sortById(junctions.map((junction) => {
    const portIds = sortStrings(uniqueStrings(junction.portIds))
    const nodeIds = sortStrings(uniqueStrings(
      portIds.map(portId => portsById.get(portId)?.nodeId).filter(isPresent),
    ))

    return {
      id: `edge:${sanitizeId(junction.pathName)}`,
      kind: "connectivity-node",
      junctionId: junction.id,
      sourceConnectivityNode: junction.pathName,
      portIds,
      nodeIds,
    }
  }))
}

function resolveEquipmentGroupId(equipment: SclEquipment): string | null {
  if (!equipment.substationName) {
    return null
  }
  if (equipment.voltageLevelName && equipment.bayName) {
    return groupId([
      "substation",
      equipment.substationName,
      "voltageLevel",
      equipment.voltageLevelName,
      "bay",
      equipment.bayName,
    ])
  }
  if (equipment.voltageLevelName) {
    return groupId(["substation", equipment.substationName, "voltageLevel", equipment.voltageLevelName])
  }
  return groupId(["substation", equipment.substationName])
}

function resolveConnectivityNodeGroupId(node: SclConnectivityNode): string | null {
  if (!node.substationName) {
    return null
  }
  if (node.voltageLevelName && node.bayName) {
    return groupId([
      "substation",
      node.substationName,
      "voltageLevel",
      node.voltageLevelName,
      "bay",
      node.bayName,
    ])
  }
  if (node.voltageLevelName) {
    return groupId(["substation", node.substationName, "voltageLevel", node.voltageLevelName])
  }
  return groupId(["substation", node.substationName])
}

type SyntheticBusbarNode = {
  connectivityNode: SclConnectivityNode
  label: string
}

function collectSyntheticBusbarNodes(model: NormalizedSclModel): SyntheticBusbarNode[] {
  const busbars: SyntheticBusbarNode[] = []
  const seenPaths = new Set<string>()

  for (const substation of model.substations) {
    for (const voltageLevel of substation.voltageLevels) {
      for (const node of voltageLevel.connectivityNodes) {
        if (isVoltageLevelBusbarNode(node) && !seenPaths.has(node.normalizedPath)) {
          busbars.push({
            connectivityNode: node,
            label: busbarLabelFromConnectivityNode(node),
          })
          seenPaths.add(node.normalizedPath)
        }
      }

      for (const bay of voltageLevel.bays) {
        if (!isBusbarBay(bay)) {
          continue
        }
        for (const node of bay.connectivityNodes) {
          if (isGroundConnectivityNode(node) || seenPaths.has(node.normalizedPath)) {
            continue
          }
          busbars.push({
            connectivityNode: node,
            label: bay.desc ?? bay.name,
          })
          seenPaths.add(node.normalizedPath)
        }
      }
    }
  }

  return busbars
}

function isBusbarBay(bay: SclBay): boolean {
  if (bay.equipments.length > 0 || bay.connectivityNodes.length === 0) {
    return false
  }
  return isBusbarName(bay.name)
    || bay.connectivityNodes.some(node => isLineConnectivityNode(node) && !isGroundConnectivityNode(node))
}

function isVoltageLevelBusbarNode(node: SclConnectivityNode): boolean {
  return !node.bayName
    && isLineConnectivityNode(node)
    && isBusbarName(busbarLabelFromConnectivityNode(node))
}

function isLineConnectivityNode(node: SclConnectivityNode): boolean {
  return /^l\d+$/i.test(node.name?.trim() ?? "")
}

function isGroundConnectivityNode(node: SclConnectivityNode): boolean {
  const value = `${node.name ?? ""} ${node.pathName ?? ""} ${node.normalizedPath}`.toLowerCase()
  return value.includes("ground")
}

function isGroundReference(value: string | null): boolean {
  return value?.trim().toLowerCase().includes("ground") ?? false
}

function isBusbarName(value: string): boolean {
  const normalized = value.trim().toLowerCase()
  return normalized.includes("busbar")
    || /^bus\w*$/i.test(value.trim())
    || normalized.includes(" bus")
    || normalized.includes("mainbus")
    || normalized.includes("reservebus")
    || /^\d+(\.\d+)?\s*kv(\s+bus)?$/i.test(value.trim())
}

function busbarLabelFromConnectivityNode(node: SclConnectivityNode): string {
  if (node.bayName) {
    return node.bayName
  }
  const parts = node.normalizedPath.split("/").filter(Boolean)
  return parts.length >= 2 ? parts[parts.length - 2] ?? (node.name ?? "Busbar") : (node.name ?? "Busbar")
}

function formatVoltageLevelLabel(voltageLevel: SclVoltageLevel): string {
  const voltage = voltageLevel.voltage
  if (!voltage?.value) {
    return voltageLevel.name
  }

  const unit = `${voltage.multiplier ?? ""}${voltage.unit ?? ""}`
  return unit ? `${voltage.value} ${unit}` : voltage.value
}

function groupId(parts: string[]): string {
  return `group/${parts.map(sanitizeId).join("/")}`
}

function junctionId(pathName: string): string {
  return `junction/${sanitizeId(pathName)}`
}

function sanitizeId(value: string): string {
  return value.trim().replace(/[\s/]+/g, "_") || "unnamed"
}

function lastPathSegment(value: string): string {
  const parts = value.split("/").map(part => part.trim()).filter(Boolean)
  return parts[parts.length - 1] ?? value
}

function normalizePath(value: string | null): string | null {
  if (!value?.trim()) {
    return null
  }
  return value.split("/").map(part => part.trim()).filter(Boolean).join("/")
}

function uniqueStrings(values: string[]): string[] {
  return Array.from(new Set(values))
}

function sortStrings(values: string[]): string[] {
  return [...values].sort((left, right) => left.localeCompare(right))
}

function sortById<T extends { id: string }>(items: T[]): T[] {
  return [...items].sort((left, right) => left.id.localeCompare(right.id))
}

function isPresent<T>(value: T | null | undefined): value is T {
  return value !== null && value !== undefined && value !== ""
}
