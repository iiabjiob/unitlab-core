import type {
  SclEquipmentKind,
  SldBayEarthSwitchPlacement,
  SldBayEquipmentRole,
  SldBayInterpretation,
  SldBayLayoutOrientation,
  SldBayOutgoingSide,
  SldElementStrokeWeight,
} from "./types"

export const FEEDER_TEMPLATE_UNITS = {
  centerX: 5,
  leftX: 0,
  rightX: 10,
  feederY: 0,
  upperDisconnectorY: 3,
  breakerY: 6,
  busSelectorBridgeY: 9,
  busDisconnectorY: 10,
  busbarUpperY: 12,
  busbarLowerY: 15,
  busEarthBranchDeltaY: 0.5,
  sideEarthXOffset: 3,
  sideEarthY: 1,
  sideEarthYStep: 3,
} as const

export type SldBayLayoutTemplatePoint = {
  x: number
  y: number
}

export type SldBayLayoutTemplateSlot = {
  id: string
  label: string
  role: SldBayEquipmentRole
  kind: SclEquipmentKind
  required: boolean
  point: SldBayLayoutTemplatePoint
}

export type SldBayLayoutTemplateWire = {
  id: string
  label: string
  points: SldBayLayoutTemplatePoint[]
  weight: SldElementStrokeWeight
}

export type SldBayLayoutTemplateLabel = {
  id: string
  text: string
  point: SldBayLayoutTemplatePoint
}

export type SldBayLayoutTemplate = {
  id: string
  name: string
  description: string
  interpretation: SldBayInterpretation
  orientation: SldBayLayoutOrientation
  busbarCount: number
  outgoingSide: SldBayOutgoingSide
  earthSwitchPlacement: SldBayEarthSwitchPlacement
  grid: {
    widthUnits: number
    heightUnits: number
  }
  slots: SldBayLayoutTemplateSlot[]
  wires: SldBayLayoutTemplateWire[]
  labels: SldBayLayoutTemplateLabel[]
}

const singleBusFeederTemplate: SldBayLayoutTemplate = {
  id: "single-bus-feeder.vertical-up",
  name: "Single bus feeder",
  description: "One bus selector, line disconnector, circuit breaker, outgoing feeder and line-side earth switch.",
  interpretation: "single-bus-feeder",
  orientation: "up",
  busbarCount: 1,
  outgoingSide: "top",
  earthSwitchPlacement: "line-side",
  grid: { widthUnits: 14, heightUnits: 14 },
  slots: [
    feederSlot("outgoing", "Outgoing feeder", FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.feederY, true),
    disconnectorSlot("line-disconnector", "Line disconnector", "lineDisconnector", FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.upperDisconnectorY, false),
    breakerSlot("circuit-breaker", "Circuit breaker", FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.breakerY, true),
    disconnectorSlot("bus-disconnector", "Bus disconnector", "busDisconnector", FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.busDisconnectorY, true),
    disconnectorSlot("earth-switch", "Earth switch", "earthSwitch", FEEDER_TEMPLATE_UNITS.centerX + FEEDER_TEMPLATE_UNITS.sideEarthXOffset, FEEDER_TEMPLATE_UNITS.sideEarthY, false),
  ],
  wires: [
    primaryWire("outgoing-to-line-disconnector", "Outgoing vertical", [
      point(FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.feederY),
      point(FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.upperDisconnectorY),
    ]),
    primaryWire("line-disconnector-to-breaker", "Line disconnector to breaker", [
      point(FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.upperDisconnectorY),
      point(FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.breakerY),
    ]),
    primaryWire("breaker-to-bus-disconnector", "Breaker to bus disconnector", [
      point(FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.breakerY),
      point(FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.busDisconnectorY),
    ]),
    primaryWire("bus-disconnector-to-busbar", "Bus disconnector to busbar", [
      point(FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.busDisconnectorY),
      point(FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.busbarUpperY),
    ]),
    busbarWire("busbar", "Busbar", [
      point(FEEDER_TEMPLATE_UNITS.leftX, FEEDER_TEMPLATE_UNITS.busbarUpperY),
      point(FEEDER_TEMPLATE_UNITS.rightX, FEEDER_TEMPLATE_UNITS.busbarUpperY),
    ]),
    primaryWire("earth-branch", "Earth switch branch", [
      point(FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.sideEarthY),
      point(FEEDER_TEMPLATE_UNITS.centerX + FEEDER_TEMPLATE_UNITS.sideEarthXOffset, FEEDER_TEMPLATE_UNITS.sideEarthY),
    ]),
  ],
  labels: [
    { id: "bay-label", text: "Bay name", point: point(FEEDER_TEMPLATE_UNITS.centerX + 1.2, FEEDER_TEMPLATE_UNITS.feederY - 0.4) },
  ],
}

