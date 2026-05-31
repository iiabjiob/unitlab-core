import type {
  NormalizedSclModel,
  Iec61850ReportSubscriptionCandidate,
  SclAccessPoint,
  SclBay,
  SclDataSet,
  SclDataSetMember,
  SclEquipment,
  SclIed,
  SclLogicalDevice,
  SclLogicalNode,
  SclLogicalNodeRef,
  SclReportControl,
  SclReportEnabled,
  SclServer,
  SclSubstation,
  SclTerminal,
  SclVoltage,
  SclVoltageLevel,
  SldCoordinate,
} from "@/modules/scd-sld-core"

export type Iec61850DebugNodeKind =
  | "site"
  | "voltage-level"
  | "bay"
  | "switchgears-group"
  | "switchgear"
  | "ieds-group"
  | "ied"
  | "access-point"
  | "server"
  | "logical-device"
  | "logical-node"
  | "datasets-group"
  | "dataset"
  | "dataset-member"
  | "reports-group"
  | "report-control"
  | "report-signal"

export type Iec61850DebugDetailRow = {
  label: string
  value: string
  signalCount?: number
  action?: Iec61850DebugDetailAction
  reportSignalsAction?: Iec61850DebugReportSignalsAction
}

export type Iec61850DebugDetailAction = {
  kind: "list-dialog"
  title: string
  subtitle: string
  emptyLabel: string
  items: Iec61850DebugListDialogItem[]
}

export type Iec61850DebugListDialogItem = {
  title: string
  subtitle: string
  rows: Iec61850DebugDetailRow[]
}

export type Iec61850DebugReportSignalsAction = {
  kind: "report-signals-dialog"
  reportControlValue: string
  reportKind: string
  dataSetRef: string | null
  signalCount: number
}

export type Iec61850DebugDetailSection = {
  title: string
  rows: Iec61850DebugDetailRow[]
}

export type Iec61850DebugDetail = {
  title: string
  subtitle: string
  sections: Iec61850DebugDetailSection[]
}

export type Iec61850DebugTreeRow = {
  value: string
  parent: string | null
  kind: Iec61850DebugNodeKind
  label: string
  valueLabel: string | null
  isLeaf: boolean
  detail: Iec61850DebugDetail
}

export type Iec61850DebugStats = {
  sites: number
  voltageLevels: number
  bays: number
  switchgears: number
  ieds: number
  logicalDevices: number
  dataSets: number
  reports: number
  reportSignals: number
}

export type Iec61850DebugSignalInventoryDataSetSignal = {
  reference: string
  dataSetId: string
  dataSetRef: string
  iedName: string
  accessPointName: string
}

export type Iec61850DebugSignalInventoryReportSignal = {
  reference: string
  reportControlId: string
  reportControlName: string
  reportKind: "BRCB" | "URCB"
  dataSetId: string | null
  dataSetRef: string | null
  iedName: string
  accessPointName: string
}

export type Iec61850DebugSignalInventory = {
  dataSetSignals: Iec61850DebugSignalInventoryDataSetSignal[]
  reportSignals: Iec61850DebugSignalInventoryReportSignal[]
}

export type Iec61850DebugDocument = {
  stats: Iec61850DebugStats
  diagnosticSummary: Iec61850DebugDiagnosticSummary
  diagnostics: NormalizedSclModel["diagnostics"]
  reportCandidates: Iec61850ReportSubscriptionCandidate[]
  signalInventory: Iec61850DebugSignalInventory
  treeRows: Iec61850DebugTreeRow[]
}

export type Iec61850DebugDiagnosticSummary = {
  error: number
  warning: number
  info: number
  total: number
  rendered: number
  omitted: number
}

export type BuildIec61850DebugTreeRowsOptions = {
  maxSignalRowsPerCollection?: number
  maxTotalSignalRows?: number
  maxDetailRowsPerSection?: number
  maxDiagnostics?: number
}

type ResolvedDebugTreeOptions = {
  maxSignalRowsPerCollection: number
  maxTotalSignalRows: number
  maxDetailRowsPerSection: number
  maxDiagnostics: number
}

export function buildIec61850DebugDocument(
  model: NormalizedSclModel,
  options: BuildIec61850DebugTreeRowsOptions = {},
): Iec61850DebugDocument {
  const resolvedOptions = resolveOptions(options)
  const diagnostics = resolvedOptions.maxDiagnostics === Number.POSITIVE_INFINITY
    ? model.diagnostics
    : model.diagnostics.slice(0, resolvedOptions.maxDiagnostics)

  return {
    stats: buildIec61850DebugStats(model),
    diagnosticSummary: buildDiagnosticSummary(model.diagnostics, diagnostics.length),
    diagnostics,
    reportCandidates: model.reportSubscriptions,
    signalInventory: buildIec61850DebugSignalInventory(model),
    treeRows: buildIec61850DebugTreeRows(model, resolvedOptions),
  }
}

export function buildIec61850DebugStats(model: NormalizedSclModel): Iec61850DebugStats {
  return {
    sites: model.substations.length,
    voltageLevels: model.substations.reduce((sum, site) => sum + site.voltageLevels.length, 0),
    bays: model.substations.reduce(
      (sum, site) => sum + site.voltageLevels.reduce((vlSum, vl) => vlSum + vl.bays.length, 0),
      0,
    ),
    switchgears: countSwitchgears(model),
    ieds: model.ieds.length,
    logicalDevices: countLogicalDevices(model),
    dataSets: collectDataSets(model).length,
    reports: model.reportSubscriptions.length,
    reportSignals: model.reportSubscriptions.reduce((sum, candidate) => sum + candidate.signalCount, 0),
  }
}

export function buildIec61850DebugSignalInventory(model: NormalizedSclModel): Iec61850DebugSignalInventory {
  return {
    dataSetSignals: collectDataSets(model).flatMap(dataSet => dataSet.members.map(member => ({
      reference: member.reference,
      dataSetId: dataSet.id,
      dataSetRef: formatDataSetReference(dataSet),
      iedName: dataSet.iedName,
      accessPointName: dataSet.accessPointName,
    }))),
    reportSignals: model.reportSubscriptions.flatMap(candidate => candidate.normalizedDatasetEntries.flatMap(entry => entry.leaves).map(signal => ({
      reference: signal.reference,
      reportControlId: candidate.reportControlId,
      reportControlName: candidate.reportControlName,
      reportKind: candidate.reportKind === "buffered" ? "BRCB" : "URCB",
      dataSetId: candidate.dataSetId,
      dataSetRef: candidate.dataSetRef,
      iedName: candidate.iedName,
      accessPointName: candidate.accessPointName,
    }))),
  }
}

