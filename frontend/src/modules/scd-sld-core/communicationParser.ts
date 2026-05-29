import type {
  Iec61850ReportSubscriptionCandidate,
  NormalizedSclModel,
  ScdDiagnostic,
  SclAccessPoint,
  SclConnectedAccessPoint,
  SclDataSet,
  SclDataSetMember,
  SclDataSetMemberKind,
  SclIed,
  SclLogicalDevice,
  SclLogicalNode,
  SclReportControl,
  SclReportEnabled,
  SclServer,
} from "./types"
import {
  findXmlElementRanges,
  readXmlAttribute,
  scanXmlElements,
  type XmlElementEvent,
} from "./xmlScanner"
import {
  buildChildId,
  buildStableId,
  last,
  makeUniqueScopedId,
  mergeUniqueDiagnostics,
  parseBooleanAttribute,
  parseNullableInteger,
  pushParentDiagnostic,
  readRequiredName,
} from "./parserUtils"

export function parseIedCommunicationModel(xmlText: string, diagnostics: ScdDiagnostic[]): SclIed[] {
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

  for (const range of findXmlElementRanges(xmlText, ["IED"], localDiagnostics)) {
    for (const event of scanXmlElements(range.text, localDiagnostics, {
      baseOffset: range.startOffset,
      baseLine: range.sourceLocation.line,
      baseColumn: range.sourceLocation.column,
      sourcePathPrefix: ["SCL:#1"],
    })) {
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
  }

  applyConnectedAccessPoints(xmlText, ieds, localDiagnostics)
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
    connectedAccessPoints: [],
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  ied.accessPoints.push(accessPoint)
  accessPointStack.push(accessPoint)
}

type ParsedSubNetwork = {
  name: string | null
  type: string | null
}

function applyConnectedAccessPoints(
  xmlText: string,
  ieds: SclIed[],
  diagnostics: ScdDiagnostic[],
) {
  const accessPointByKey = new Map<string, SclAccessPoint>()
  for (const ied of ieds) {
    for (const accessPoint of ied.accessPoints) {
      accessPointByKey.set(buildConnectedAccessPointKey(ied.name, accessPoint.name), accessPoint)
    }
  }

  for (const connectedAccessPoint of parseConnectedAccessPoints(xmlText, diagnostics)) {
    if (!connectedAccessPoint.iedName || !connectedAccessPoint.accessPointName) {
      continue
    }

    const accessPoint = accessPointByKey.get(buildConnectedAccessPointKey(
      connectedAccessPoint.iedName,
      connectedAccessPoint.accessPointName,
    ))
    if (!accessPoint) {
      continue
    }

    accessPoint.connectedAccessPoints.push(connectedAccessPoint)
  }
}

function parseConnectedAccessPoints(
  xmlText: string,
  diagnostics: ScdDiagnostic[],
): SclConnectedAccessPoint[] {
  const connectedAccessPoints: SclConnectedAccessPoint[] = []
  const subNetworkStack: ParsedSubNetwork[] = []
  const connectedAccessPointStack: SclConnectedAccessPoint[] = []
  let addressDepth = 0

  for (const range of findXmlElementRanges(xmlText, ["Communication"], diagnostics)) {
    for (const event of scanXmlElements(range.text, diagnostics, {
      baseOffset: range.startOffset,
      baseLine: range.sourceLocation.line,
      baseColumn: range.sourceLocation.column,
      sourcePathPrefix: ["SCL:#1"],
    })) {
      if (event.kind === "close") {
        handleConnectedAccessPointCloseEvent(event, subNetworkStack, connectedAccessPointStack, () => {
          addressDepth = Math.max(0, addressDepth - 1)
        })
        continue
      }

      switch (event.localName) {
        case "SubNetwork":
          subNetworkStack.push({
            name: readXmlAttribute(event.attributes, "name"),
            type: readXmlAttribute(event.attributes, "type"),
          })
          break
        case "ConnectedAP":
          openConnectedAccessPoint(
            connectedAccessPoints,
            connectedAccessPointStack,
            subNetworkStack,
            event,
          )
          break
        case "Address":
          if (connectedAccessPointStack.length) {
            addressDepth += 1
          }
          break
        case "P":
          appendAddressParameter(connectedAccessPointStack, addressDepth, event)
          break
        default:
          break
      }

      if (event.selfClosing) {
        handleConnectedAccessPointCloseEvent(event, subNetworkStack, connectedAccessPointStack, () => {
          addressDepth = Math.max(0, addressDepth - 1)
        })
      }
    }
  }

  return connectedAccessPoints
}

function handleConnectedAccessPointCloseEvent(
  event: XmlElementEvent,
  subNetworkStack: ParsedSubNetwork[],
  connectedAccessPointStack: SclConnectedAccessPoint[],
  closeAddress: () => void,
) {
  switch (event.localName) {
    case "SubNetwork":
      subNetworkStack.pop()
      break
    case "ConnectedAP":
      connectedAccessPointStack.pop()
      break
    case "Address":
      closeAddress()
      break
    default:
      break
  }
}

function openConnectedAccessPoint(
  connectedAccessPoints: SclConnectedAccessPoint[],
  connectedAccessPointStack: SclConnectedAccessPoint[],
  subNetworkStack: ParsedSubNetwork[],
  event: XmlElementEvent,
) {
  const subNetwork = last(subNetworkStack) ?? null
  const connectedAccessPoint: SclConnectedAccessPoint = {
    iedName: readXmlAttribute(event.attributes, "iedName"),
    accessPointName: readXmlAttribute(event.attributes, "apName"),
    subNetworkName: subNetwork?.name ?? null,
    subNetworkType: subNetwork?.type ?? null,
    ipAddress: null,
    ipSubnet: null,
    ipGateway: null,
    addressParameters: [],
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }

  connectedAccessPoints.push(connectedAccessPoint)
  connectedAccessPointStack.push(connectedAccessPoint)
}

function appendAddressParameter(
  connectedAccessPointStack: SclConnectedAccessPoint[],
  addressDepth: number,
  event: XmlElementEvent,
) {
  const connectedAccessPoint = last(connectedAccessPointStack)
  if (!connectedAccessPoint || addressDepth <= 0) {
    return
  }

  const parameter = {
    type: readXmlAttribute(event.attributes, "type"),
    value: event.textContent,
    sourcePath: event.sourcePath,
    sourceLocation: event.sourceLocation,
  }
  connectedAccessPoint.addressParameters.push(parameter)

  switch (normalizeAddressParameterType(parameter.type)) {
    case "IP":
      connectedAccessPoint.ipAddress ??= parameter.value
      break
    case "IP-SUBNET":
      connectedAccessPoint.ipSubnet ??= parameter.value
      break
    case "IP-GATEWAY":
      connectedAccessPoint.ipGateway ??= parameter.value
      break
    default:
      break
  }
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

export function normalizeReportDataSetReferences(model: NormalizedSclModel) {
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

function buildConnectedAccessPointKey(iedName: string, accessPointName: string): string {
  return `${iedName}\u0000${accessPointName}`
}

function normalizeAddressParameterType(value: string | null): string {
  return value?.trim().toUpperCase() ?? ""
}

function formatLogicalNodeName(prefix: string | null, lnClass: string, lnInst: string | null): string {
  return `${prefix?.trim() ?? ""}${lnClass.trim() || "unknown"}${lnInst?.trim() ?? ""}`
}
