import type { Iec61850SclIedDiscoveryResponse, Iec61850SclImportResponse } from "@/api/iec61850Client.api"

export type Iec61850NativeTreeNodeKind =
  | "import"
  | "logical-devices-group"
  | "logical-device"
  | "logical-node"
  | "datasets-group"
  | "dataset"
  | "dataset-member"
  | "reports-group"
  | "report-control"
  | "network-group"
  | "connected-access-point"
  | "ied-index"
  | "report-dataset-link"
  | "report-signal-preview"

export type Iec61850NativeDetailRow = {
  label: string
  value: string
}

export type Iec61850NativeTreeRow = {
  value: string
  parent: string | null
  kind: Iec61850NativeTreeNodeKind
  label: string
  meta: string | null
  isLeaf: boolean
  detail: {
    title: string
    subtitle: string
    rows: Iec61850NativeDetailRow[]
  }
}

export type Iec61850NativeStats = {
  logicalDevices: number
  logicalNodes: number
  dataSets: number
  reports: number
  signals: number
  errors: number
  warnings: number
}

export type Iec61850NativeTreeDocument = {
  stats: Iec61850NativeStats
  rows: Iec61850NativeTreeRow[]
}

type NativeRecord = Record<string, unknown>


export function buildIec61850IedDiscoveryTreeDocument(response: Iec61850SclIedDiscoveryResponse, filename: string | null): Iec61850NativeTreeDocument {
  const root = "discovery:root"
  const rows: Iec61850NativeTreeRow[] = [{
    value: root,
    parent: null,
    kind: "import",
    label: filename ?? "SCD",
    meta: response.schema,
    isLeaf: false,
    detail: {
      title: filename ?? "SCD",
      subtitle: "SCL IED discovery index",
      rows: [
        { label: "schema", value: response.schema },
        { label: "source size", value: String(response.sourceSize) },
        { label: "IED devices", value: String(response.ieds.length) },
      ],
    },
  }]

  const iedGroup = "discovery:ieds"
  rows.push(groupRow(iedGroup, root, "logical-devices-group", "IED devices", String(response.ieds.length)))
  for (const ied of response.ieds) {
    rows.push({
      value: `discovery:ied:${ied.name}`,
      parent: iedGroup,
      kind: "ied-index",
      label: ied.name,
      meta: `${ied.accessPointCount} AP`,
      isLeaf: true,
      detail: {
        title: ied.name,
        subtitle: "discovered IED",
        rows: [
          { label: "name", value: ied.name },
          { label: "AccessPoints", value: String(ied.accessPointCount) },
          { label: "runtime model", value: "not compiled" },
        ],
      },
    })
  }

  return {
    stats: {
      logicalDevices: 0,
      logicalNodes: response.ieds.length,
      dataSets: 0,
      reports: 0,
      signals: 0,
      errors: response.diagnostics.filter(diagnostic => diagnostic.severity === "error").length,
      warnings: response.diagnostics.filter(diagnostic => diagnostic.severity === "warning").length,
    },
    rows,
  }
}

