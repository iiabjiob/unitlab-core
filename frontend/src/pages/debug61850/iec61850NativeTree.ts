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
  | "report-signal"

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
          ...addressParameterRows(connectedAccessPoint),
        ],
      },
    })
  }

  const ldGroup = "group:logical-devices"
  rows.push(groupRow(ldGroup, root, "logical-devices-group", "Logical devices", `${logicalDevices.length}`))
  for (const device of logicalDevices) {
    const inst = text(device.inst)
    if (!inst) continue
    const ldValue = `ld:${inst}`
    const childNodes = logicalNodes.filter(node => text(node.logicalDeviceInst) === inst)
    rows.push({
      value: ldValue,
      parent: ldGroup,
      kind: "logical-device",
      label: inst,
      meta: `${childNodes.length} LN`,
      isLeaf: childNodes.length === 0,
      detail: {
        title: inst,
        subtitle: "logical device",
        rows: [
          { label: "logical nodes", value: String(childNodes.length) },
          { label: "signals", value: String(signals.filter(signal => text(signal.logicalDeviceInst) === inst).length) },
        ],
      },
    })
    for (const node of childNodes) {
      const name = text(node.name)
      if (!name) continue
      rows.push({
        value: `ln:${inst}:${name}`,
        parent: ldValue,
        kind: "logical-node",
        label: name,
        meta: String(signals.filter(signal => text(signal.logicalDeviceInst) === inst && text(signal.logicalNodeName) === name).length),
        isLeaf: true,
        detail: {
          title: `${inst}/${name}`,
          subtitle: "logical node",
          rows: [
            { label: "logical device", value: inst },
            { label: "logical node", value: name },
          ],
        },
      })
    }
  }

  const dataSetGroup = "group:datasets"
  rows.push(groupRow(dataSetGroup, root, "datasets-group", "DataSets", `${dataSets.length}`))
  for (const [index, dataSet] of dataSets.entries()) {
    const name = text(dataSet.name) || `DataSet ${index + 1}`
    const reference = text(dataSet.reference)
    const firstSignalIndex = numberValue(dataSet.firstSignalIndex)
    const memberCount = numberValue(dataSet.memberCount)
    const members = signals.filter(signal => numberValue(signal.dataSetIndex) === index)
    const dataSetValue = `dataset:${index}`
    rows.push({
      value: dataSetValue,
      parent: dataSetGroup,
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

  const reportGroup = "group:reports"
  rows.push(groupRow(reportGroup, root, "reports-group", "ReportControls", `${reports.length}`))
  for (const [index, report] of reports.entries()) {
    const name = text(report.name) || `Report ${index + 1}`
    const dataSetIndex = numberValue(report.dataSetIndex)
    const reportSignals = dataSetIndex == null ? [] : signals.filter(signal => numberValue(signal.dataSetIndex) === dataSetIndex)
    const reportValue = `report:${index}`
    rows.push({
      value: reportValue,
      parent: reportGroup,
      kind: "report-control",
      label: name,
      meta: `${text(report.reportKind) || "report"} · ${reportSignals.length} leaves`,
      isLeaf: reportSignals.length === 0,
      detail: {
        title: name,
        subtitle: text(report.key),
        rows: [
          { label: "kind", value: text(report.reportKind) },
          { label: "buffered", value: String(Boolean(report.isBuffered)) },
          { label: "DataSet", value: text(report.dataSetRef) },
          { label: "DataSet index", value: formatOptionalNumber(dataSetIndex) },
          { label: "ConfRev", value: formatOptionalNumber(numberValue(report.confRev)) },
          { label: "TrgOps mask", value: formatOptionalNumber(numberValue(report.triggerOptionsMask)) },
          { label: "OptFlds mask", value: formatOptionalNumber(numberValue(report.optionalFieldsMask)) },
          { label: "BufTm", value: formatOptionalNumber(numberValue(report.bufferTimeMs)) },
          { label: "IntgPd", value: formatOptionalNumber(numberValue(report.integrityPeriodMs)) },
          { label: "resolved leaves", value: String(reportSignals.length) },
        ],
      },
    })
    for (const [signalIndex, signal] of reportSignals.entries()) {
      rows.push(signalRow(`report-signal:${index}:${signalIndex}`, reportValue, signal, "report-signal"))
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

function signalRow(value: string, parent: string, signal: NativeRecord, kind: "dataset-member" | "report-signal" = "dataset-member"): Iec61850NativeTreeRow {
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
  const address = recordValue(connectedAccessPoint.address)
  return text(address[type])
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
