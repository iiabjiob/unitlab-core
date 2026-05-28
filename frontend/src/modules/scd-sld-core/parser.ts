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
    })
    return model
  }

  const substationStack: SclSubstation[] = []
  const voltageLevelStack: SclVoltageLevel[] = []
  const bayStack: SclBay[] = []
  const equipmentStack: SclEquipment[] = []

  for (const event of scanXmlElements(source.xmlText, diagnostics)) {
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
  const substation: SclSubstation = {
    id: buildStableId(["substation", name]),
    name,
    desc: readXmlAttribute(event.attributes, "desc"),
    coordinates: readCoordinates(event),
    lNodes: [],
    connectivityNodes: [],
    voltageLevels: [],
    powerTransformers: [],
    sourcePath: event.sourcePath,
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
  const voltageLevel: SclVoltageLevel = {
    id: buildStableId(["substation", substation.name, "voltageLevel", name]),
    name,
    voltage: null,
    lNodes: [],
    connectivityNodes: [],
    bays: [],
    substationName: substation.name,
    sourcePath: event.sourcePath,
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
  const bay: SclBay = {
    id: buildStableId(["substation", substation.name, "voltageLevel", voltageLevel.name, "bay", name]),
    name,
    desc: readXmlAttribute(event.attributes, "desc"),
    coordinates: readCoordinates(event),
    lNodes: [],
    connectivityNodes: [],
    equipments: [],
    substationName: substation.name,
    voltageLevelName: voltageLevel.name,
    sourcePath: event.sourcePath,
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
    event,
    tagName: "ConductingEquipment",
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
    event,
    tagName: "PowerTransformer",
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
  const node: SclConnectivityNode = {
    id: buildStableId([
      "substation",
      substation.name,
      "voltageLevel",
      voltageLevel?.name ?? "none",
      "bay",
      bay?.name ?? "none",
      "connectivityNode",
      name ?? String((bay?.connectivityNodes.length ?? voltageLevel?.connectivityNodes.length ?? substation.connectivityNodes.length) + 1),
    ]),
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
  const ied: SclIed = {
    id: buildStableId(["ied", name]),
    name,
    desc: readXmlAttribute(event.attributes, "desc"),
    manufacturer: readXmlAttribute(event.attributes, "manufacturer"),
    type: readXmlAttribute(event.attributes, "type"),
    configVersion: readXmlAttribute(event.attributes, "configVersion"),
    sourcePath: event.sourcePath,
  }
  model.ieds.push(ied)
}

function createEquipment(input: {
  event: XmlElementEvent
  tagName: "ConductingEquipment" | "PowerTransformer"
  typeFallback: string
  substationName: string | null
  voltageLevelName: string | null
  bayName: string | null
}): SclEquipment {
  const name = readRequiredName(input.event)
  const type = readXmlAttribute(input.event.attributes, "type") ?? input.typeFallback
  const idParts = [
    "substation",
    input.substationName ?? "none",
    ...(input.voltageLevelName ? ["voltageLevel", input.voltageLevelName] : []),
    ...(input.bayName ? ["bay", input.bayName] : []),
    input.tagName === "PowerTransformer" ? "powerTransformer" : "equipment",
    name,
  ]

  return {
    id: buildStableId(idParts),
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
  }
}

function readRequiredName(event: XmlElementEvent): string {
  return readXmlAttribute(event.attributes, "name")?.trim() || "unnamed"
}

function normalizeTerminalConnectivityReferences(model: NormalizedSclModel) {
  const declaredNodes = collectConnectivityNodes(model)
  const declaredNodesByPath = new Map<string, SclConnectivityNode>()

  for (const node of declaredNodes) {
    const existing = declaredNodesByPath.get(node.normalizedPath)
    if (existing) {
      model.diagnostics.push({
        severity: "warning",
        stage: "normalizer",
        code: "normalizer.duplicate-connectivity-node",
        message: `Duplicate connectivity node "${node.normalizedPath}" was collapsed by path for terminal resolution.`,
        sourcePath: node.sourcePath,
        sourceId: node.id,
      })
      continue
    }
    declaredNodesByPath.set(node.normalizedPath, node)
  }

  for (const equipment of collectEquipment(model)) {
    for (const terminal of equipment.terminals) {
      const normalizedPath = normalizeTerminalConnectivityPath(terminal)
      terminal.resolvedConnectivityNodePath = normalizedPath

      if (!normalizedPath) {
        continue
      }

      const declaredNode = declaredNodesByPath.get(normalizedPath)
      terminal.resolvedConnectivityNodeId = declaredNode?.id ?? null

      if (!declaredNode) {
        model.diagnostics.push({
          severity: "warning",
          stage: "normalizer",
          code: "normalizer.unresolved-connectivity-node",
          message: `Terminal references connectivity node "${normalizedPath}" that is not declared in the parsed SCL topology.`,
          sourcePath: terminal.sourcePath,
          sourceId: terminal.id,
        })
      }
    }
  }
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

function normalizeTerminalConnectivityPath(terminal: SclTerminal): string | null {
  if (terminal.connectivityNode?.trim()) {
    return normalizePath(terminal.connectivityNode)
  }

  return normalizePathParts([
    terminal.substationName,
    terminal.voltageLevelName,
    terminal.bayName,
    terminal.cNodeName,
  ])
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