export function buildIec61850DebugTreeRows(
  model: NormalizedSclModel,
  options: BuildIec61850DebugTreeRowsOptions = {},
): Iec61850DebugTreeRow[] {
  const resolvedOptions = resolveOptions(options)
  const rows: Iec61850DebugTreeRow[] = []
  const dataSetById = new Map(collectDataSets(model).map(dataSet => [dataSet.id, dataSet]))
  let renderedSignalRows = 0
  const takeVisibleSignalRows = <T>(items: readonly T[]): T[] => {
    const remainingGlobalRows = Math.max(0, resolvedOptions.maxTotalSignalRows - renderedSignalRows)
    const limit = Math.min(resolvedOptions.maxSignalRowsPerCollection, remainingGlobalRows)
    const visibleRows = items.slice(0, limit)
    renderedSignalRows += visibleRows.length
    return visibleRows
  }

  model.substations.forEach((site) => {
    const siteValue = siteNodeValue(site)
    rows.push({
      value: siteValue,
      parent: null,
      kind: "site",
      label: site.name,
      valueLabel: `${site.voltageLevels.length} VL`,
      isLeaf: site.voltageLevels.length === 0,
      detail: buildSiteDetail(site),
    })

    site.voltageLevels.forEach((voltageLevel) => {
      const voltageLevelValue = voltageLevelNodeValue(voltageLevel)
      rows.push({
        value: voltageLevelValue,
        parent: siteValue,
        kind: "voltage-level",
        label: voltageLevel.name,
        valueLabel: formatVoltage(voltageLevel.voltage),
        isLeaf: voltageLevel.bays.length === 0,
        detail: buildVoltageLevelDetail(voltageLevel),
      })

      voltageLevel.bays.forEach((bay) => {
        const bayValue = bayNodeValue(bay)
        const switchgears = bay.equipments.filter(isSwitchgearEquipment)
        rows.push({
          value: bayValue,
          parent: voltageLevelValue,
          kind: "bay",
          label: bay.name,
          valueLabel: `${switchgears.length} SG`,
          isLeaf: false,
          detail: buildBayDetail(bay, switchgears.length),
        })

        const switchgearsValue = switchgearsGroupNodeValue(bay)
        rows.push({
          value: switchgearsValue,
          parent: bayValue,
          kind: "switchgears-group",
          label: "Switchgears",
          valueLabel: String(switchgears.length),
          isLeaf: switchgears.length === 0,
          detail: buildSwitchgearsGroupDetail(bay, switchgears, resolvedOptions),
        })

        switchgears.forEach((equipment) => {
          rows.push({
            value: switchgearNodeValue(equipment),
            parent: switchgearsValue,
            kind: "switchgear",
            label: equipment.name,
            valueLabel: equipment.type,
            isLeaf: true,
            detail: buildSwitchgearDetail(equipment),
          })
        })
      })
    })
  })

  if (model.ieds.length) {
    const iedsGroupValue = iedsGroupNodeValue()
    rows.push({
      value: iedsGroupValue,
      parent: null,
      kind: "ieds-group",
      label: "IEDs",
      valueLabel: String(model.ieds.length),
      isLeaf: model.ieds.length === 0,
      detail: buildIedsGroupDetail(model, resolvedOptions),
    })

    model.ieds.forEach((ied) => {
      const iedValue = iedNodeValue(ied)
      rows.push({
        value: iedValue,
        parent: iedsGroupValue,
        kind: "ied",
        label: ied.name,
        valueLabel: `${ied.accessPoints.length} AP`,
        isLeaf: ied.accessPoints.length === 0,
        detail: buildIedDetail(ied, resolvedOptions),
      })

      ied.accessPoints.forEach((accessPoint) => {
        const accessPointValue = accessPointNodeValue(accessPoint)
        rows.push({
          value: accessPointValue,
          parent: iedValue,
          kind: "access-point",
          label: accessPoint.name,
          valueLabel: accessPoint.server ? "Server" : "No server",
          isLeaf: !accessPoint.server,
          detail: buildAccessPointDetail(accessPoint, ied, resolvedOptions),
        })

        if (!accessPoint.server) {
          return
        }

        const serverValue = serverNodeValue(accessPoint.server)
        rows.push({
          value: serverValue,
          parent: accessPointValue,
          kind: "server",
          label: "Server",
          valueLabel: `${accessPoint.server.logicalDevices.length} LD`,
          isLeaf: accessPoint.server.logicalDevices.length === 0,
          detail: buildServerDetail(accessPoint.server, accessPoint, ied, resolvedOptions),
        })

        accessPoint.server.logicalDevices.forEach((logicalDevice) => {
          const logicalDeviceValue = logicalDeviceNodeValue(logicalDevice)
          rows.push({
            value: logicalDeviceValue,
            parent: serverValue,
            kind: "logical-device",
            label: logicalDevice.inst,
            valueLabel: `${logicalDevice.logicalNodes.length} LN`,
            isLeaf: logicalDevice.logicalNodes.length === 0,
            detail: buildLogicalDeviceDetail(logicalDevice, accessPoint, ied, resolvedOptions),
          })

          logicalDevice.logicalNodes.forEach((logicalNode) => {
            const logicalNodeValue = logicalNodeNodeValue(logicalNode)
            const childCount = logicalNode.dataSets.length + logicalNode.reportControls.length
            rows.push({
              value: logicalNodeValue,
              parent: logicalDeviceValue,
              kind: "logical-node",
              label: logicalNode.logicalNodeName,
              valueLabel: `${logicalNode.dataSets.length} DS · ${logicalNode.reportControls.length} RCB`,
              isLeaf: childCount === 0,
              detail: buildLogicalNodeDetail(logicalNode, resolvedOptions),
            })

            if (logicalNode.dataSets.length) {
              const dataSetsValue = dataSetsGroupNodeValue(logicalNode)
              rows.push({
                value: dataSetsValue,
                parent: logicalNodeValue,
                kind: "datasets-group",
                label: "DataSets",
                valueLabel: String(logicalNode.dataSets.length),
                isLeaf: false,
                detail: buildDataSetsGroupDetail(logicalNode, resolvedOptions),
              })

              logicalNode.dataSets.forEach((dataSet) => {
                const dataSetValue = dataSetNodeValue(dataSet)
                rows.push({
                  value: dataSetValue,
                  parent: dataSetsValue,
                  kind: "dataset",
                  label: dataSet.name,
                  valueLabel: `${dataSet.members.length} signals`,
                  isLeaf: dataSet.members.length === 0,
                  detail: buildDataSetDetail(dataSet, resolvedOptions),
                })

                const visibleMembers = takeVisibleSignalRows(dataSet.members)
                visibleMembers.forEach((member, index) => {
                  rows.push({
                    value: dataSetMemberNodeValue(member),
                    parent: dataSetValue,
                    kind: "dataset-member",
                    label: member.reference,
                    valueLabel: member.kind,
                    isLeaf: true,
                    detail: buildDataSetMemberDetail(member, dataSet, index),
                  })
                })
                appendOmittedSignalRow(rows, {
                  parent: dataSetValue,
                  value: `${dataSetValue}:omitted`,
                  kind: "dataset-member",
                  total: dataSet.members.length,
                  rendered: visibleMembers.length,
                  title: `${dataSet.name} omitted signals`,
                  subtitle: "DataSet signal rows capped for debug rendering",
                })
              })
            }

            if (logicalNode.reportControls.length) {
              const reportsValue = reportsGroupNodeValue(logicalNode)
              rows.push({
                value: reportsValue,
                parent: logicalNodeValue,
                kind: "reports-group",
                label: "ReportControls",
                valueLabel: String(logicalNode.reportControls.length),
                isLeaf: false,
                detail: buildReportsGroupDetail(logicalNode, resolvedOptions),
              })

              logicalNode.reportControls.forEach((reportControl) => {
                const reportDataSet = reportControl.dataSetId ? dataSetById.get(reportControl.dataSetId) ?? null : null
                const reportSignals = reportDataSet?.members ?? []
                const reportControlValue = reportControlNodeValue(reportControl)
                rows.push({
                  value: reportControlValue,
                  parent: reportsValue,
                  kind: "report-control",
                  label: reportControl.name,
                  valueLabel: reportKindLabel(reportControl),
                  isLeaf: reportSignals.length === 0,
                  detail: buildReportControlDetail(reportControl, reportDataSet, resolvedOptions),
                })

                const visibleReportSignals = takeVisibleSignalRows(reportSignals)
                visibleReportSignals.forEach((member, index) => {
                  rows.push({
                    value: reportSignalNodeValue(reportControl, member),
                    parent: reportControlValue,
                    kind: "report-signal",
                    label: member.reference,
                    valueLabel: member.fc,
                    isLeaf: true,
                    detail: buildReportSignalDetail(reportControl, member, reportDataSet, index),
                  })
                })
                appendOmittedSignalRow(rows, {
                  parent: reportControlValue,
                  value: `${reportControlValue}:omitted`,
                  kind: "report-signal",
                  total: reportSignals.length,
                  rendered: visibleReportSignals.length,
                  title: `${reportControl.name} omitted report signals`,
                  subtitle: "Report signal rows capped for debug rendering",
                })
              })
            }
          })
        })
      })
    })
  }

  return rows
}