const doubleBusFeederTemplate: SldBayLayoutTemplate = {
  id: "double-bus-feeder.vertical-up",
  name: "Double bus feeder",
  description: "Two bus selectors, line disconnector, circuit breaker, outgoing feeder and bus-side earth switches.",
  interpretation: "double-bus-feeder",
  orientation: "up",
  busbarCount: 2,
  outgoingSide: "top",
  earthSwitchPlacement: "bus-side",
  grid: { widthUnits: 14, heightUnits: 17 },
  slots: [
    feederSlot("outgoing", "Outgoing feeder", FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.feederY, true),
    disconnectorSlot("line-disconnector", "Line disconnector", "lineDisconnector", FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.upperDisconnectorY, false),
    breakerSlot("circuit-breaker", "Circuit breaker", FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.breakerY, true),
    disconnectorSlot("bus-disconnector-a", "Bus disconnector A", "busDisconnector", FEEDER_TEMPLATE_UNITS.leftX, FEEDER_TEMPLATE_UNITS.busDisconnectorY, true),
    disconnectorSlot("bus-disconnector-b", "Bus disconnector B", "busDisconnector", FEEDER_TEMPLATE_UNITS.rightX, FEEDER_TEMPLATE_UNITS.busDisconnectorY, true),
    disconnectorSlot("earth-switch-a", "Earth switch A", "earthSwitch", FEEDER_TEMPLATE_UNITS.leftX + FEEDER_TEMPLATE_UNITS.sideEarthXOffset, FEEDER_TEMPLATE_UNITS.busDisconnectorY, false),
    disconnectorSlot("earth-switch-b", "Earth switch B", "earthSwitch", FEEDER_TEMPLATE_UNITS.rightX + FEEDER_TEMPLATE_UNITS.sideEarthXOffset, FEEDER_TEMPLATE_UNITS.busDisconnectorY, false),
  ],
  wires: [
    primaryWire("outgoing-to-line-disconnector", "Outgoing vertical", [
      point(FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.feederY),
      point(FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.upperDisconnectorY),
    ]),
    primaryWire("line-disconnector-to-breaker", "Line disconnector to breaker", [
      point(FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.upperDisconnectorY),
      point(FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.breakerY),
    ]),
    primaryWire("breaker-to-selector-bridge", "Breaker to selector bridge", [
      point(FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.breakerY),
      point(FEEDER_TEMPLATE_UNITS.centerX, FEEDER_TEMPLATE_UNITS.busSelectorBridgeY),
    ]),
    primaryWire("selector-bridge", "Selector bridge", [
      point(FEEDER_TEMPLATE_UNITS.leftX, FEEDER_TEMPLATE_UNITS.busSelectorBridgeY),
      point(FEEDER_TEMPLATE_UNITS.rightX, FEEDER_TEMPLATE_UNITS.busSelectorBridgeY),
    ]),
    primaryWire("selector-a-to-busbar-a", "Bus selector A to busbar A", [
      point(FEEDER_TEMPLATE_UNITS.leftX, FEEDER_TEMPLATE_UNITS.busSelectorBridgeY),
      point(FEEDER_TEMPLATE_UNITS.leftX, FEEDER_TEMPLATE_UNITS.busbarUpperY),
    ]),
    primaryWire("selector-b-to-busbar-b", "Bus selector B to busbar B", [
      point(FEEDER_TEMPLATE_UNITS.rightX, FEEDER_TEMPLATE_UNITS.busSelectorBridgeY),
      point(FEEDER_TEMPLATE_UNITS.rightX, FEEDER_TEMPLATE_UNITS.busbarLowerY),
    ]),
    busbarWire("busbar-a", "Busbar A", [
      point(FEEDER_TEMPLATE_UNITS.leftX - 2, FEEDER_TEMPLATE_UNITS.busbarUpperY),
      point(FEEDER_TEMPLATE_UNITS.rightX + 2, FEEDER_TEMPLATE_UNITS.busbarUpperY),
    ]),
    busbarWire("busbar-b", "Busbar B", [
      point(FEEDER_TEMPLATE_UNITS.leftX - 2, FEEDER_TEMPLATE_UNITS.busbarLowerY),
      point(FEEDER_TEMPLATE_UNITS.rightX + 2, FEEDER_TEMPLATE_UNITS.busbarLowerY),
    ]),
    primaryWire("earth-a", "Earth switch A branch", [
      point(FEEDER_TEMPLATE_UNITS.leftX, FEEDER_TEMPLATE_UNITS.busDisconnectorY + FEEDER_TEMPLATE_UNITS.busEarthBranchDeltaY),
      point(FEEDER_TEMPLATE_UNITS.leftX + FEEDER_TEMPLATE_UNITS.sideEarthXOffset, FEEDER_TEMPLATE_UNITS.busDisconnectorY),
    ]),
    primaryWire("earth-b", "Earth switch B branch", [
      point(FEEDER_TEMPLATE_UNITS.rightX, FEEDER_TEMPLATE_UNITS.busDisconnectorY + FEEDER_TEMPLATE_UNITS.busEarthBranchDeltaY),
      point(FEEDER_TEMPLATE_UNITS.rightX + FEEDER_TEMPLATE_UNITS.sideEarthXOffset, FEEDER_TEMPLATE_UNITS.busDisconnectorY),
    ]),
  ],
  labels: [
    { id: "bay-label", text: "Bay name", point: point(FEEDER_TEMPLATE_UNITS.centerX + 1.2, FEEDER_TEMPLATE_UNITS.feederY - 0.4) },
  ],
}