export function buildIec61850NativeTreeDocument(response: Iec61850SclImportResponse): Iec61850NativeTreeDocument {
  const model = response.normalized_model ?? {}
  const logicalDevices = asRecords(model.logicalDevices)
  const logicalNodes = asRecords(model.logicalNodes)
  const dataSets = asRecords(model.dataSets)
  const reports = asRecords(model.reports)
  const signals = asRecords(model.signals)
  const network = recordValue(model.network)
  const connectedAccessPoints = asRecords(network.connectedAccessPoints)
  const rows: Iec61850NativeTreeRow[] = []
  const root = "import:root"

  rows.push({
    value: root,
    parent: null,
    kind: "import",
    label: response.selected_ied,
    meta: response.normalized_schema,
    isLeaf: false,
    detail: {
      title: response.selected_ied,
      subtitle: response.source_filename ?? "compiled SCL import",
      rows: [
        { label: "import id", value: response.import_id },
        { label: "workspace", value: String(response.workspace_id) },
        { label: "schema", value: response.normalized_schema },
        { label: "source size", value: String(response.source_size) },
        { label: "source hash", value: response.source_hash },
        { label: "ConnectedAP", value: String(connectedAccessPoints.length) },
        { label: "IP addresses", value: connectedAccessPoints.map(formatConnectedAccessPointAddress).filter(Boolean).join(", ") },
      ],
    },
  })


  const networkGroup = "group:network"
  rows.push(groupRow(networkGroup, root, "network-group", "Network", `${connectedAccessPoints.length}`))
  for (const [index, connectedAccessPoint] of connectedAccessPoints.entries()) {
    const apName = text(connectedAccessPoint.accessPointName) || `AP ${index + 1}`
    const subNetworkName = text(connectedAccessPoint.subNetworkName)
    const ip = addressValue(connectedAccessPoint, "IP")
    rows.push({
      value: `network:connected-ap:${index}`,
      parent: networkGroup,
      kind: "connected-access-point",
      label: apName,
      meta: ip || subNetworkName,
      isLeaf: true,
      detail: {
        title: apName,
        subtitle: subNetworkName || "ConnectedAP",
        rows: [
          { label: "IED", value: text(connectedAccessPoint.iedName) },
          { label: "AccessPoint", value: apName },
          { label: "SubNetwork", value: subNetworkName },
          { label: "SubNetwork type", value: text(connectedAccessPoint.subNetworkType) },
          { label: "IP", value: ip },
          { label: "IP-SUBNET", value: addressValue(connectedAccessPoint, "IP-SUBNET") },
          { label: "IP-GATEWAY", value: addressValue(connectedAccessPoint, "IP-GATEWAY") },
          { label: "OSI-AP-Title", value: text(connectedAccessPoint.osiApTitle) || addressValue(connectedAccessPoint, "OSI-AP-Title") },
          { label: "OSI-AE-Qualifier", value: text(connectedAccessPoint.osiAeQualifier) || addressValue(connectedAccessPoint, "OSI-AE-Qualifier") },
          { label: "OSI-PSEL", value: text(connectedAccessPoint.osiPSelector) || addressValue(connectedAccessPoint, "OSI-PSEL") },
          { label: "OSI-SSEL", value: text(connectedAccessPoint.osiSSelector) || addressValue(connectedAccessPoint, "OSI-SSEL") },
          { label: "OSI-TSEL", value: text(connectedAccessPoint.osiTSelector) || addressValue(connectedAccessPoint, "OSI-TSEL") },
          ...addressParameterRows(connectedAccessPoint),
        ],
      },
    })
  }

  const dataSetsByScope = new Map<string, NativeRecord[]>()
  const reportsByScope = new Map<string, NativeRecord[]>()
  dataSets.forEach((dataSet, index) => {
    const key = scopeKey(text(dataSet.logicalDeviceInst), text(dataSet.logicalNodeName))
    const bucket = dataSetsByScope.get(key) ?? []
    bucket.push({ ...dataSet, __nativeIndex: index })
    dataSetsByScope.set(key, bucket)
  })
  reports.forEach((report, index) => {
    const key = scopeKey(text(report.logicalDeviceInst), text(report.logicalNodeName))
    const bucket = reportsByScope.get(key) ?? []
    bucket.push({ ...report, __nativeIndex: index })
    reportsByScope.set(key, bucket)
  })

  const ldGroup = "group:logical-devices"
  rows.push(groupRow(ldGroup, root, "logical-devices-group", "Logical devices", `${logicalDevices.length}`))
  for (const device of logicalDevices) {
    const inst = text(device.inst)
    if (!inst) continue
    const ldValue = `ld:${inst}`
    const childNodes = logicalNodes.filter(node => text(node.logicalDeviceInst) === inst)
    const deviceSignals = signals.filter(signal => text(signal.logicalDeviceInst) === inst)
    const deviceDataSets = dataSets.filter(dataSet => text(dataSet.logicalDeviceInst) === inst)
    const deviceReports = reports.filter(report => text(report.logicalDeviceInst) === inst)
    rows.push({
      value: ldValue,
      parent: ldGroup,
      kind: "logical-device",
      label: inst,
      meta: `${childNodes.length} LN · ${deviceDataSets.length} DS · ${deviceReports.length} RCB`,
      isLeaf: childNodes.length === 0,
      detail: {
        title: inst,
        subtitle: "logical device",
        rows: [
          { label: "logical nodes", value: String(childNodes.length) },
          { label: "DataSets", value: String(deviceDataSets.length) },
          { label: "ReportControls", value: String(deviceReports.length) },
          { label: "signals", value: String(deviceSignals.length) },
        ],
      },
    })
    for (const node of childNodes) {
      const name = text(node.name)
      if (!name) continue
      const nodeValue = `ln:${inst}:${name}`
      const scopedDataSets = dataSetsByScope.get(scopeKey(inst, name)) ?? []
      const scopedReports = reportsByScope.get(scopeKey(inst, name)) ?? []
      const nodeSignals = signals.filter(signal => text(signal.logicalDeviceInst) === inst && text(signal.logicalNodeName) === name)
      rows.push({
        value: nodeValue,
        parent: ldValue,
        kind: "logical-node",
        label: name,
        meta: `${scopedDataSets.length} DS · ${scopedReports.length} RCB · ${nodeSignals.length} leaves`,
        isLeaf: scopedDataSets.length === 0 && scopedReports.length === 0,
        detail: {
          title: `${inst}/${name}`,
          subtitle: "logical node",
          rows: [
            { label: "logical device", value: inst },
            { label: "logical node", value: name },
            { label: "DataSets", value: String(scopedDataSets.length) },
            { label: "ReportControls", value: String(scopedReports.length) },
            { label: "resolved leaves", value: String(nodeSignals.length) },
          ],
        },
      })

      if (scopedDataSets.length > 0) {
        const dataSetGroup = `ln:${inst}:${name}:datasets`
        rows.push(groupRow(dataSetGroup, nodeValue, "datasets-group", "DataSets", `${scopedDataSets.length}`))
        for (const dataSet of scopedDataSets) {
          appendDataSetRows(rows, dataSetGroup, dataSet, signals)
        }
      }

      if (scopedReports.length > 0) {
        const reportGroup = `ln:${inst}:${name}:reports`
        rows.push(groupRow(reportGroup, nodeValue, "reports-group", "ReportControls", `${scopedReports.length}`))
        for (const report of scopedReports) {
          appendReportRows(rows, reportGroup, report, dataSets, signals)
        }
      }
    }
  }

  return {
    stats: {
      logicalDevices: logicalDevices.length,
      logicalNodes: logicalNodes.length,
      dataSets: dataSets.length,
      reports: reports.length,
      signals: signals.length,
      errors: response.diagnostics.filter(diagnostic => diagnostic.severity === "error").length,
      warnings: response.diagnostics.filter(diagnostic => diagnostic.severity === "warning").length,
    },
    rows,
  }
}