export function isSwitchgearEquipment(equipment: SclEquipment): boolean {
  return equipment.kind === "breaker" || equipment.kind === "disconnector"
}

function countSwitchgears(value: NormalizedSclModel): number {
  return value.substations.reduce((siteSum, site) => (
    siteSum + site.voltageLevels.reduce((voltageSum, voltageLevel) => (
      voltageSum + voltageLevel.bays.reduce((baySum, bay) => (
        baySum + bay.equipments.filter(isSwitchgearEquipment).length
      ), 0)
    ), 0)
  ), 0)
}

function resolveOptions(options: BuildIec61850DebugTreeRowsOptions): ResolvedDebugTreeOptions {
  return {
    maxSignalRowsPerCollection: normalizeLimit(options.maxSignalRowsPerCollection),
    maxTotalSignalRows: normalizeLimit(options.maxTotalSignalRows),
    maxDetailRowsPerSection: normalizeLimit(options.maxDetailRowsPerSection),
    maxDiagnostics: normalizeLimit(options.maxDiagnostics),
  }
}

function normalizeLimit(value: number | undefined): number {
  if (value === undefined) return Number.POSITIVE_INFINITY
  if (!Number.isFinite(value)) return Number.POSITIVE_INFINITY
  return Math.max(0, Math.floor(value))
}

function appendOmittedSignalRow(
  rows: Iec61850DebugTreeRow[],
  input: {
    parent: string
    value: string
    kind: "dataset-member" | "report-signal"
    total: number
    rendered: number
    title: string
    subtitle: string
  },
) {
  const omitted = Math.max(0, input.total - input.rendered)
  if (omitted === 0) return

  rows.push({
    value: input.value,
    parent: input.parent,
    kind: input.kind,
    label: `${omitted} more signals not rendered`,
    valueLabel: "capped",
    isLeaf: true,
    detail: {
      title: input.title,
      subtitle: input.subtitle,
      sections: [
        section("Rendering cap", [
          row("rendered", input.rendered),
          row("omitted", omitted),
          row("total", input.total),
        ]),
      ],
    },
  })
}

function buildDiagnosticSummary(
  diagnostics: NormalizedSclModel["diagnostics"],
  rendered: number,
): Iec61850DebugDiagnosticSummary {
  const summary: Iec61850DebugDiagnosticSummary = {
    error: 0,
    warning: 0,
    info: 0,
    total: diagnostics.length,
    rendered,
    omitted: Math.max(0, diagnostics.length - rendered),
  }

  for (const diagnostic of diagnostics) {
    summary[diagnostic.severity] += 1
  }

  return summary
}

function siteNodeValue(site: SclSubstation): string {
  return `site:${site.id}`
}

function voltageLevelNodeValue(voltageLevel: SclVoltageLevel): string {
  return `voltage-level:${voltageLevel.id}`
}

function bayNodeValue(bay: SclBay): string {
  return `bay:${bay.id}`
}

function switchgearsGroupNodeValue(bay: SclBay): string {
  return `switchgears:${bay.id}`
}

