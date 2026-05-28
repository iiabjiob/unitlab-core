import type {
  NormalizedSclModel,
  SclBay,
  SclEquipment,
  SclLogicalNodeRef,
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