function appendDataSetRows(rows: Iec61850NativeTreeRow[], parent: string, dataSet: NativeRecord, signals: NativeRecord[]) {
  const index = numberValue(dataSet.__nativeIndex)
  if (index == null) return
  const name = text(dataSet.name) || `DataSet ${index + 1}`
  const reference = text(dataSet.reference)
  const firstSignalIndex = numberValue(dataSet.firstSignalIndex)
  const memberCount = numberValue(dataSet.memberCount)
  const members = signals.filter(signal => numberValue(signal.dataSetIndex) === index)
  const dataSetValue = `dataset:${index}`
  rows.push({
    value: dataSetValue,
    parent,
    kind: "dataset",
    label: name,
    meta: `${members.length} leaves`,
    isLeaf: members.length === 0,
    detail: {
      title: name,
      subtitle: reference,
      rows: [
        { label: "reference", value: reference },
        { label: "logical device", value: text(dataSet.logicalDeviceInst) },
        { label: "logical node", value: text(dataSet.logicalNodeName) },
        { label: "first signal index", value: formatOptionalNumber(firstSignalIndex) },
        { label: "declared member count", value: formatOptionalNumber(memberCount) },
        { label: "resolved leaves", value: String(members.length) },
      ],
    },
  })
  for (const [memberIndex, signal] of members.entries()) {
    rows.push(signalRow(`dataset-member:${index}:${memberIndex}`, dataSetValue, signal))
  }
}