function switchgearNodeValue(equipment: SclEquipment): string {
  return `switchgear:${equipment.id}`
}

function iedsGroupNodeValue(): string {
  return "ieds:root"
}

function iedNodeValue(ied: SclIed): string {
  return `ied:${ied.id}`
}

function accessPointNodeValue(accessPoint: SclAccessPoint): string {
  return `access-point:${accessPoint.id}`
}

function serverNodeValue(server: SclServer): string {
  return `server:${server.id}`
}

function logicalDeviceNodeValue(logicalDevice: SclLogicalDevice): string {
  return `logical-device:${logicalDevice.id}`
}

function logicalNodeNodeValue(logicalNode: SclLogicalNode): string {
  return `logical-node:${logicalNode.id}`
}

function dataSetsGroupNodeValue(logicalNode: SclLogicalNode): string {
  return `datasets:${logicalNode.id}`
}

function dataSetNodeValue(dataSet: SclDataSet): string {
  return `dataset:${dataSet.id}`
}

function dataSetMemberNodeValue(member: SclDataSetMember): string {
  return `dataset-member:${member.id}`
}

function reportsGroupNodeValue(logicalNode: SclLogicalNode): string {
  return `reports:${logicalNode.id}`
}

function reportControlNodeValue(reportControl: SclReportControl): string {
  return `report-control:${reportControl.id}`
}

function reportSignalNodeValue(reportControl: SclReportControl, member: SclDataSetMember): string {
  return `report-signal:${reportControl.id}:${member.id}`
}

function buildSiteDetail(site: SclSubstation): Iec61850DebugDetail {
  return {
    title: site.name,
    subtitle: "SCL Substation",
    sections: [
      section("Identity", [
        row("Standard element", "Substation"),
        row("name", site.name),
        row("desc", site.desc),
        row("normalized id", site.id),
        row("source path", site.sourcePath),
      ]),
      section("Topology", [
        row("voltage levels", site.voltageLevels.length),
        row("power transformers", site.powerTransformers.length),
        row("connectivity nodes", site.connectivityNodes.length),
        row("logical nodes", site.lNodes.length),
      ]),
      section("Coordinates", coordinateRows(site.coordinates)),
    ],
  }
}

function buildVoltageLevelDetail(voltageLevel: SclVoltageLevel): Iec61850DebugDetail {
  return {
    title: voltageLevel.name,
    subtitle: "SCL VoltageLevel",
    sections: [
      section("Identity", [
        row("Standard element", "VoltageLevel"),
        row("name", voltageLevel.name),
        row("normalized id", voltageLevel.id),
        row("source path", voltageLevel.sourcePath),
      ]),
      section("Voltage", [
        row("value", voltageLevel.voltage?.value),
        row("multiplier", voltageLevel.voltage?.multiplier),
        row("unit", voltageLevel.voltage?.unit),
        row("display", formatVoltage(voltageLevel.voltage)),
      ]),
      section("Topology", [
        row("bays", voltageLevel.bays.length),
        row("connectivity nodes", voltageLevel.connectivityNodes.length),
        row("logical nodes", voltageLevel.lNodes.length),
      ]),
      section("Coordinates", coordinateRows(voltageLevel.coordinates)),
    ],
  }
}

function buildBayDetail(bay: SclBay, switchgearCount: number): Iec61850DebugDetail {
  return {
    title: bay.name,
    subtitle: "SCL Bay",
    sections: [
      section("Identity", [
        row("Standard element", "Bay"),
        row("name", bay.name),
        row("desc", bay.desc),
        row("normalized id", bay.id),
        row("source path", bay.sourcePath),
      ]),
      section("Hierarchy", [
        row("substationName", bay.substationName),
        row("voltageLevelName", bay.voltageLevelName),
      ]),
      section("Topology", [
        row("switchgears", switchgearCount),
        row("conducting equipment", bay.equipments.length),
        row("connectivity nodes", bay.connectivityNodes.length),
        row("logical nodes", bay.lNodes.length),
      ]),
      section("Coordinates", coordinateRows(bay.coordinates)),
    ],
  }
}

function buildSwitchgearsGroupDetail(
  bay: SclBay,
  switchgears: SclEquipment[],
  options: ResolvedDebugTreeOptions,
): Iec61850DebugDetail {
  return {
    title: `${bay.name} switchgears`,
    subtitle: "ConductingEquipment CBR/DIS subset",
    sections: [
      section("Scope", [
        row("parent bay", bay.name),
        row("substationName", bay.substationName),
        row("voltageLevelName", bay.voltageLevelName),
        row("switchgear count", switchgears.length),
      ]),
      section("Members", switchgears.length
        ? limitedRows(switchgears, options.maxDetailRowsPerSection, equipment => (
          row(equipment.name, `${equipment.kind} · ${equipment.type}`)
        ))
        : [row("members", "none")]),
    ],
  }
}

function buildSwitchgearDetail(equipment: SclEquipment): Iec61850DebugDetail {
  return {
    title: equipment.name,
    subtitle: `SCL ConductingEquipment · ${equipment.kind}`,
    sections: [
      section("Identity", [
        row("Standard element", equipment.tagName),
        row("name", equipment.name),
        row("desc", equipment.desc),
        row("type", equipment.type),
        row("normalized kind", equipment.kind),
        row("normalized id", equipment.id),
        row("source path", equipment.sourcePath),
      ]),
      section("Hierarchy", [
        row("substationName", equipment.substationName),
        row("voltageLevelName", equipment.voltageLevelName),
        row("bayName", equipment.bayName),
      ]),
      section("Coordinates", coordinateRows(equipment.coordinates)),
      section("Terminals", terminalRows(equipment.terminals)),
      section("Logical Nodes", logicalNodeRows(equipment.lNodes)),
    ],
  }
}

function buildIedsGroupDetail(
  model: NormalizedSclModel,
  options: ResolvedDebugTreeOptions,
): Iec61850DebugDetail {
  const dataSets = collectDataSets(model)
  const reportControls = collectReportControls(model)

  return {
    title: "IEDs",
    subtitle: "IEC 61850 device runtime inventory",
    sections: [
      section("Source", [
        row("file", model.source.fileName),
        row("content hash", model.source.contentHash),
        row("SCL version", model.scl.version),
        row("SCL revision", model.scl.revision),
      ]),
      section("Inventory", [
        row("IEDs", model.ieds.length),
        row("access points", countAccessPoints(model)),
        row("logical devices", countLogicalDevices(model)),
        row("logical nodes", countLogicalNodes(model)),
        row("DataSets", dataSets.length),
        row("ReportControls", reportControls.length),
        row("subscription candidates", model.reportSubscriptions.length),
      ]),
      section("Report candidates", model.reportSubscriptions.length
        ? limitedRows(model.reportSubscriptions, options.maxDetailRowsPerSection, reportCandidateRow)
        : [row("candidates", "none")]),
    ],
  }
}

