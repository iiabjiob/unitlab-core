import type {
  Iec61850ReportSubscriptionCandidate,
  NormalizedSclModel,
  ScdDiagnostic,
  ScdSource,
  SclAccessPoint,
  SclBay,
  SclConnectivityNode,
  SclDataSet,
  SclDataSetMember,
  SclDataSetMemberKind,
  SclEquipment,
  SclEquipmentKind,
  SclIed,
  SclLogicalDevice,
  SclLogicalNode,
  SclLogicalNodeRef,
  SclReportControl,
  SclReportEnabled,
  SclSubstation,
  SclServer,
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
    reportSubscriptions: [],
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

    if (event.selfClosing) {
      handleCloseEvent(event, substationStack, voltageLevelStack, bayStack, equipmentStack)
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

  model.ieds = parseIedCommunicationModel(source.xmlText, model.diagnostics)
  normalizeReportDataSetReferences(model)
  model.reportSubscriptions = buildIec61850ReportSubscriptionInventory(model)
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
    accessPoints: [],
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  model.ieds.push(ied)
}

function parseIedCommunicationModel(xmlText: string, diagnostics: ScdDiagnostic[]): SclIed[] {
  const localDiagnostics: ScdDiagnostic[] = []
  const ieds: SclIed[] = []
  const iedStack: SclIed[] = []
  const accessPointStack: SclAccessPoint[] = []
  const serverStack: SclServer[] = []
  const logicalDeviceStack: SclLogicalDevice[] = []
  const logicalNodeStack: SclLogicalNode[] = []
  const dataSetStack: SclDataSet[] = []
  const reportControlStack: SclReportControl[] = []
  const rptEnabledStack: SclReportEnabled[] = []

  for (const event of scanXmlElements(xmlText, localDiagnostics)) {
    if (event.kind === "close") {
      handleIedCommunicationCloseEvent(
        event,
        iedStack,
        accessPointStack,
        serverStack,
        logicalDeviceStack,
        logicalNodeStack,
        dataSetStack,
        reportControlStack,
        rptEnabledStack,
      )
      continue
    }

    switch (event.localName) {
      case "IED":
        openCommunicationIed(ieds, iedStack, localDiagnostics, event)
        break
      case "AccessPoint":
        openAccessPoint(iedStack, accessPointStack, localDiagnostics, event)
        break
      case "Server":
        openServer(accessPointStack, serverStack, localDiagnostics, event)
        break
      case "LDevice":
        openLogicalDevice(serverStack, logicalDeviceStack, localDiagnostics, event)
        break
      case "LN0":
      case "LN":
        openRuntimeLogicalNode(
          iedStack,
          accessPointStack,
          logicalDeviceStack,
          logicalNodeStack,
          localDiagnostics,
          event,
        )
        break
      case "DataSet":
        openDataSet(logicalNodeStack, dataSetStack, localDiagnostics, event)
        break
      case "FCDA":
      case "FCD":
        appendDataSetMember(dataSetStack, localDiagnostics, event)
        break
      case "ReportControl":
        openReportControl(logicalNodeStack, reportControlStack, localDiagnostics, event)
        break
      case "TrgOps":
        applyReportTriggerOptions(reportControlStack, localDiagnostics, event)
        break
      case "OptFields":
        applyReportOptionalFields(reportControlStack, localDiagnostics, event)
        break
      case "RptEnabled":
        openRptEnabled(reportControlStack, rptEnabledStack, localDiagnostics, event)
        break
      case "ClientLN":
        appendReportClient(rptEnabledStack, localDiagnostics, event)
        break
      default:
        break
    }

    if (event.selfClosing) {
      handleIedCommunicationCloseEvent(
        event,
        iedStack,
        accessPointStack,
        serverStack,
        logicalDeviceStack,
        logicalNodeStack,
        dataSetStack,
        reportControlStack,
        rptEnabledStack,
      )
    }
  }

  mergeUniqueDiagnostics(diagnostics, localDiagnostics)
  return ieds
}

function handleIedCommunicationCloseEvent(
  event: XmlElementEvent,
  iedStack: SclIed[],
  accessPointStack: SclAccessPoint[],
  serverStack: SclServer[],
  logicalDeviceStack: SclLogicalDevice[],
  logicalNodeStack: SclLogicalNode[],
  dataSetStack: SclDataSet[],
  reportControlStack: SclReportControl[],
  rptEnabledStack: SclReportEnabled[],
) {
  switch (event.localName) {
    case "IED":
      iedStack.pop()
      break
    case "AccessPoint":
      accessPointStack.pop()
      break
    case "Server":
      serverStack.pop()
      break
    case "LDevice":
      logicalDeviceStack.pop()
      break
    case "LN0":
    case "LN":
      logicalNodeStack.pop()
      break
    case "DataSet":
      dataSetStack.pop()
      break
    case "ReportControl":
      reportControlStack.pop()
      break
    case "RptEnabled":
      rptEnabledStack.pop()
      break
    default:
      break
  }
}

function openCommunicationIed(
  ieds: SclIed[],
  iedStack: SclIed[],
  diagnostics: ScdDiagnostic[],
  event: XmlElementEvent,
) {
  const name = readRequiredName(event)
  const id = makeUniqueScopedId({
    baseId: buildStableId(["ied", name]),
    existingIds: ieds.map(ied => ied.id),
    diagnostics,
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
    accessPoints: [],
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  ieds.push(ied)
  iedStack.push(ied)
}

function openAccessPoint(
  iedStack: SclIed[],
  accessPointStack: SclAccessPoint[],
  diagnostics: ScdDiagnostic[],
  event: XmlElementEvent,
) {
  const ied = last(iedStack)
  if (!ied) {
    pushParentDiagnostic(diagnostics, event, "AccessPoint", "IED")
    return
  }

  const name = readRequiredName(event)
  const id = makeUniqueScopedId({
    baseId: buildChildId(ied.id, "accessPoint", name),
    existingIds: ied.accessPoints.map(accessPoint => accessPoint.id),
    diagnostics,
    event,
    entityKind: "AccessPoint",
  })
  const accessPoint: SclAccessPoint = {
    id,
    name,
    desc: readXmlAttribute(event.attributes, "desc"),
    router: parseBooleanAttribute(readXmlAttribute(event.attributes, "router")),
    clock: parseBooleanAttribute(readXmlAttribute(event.attributes, "clock")),
    server: null,
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  ied.accessPoints.push(accessPoint)
  accessPointStack.push(accessPoint)
}

function openServer(
  accessPointStack: SclAccessPoint[],
  serverStack: SclServer[],
  diagnostics: ScdDiagnostic[],
  event: XmlElementEvent,
) {
  const accessPoint = last(accessPointStack)
  if (!accessPoint) {
    pushParentDiagnostic(diagnostics, event, "Server", "AccessPoint")
    return
  }

  if (accessPoint.server) {
    diagnostics.push({
      severity: "warning",
      stage: "parser",
      code: "parser.duplicate-server",
      message: `AccessPoint "${accessPoint.name}" has multiple Server elements; the first one was kept.`,
      sourcePath: event.sourcePath,
      sourceId: accessPoint.id,
      sourceLocation: event.sourceLocation,
    })
    serverStack.push(accessPoint.server)
    return
  }

  const server: SclServer = {
    id: `${accessPoint.id}/server`,
    logicalDevices: [],
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  accessPoint.server = server
  serverStack.push(server)
}

function openLogicalDevice(
  serverStack: SclServer[],
  logicalDeviceStack: SclLogicalDevice[],
  diagnostics: ScdDiagnostic[],
  event: XmlElementEvent,
) {
  const server = last(serverStack)
  if (!server) {
    pushParentDiagnostic(diagnostics, event, "LDevice", "Server")
    return
  }

  const inst = readXmlAttribute(event.attributes, "inst")?.trim() || "unnamed"
  const id = makeUniqueScopedId({
    baseId: buildChildId(server.id, "lDevice", inst),
    existingIds: server.logicalDevices.map(device => device.id),
    diagnostics,
    event,
    entityKind: "LDevice",
  })
  const logicalDevice: SclLogicalDevice = {
    id,
    inst,
    desc: readXmlAttribute(event.attributes, "desc"),
    ldName: readXmlAttribute(event.attributes, "ldName"),
    logicalNodes: [],
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  server.logicalDevices.push(logicalDevice)
  logicalDeviceStack.push(logicalDevice)
}

function openRuntimeLogicalNode(
  iedStack: SclIed[],
  accessPointStack: SclAccessPoint[],
  logicalDeviceStack: SclLogicalDevice[],
  logicalNodeStack: SclLogicalNode[],
  diagnostics: ScdDiagnostic[],
  event: XmlElementEvent,
) {
  const ied = last(iedStack)
  const accessPoint = last(accessPointStack)
  const logicalDevice = last(logicalDeviceStack)
  if (!ied || !accessPoint || !logicalDevice) {
    pushParentDiagnostic(diagnostics, event, event.localName, "LDevice")
    return
  }

  const tagName = event.localName === "LN0" ? "LN0" : "LN"
  const lnClass = readXmlAttribute(event.attributes, "lnClass")?.trim() || (tagName === "LN0" ? "LLN0" : "unknown")
  const lnInst = tagName === "LN0" ? null : readXmlAttribute(event.attributes, "inst")
  const prefix = readXmlAttribute(event.attributes, "prefix")
  const logicalNodeName = formatLogicalNodeName(prefix, lnClass, lnInst)
  const id = makeUniqueScopedId({
    baseId: buildChildId(logicalDevice.id, "ln", logicalNodeName),
    existingIds: logicalDevice.logicalNodes.map(node => node.id),
    diagnostics,
    event,
    entityKind: event.localName,
  })
  const logicalNode: SclLogicalNode = {
    id,
    tagName,
    logicalNodeName,
    prefix,
    lnClass,
    lnInst,
    lnType: readXmlAttribute(event.attributes, "lnType"),
    desc: readXmlAttribute(event.attributes, "desc"),
    iedName: ied.name,
    accessPointName: accessPoint.name,
    logicalDeviceInst: logicalDevice.inst,
    dataSets: [],
    reportControls: [],
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  logicalDevice.logicalNodes.push(logicalNode)
  logicalNodeStack.push(logicalNode)
}

function openDataSet(
  logicalNodeStack: SclLogicalNode[],
  dataSetStack: SclDataSet[],
  diagnostics: ScdDiagnostic[],
  event: XmlElementEvent,
) {
  const logicalNode = last(logicalNodeStack)
  if (!logicalNode) {
    pushParentDiagnostic(diagnostics, event, "DataSet", "LN0 or LN")
    return
  }

  const name = readRequiredName(event)
  const id = makeUniqueScopedId({
    baseId: buildChildId(logicalNode.id, "dataSet", name),
    existingIds: logicalNode.dataSets.map(dataSet => dataSet.id),
    diagnostics,
    event,
    entityKind: "DataSet",
  })
  const dataSet: SclDataSet = {
    id,
    name,
    desc: readXmlAttribute(event.attributes, "desc"),
    iedName: logicalNode.iedName,
    accessPointName: logicalNode.accessPointName,
    logicalDeviceInst: logicalNode.logicalDeviceInst,
    logicalNodeName: logicalNode.logicalNodeName,
    members: [],
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  logicalNode.dataSets.push(dataSet)
  dataSetStack.push(dataSet)
}

function appendDataSetMember(
  dataSetStack: SclDataSet[],
  diagnostics: ScdDiagnostic[],
  event: XmlElementEvent,
) {
  const dataSet = last(dataSetStack)
  if (!dataSet) {
    pushParentDiagnostic(diagnostics, event, event.localName, "DataSet")
    return
  }

  const kind: SclDataSetMemberKind = event.localName === "FCD" ? "FCD" : "FCDA"
  const member: SclDataSetMember = {
    id: `${dataSet.id}/member/${dataSet.members.length + 1}`,
    kind,
    ldInst: readXmlAttribute(event.attributes, "ldInst"),
    prefix: readXmlAttribute(event.attributes, "prefix"),
    lnClass: readXmlAttribute(event.attributes, "lnClass"),
    lnInst: readXmlAttribute(event.attributes, "lnInst"),
    doName: readXmlAttribute(event.attributes, "doName"),
    daName: readXmlAttribute(event.attributes, "daName"),
    fc: readXmlAttribute(event.attributes, "fc"),
    ix: readXmlAttribute(event.attributes, "ix"),
    reference: "",
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  member.reference = formatDataSetMemberReference(member, dataSet)
  dataSet.members.push(member)
}

function openReportControl(
  logicalNodeStack: SclLogicalNode[],
  reportControlStack: SclReportControl[],
  diagnostics: ScdDiagnostic[],
  event: XmlElementEvent,
) {
  const logicalNode = last(logicalNodeStack)
  if (!logicalNode) {
    pushParentDiagnostic(diagnostics, event, "ReportControl", "LN0 or LN")
    return
  }

  const name = readRequiredName(event)
  const id = makeUniqueScopedId({
    baseId: buildChildId(logicalNode.id, "reportControl", name),
    existingIds: logicalNode.reportControls.map(control => control.id),
    diagnostics,
    event,
    entityKind: "ReportControl",
  })
  const reportControl: SclReportControl = {
    id,
    name,
    desc: readXmlAttribute(event.attributes, "desc"),
    rptId: readXmlAttribute(event.attributes, "rptID"),
    dataSetName: readXmlAttribute(event.attributes, "datSet"),
    dataSetId: null,
    dataSetRef: null,
    confRev: readXmlAttribute(event.attributes, "confRev"),
    buffered: parseBooleanAttribute(readXmlAttribute(event.attributes, "buffered")) ?? false,
    indexed: parseBooleanAttribute(readXmlAttribute(event.attributes, "indexed")),
    bufferTimeMs: parseNullableInteger(readXmlAttribute(event.attributes, "bufTime")),
    integrityPeriodMs: parseNullableInteger(readXmlAttribute(event.attributes, "intgPd")),
    triggerOptions: emptyReportTriggerOptions(),
    optionalFields: emptyReportOptionalFields(),
    rptEnabled: null,
    iedName: logicalNode.iedName,
    accessPointName: logicalNode.accessPointName,
    logicalDeviceInst: logicalNode.logicalDeviceInst,
    logicalNodeName: logicalNode.logicalNodeName,
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  logicalNode.reportControls.push(reportControl)
  reportControlStack.push(reportControl)
}

function applyReportTriggerOptions(
  reportControlStack: SclReportControl[],
  diagnostics: ScdDiagnostic[],
  event: XmlElementEvent,
) {
  const reportControl = last(reportControlStack)
  if (!reportControl) {
    pushParentDiagnostic(diagnostics, event, "TrgOps", "ReportControl")
    return
  }

  reportControl.triggerOptions = {
    dataChange: parseBooleanAttribute(readXmlAttribute(event.attributes, "dchg")),
    qualityChange: parseBooleanAttribute(readXmlAttribute(event.attributes, "qchg")),
    dataUpdate: parseBooleanAttribute(readXmlAttribute(event.attributes, "dupd")),
    periodic: parseBooleanAttribute(readXmlAttribute(event.attributes, "period")),
    generalInterrogation: parseBooleanAttribute(readXmlAttribute(event.attributes, "gi")),
  }
}

function applyReportOptionalFields(
  reportControlStack: SclReportControl[],
  diagnostics: ScdDiagnostic[],
  event: XmlElementEvent,
) {
  const reportControl = last(reportControlStack)
  if (!reportControl) {
    pushParentDiagnostic(diagnostics, event, "OptFields", "ReportControl")
    return
  }

  reportControl.optionalFields = {
    sequenceNumber: parseBooleanAttribute(readXmlAttribute(event.attributes, "seqNum")),
    timestamp: parseBooleanAttribute(readXmlAttribute(event.attributes, "timeStamp")),
    reasonCode: parseBooleanAttribute(readXmlAttribute(event.attributes, "reasonCode")),
    dataSetName: parseBooleanAttribute(readXmlAttribute(event.attributes, "dataSet")),
    dataReference: parseBooleanAttribute(readXmlAttribute(event.attributes, "dataRef")),
    entryId: parseBooleanAttribute(readXmlAttribute(event.attributes, "entryID")),
    configRevision: parseBooleanAttribute(readXmlAttribute(event.attributes, "configRef")),
    bufferOverflow: parseBooleanAttribute(readXmlAttribute(event.attributes, "bufOvfl")),
  }
}

function openRptEnabled(
  reportControlStack: SclReportControl[],
  rptEnabledStack: SclReportEnabled[],
  diagnostics: ScdDiagnostic[],
  event: XmlElementEvent,
) {
  const reportControl = last(reportControlStack)
  if (!reportControl) {
    pushParentDiagnostic(diagnostics, event, "RptEnabled", "ReportControl")
    return
  }

  const rptEnabled: SclReportEnabled = {
    max: parseNullableInteger(readXmlAttribute(event.attributes, "max")),
    desc: readXmlAttribute(event.attributes, "desc"),
    clients: [],
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  reportControl.rptEnabled = rptEnabled
  rptEnabledStack.push(rptEnabled)
}

function appendReportClient(
  rptEnabledStack: SclReportEnabled[],
  diagnostics: ScdDiagnostic[],
  event: XmlElementEvent,
) {
  const rptEnabled = last(rptEnabledStack)
  if (!rptEnabled) {
    pushParentDiagnostic(diagnostics, event, "ClientLN", "RptEnabled")
    return
  }

  rptEnabled.clients.push({
    iedName: readXmlAttribute(event.attributes, "iedName"),
    accessPointRef: readXmlAttribute(event.attributes, "apRef"),
    logicalDeviceInst: readXmlAttribute(event.attributes, "ldInst"),
    prefix: readXmlAttribute(event.attributes, "prefix"),
    lnClass: readXmlAttribute(event.attributes, "lnClass"),
    lnInst: readXmlAttribute(event.attributes, "lnInst"),
    desc: readXmlAttribute(event.attributes, "desc"),
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  })
}

function normalizeReportDataSetReferences(model: NormalizedSclModel) {
  const dataSetsById = new Map<string, SclDataSet>()
  const dataSetsByExactScope = new Map<string, SclDataSet>()
  const dataSetsByDeviceScope = new Map<string, SclDataSet[]>()

  for (const dataSet of collectIedDataSets(model.ieds)) {
    dataSetsById.set(dataSet.id, dataSet)
    dataSetsByExactScope.set(buildReportDataSetExactScopeKey(dataSet), dataSet)
    const deviceKey = buildReportDataSetDeviceScopeKey(dataSet)
    const scoped = dataSetsByDeviceScope.get(deviceKey) ?? []
    scoped.push(dataSet)
    dataSetsByDeviceScope.set(deviceKey, scoped)
  }

  for (const reportControl of collectReportControls(model.ieds)) {
    if (!reportControl.dataSetName) {
      continue
    }

    const exact = dataSetsByExactScope.get(buildReportControlExactScopeKey(reportControl))
    const deviceScoped = dataSetsByDeviceScope.get(buildReportControlDeviceScopeKey(reportControl)) ?? []
    const resolved = exact ?? (deviceScoped.length === 1 ? deviceScoped[0] : null)

    if (resolved) {
      reportControl.dataSetId = resolved.id
      reportControl.dataSetRef = formatDataSetReference(resolved)
      continue
    }

    reportControl.dataSetRef = formatUnresolvedDataSetReference(reportControl)
    model.diagnostics.push({
      severity: "warning",
      stage: "normalizer",
      code: "normalizer.unresolved-report-dataset",
      message: `ReportControl "${reportControl.name}" references DataSet "${reportControl.dataSetName}" that was not found in the same logical device.`,
      sourcePath: reportControl.sourcePath,
      sourceId: reportControl.id,
      sourceLocation: reportControl.sourceLocation,
    })
  }

  for (const reportControl of collectReportControls(model.ieds)) {
    if (reportControl.dataSetId && !dataSetsById.has(reportControl.dataSetId)) {
      reportControl.dataSetId = null
    }
  }
}

export function buildIec61850ReportSubscriptionInventory(
  model: NormalizedSclModel,
): Iec61850ReportSubscriptionCandidate[] {
  const dataSetsById = new Map(collectIedDataSets(model.ieds).map(dataSet => [dataSet.id, dataSet]))

  return collectReportControls(model.ieds).map((reportControl): Iec61850ReportSubscriptionCandidate => {
    const dataSet = reportControl.dataSetId ? dataSetsById.get(reportControl.dataSetId) ?? null : null
    const signals = dataSet?.members ?? []

    return {
      id: `${reportControl.id}/subscription`,
      iedName: reportControl.iedName,
      accessPointName: reportControl.accessPointName,
      logicalDeviceInst: reportControl.logicalDeviceInst,
      logicalNodeName: reportControl.logicalNodeName,
      reportControlId: reportControl.id,
      reportControlName: reportControl.name,
      reportKind: reportControl.buffered ? "buffered" : "unbuffered",
      rptId: reportControl.rptId,
      dataSetId: reportControl.dataSetId,
      dataSetRef: reportControl.dataSetRef,
      confRev: reportControl.confRev,
      indexed: reportControl.indexed,
      signalCount: signals.length,
      signals,
    }
  })
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

function emptyReportTriggerOptions(): SclReportControl["triggerOptions"] {
  return {
    dataChange: null,
    qualityChange: null,
    dataUpdate: null,
    periodic: null,
    generalInterrogation: null,
  }
}

function emptyReportOptionalFields(): SclReportControl["optionalFields"] {
  return {
    sequenceNumber: null,
    timestamp: null,
    reasonCode: null,
    dataSetName: null,
    dataReference: null,
    entryId: null,
    configRevision: null,
    bufferOverflow: null,
  }
}

function collectIedDataSets(ieds: SclIed[]): SclDataSet[] {
  return ieds.flatMap(ied => ied.accessPoints.flatMap(accessPoint => (
    accessPoint.server?.logicalDevices.flatMap(logicalDevice => (
      logicalDevice.logicalNodes.flatMap(logicalNode => logicalNode.dataSets)
    )) ?? []
  )))
}

function collectReportControls(ieds: SclIed[]): SclReportControl[] {
  return ieds.flatMap(ied => ied.accessPoints.flatMap(accessPoint => (
    accessPoint.server?.logicalDevices.flatMap(logicalDevice => (
      logicalDevice.logicalNodes.flatMap(logicalNode => logicalNode.reportControls)
    )) ?? []
  )))
}

function buildReportDataSetExactScopeKey(dataSet: SclDataSet): string {
  return [
    dataSet.iedName,
    dataSet.accessPointName,
    dataSet.logicalDeviceInst,
    dataSet.logicalNodeName,
    dataSet.name,
  ].join("\u0000")
}

function buildReportControlExactScopeKey(reportControl: SclReportControl): string {
  return [
    reportControl.iedName,
    reportControl.accessPointName,
    reportControl.logicalDeviceInst,
    reportControl.logicalNodeName,
    reportControl.dataSetName ?? "",
  ].join("\u0000")
}

function buildReportDataSetDeviceScopeKey(dataSet: SclDataSet): string {
  return [
    dataSet.iedName,
    dataSet.accessPointName,
    dataSet.logicalDeviceInst,
    dataSet.name,
  ].join("\u0000")
}

function buildReportControlDeviceScopeKey(reportControl: SclReportControl): string {
  return [
    reportControl.iedName,
    reportControl.accessPointName,
    reportControl.logicalDeviceInst,
    reportControl.dataSetName ?? "",
  ].join("\u0000")
}

function formatDataSetReference(dataSet: SclDataSet): string {
  return `${dataSet.iedName}/${dataSet.accessPointName}/${dataSet.logicalDeviceInst}/${dataSet.logicalNodeName}.${dataSet.name}`
}

function formatUnresolvedDataSetReference(reportControl: SclReportControl): string | null {
  if (!reportControl.dataSetName) {
    return null
  }
  return `${reportControl.iedName}/${reportControl.accessPointName}/${reportControl.logicalDeviceInst}/${reportControl.logicalNodeName}.${reportControl.dataSetName}`
}

function formatDataSetMemberReference(member: SclDataSetMember, dataSet: SclDataSet): string {
  const ldInst = member.ldInst?.trim() || dataSet.logicalDeviceInst
  const logicalNodeName = member.lnClass?.trim()
    ? formatLogicalNodeName(member.prefix, member.lnClass, member.lnInst)
    : dataSet.logicalNodeName
  const dataPath = [member.doName, member.daName].map(part => part?.trim() ?? "").filter(Boolean).join(".")
  const fcSuffix = member.fc?.trim() ? `[${member.fc.trim()}]` : ""
  const ixSuffix = member.ix?.trim() ? `#${member.ix.trim()}` : ""
  const objectReference = `${ldInst}/${logicalNodeName}${dataPath ? `.${dataPath}` : ""}`

  return `${objectReference}${fcSuffix}${ixSuffix}`
}

function formatLogicalNodeName(prefix: string | null, lnClass: string, lnInst: string | null): string {
  return `${prefix?.trim() ?? ""}${lnClass.trim() || "unknown"}${lnInst?.trim() ?? ""}`
}

function parseBooleanAttribute(value: string | null): boolean | null {
  const normalized = value?.trim().toLowerCase()
  if (normalized === "true" || normalized === "1") {
    return true
  }
  if (normalized === "false" || normalized === "0") {
    return false
  }
  return null
}

function parseNullableInteger(value: string | null): number | null {
  if (value === null || value.trim() === "") {
    return null
  }
  const numeric = Number.parseInt(value, 10)
  return Number.isFinite(numeric) ? numeric : null
}

function mergeUniqueDiagnostics(target: ScdDiagnostic[], source: ScdDiagnostic[]) {
  const seen = new Set(target.map(diagnosticFingerprint))
  for (const diagnostic of source) {
    const key = diagnosticFingerprint(diagnostic)
    if (seen.has(key)) {
      continue
    }
    seen.add(key)
    target.push(diagnostic)
  }
}

function diagnosticFingerprint(diagnostic: ScdDiagnostic): string {
  return [
    diagnostic.severity,
    diagnostic.stage,
    diagnostic.code,
    diagnostic.sourceId ?? "",
    diagnostic.sourcePath ?? "",
    diagnostic.sourceLocation?.line ?? "",
    diagnostic.sourceLocation?.column ?? "",
    diagnostic.message,
  ].join("\u0000")
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
