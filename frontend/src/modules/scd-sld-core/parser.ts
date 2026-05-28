import type {
  NormalizedSclModel,
  ScdDiagnostic,
  ScdSource,
  SclBay,
  SclConnectivityNode,
  SclEquipment,
  SclEquipmentKind,
  SclIed,
  SclLogicalNodeRef,
  SclSubstation,
  SclTerminal,
  SclVoltage,
  SclVoltageLevel,
  SldCoordinate,
} from "./types"
import {
  readXmlAttribute,
  readXmlAttributeByLocalName,
  scanXmlElements,
  type XmlElementEvent,
} from "./xmlScanner"

export function parseScdSource(source: ScdSource): NormalizedSclModel {
  const diagnostics: ScdDiagnostic[] = []
  const model: NormalizedSclModel = {
    schema: "unitlab.scd-sld.normalized-scl",
    version: 1,
    source: {
      fileName: source.fileName,
      contentHash: source.contentHash,
    },
    scl: {
      version: null,
      revision: null,
    },
    substations: [],
    ieds: [],
    diagnostics,
  }

  if (!source.xmlText.trim()) {
    diagnostics.push({
      severity: "error",
      stage: "xml",
      code: "xml.empty-source",
      message: "SCD source is empty.",
      sourceLocation: { line: 1, column: 1, offset: 0 },
    })
    return model
  }

  const substationStack: SclSubstation[] = []
  const voltageLevelStack: SclVoltageLevel[] = []
  const bayStack: SclBay[] = []
  const equipmentStack: SclEquipment[] = []
  let firstElementLocation: XmlElementEvent["sourceLocation"] | undefined

  for (const event of scanXmlElements(source.xmlText, diagnostics)) {
    firstElementLocation ??= event.sourceLocation
    if (event.kind === "close") {
      handleCloseEvent(event, substationStack, voltageLevelStack, bayStack, equipmentStack)
      continue
    }

    if (shouldStopAfterSubstationTopology(model, substationStack, event)) {
      break
    }

    switch (event.localName) {
      case "SCL":
        model.scl.version = readXmlAttribute(event.attributes, "version")
        model.scl.revision = readXmlAttribute(event.attributes, "revision")
        break
      case "Substation":
        openSubstation(model, substationStack, event)
        break
      case "VoltageLevel":
        openVoltageLevel(diagnostics, substationStack, voltageLevelStack, event)
        break
      case "Voltage":
        appendVoltage(diagnostics, voltageLevelStack, event)
        break
      case "Bay":
        openBay(diagnostics, substationStack, voltageLevelStack, bayStack, event)
        break
      case "ConductingEquipment":
        openConductingEquipment(diagnostics, substationStack, voltageLevelStack, bayStack, equipmentStack, event)
        break
      case "PowerTransformer":
        openPowerTransformer(diagnostics, substationStack, equipmentStack, event)
        break
      case "Terminal":
        appendTerminal(diagnostics, equipmentStack, event)
        break
      case "ConnectivityNode":
        appendConnectivityNode(diagnostics, substationStack, voltageLevelStack, bayStack, event)
        break
      case "LNode":
        appendLogicalNode(substationStack, voltageLevelStack, bayStack, equipmentStack, event)
        break
      case "IED":
        appendIed(model, event)
        break
      default:
        break
    }
  }

  if (model.substations.length === 0) {
    diagnostics.push({
      severity: "error",
      stage: "parser",
      code: "parser.no-substation",
      message: "No Substation section was found in the SCD file.",
      sourceLocation: firstElementLocation ?? { line: 1, column: 1, offset: 0 },
    })
  }

  normalizeTerminalConnectivityReferences(model)

  return model
}

function shouldStopAfterSubstationTopology(
  model: NormalizedSclModel,
  substationStack: SclSubstation[],
  event: XmlElementEvent,
): boolean {
  return model.substations.length > 0
    && substationStack.length === 0
    && event.depth <= 1
    && event.localName !== "SCL"
    && event.localName !== "Substation"
}