function appendReportRows(rows: Iec61850NativeTreeRow[], parent: string, report: NativeRecord, dataSets: NativeRecord[], signals: NativeRecord[]) {
  const index = numberValue(report.__nativeIndex)
  if (index == null) return
  const name = text(report.name) || `Report ${index + 1}`
  const dataSetIndex = numberValue(report.dataSetIndex)
  const dataSet = dataSetIndex == null ? null : dataSets[dataSetIndex] ?? null
  const reportSignals = dataSetIndex == null ? [] : signals.filter(signal => numberValue(signal.dataSetIndex) === dataSetIndex)
  const dataSetName = dataSet ? text(dataSet.name) : ""
  const reportValue = `report:${index}`
  rows.push({
    value: reportValue,
    parent,
    kind: "report-control",
    label: name,
    meta: `${text(report.reportKind) || "report"} · ${dataSetName || text(report.dataSetRef) || "no DatSet"}`,
    isLeaf: false,
    detail: {
      title: name,
      subtitle: text(report.key),
      rows: [
        { label: "kind", value: text(report.reportKind) },
        { label: "buffered", value: String(Boolean(report.isBuffered)) },
        { label: "DataSet", value: text(report.dataSetRef) },
        { label: "DataSet name", value: dataSetName },
        { label: "DataSet index", value: formatOptionalNumber(dataSetIndex) },
        { label: "ConfRev", value: formatOptionalNumber(numberValue(report.confRev)) },
        { label: "RptID", value: text(report.rptId) },
        { label: "TrgOps mask", value: formatOptionalNumber(numberValue(report.triggerOptionsMask)) },
        { label: "TrgOps dchg", value: nestedText(report, "triggerOptions", "dataChange") },
        { label: "TrgOps qchg", value: nestedText(report, "triggerOptions", "qualityChange") },
        { label: "TrgOps dupd", value: nestedText(report, "triggerOptions", "dataUpdate") },
        { label: "TrgOps period", value: nestedText(report, "triggerOptions", "periodic") },
        { label: "TrgOps gi", value: nestedText(report, "triggerOptions", "generalInterrogation") },
        { label: "OptFlds mask", value: formatOptionalNumber(numberValue(report.optionalFieldsMask)) },
        { label: "OptFlds seqNum", value: nestedText(report, "optionalFields", "sequenceNumber") },
        { label: "OptFlds timeStamp", value: nestedText(report, "optionalFields", "timestamp") },
        { label: "OptFlds reasonCode", value: nestedText(report, "optionalFields", "reasonCode") },
        { label: "OptFlds dataSet", value: nestedText(report, "optionalFields", "dataSetName") },
        { label: "OptFlds dataRef", value: nestedText(report, "optionalFields", "dataReference") },
        { label: "OptFlds entryID", value: nestedText(report, "optionalFields", "entryId") },
        { label: "OptFlds configRef", value: nestedText(report, "optionalFields", "configRevision") },
        { label: "OptFlds bufOvfl", value: nestedText(report, "optionalFields", "bufferOverflow") },
        { label: "BufTm", value: formatOptionalNumber(numberValue(report.bufferTimeMs)) },
        { label: "IntgPd", value: formatOptionalNumber(numberValue(report.integrityPeriodMs)) },
        { label: "resolved DataSet leaves", value: String(reportSignals.length) },
      ],
    },
  })

  const linkValue = `report:${index}:dataset-link`
  rows.push({
    value: linkValue,
    parent: reportValue,
    kind: "report-dataset-link",
    label: dataSetName || text(report.dataSetRef) || "DatSet unresolved",
    meta: `${reportSignals.length} preview leaves`,
    isLeaf: reportSignals.length === 0,
    detail: {
      title: dataSetName || "DatSet unresolved",
      subtitle: "ReportControl DatSet link",
      rows: [
        { label: "ReportControl", value: name },
        { label: "DatSet reference", value: text(report.dataSetRef) },
        { label: "DataSet name", value: dataSetName },
        { label: "DataSet index", value: formatOptionalNumber(dataSetIndex) },
        { label: "preview ownership", value: "DataSet owns these members; ReportControl only references them." },
      ],
    },
  })
  for (const [signalIndex, signal] of reportSignals.entries()) {
    rows.push(signalRow(`report-dataset-preview:${index}:${signalIndex}`, linkValue, signal, "report-signal-preview"))
  }
}