const templates = [
  singleBusFeederTemplate,
  doubleBusFeederTemplate,
] as const

export function getSldBayLayoutTemplates(): SldBayLayoutTemplate[] {
  return [...templates]
}

function point(x: number, y: number): SldBayLayoutTemplatePoint {
  return { x, y }
}

function feederSlot(
  id: string,
  label: string,
  x: number,
  y: number,
  required: boolean,
): SldBayLayoutTemplateSlot {
  return {
    id,
    label,
    role: "feederTerminal",
    kind: "feeder",
    required,
    point: point(x, y),
  }
}

function breakerSlot(
  id: string,
  label: string,
  x: number,
  y: number,
  required: boolean,
): SldBayLayoutTemplateSlot {
  return {
    id,
    label,
    role: "circuitBreaker",
    kind: "breaker",
    required,
    point: point(x, y),
  }
}

function disconnectorSlot(
  id: string,
  label: string,
  role: Extract<SldBayEquipmentRole, "busDisconnector" | "lineDisconnector" | "earthSwitch">,
  x: number,
  y: number,
  required: boolean,
): SldBayLayoutTemplateSlot {
  return {
    id,
    label,
    role,
    kind: "disconnector",
    required,
    point: point(x, y),
  }
}

function primaryWire(
  id: string,
  label: string,
  points: SldBayLayoutTemplatePoint[],
): SldBayLayoutTemplateWire {
  return {
    id,
    label,
    points,
    weight: "normal",
  }
}

function busbarWire(
  id: string,
  label: string,
  points: SldBayLayoutTemplatePoint[],
): SldBayLayoutTemplateWire {
  return {
    id,
    label,
    points,
    weight: "bold",
  }
}