function handleCloseEvent(
  event: XmlElementEvent,
  substationStack: SclSubstation[],
  voltageLevelStack: SclVoltageLevel[],
  bayStack: SclBay[],
  equipmentStack: SclEquipment[],
) {
  switch (event.localName) {
    case "Substation":
      substationStack.pop()
      break
    case "VoltageLevel":
      voltageLevelStack.pop()
      break
    case "Bay":
      bayStack.pop()
      break
    case "ConductingEquipment":
    case "PowerTransformer":
      equipmentStack.pop()
      break
    default:
      break
  }
}

function openSubstation(model: NormalizedSclModel, substationStack: SclSubstation[], event: XmlElementEvent) {
  const name = readRequiredName(event)
  const id = makeUniqueScopedId({
    baseId: buildStableId(["substation", name]),
    existingIds: model.substations.map(substation => substation.id),
    diagnostics: model.diagnostics,
    event,
    entityKind: "Substation",
  })
  const substation: SclSubstation = {
    id,
    name,
    desc: readXmlAttribute(event.attributes, "desc"),
    coordinates: readCoordinates(event),
    lNodes: [],
    connectivityNodes: [],
    voltageLevels: [],
    powerTransformers: [],
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  model.substations.push(substation)
  substationStack.push(substation)
}

function openVoltageLevel(
  diagnostics: ScdDiagnostic[],
  substationStack: SclSubstation[],
  voltageLevelStack: SclVoltageLevel[],
  event: XmlElementEvent,
) {
  const substation = last(substationStack)
  if (!substation) {
    pushParentDiagnostic(diagnostics, event, "VoltageLevel", "Substation")
    return
  }

  const name = readRequiredName(event)
  const id = makeUniqueScopedId({
    baseId: buildChildId(substation.id, "voltageLevel", name),
    existingIds: substation.voltageLevels.map(voltageLevel => voltageLevel.id),
    diagnostics,
    event,
    entityKind: "VoltageLevel",
  })
  const voltageLevel: SclVoltageLevel = {
    id,
    name,
    coordinates: readCoordinates(event),
    voltage: null,
    lNodes: [],
    connectivityNodes: [],
    bays: [],
    substationName: substation.name,
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  substation.voltageLevels.push(voltageLevel)
  voltageLevelStack.push(voltageLevel)
}

function appendVoltage(
  diagnostics: ScdDiagnostic[],
  voltageLevelStack: SclVoltageLevel[],
  event: XmlElementEvent,
) {
  const voltageLevel = last(voltageLevelStack)
  if (!voltageLevel) {
    pushParentDiagnostic(diagnostics, event, "Voltage", "VoltageLevel")
    return
  }

  if (voltageLevel.voltage) {
    diagnostics.push({
      severity: "warning",
      stage: "parser",
      code: "parser.duplicate-voltage",
      message: `VoltageLevel "${voltageLevel.name}" has multiple Voltage elements; the first value was kept.`,
      sourcePath: event.sourcePath,
      sourceId: voltageLevel.id,
      sourceLocation: event.sourceLocation,
    })
    return
  }

  voltageLevel.voltage = readVoltage(event)
}

function openBay(
  diagnostics: ScdDiagnostic[],
  substationStack: SclSubstation[],
  voltageLevelStack: SclVoltageLevel[],
  bayStack: SclBay[],
  event: XmlElementEvent,
) {
  const substation = last(substationStack)
  const voltageLevel = last(voltageLevelStack)
  if (!substation || !voltageLevel) {
    pushParentDiagnostic(diagnostics, event, "Bay", "VoltageLevel")
    return
  }

  const name = readRequiredName(event)
  const id = makeUniqueScopedId({
    baseId: buildChildId(voltageLevel.id, "bay", name),
    existingIds: voltageLevel.bays.map(bay => bay.id),
    diagnostics,
    event,
    entityKind: "Bay",
  })
  const bay: SclBay = {
    id,
    name,
    desc: readXmlAttribute(event.attributes, "desc"),
    coordinates: readCoordinates(event),
    lNodes: [],
    connectivityNodes: [],
    equipments: [],
    substationName: substation.name,
    voltageLevelName: voltageLevel.name,
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  voltageLevel.bays.push(bay)
  bayStack.push(bay)
}

function openConductingEquipment(
  diagnostics: ScdDiagnostic[],
  substationStack: SclSubstation[],
  voltageLevelStack: SclVoltageLevel[],
  bayStack: SclBay[],
  equipmentStack: SclEquipment[],
  event: XmlElementEvent,
) {
  const substation = last(substationStack)
  const voltageLevel = last(voltageLevelStack)
  const bay = last(bayStack)
  if (!substation || !voltageLevel || !bay) {
    pushParentDiagnostic(diagnostics, event, "ConductingEquipment", "Bay")
    return
  }

  const equipment = createEquipment({
    diagnostics,
    event,
    tagName: "ConductingEquipment",
    parentId: bay.id,
    idSegment: "equipment",
    existingIds: bay.equipments.map(item => item.id),
    typeFallback: "unknown",
    substationName: substation.name,
    voltageLevelName: voltageLevel.name,
    bayName: bay.name,
  })
  bay.equipments.push(equipment)
  equipmentStack.push(equipment)
}

function openPowerTransformer(
  diagnostics: ScdDiagnostic[],
  substationStack: SclSubstation[],
  equipmentStack: SclEquipment[],
  event: XmlElementEvent,
) {
  const substation = last(substationStack)
  if (!substation) {
    pushParentDiagnostic(diagnostics, event, "PowerTransformer", "Substation")
    return
  }

  const equipment = createEquipment({
    diagnostics,
    event,
    tagName: "PowerTransformer",
    parentId: substation.id,
    idSegment: "powerTransformer",
    existingIds: substation.powerTransformers.map(item => item.id),
    typeFallback: "PTR",
    substationName: substation.name,
    voltageLevelName: null,
    bayName: null,
  })
  substation.powerTransformers.push(equipment)
  equipmentStack.push(equipment)
}

function appendTerminal(
  diagnostics: ScdDiagnostic[],
  equipmentStack: SclEquipment[],
  event: XmlElementEvent,
) {
  const equipment = last(equipmentStack)
  if (!equipment) {
    pushParentDiagnostic(diagnostics, event, "Terminal", "ConductingEquipment or PowerTransformer")
    return
  }

  const terminalName = readXmlAttribute(event.attributes, "name")
  const terminalOrdinal = equipment.terminals.length + 1
  const terminalIdPart = buildStableId([terminalName ? `${terminalName}_${terminalOrdinal}` : String(terminalOrdinal)])
  const terminal: SclTerminal = {
    id: `${equipment.id}/terminal/${terminalIdPart}`,
    name: terminalName,
    connectivityNode: readXmlAttribute(event.attributes, "connectivityNode"),
    resolvedConnectivityNodeId: null,
    resolvedConnectivityNodePath: null,
    cNodeName: readXmlAttribute(event.attributes, "cNodeName"),
    substationName: readXmlAttribute(event.attributes, "substationName"),
    voltageLevelName: readXmlAttribute(event.attributes, "voltageLevelName"),
    bayName: readXmlAttribute(event.attributes, "bayName"),
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  equipment.terminals.push(terminal)
}

function appendConnectivityNode(
  diagnostics: ScdDiagnostic[],
  substationStack: SclSubstation[],
  voltageLevelStack: SclVoltageLevel[],
  bayStack: SclBay[],
  event: XmlElementEvent,
) {
  const substation = last(substationStack)
  if (!substation) {
    pushParentDiagnostic(diagnostics, event, "ConnectivityNode", "Substation")
    return
  }

  const voltageLevel = last(voltageLevelStack)
  const bay = last(bayStack)
  const name = readXmlAttribute(event.attributes, "name")
  const pathName = readXmlAttribute(event.attributes, "pathName")
  const targetCollection = bay?.connectivityNodes ?? voltageLevel?.connectivityNodes ?? substation.connectivityNodes
  const ordinal = targetCollection.length + 1
  const idName = name ?? String(ordinal)
  const parentId = bay?.id ?? voltageLevel?.id ?? substation.id
  const id = makeUniqueScopedId({
    baseId: buildChildId(parentId, "connectivityNode", idName),
    existingIds: targetCollection.map(node => node.id),
    diagnostics,
    event,
    entityKind: "ConnectivityNode",
  })
  const node: SclConnectivityNode = {
    id,
    name,
    pathName,
    normalizedPath: normalizeConnectivityNodePath({
      pathName,
      substationName: substation.name,
      voltageLevelName: voltageLevel?.name ?? null,
      bayName: bay?.name ?? null,
      nodeName: name,
      fallbackPath: event.sourcePath,
    }),
    substationName: substation.name,
    voltageLevelName: voltageLevel?.name ?? null,
    bayName: bay?.name ?? null,
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }

  if (bay) {
    bay.connectivityNodes.push(node)
    return
  }
  if (voltageLevel) {
    voltageLevel.connectivityNodes.push(node)
    return
  }
  substation.connectivityNodes.push(node)
}

function appendLogicalNode(
  substationStack: SclSubstation[],
  voltageLevelStack: SclVoltageLevel[],
  bayStack: SclBay[],
  equipmentStack: SclEquipment[],
  event: XmlElementEvent,
) {
  const lNode: SclLogicalNodeRef = {
    iedName: readXmlAttribute(event.attributes, "iedName"),
    ldInst: readXmlAttribute(event.attributes, "ldInst"),
    lnClass: readXmlAttribute(event.attributes, "lnClass"),
    lnInst: readXmlAttribute(event.attributes, "lnInst"),
    lnType: readXmlAttribute(event.attributes, "lnType"),
    prefix: readXmlAttribute(event.attributes, "prefix"),
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }

  const equipment = last(equipmentStack)
  if (equipment) {
    equipment.lNodes.push(lNode)
    return
  }

  const bay = last(bayStack)
  if (bay) {
    bay.lNodes.push(lNode)
    return
  }

  const voltageLevel = last(voltageLevelStack)
  if (voltageLevel) {
    voltageLevel.lNodes.push(lNode)
    return
  }

  const substation = last(substationStack)
  if (substation) {
    substation.lNodes.push(lNode)
  }
}

function appendIed(model: NormalizedSclModel, event: XmlElementEvent) {
  const name = readRequiredName(event)
  const id = makeUniqueScopedId({
    baseId: buildStableId(["ied", name]),
    existingIds: model.ieds.map(ied => ied.id),
    diagnostics: model.diagnostics,
    event,
    entityKind: "IED",
  })
  const ied: SclIed = {
    id,
    name,
    desc: readXmlAttribute(event.attributes, "desc"),
    manufacturer: readXmlAttribute(event.attributes, "manufacturer"),
    type: readXmlAttribute(event.attributes, "type"),
    configVersion: readXmlAttribute(event.attributes, "configVersion"),
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  model.ieds.push(ied)
}

function createEquipment(input: {
  diagnostics: ScdDiagnostic[]
  event: XmlElementEvent
  tagName: "ConductingEquipment" | "PowerTransformer"
  parentId: string
  idSegment: "equipment" | "powerTransformer"
  existingIds: string[]
  typeFallback: string
  substationName: string | null
  voltageLevelName: string | null
  bayName: string | null
}): SclEquipment {
  const name = readRequiredName(input.event)
  const type = readXmlAttribute(input.event.attributes, "type") ?? input.typeFallback
  const id = makeUniqueScopedId({
    baseId: buildChildId(input.parentId, input.idSegment, name),
    existingIds: input.existingIds,
    diagnostics: input.diagnostics,
    event: input.event,
    entityKind: input.tagName,
  })

  return {
    id,
    name,
    desc: readXmlAttribute(input.event.attributes, "desc"),
    type,
    kind: normalizeEquipmentKind(type, input.tagName),
    tagName: input.tagName,
    coordinates: readCoordinates(input.event),
    terminals: [],
    lNodes: [],
    substationName: input.substationName,
    voltageLevelName: input.voltageLevelName,
    bayName: input.bayName,
    sourcePath: input.event.sourcePath,
    sourceLocation: input.event.sourceLocation,
  }
}

function normalizeEquipmentKind(type: string, tagName: "ConductingEquipment" | "PowerTransformer"): SclEquipmentKind {
  if (tagName === "PowerTransformer") {
    return "transformer"
  }

  switch (type.trim().toUpperCase()) {
    case "CBR":
      return "breaker"
    case "DIS":
      return "disconnector"
    case "BBS":
      return "busbar"
    case "PTR":
      return "transformer"
    case "VTR":
    case "TCTR":
      return "measurement"
    case "IFL":
      return "feeder"
    case "GND":
      return "ground"
    default:
      return "unknown"
  }
}

function readCoordinates(event: XmlElementEvent): SldCoordinate {
  return {
    x: parseNullableNumber(readXmlAttribute(event.attributes, "sxy:x") ?? readXmlAttributeByLocalName(event.attributes, "x")),
    y: parseNullableNumber(readXmlAttribute(event.attributes, "sxy:y") ?? readXmlAttributeByLocalName(event.attributes, "y")),
  }
}

function readVoltage(event: XmlElementEvent): SclVoltage {
  return {
    value: event.textContent,
    multiplier: readXmlAttribute(event.attributes, "multiplier"),
    unit: readXmlAttribute(event.attributes, "unit"),
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
}

function readRequiredName(event: XmlElementEvent): string {
  return readXmlAttribute(event.attributes, "name")?.trim() || "unnamed"
}

function normalizeTerminalConnectivityReferences(model: NormalizedSclModel) {
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

function normalizeConnectivityNodePath(input: {
  pathName: string | null
  substationName: string | null
  voltageLevelName: string | null
  bayName: string | null
  nodeName: string | null
  fallbackPath: string
}): string {
  if (input.pathName?.trim()) {
    return normalizePath(input.pathName)
  }

  return normalizePathParts([
    input.substationName,
    input.voltageLevelName,
    input.bayName,
    input.nodeName,
  ]) ?? input.fallbackPath
}

function normalizePath(value: string): string {
  return value.split("/").map(part => part.trim()).filter(Boolean).join("/")
}

function normalizePathParts(parts: Array<string | null>): string | null {
  const path = parts.map(part => part?.trim() ?? "").filter(Boolean).join("/")
  return path || null
}

function uniqueStrings(items: string[]): string[] {
  return Array.from(new Set(items))
}

function isPresent<T>(value: T | null | undefined): value is T {
  return value !== null && value !== undefined
}

function makeUniqueScopedId(input: {
  baseId: string
  existingIds: string[]
  diagnostics: ScdDiagnostic[]
  event: XmlElementEvent
  entityKind: string
}): string {
  const existingIds = new Set(input.existingIds)
  if (!existingIds.has(input.baseId)) {
    return input.baseId
  }

  let suffix = 2
  let id = `${input.baseId}__${suffix}`
  while (existingIds.has(id)) {
    suffix += 1
    id = `${input.baseId}__${suffix}`
  }

  input.diagnostics.push({
    severity: "warning",
    stage: "normalizer",
    code: "normalizer.duplicate-normalized-id",
    message: `${input.entityKind} normalized id "${input.baseId}" is duplicated in the same parent scope; it was disambiguated as "${id}".`,
    sourcePath: input.event.sourcePath,
    sourceId: id,
    sourceLocation: input.event.sourceLocation,
  })

  return id
}

function buildChildId(parentId: string, childKind: string, childName: string): string {
  return `${parentId}/${buildStableId([childKind, childName])}`
}

function parseNullableNumber(value: string | null): number | null {
  if (value === null || value.trim() === "") {
    return null
  }
  const numeric = Number(value)
  return Number.isFinite(numeric) ? numeric : null
}

function pushParentDiagnostic(
  diagnostics: ScdDiagnostic[],
  event: XmlElementEvent,
  child: string,
  parent: string,
) {
  diagnostics.push({
    severity: "warning",
    stage: "parser",
    code: "parser.missing-parent",
    message: `${child} is outside ${parent}; it was skipped.`,
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  })
}

function buildStableId(parts: string[]): string {
  return parts
    .map(part => part.trim())
    .filter(Boolean)
    .map(part => part.replace(/[\s/]+/g, "_"))
    .join("/")
}

function last<T>(items: T[]): T | null {
  return items[items.length - 1] ?? null
}