function scopeKey(logicalDeviceInst: string, logicalNodeName: string): string {
  return `${logicalDeviceInst}\u0000${logicalNodeName}`
}

function groupRow(value: string, parent: string, kind: Iec61850NativeTreeNodeKind, label: string, meta: string): Iec61850NativeTreeRow {
  return {
    value,
    parent,
    kind,
    label,
    meta,
    isLeaf: false,
    detail: {
      title: label,
      subtitle: `${meta} items`,
      rows: [{ label: "count", value: meta }],
    },
  }
}

function signalRow(value: string, parent: string, signal: NativeRecord, kind: "dataset-member" | "report-signal-preview" = "dataset-member"): Iec61850NativeTreeRow {
  const reference = text(signal.reference)
  return {
    value,
    parent,
    kind,
    label: reference || text(signal.objectReference),
    meta: text(signal.fc),
    isLeaf: true,
    detail: {
      title: reference,
      subtitle: text(signal.dataSetEntryVariable),
      rows: [
        { label: "canonical variable", value: text(signal.dataSetEntryVariable) },
        { label: "object reference", value: text(signal.objectReference) },
        { label: "logical device", value: text(signal.logicalDeviceInst) },
        { label: "logical node", value: text(signal.logicalNodeName) },
        { label: "data object", value: text(signal.dataObjectName) },
        { label: "data attribute", value: text(signal.dataAttributePath) },
        { label: "FC", value: text(signal.fc) },
        { label: "initial value", value: text(signal.initialValue) },
      ],
    },
  }
}

function asRecords(value: unknown): NativeRecord[] {
  return Array.isArray(value) ? value.filter((item): item is NativeRecord => Boolean(item) && typeof item === "object" && !Array.isArray(item)) : []
}

function recordValue(value: unknown): NativeRecord {
  return Boolean(value) && typeof value === "object" && !Array.isArray(value) ? value as NativeRecord : {}
}

function addressValue(connectedAccessPoint: NativeRecord, type: string): string {
  if (type === "IP") return text(connectedAccessPoint.ipAddress) || text(recordValue(connectedAccessPoint.address)[type])
  if (type === "IP-SUBNET") return text(connectedAccessPoint.ipSubnet) || text(recordValue(connectedAccessPoint.address)[type])
  if (type === "IP-GATEWAY") return text(connectedAccessPoint.ipGateway) || text(recordValue(connectedAccessPoint.address)[type])
  const address = recordValue(connectedAccessPoint.address)
  return text(address[type])
}

function nestedText(record: NativeRecord, objectKey: string, valueKey: string): string {
  return text(recordValue(record[objectKey])[valueKey])
}

function addressParameterRows(connectedAccessPoint: NativeRecord): Iec61850NativeDetailRow[] {
  return asRecords(connectedAccessPoint.addressParameters)
    .map(parameter => ({ label: `P:${text(parameter.type)}`, value: text(parameter.value) }))
    .filter(row => Boolean(row.label !== "P:" || row.value))
}

function formatConnectedAccessPointAddress(connectedAccessPoint: NativeRecord): string {
  const ap = text(connectedAccessPoint.accessPointName)
  const ip = addressValue(connectedAccessPoint, "IP")
  if (!ap && !ip) return ""
  return ip ? `${ap || "AP"} ${ip}` : ap
}

function text(value: unknown): string {
  if (typeof value === "string") return value
  if (typeof value === "number" || typeof value === "boolean") return String(value)
  return ""
}

function numberValue(value: unknown): number | null {
  return typeof value === "number" && Number.isFinite(value) ? value : null
}

function formatOptionalNumber(value: number | null): string {
  return value == null ? "" : String(value)
}