function buildIedDetail(ied: SclIed, options: ResolvedDebugTreeOptions): Iec61850DebugDetail {
  const dataSets = collectIedDataSets(ied)
  const reportControls = collectIedReportControls(ied)
  return {
    title: ied.name,
    subtitle: "SCL IED",
    sections: [
      section("Identity", [
        row("name", ied.name),
        row("desc", ied.desc),
        row("manufacturer", ied.manufacturer),
        row("type", ied.type),
        row("configVersion", ied.configVersion),
        row("IP addresses", formatIedIpAddresses(ied)),
        row("normalized id", ied.id),
        row("source path", ied.sourcePath),
      ]),
      section("Runtime inventory", [
        row("access points", ied.accessPoints.length),
        row("logical devices", countIedLogicalDevices(ied)),
        row("logical nodes", countIedLogicalNodes(ied)),
        actionRow("DataSets", dataSets.length, dataSetListAction(`${ied.name} DataSets`, "IED DataSet declarations", dataSets, options)),
        actionRow("ReportControls", reportControls.length, reportControlListAction(`${ied.name} ReportControls`, "IED report control declarations", reportControls)),
      ]),
    ],
  }
}

function buildAccessPointDetail(
  accessPoint: SclAccessPoint,
  ied: SclIed,
  options: ResolvedDebugTreeOptions,
): Iec61850DebugDetail {
  const dataSets = collectAccessPointDataSets(accessPoint)
  const reportControls = collectAccessPointReportControls(accessPoint)
  return {
    title: accessPoint.name,
    subtitle: "SCL AccessPoint",
    sections: [
      section("Identity", [
        row("IED", ied.name),
        row("name", accessPoint.name),
        row("desc", accessPoint.desc),
        row("router", accessPoint.router),
        row("clock", accessPoint.clock),
        row("IP address", formatAccessPointIpAddress(accessPoint)),
        row("server present", Boolean(accessPoint.server)),
        row("normalized id", accessPoint.id),
        row("source path", accessPoint.sourcePath),
      ]),
      section("Communication", accessPointCommunicationRows(accessPoint)),
      section("Runtime inventory", [
        row("logical devices", accessPoint.server?.logicalDevices.length ?? 0),
        row("logical nodes", countAccessPointLogicalNodes(accessPoint)),
        actionRow("DataSets", dataSets.length, dataSetListAction(`${ied.name}/${accessPoint.name} DataSets`, "AccessPoint DataSet declarations", dataSets, options)),
        actionRow("ReportControls", reportControls.length, reportControlListAction(`${ied.name}/${accessPoint.name} ReportControls`, "AccessPoint report control declarations", reportControls)),
      ]),
    ],
  }
}

function buildServerDetail(
  server: SclServer,
  accessPoint: SclAccessPoint,
  ied: SclIed,
  options: ResolvedDebugTreeOptions,
): Iec61850DebugDetail {
  const dataSets = collectServerDataSets(server)
  const reportControls = collectServerReportControls(server)
  return {
    title: "Server",
    subtitle: "SCL AccessPoint Server",
    sections: [
      section("Scope", [
        row("IED", ied.name),
        row("access point", accessPoint.name),
        row("IP address", formatAccessPointIpAddress(accessPoint)),
        row("normalized id", server.id),
        row("source path", server.sourcePath),
      ]),
      section("Runtime inventory", [
        row("logical devices", server.logicalDevices.length),
        row("logical nodes", countServerLogicalNodes(server)),
        actionRow("DataSets", dataSets.length, dataSetListAction(`${ied.name}/${accessPoint.name} Server DataSets`, "Server DataSet declarations", dataSets, options)),
        actionRow("ReportControls", reportControls.length, reportControlListAction(`${ied.name}/${accessPoint.name} Server ReportControls`, "Server report control declarations", reportControls)),
      ]),
    ],
  }
}

function buildLogicalDeviceDetail(
  logicalDevice: SclLogicalDevice,
  accessPoint: SclAccessPoint,
  ied: SclIed,
  options: ResolvedDebugTreeOptions,
): Iec61850DebugDetail {
  const dataSets = logicalDevice.logicalNodes.flatMap(logicalNode => logicalNode.dataSets)
  const reportControls = logicalDevice.logicalNodes.flatMap(logicalNode => logicalNode.reportControls)
  return {
    title: logicalDevice.inst,
    subtitle: "SCL LDevice",
    sections: [
      section("Identity", [
        row("IED", ied.name),
        row("access point", accessPoint.name),
        row("inst", logicalDevice.inst),
        row("ldName", logicalDevice.ldName),
        row("desc", logicalDevice.desc),
        row("normalized id", logicalDevice.id),
        row("source path", logicalDevice.sourcePath),
      ]),
      section("Runtime inventory", [
        row("logical nodes", logicalDevice.logicalNodes.length),
        actionRow("DataSets", dataSets.length, dataSetListAction(`${ied.name}/${logicalDevice.inst} DataSets`, "Logical device DataSet declarations", dataSets, options)),
        actionRow("ReportControls", reportControls.length, reportControlListAction(`${ied.name}/${logicalDevice.inst} ReportControls`, "Logical device report control declarations", reportControls)),
      ]),
    ],
  }
}

