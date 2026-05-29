import type {
  NormalizedSclModel,
  SclConnectivityNode,
  SclEquipment,
  SclTerminal,
} from "./types"
import {
  isPresent,
  normalizePath,
  normalizePathParts,
  uniqueStrings,
} from "./parserUtils"

export function normalizeTerminalConnectivityReferences(model: NormalizedSclModel) {
  const lookup = buildConnectivityNodeLookup(model)

  for (const equipment of collectEquipment(model)) {
    for (const terminal of equipment.terminals) {
      const resolved = resolveTerminalConnectivityNode(terminal, equipment, lookup)
      const normalizedPath = resolved.normalizedPath
      terminal.resolvedConnectivityNodePath = normalizedPath

      if (!normalizedPath) {
        continue
      }

      const declaredNode = resolved.node
      terminal.resolvedConnectivityNodeId = declaredNode?.id ?? null

      if (!declaredNode) {
        model.diagnostics.push({
          severity: "warning",
          stage: "normalizer",
          code: "normalizer.unresolved-connectivity-node",
          message: `Terminal references connectivity node "${normalizedPath}" that is not declared in the parsed SCL topology.`,
          sourcePath: terminal.sourcePath,
          sourceId: terminal.id,
          sourceLocation: terminal.sourceLocation,
        })
      }
    }
  }
}

type ConnectivityNodeLookup = {
  byPath: Map<string, SclConnectivityNode>
  byScopedName: Map<string, SclConnectivityNode>
  byLooseScopedName: Map<string, SclConnectivityNode | null>
}

function buildConnectivityNodeLookup(model: NormalizedSclModel): ConnectivityNodeLookup {
  const lookup: ConnectivityNodeLookup = {
    byPath: new Map(),
    byScopedName: new Map(),
    byLooseScopedName: new Map(),
  }

  for (const node of collectConnectivityNodes(model)) {
    if (!lookup.byPath.has(node.normalizedPath)) {
      lookup.byPath.set(node.normalizedPath, node)
    } else {
      model.diagnostics.push({
        severity: "info",
        stage: "normalizer",
        code: "normalizer.duplicate-connectivity-node",
        message: `Duplicate connectivity node "${node.normalizedPath}" was collapsed by path for terminal resolution.`,
        sourcePath: node.sourcePath,
        sourceId: node.id,
        sourceLocation: node.sourceLocation,
      })
    }

    const scopedName = normalizePathParts([
      node.substationName,
      node.voltageLevelName,
      node.bayName,
      node.name,
    ])
    if (scopedName && !lookup.byScopedName.has(scopedName)) {
      lookup.byScopedName.set(scopedName, node)
    }

    const looseScopedName = normalizePathParts([
      node.substationName,
      node.bayName,
      node.name,
    ])
    setUniqueConnectivityNode(lookup.byLooseScopedName, looseScopedName, node)
  }

  return lookup
}

function collectConnectivityNodes(model: NormalizedSclModel): SclConnectivityNode[] {
  return model.substations.flatMap(substation => [
    ...substation.connectivityNodes,
    ...substation.voltageLevels.flatMap(voltageLevel => [
      ...voltageLevel.connectivityNodes,
      ...voltageLevel.bays.flatMap(bay => bay.connectivityNodes),
    ]),
  ])
}

function collectEquipment(model: NormalizedSclModel): SclEquipment[] {
  return model.substations.flatMap(substation => [
    ...substation.powerTransformers,
    ...substation.voltageLevels.flatMap(voltageLevel => (
      voltageLevel.bays.flatMap(bay => bay.equipments)
    )),
  ])
}

function resolveTerminalConnectivityNode(
  terminal: SclTerminal,
  equipment: SclEquipment,
  lookup: ConnectivityNodeLookup,
): { normalizedPath: string | null; node: SclConnectivityNode | null } {
  const explicitPath = terminal.connectivityNode?.trim()
    ? normalizePath(terminal.connectivityNode)
    : null
  const explicitPathParts = explicitPath ? explicitPath.split("/") : []
  const explicitNodeName = explicitPathParts.length > 0
    ? explicitPathParts[explicitPathParts.length - 1]
    : null
  const explicitBayName = explicitPathParts.length >= 3
    ? normalizeBayAlias(explicitPathParts[explicitPathParts.length - 2] ?? null)
    : null
  const scopedPath = terminal.cNodeName
    ? normalizePathParts([
      terminal.substationName,
      terminal.voltageLevelName,
      terminal.bayName,
      terminal.cNodeName,
    ])
    : null
  const terminalNodeName = terminal.cNodeName ?? explicitNodeName
  const equipmentScopedPath = terminalNodeName
    ? normalizePathParts([
      equipment.substationName,
      equipment.voltageLevelName,
      equipment.bayName,
      terminalNodeName,
    ])
    : null
  const looseExplicitPath = normalizePathParts([
    explicitPathParts[0] ?? null,
    explicitBayName,
    explicitNodeName,
  ])
  const looseEquipmentPath = terminalNodeName
    ? normalizePathParts([
      equipment.substationName,
      equipment.bayName,
      terminalNodeName,
    ])
    : null

  for (const path of uniqueStrings([explicitPath, scopedPath, equipmentScopedPath].filter(isPresent))) {
    const node = lookup.byPath.get(path)
    if (node) {
      return {
        normalizedPath: node.normalizedPath,
        node,
      }
    }
  }

  for (const path of uniqueStrings([scopedPath, equipmentScopedPath].filter(isPresent))) {
    const node = lookup.byScopedName.get(path)
    if (node) {
      return {
        normalizedPath: node.normalizedPath,
        node,
      }
    }
  }

  for (const path of uniqueStrings([looseExplicitPath, looseEquipmentPath].filter(isPresent))) {
    const node = lookup.byLooseScopedName.get(path)
    if (node) {
      return {
        normalizedPath: node.normalizedPath,
        node,
      }
    }
  }

  return {
    normalizedPath: explicitPath ?? scopedPath ?? equipmentScopedPath,
    node: null,
  }
}

function setUniqueConnectivityNode(
  lookup: Map<string, SclConnectivityNode | null>,
  key: string | null,
  node: SclConnectivityNode,
) {
  if (!key) {
    return
  }
  const existing = lookup.get(key)
  if (existing === undefined) {
    lookup.set(key, node)
    return
  }
  if (existing?.id !== node.id) {
    lookup.set(key, null)
  }
}

function normalizeBayAlias(value: string | null): string | null {
  return value?.replace(/^Copy_\d+_/i, "") ?? null
}
