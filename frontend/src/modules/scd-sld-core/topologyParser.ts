import type {
  ScdDiagnostic,
  ScdSourceLocation,
  SclBay,
  SclConnectivityNode,
  SclEquipment,
  SclEquipmentKind,
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
import {
  buildChildId,
  buildStableId,
  createDuplicateScopedIdTracker,
  flushDuplicateIdDiagnostics,
  last,
  makeUniqueScopedId,
  normalizeConnectivityNodePath,
  parseNullableNumber,
  pushParentDiagnostic,
  readRequiredName,
} from "./parserUtils"

export type ParsedSclTopology = {
  scl: {
    version: string | null
    revision: string | null
  }
  substations: SclSubstation[]
  firstElementLocation?: ScdSourceLocation
}

export function parseSclTopology(xmlText: string, diagnostics: ScdDiagnostic[]): ParsedSclTopology {
  const result: ParsedSclTopology = {
    scl: {
      version: null,
      revision: null,
    },
    substations: [],
  }
  const duplicateIdTracker = createDuplicateScopedIdTracker()

  const substationStack: SclSubstation[] = []
  const voltageLevelStack: SclVoltageLevel[] = []
  const bayStack: SclBay[] = []
  const equipmentStack: SclEquipment[] = []

  for (const event of scanXmlElements(xmlText, diagnostics)) {
    result.firstElementLocation ??= event.sourceLocation
    if (event.kind === "close") {
      handleCloseEvent(event, substationStack, voltageLevelStack, bayStack, equipmentStack)
      continue
    }

    if (shouldStopAfterSubstationTopology(result.substations, substationStack, event)) {
      break
    }

    switch (event.localName) {
      case "SCL":
        result.scl.version = readXmlAttribute(event.attributes, "version")
        result.scl.revision = readXmlAttribute(event.attributes, "revision")
        break
      case "Substation":
        openSubstation(result.substations, diagnostics, substationStack, event, duplicateIdTracker)
        break
      case "VoltageLevel":
        openVoltageLevel(diagnostics, substationStack, voltageLevelStack, event, duplicateIdTracker)
        break
      case "Voltage":
        appendVoltage(diagnostics, voltageLevelStack, event)
        break
      case "Bay":
        openBay(diagnostics, substationStack, voltageLevelStack, bayStack, event, duplicateIdTracker)
        break
      case "ConductingEquipment":
        openConductingEquipment(diagnostics, substationStack, voltageLevelStack, bayStack, equipmentStack, event, duplicateIdTracker)
        break
      case "PowerTransformer":
        openPowerTransformer(diagnostics, substationStack, equipmentStack, event, duplicateIdTracker)
        break
      case "Terminal":
        appendTerminal(diagnostics, equipmentStack, event)
        break
      case "ConnectivityNode":
        appendConnectivityNode(diagnostics, substationStack, voltageLevelStack, bayStack, event, duplicateIdTracker)
        break
      case "LNode":
        appendLogicalNode(substationStack, voltageLevelStack, bayStack, equipmentStack, event)
        break
      default:
        break
    }

    if (event.selfClosing) {
      handleCloseEvent(event, substationStack, voltageLevelStack, bayStack, equipmentStack)
    }
  }

  flushDuplicateIdDiagnostics(duplicateIdTracker, diagnostics)
  return result
}

function shouldStopAfterSubstationTopology(
  substations: SclSubstation[],
  substationStack: SclSubstation[],
  event: XmlElementEvent,
): boolean {
  return substations.length > 0
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

function openSubstation(
  substations: SclSubstation[],
  diagnostics: ScdDiagnostic[],
  substationStack: SclSubstation[],
  event: XmlElementEvent,
  duplicateTracker: ReturnType<typeof createDuplicateScopedIdTracker>,
) {
  const name = readRequiredName(event)
  const id = makeUniqueScopedId({
    baseId: buildStableId(["substation", name]),
    existingIds: substations.map(substation => substation.id),
    diagnostics,
    event,
    entityKind: "Substation",
    duplicateTracker,
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
  substations.push(substation)
  substationStack.push(substation)
}

function openVoltageLevel(
  diagnostics: ScdDiagnostic[],
  substationStack: SclSubstation[],
  voltageLevelStack: SclVoltageLevel[],
  event: XmlElementEvent,
  duplicateTracker: ReturnType<typeof createDuplicateScopedIdTracker>,
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
    duplicateTracker,
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
  duplicateTracker: ReturnType<typeof createDuplicateScopedIdTracker>,
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
    duplicateTracker,
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
  duplicateTracker: ReturnType<typeof createDuplicateScopedIdTracker>,
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
    duplicateTracker,
  })
  bay.equipments.push(equipment)
  equipmentStack.push(equipment)
}

function openPowerTransformer(
  diagnostics: ScdDiagnostic[],
  substationStack: SclSubstation[],
  equipmentStack: SclEquipment[],
  event: XmlElementEvent,
  duplicateTracker: ReturnType<typeof createDuplicateScopedIdTracker>,
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
    duplicateTracker,
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
  duplicateTracker: ReturnType<typeof createDuplicateScopedIdTracker>,
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
    duplicateTracker,
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
  duplicateTracker: ReturnType<typeof createDuplicateScopedIdTracker>
}): SclEquipment {
  const name = readRequiredName(input.event)
  const type = readXmlAttribute(input.event.attributes, "type") ?? input.typeFallback
  const id = makeUniqueScopedId({
    baseId: buildChildId(input.parentId, input.idSegment, name),
    existingIds: input.existingIds,
    diagnostics: input.diagnostics,
    event: input.event,
    entityKind: input.tagName,
    duplicateTracker: input.duplicateTracker,
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