function buildLogicalNodeDetail(
  logicalNode: SclLogicalNode,
  options: ResolvedDebugTreeOptions,
): Iec61850DebugDetail {
  return {
    title: logicalNode.logicalNodeName,
    subtitle: `SCL ${logicalNode.tagName}`,
    sections: [
      section("Identity", [
        row("IED", logicalNode.iedName),
        row("access point", logicalNode.accessPointName),
        row("ldInst", logicalNode.logicalDeviceInst),
        row("logical node", logicalNode.logicalNodeName),
        row("prefix", logicalNode.prefix),
        row("lnClass", logicalNode.lnClass),
        row("lnInst", logicalNode.lnInst),
        row("lnType", logicalNode.lnType),
        row("desc", logicalNode.desc),
        row("normalized id", logicalNode.id),
        row("source path", logicalNode.sourcePath),
      ]),
      section("Runtime inventory", [
        actionRow("DataSets", logicalNode.dataSets.length, dataSetListAction(`${logicalNode.logicalNodeName} DataSets`, "Logical node DataSet declarations", logicalNode.dataSets, options)),
        row("DataSet members", logicalNode.dataSets.reduce((sum, dataSet) => sum + dataSet.members.length, 0)),
        actionRow("ReportControls", logicalNode.reportControls.length, reportControlListAction(`${logicalNode.logicalNodeName} ReportControls`, "Logical node report control declarations", logicalNode.reportControls)),
      ]),
    ],
  }
}

function buildDataSetsGroupDetail(
  logicalNode: SclLogicalNode,
  options: ResolvedDebugTreeOptions,
): Iec61850DebugDetail {
  return {
    title: `${logicalNode.logicalNodeName} DataSets`,
    subtitle: "IEC 61850 DataSet declarations",
    sections: [
      section("Scope", [
        row("IED", logicalNode.iedName),
        row("access point", logicalNode.accessPointName),
        row("ldInst", logicalNode.logicalDeviceInst),
        row("logical node", logicalNode.logicalNodeName),
        row("DataSets", logicalNode.dataSets.length),
      ]),
      section("Members", limitedRows(logicalNode.dataSets, options.maxDetailRowsPerSection, dataSet => (
        row(dataSet.name, `${dataSet.members.length} signals`)
      ))),
    ],
  }
}

function buildDataSetDetail(dataSet: SclDataSet, options: ResolvedDebugTreeOptions): Iec61850DebugDetail {
  const signalRows = limitedRows(
    dataSet.members,
    options.maxDetailRowsPerSection,
    (member, index) => row(`signal ${index + 1}`, member.reference),
  )

  return {
    title: dataSet.name,
    subtitle: "IEC 61850 DataSet",
    sections: [
      section("Identity", [
        row("name", dataSet.name),
        row("desc", dataSet.desc),
        row("DataSet ref", `${dataSet.iedName}/${dataSet.accessPointName}/${dataSet.logicalDeviceInst}/${dataSet.logicalNodeName}.${dataSet.name}`),
        row("normalized id", dataSet.id),
        row("source path", dataSet.sourcePath),
      ]),
      section("Scope", [
        row("IED", dataSet.iedName),
        row("access point", dataSet.accessPointName),
        row("ldInst", dataSet.logicalDeviceInst),
        row("logical node", dataSet.logicalNodeName),
      ]),
      section("Signals", dataSet.members.length
        ? signalRows
        : [row("signals", "none")]),
    ],
  }
}

function buildDataSetMemberDetail(
  member: SclDataSetMember,
  dataSet: SclDataSet,
  index: number,
): Iec61850DebugDetail {
  return {
    title: member.reference,
    subtitle: `IEC 61850 DataSet member · ${member.kind}`,
    sections: [
      section("Scope", [
        row("DataSet", dataSet.name),
        row("member index", index + 1),
        row("normalized id", member.id),
        row("source path", member.sourcePath),
      ]),
      section("Address", dataSetMemberRows(member)),
    ],
  }
}

function buildReportsGroupDetail(
  logicalNode: SclLogicalNode,
  options: ResolvedDebugTreeOptions,
): Iec61850DebugDetail {
  return {
    title: `${logicalNode.logicalNodeName} ReportControls`,
    subtitle: "IEC 61850 report control declarations",
    sections: [
      section("Scope", [
        row("IED", logicalNode.iedName),
        row("access point", logicalNode.accessPointName),
        row("ldInst", logicalNode.logicalDeviceInst),
        row("logical node", logicalNode.logicalNodeName),
        row("ReportControls", logicalNode.reportControls.length),
      ]),
      section("Members", limitedRows(logicalNode.reportControls, options.maxDetailRowsPerSection, reportControl => row(
        reportControl.name,
        `${reportKindLabel(reportControl)} · ${reportControl.dataSetRef ?? "unresolved DataSet"}`,
      ))),
    ],
  }
}

function buildReportControlDetail(
  reportControl: SclReportControl,
  dataSet: SclDataSet | null,
  options: ResolvedDebugTreeOptions,
): Iec61850DebugDetail {
  const signalRows = dataSet
    ? limitedRows(dataSet.members, options.maxDetailRowsPerSection, (member, index) => row(`signal ${index + 1}`, member.reference))
    : [row("signals", "none")]

  return {
    title: reportControl.name,
    subtitle: `IEC 61850 ${reportKindLabel(reportControl)}`,
    sections: [
      section("Identity", [
        row("name", reportControl.name),
        row("desc", reportControl.desc),
        row("rptID", reportControl.rptId),
        row("kind", reportKindLabel(reportControl)),
        row("indexed", reportControl.indexed),
        row("confRev", reportControl.confRev),
        row("normalized id", reportControl.id),
        row("source path", reportControl.sourcePath),
      ]),
      section("Scope", [
        row("IED", reportControl.iedName),
        row("access point", reportControl.accessPointName),
        row("ldInst", reportControl.logicalDeviceInst),
        row("logical node", reportControl.logicalNodeName),
      ]),
      section("DataSet", [
        row("datSet", reportControl.dataSetName),
        row("resolved DataSet", dataSet?.name),
        row("DataSet ref", reportControl.dataSetRef),
        row("signal count", dataSet?.members.length ?? 0),
      ]),
      section("Timing", [
        row("bufTime ms", reportControl.bufferTimeMs),
        row("intgPd ms", reportControl.integrityPeriodMs),
      ]),
      section("Trigger options", [
        row("dchg", reportControl.triggerOptions.dataChange),
        row("qchg", reportControl.triggerOptions.qualityChange),
        row("dupd", reportControl.triggerOptions.dataUpdate),
        row("period", reportControl.triggerOptions.periodic),
        row("gi", reportControl.triggerOptions.generalInterrogation),
      ]),
      section("Optional fields", [
        row("seqNum", reportControl.optionalFields.sequenceNumber),
        row("timeStamp", reportControl.optionalFields.timestamp),
        row("reasonCode", reportControl.optionalFields.reasonCode),
        row("dataSet", reportControl.optionalFields.dataSetName),
        row("dataRef", reportControl.optionalFields.dataReference),
        row("entryID", reportControl.optionalFields.entryId),
        row("configRef", reportControl.optionalFields.configRevision),
        row("bufOvfl", reportControl.optionalFields.bufferOverflow),
      ]),
      section("RptEnabled", reportEnabledRows(reportControl.rptEnabled)),
      section("Resolved signals", signalRows),
    ],
  }
}

