import type {
  NormalizedSclModel,
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

export function buildIec61850DebugTreeRows(model: NormalizedSclModel): Iec61850DebugTreeRow[] {
  const rows: Iec61850DebugTreeRow[] = []
  const dataSetById = new Map(collectDataSets(model).map(dataSet => [dataSet.id, dataSet]))

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
          detail: buildSwitchgearsGroupDetail(bay, switchgears),
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
      detail: buildIedsGroupDetail(model),
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
        detail: buildIedDetail(ied),
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
          detail: buildAccessPointDetail(accessPoint, ied),
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
          detail: buildServerDetail(accessPoint.server, accessPoint, ied),
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
            detail: buildLogicalDeviceDetail(logicalDevice, accessPoint, ied),
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
              detail: buildLogicalNodeDetail(logicalNode),
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
                detail: buildDataSetsGroupDetail(logicalNode),
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
                  detail: buildDataSetDetail(dataSet),
                })

                dataSet.members.forEach((member, index) => {
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
                detail: buildReportsGroupDetail(logicalNode),
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
                  detail: buildReportControlDetail(reportControl, reportDataSet),
                })

                reportSignals.forEach((member, index) => {
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

function buildSwitchgearsGroupDetail(bay: SclBay, switchgears: SclEquipment[]): Iec61850DebugDetail {
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
        ? switchgears.map(equipment => row(equipment.name, `${equipment.kind} · ${equipment.type}`))
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

function buildIedsGroupDetail(model: NormalizedSclModel): Iec61850DebugDetail {
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
        ? model.reportSubscriptions.map(candidate => row(
          candidate.reportControlName,
          `${candidate.reportKind} · ${candidate.dataSetRef ?? "unresolved DataSet"} · ${candidate.signalCount} signals`,
        ))
        : [row("candidates", "none")]),
    ],
  }
}

function buildIedDetail(ied: SclIed): Iec61850DebugDetail {
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
        row("normalized id", ied.id),
        row("source path", ied.sourcePath),
      ]),
      section("Runtime inventory", [
        row("access points", ied.accessPoints.length),
        row("logical devices", countIedLogicalDevices(ied)),
        row("logical nodes", countIedLogicalNodes(ied)),
        row("DataSets", collectIedDataSets(ied).length),
        row("ReportControls", collectIedReportControls(ied).length),
      ]),
    ],
  }
}

function buildAccessPointDetail(accessPoint: SclAccessPoint, ied: SclIed): Iec61850DebugDetail {
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
        row("server present", Boolean(accessPoint.server)),
        row("normalized id", accessPoint.id),
        row("source path", accessPoint.sourcePath),
      ]),
      section("Runtime inventory", [
        row("logical devices", accessPoint.server?.logicalDevices.length ?? 0),
        row("logical nodes", countAccessPointLogicalNodes(accessPoint)),
        row("DataSets", collectAccessPointDataSets(accessPoint).length),
        row("ReportControls", collectAccessPointReportControls(accessPoint).length),
      ]),
    ],
  }
}

function buildServerDetail(server: SclServer, accessPoint: SclAccessPoint, ied: SclIed): Iec61850DebugDetail {
  return {
    title: "Server",
    subtitle: "SCL AccessPoint Server",
    sections: [
      section("Scope", [
        row("IED", ied.name),
        row("access point", accessPoint.name),
        row("normalized id", server.id),
        row("source path", server.sourcePath),
      ]),
      section("Runtime inventory", [
        row("logical devices", server.logicalDevices.length),
        row("logical nodes", countServerLogicalNodes(server)),
        row("DataSets", collectServerDataSets(server).length),
        row("ReportControls", collectServerReportControls(server).length),
      ]),
    ],
  }
}

function buildLogicalDeviceDetail(
  logicalDevice: SclLogicalDevice,
  accessPoint: SclAccessPoint,
  ied: SclIed,
): Iec61850DebugDetail {
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
        row("DataSets", logicalDevice.logicalNodes.reduce((sum, node) => sum + node.dataSets.length, 0)),
        row("ReportControls", logicalDevice.logicalNodes.reduce((sum, node) => sum + node.reportControls.length, 0)),
      ]),
    ],
  }
}

function buildLogicalNodeDetail(logicalNode: SclLogicalNode): Iec61850DebugDetail {
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
        row("DataSets", logicalNode.dataSets.length),
        row("DataSet members", logicalNode.dataSets.reduce((sum, dataSet) => sum + dataSet.members.length, 0)),
        row("ReportControls", logicalNode.reportControls.length),
      ]),
    ],
  }
}

function buildDataSetsGroupDetail(logicalNode: SclLogicalNode): Iec61850DebugDetail {
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
      section("Members", logicalNode.dataSets.map(dataSet => row(dataSet.name, `${dataSet.members.length} signals`))),
    ],
  }
}

function buildDataSetDetail(dataSet: SclDataSet): Iec61850DebugDetail {
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
        ? dataSet.members.map((member, index) => row(`signal ${index + 1}`, member.reference))
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

function buildReportsGroupDetail(logicalNode: SclLogicalNode): Iec61850DebugDetail {
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
      section("Members", logicalNode.reportControls.map(reportControl => row(
        reportControl.name,
        `${reportKindLabel(reportControl)} · ${reportControl.dataSetRef ?? "unresolved DataSet"}`,
      ))),
    ],
  }
}

function buildReportControlDetail(
  reportControl: SclReportControl,
  dataSet: SclDataSet | null,
): Iec61850DebugDetail {
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

function formatVoltage(voltage: SclVoltage | null): string {
  if (!voltage?.value) {
    return "—"
  }
  return `${voltage.value} ${voltage.multiplier ?? ""}${voltage.unit ?? ""}`.trim()
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