function buildReportSignalDetail(
  reportControl: SclReportControl,
  member: SclDataSetMember,
  dataSet: SclDataSet | null,
  index: number,
): Iec61850DebugDetail {
  return {
    title: member.reference,
    subtitle: `Report signal · ${reportControl.name}`,
    sections: [
      section("Report", [
        row("ReportControl", reportControl.name),
        row("kind", reportKindLabel(reportControl)),
        row("rptID", reportControl.rptId),
        row("DataSet", dataSet?.name),
        row("DataSet ref", reportControl.dataSetRef),
        row("signal index", index + 1),
      ]),
      section("Address", dataSetMemberRows(member)),
    ],
  }
}

function accessPointCommunicationRows(accessPoint: SclAccessPoint): Iec61850DebugDetailRow[] {
  if (!accessPoint.connectedAccessPoints.length) {
    return [row("ConnectedAP", "not declared")]
  }

  return accessPoint.connectedAccessPoints.flatMap((connectedAccessPoint, index) => {
    const label = connectedAccessPoint.subNetworkName ?? `ConnectedAP ${index + 1}`
    return [
      row(`${label} subnet`, connectedAccessPoint.subNetworkName),
      row(`${label} type`, connectedAccessPoint.subNetworkType),
      row(`${label} IP`, connectedAccessPoint.ipAddress),
      row(`${label} subnet mask`, connectedAccessPoint.ipSubnet),
      row(`${label} gateway`, connectedAccessPoint.ipGateway),
      ...connectedAccessPoint.addressParameters.map(parameter => row(
        `${label} ${parameter.type ?? "P"}`,
        parameter.value,
      )),
    ]
  })
}

function formatIedIpAddresses(ied: SclIed): string {
  const addresses = ied.accessPoints.flatMap(accessPoint => (
    accessPoint.connectedAccessPoints
      .map(connectedAccessPoint => connectedAccessPoint.ipAddress)
      .filter(isPresent)
  ))
  return addresses.length ? Array.from(new Set(addresses)).join(", ") : "—"
}

function formatAccessPointIpAddress(accessPoint: SclAccessPoint): string {
  const addresses = accessPoint.connectedAccessPoints
    .map(connectedAccessPoint => connectedAccessPoint.ipAddress)
    .filter(isPresent)
  return addresses.length ? Array.from(new Set(addresses)).join(", ") : "—"
}

function dataSetListAction(
  title: string,
  subtitle: string,
  dataSets: readonly SclDataSet[],
  options: ResolvedDebugTreeOptions,
): Iec61850DebugDetailAction {
  return {
    kind: "list-dialog",
    title,
    subtitle,
    emptyLabel: "No DataSets.",
    items: dataSets.map(dataSet => ({
      title: dataSet.name,
      subtitle: `${dataSet.iedName}/${dataSet.accessPointName}/${dataSet.logicalDeviceInst}/${dataSet.logicalNodeName} · ${dataSet.members.length} signals`,
      rows: [
        row("DataSet ref", formatDataSetReference(dataSet)),
        row("desc", dataSet.desc),
        row("signals", dataSet.members.length),
        row("normalized id", dataSet.id),
        row("source path", dataSet.sourcePath),
        ...limitedRows(dataSet.members, options.maxDetailRowsPerSection, (member, index) => (
          row(`signal ${index + 1}`, member.reference)
        )),
      ],
    })),
  }
}

function reportControlListAction(
  title: string,
  subtitle: string,
  reportControls: readonly SclReportControl[],
): Iec61850DebugDetailAction {
  return {
    kind: "list-dialog",
    title,
    subtitle,
    emptyLabel: "No ReportControls.",
    items: reportControls.map(reportControl => ({
      title: reportControl.name,
      subtitle: `${reportKindLabel(reportControl)} · ${reportControl.dataSetRef ?? "unresolved DataSet"}`,
      rows: [
        row("rptID", reportControl.rptId),
        row("datSet", reportControl.dataSetName),
        row("confRev", reportControl.confRev),
        row("buffered", reportControl.buffered),
        row("indexed", reportControl.indexed),
        row("bufTime ms", reportControl.bufferTimeMs),
        row("intgPd ms", reportControl.integrityPeriodMs),
        row("normalized id", reportControl.id),
        row("source path", reportControl.sourcePath),
      ],
    })),
  }
}

function terminalRows(terminals: SclTerminal[]): Iec61850DebugDetailRow[] {
  if (!terminals.length) {
    return [row("terminals", "none")]
  }

  return terminals.flatMap((terminal, index) => {
    const label = terminal.name ?? `#${index + 1}`
    return [
      row(`${label} name`, terminal.name),
      row(`${label} connectivityNode`, terminal.connectivityNode),
      row(`${label} resolved path`, terminal.resolvedConnectivityNodePath),
      row(`${label} cNodeName`, terminal.cNodeName),
      row(`${label} terminal id`, terminal.id),
    ]
  })
}

function logicalNodeRows(lNodes: SclLogicalNodeRef[]): Iec61850DebugDetailRow[] {
  if (!lNodes.length) {
    return [row("logical nodes", "none")]
  }

  return lNodes.map((lNode, index) => row(
    `LNode ${index + 1}`,
    [
      lNode.iedName,
      lNode.ldInst,
      `${lNode.prefix ?? ""}${lNode.lnClass ?? ""}${lNode.lnInst ?? ""}` || null,
      lNode.lnType ? `type=${lNode.lnType}` : null,
    ].filter(Boolean).join(" / "),
  ))
}

function dataSetMemberRows(member: SclDataSetMember): Iec61850DebugDetailRow[] {
  return [
    row("kind", member.kind),
    row("reference", member.reference),
    row("ldInst", member.ldInst),
    row("prefix", member.prefix),
    row("lnClass", member.lnClass),
    row("lnInst", member.lnInst),
    row("doName", member.doName),
    row("daName", member.daName),
    row("fc", member.fc),
    row("ix", member.ix),
  ]
}

function reportEnabledRows(rptEnabled: SclReportEnabled | null): Iec61850DebugDetailRow[] {
  if (!rptEnabled) {
    return [row("RptEnabled", "not declared")]
  }

  return [
    row("max", rptEnabled.max),
    row("desc", rptEnabled.desc),
    row("clients", rptEnabled.clients.length),
    ...rptEnabled.clients.flatMap((client, index) => [
      row(`client ${index + 1} IED`, client.iedName),
      row(`client ${index + 1} apRef`, client.accessPointRef),
      row(`client ${index + 1} ldInst`, client.logicalDeviceInst),
      row(`client ${index + 1} LN`, [
        client.prefix,
        client.lnClass,
        client.lnInst,
      ].filter(Boolean).join("") || null),
    ]),
  ]
}

function limitedRows<T>(
  items: readonly T[],
  limit: number,
  mapItem: (item: T, index: number) => Iec61850DebugDetailRow,
): Iec61850DebugDetailRow[] {
  const rendered = items.slice(0, limit).map(mapItem)
  const omitted = Math.max(0, items.length - rendered.length)
  if (omitted > 0) {
    rendered.push(row("omitted", `${omitted} rows not rendered in debug view`))
  }
  return rendered
}

function coordinateRows(coordinates: SldCoordinate): Iec61850DebugDetailRow[] {
  return [
    row("x", coordinates.x),
    row("y", coordinates.y),
  ]
}

function section(title: string, rows: Iec61850DebugDetailRow[]): Iec61850DebugDetailSection {
  return { title, rows }
}

function row(label: string, value: unknown): Iec61850DebugDetailRow {
  return {
    label,
    value: formatValue(value),
  }
}

function actionRow(
  label: string,
  value: unknown,
  action: Iec61850DebugDetailAction,
): Iec61850DebugDetailRow {
  return {
    label,
    value: formatValue(value),
    action,
  }
}

function reportCandidateRow(candidate: NormalizedSclModel["reportSubscriptions"][number]): Iec61850DebugDetailRow {
  return {
    label: candidate.reportControlName,
    value: `${candidate.reportKind} · ${candidate.dataSetRef ?? "unresolved DataSet"} · ${candidate.signalCount} signals`,
    signalCount: candidate.signalCount,
    reportSignalsAction: {
      kind: "report-signals-dialog",
      reportControlValue: `report-control:${candidate.reportControlId}`,
      reportKind: candidate.reportKind,
      dataSetRef: candidate.dataSetRef,
      signalCount: candidate.signalCount,
    },
  }
}

function formatVoltage(voltage: SclVoltage | null): string {
  if (!voltage?.value) {
    return "—"
  }
  return `${voltage.value} ${voltage.multiplier ?? ""}${voltage.unit ?? ""}`.trim()
}

function formatDataSetReference(dataSet: SclDataSet): string {
  return `${dataSet.iedName}/${dataSet.accessPointName}/${dataSet.logicalDeviceInst}/${dataSet.logicalNodeName}.${dataSet.name}`
}

function reportKindLabel(reportControl: SclReportControl): "BRCB" | "URCB" {
  return reportControl.buffered ? "BRCB" : "URCB"
}

function collectDataSets(model: NormalizedSclModel): SclDataSet[] {
  return model.ieds.flatMap(ied => collectIedDataSets(ied))
}

function collectReportControls(model: NormalizedSclModel): SclReportControl[] {
  return model.ieds.flatMap(ied => collectIedReportControls(ied))
}

function collectIedDataSets(ied: SclIed): SclDataSet[] {
  return ied.accessPoints.flatMap(accessPoint => collectAccessPointDataSets(accessPoint))
}

function collectIedReportControls(ied: SclIed): SclReportControl[] {
  return ied.accessPoints.flatMap(accessPoint => collectAccessPointReportControls(accessPoint))
}

function collectAccessPointDataSets(accessPoint: SclAccessPoint): SclDataSet[] {
  return accessPoint.server ? collectServerDataSets(accessPoint.server) : []
}

function collectAccessPointReportControls(accessPoint: SclAccessPoint): SclReportControl[] {
  return accessPoint.server ? collectServerReportControls(accessPoint.server) : []
}

function collectServerDataSets(server: SclServer): SclDataSet[] {
  return server.logicalDevices.flatMap(logicalDevice => (
    logicalDevice.logicalNodes.flatMap(logicalNode => logicalNode.dataSets)
  ))
}

function collectServerReportControls(server: SclServer): SclReportControl[] {
  return server.logicalDevices.flatMap(logicalDevice => (
    logicalDevice.logicalNodes.flatMap(logicalNode => logicalNode.reportControls)
  ))
}

function countAccessPoints(model: NormalizedSclModel): number {
  return model.ieds.reduce((sum, ied) => sum + ied.accessPoints.length, 0)
}

function countLogicalDevices(model: NormalizedSclModel): number {
  return model.ieds.reduce((sum, ied) => sum + countIedLogicalDevices(ied), 0)
}

function countLogicalNodes(model: NormalizedSclModel): number {
  return model.ieds.reduce((sum, ied) => sum + countIedLogicalNodes(ied), 0)
}

function countIedLogicalDevices(ied: SclIed): number {
  return ied.accessPoints.reduce((sum, accessPoint) => (
    sum + (accessPoint.server?.logicalDevices.length ?? 0)
  ), 0)
}

function countIedLogicalNodes(ied: SclIed): number {
  return ied.accessPoints.reduce((sum, accessPoint) => sum + countAccessPointLogicalNodes(accessPoint), 0)
}

function countAccessPointLogicalNodes(accessPoint: SclAccessPoint): number {
  return accessPoint.server ? countServerLogicalNodes(accessPoint.server) : 0
}

function countServerLogicalNodes(server: SclServer): number {
  return server.logicalDevices.reduce((sum, logicalDevice) => sum + logicalDevice.logicalNodes.length, 0)
}

function formatValue(value: unknown): string {
  if (value === null || value === undefined || value === "") {
    return "—"
  }
  if (typeof value === "number") {
    return Number.isFinite(value) ? String(value) : "—"
  }
  if (typeof value === "boolean") {
    return value ? "true" : "false"
  }
  return String(value)
}

function isPresent(value: string | null | undefined): value is string {
  return Boolean(value)
}
