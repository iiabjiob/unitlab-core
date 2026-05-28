import type {
  ScdSourceLocation,
  SclEquipmentKind,
  SldConnection,
  SldDocument,
  SldElement,
  SldRoutePoint,
} from "@/modules/scd-sld-core"
import type { Switchgear } from "@/types/switchgear"
import type {
  DiagramEdge,
  DiagramStaticElement,
  DiagramStaticKind,
  DiagramTextElement,
  StoredDiagramState,
} from "./switchgearSldDiagramTypes"

export type SwitchgearSldImportCandidate = {
  id: string
  sourceId: string
  sourcePath: string
  label: string
  equipmentType: string
  kind: Extract<SclEquipmentKind, "breaker" | "disconnector">
  switchgearType: "switchgear" | "disconnector" | "earthing"
  position: SldRoutePoint
}

export type SwitchgearSldImportAdapterDiagnostic = {
  severity: "warning"
  code: string
  message: string
  sourceId?: string
  sourcePath?: string
  sourceLocation?: ScdSourceLocation
}

export type SwitchgearSldImportAdapterResult = {
  diagram: StoredDiagramState
  switchgearCandidates: SwitchgearSldImportCandidate[]
  diagnostics: SwitchgearSldImportAdapterDiagnostic[]
}

export type SwitchgearSldCandidateDecision = {
  candidate: SwitchgearSldImportCandidate
  action: "create" | "reuse-existing"
  existingSwitchgearId: number | null
  createName: string
}

export type SwitchgearSldImportAdapterOptions = {
  stagePadding?: number
}

const DEFAULT_STAGE_PADDING = 50000
const DEFAULT_BUSBAR_WIDTH = 144
const DEFAULT_BUSBAR_HEIGHT = 24
const LABEL_OFFSET_Y = 32
const FEEDER_EXIT_LABEL_OFFSET_X = 44
const FEEDER_EXIT_LABEL_OFFSET_Y = -18
const GROUND_TERMINATOR_OFFSET = 48
const GROUND_TERMINATOR_CONNECTOR_OFFSET = 12
const DEFAULT_TEXT_SIZE = "md" as const
const DEFAULT_STATIC_SIZE = "md" as const
const GROUND_TERMINATOR_STATIC_SIZE = "sm" as const

type PositionedSldElement = {
  element: SldElement
  position: SldRoutePoint
}

export function adaptSldDocumentToSwitchgearDiagram(
  document: SldDocument,
  options: SwitchgearSldImportAdapterOptions = {},
): SwitchgearSldImportAdapterResult {
  const stagePadding = normalizeStagePadding(options.stagePadding)
  const diagnostics: SwitchgearSldImportAdapterDiagnostic[] = []
  const switchgearCandidates: SwitchgearSldImportCandidate[] = []
  const edges: DiagramEdge[] = []
  const staticElements: DiagramStaticElement[] = []
  const textElements: DiagramTextElement[] = []
  const positionedElements: PositionedSldElement[] = []
  const textElementSourceIds = new Set<string>()
  const elementsBySourceId = new Map(document.elements.map(element => [element.sourceId, element]))

  for (const element of document.elements) {
    const position = toDiagramPoint(element.position, stagePadding)
    if (!position) {
      diagnostics.push(missingPositionDiagnostic(element))
      continue
    }
    positionedElements.push({ element, position })

    if (element.visual.representation === "busbar") {
      edges.push(buildBusbarEdge(element, position))
      continue
    }

    const staticKind = staticKindForElement(element)
    if (staticKind) {
      staticElements.push({
        id: buildElementId("static", element),
        kind: staticKind,
        size: DEFAULT_STATIC_SIZE,
        x: position.x,
        y: position.y,
        rotation: 0,
      })
      continue
    }

    if (isSwitchgearCandidateKind(element.kind)) {
      textElementSourceIds.add(element.sourceId)
      switchgearCandidates.push({
        id: buildElementId("candidate", element),
        sourceId: element.sourceId,
        sourcePath: element.sourcePath,
        label: element.label,
        equipmentType: element.equipmentType,
        kind: element.kind,
        switchgearType: switchgearTypeForElement(element),
        position,
      })
      continue
    }

    textElementSourceIds.add(element.sourceId)
    textElements.push(buildElementTextElement(element, position))
  }

  appendGroundedDisconnectorTerminators(positionedElements, edges, staticElements)

  for (const connection of document.connections) {
    edges.push(...buildConnectionEdges(connection, stagePadding, elementsBySourceId))
  }

  for (const label of document.labels) {
    if (textElementSourceIds.has(label.sourceId)) {
      continue
    }

    const position = toDiagramPoint(label.position, stagePadding)
    if (!position) {
      continue
    }

    textElements.push({
      id: `sld-import-label:${sanitizeId(label.id)}`,
      text: label.text,
      size: DEFAULT_TEXT_SIZE,
      x: position.x,
      y: position.y + LABEL_OFFSET_Y,
    })
  }

  return {
    diagram: {
      layoutById: {},
      edges,
      staticElements,
      textElements,
      snapEnabled: true,
    },
    switchgearCandidates,
    diagnostics,
  }
}

export function mergeGeneratedSldDiagramOverlay(
  current: StoredDiagramState,
  generated: StoredDiagramState,
): StoredDiagramState {
  const currentEdges = current.edges ?? current.lines ?? []
  const generatedEdges = generated.edges ?? generated.lines ?? []

  return {
    ...current,
    edges: [
      ...currentEdges.filter(edge => !isGeneratedSldImportId(edge.id)),
      ...generatedEdges,
    ],
    staticElements: [
      ...(current.staticElements ?? []).filter(element => !isGeneratedSldImportId(element.id)),
      ...(generated.staticElements ?? []),
    ],
    textElements: [
      ...(current.textElements ?? []).filter(element => !isGeneratedSldImportId(element.id)),
      ...(generated.textElements ?? []),
    ],
    snapEnabled: current.snapEnabled ?? generated.snapEnabled,
  }
}

export function isGeneratedSldImportId(id: string): boolean {
  return id.startsWith("sld-import-")
}

export function buildSwitchgearCandidateDecisions(
  candidates: SwitchgearSldImportCandidate[],
  existingSwitchgears: Switchgear[],
): SwitchgearSldCandidateDecision[] {
  const existingByNameAndType = new Map<string, Switchgear[]>()
  const usedNames = new Set<string>()

  for (const switchgear of existingSwitchgears) {
    const key = candidateMatchKey(switchgear.name, switchgear.switchgear_type)
    existingByNameAndType.set(key, [...(existingByNameAndType.get(key) ?? []), switchgear])
    usedNames.add(normalizeCandidateName(switchgear.name))
  }

  return candidates.map((candidate) => {
    const key = candidateMatchKey(candidate.label, candidate.switchgearType)
    const existingCandidates = existingByNameAndType.get(key) ?? []
    const existing = existingCandidates.shift() ?? null
    const createName = existing
      ? existing.name
      : reserveUniqueCandidateName(candidate.label, usedNames)
    existingByNameAndType.set(key, existingCandidates)

    return {
      candidate,
      action: existing ? "reuse-existing" : "create",
      existingSwitchgearId: existing?.id ?? null,
      createName,
    }
  })
}

function buildBusbarEdge(element: SldElement, position: SldRoutePoint): DiagramEdge {
  const dimensions = element.visual.dimensions ?? {
    width: DEFAULT_BUSBAR_WIDTH,
    height: DEFAULT_BUSBAR_HEIGHT,
  }
  const horizontal = element.visual.orientation !== "vertical"
  const halfLength = horizontal ? dimensions.width / 2 : dimensions.height / 2

  return {
    id: buildElementId("busbar", element),
    x1: horizontal ? position.x - halfLength : position.x,
    y1: horizontal ? position.y : position.y - halfLength,
    x2: horizontal ? position.x + halfLength : position.x,
    y2: horizontal ? position.y : position.y + halfLength,
    kind: "line",
    weight: element.visual.strokeWeight === "bold" ? "bold" : "normal",
    startBinding: null,
    endBinding: null,
  }
}

function buildConnectionEdges(
  connection: SldConnection,
  stagePadding: number,
  elementsBySourceId: Map<string, SldElement>,
): DiagramEdge[] {
  const route = connection.route
  if (!route) {
    return []
  }

  return route.segments.flatMap((segment, segmentIndex) => {
    const edges: DiagramEdge[] = []
    for (let pointIndex = 1; pointIndex < segment.points.length; pointIndex += 1) {
      const start = toDiagramRoutePoint(segment.points[pointIndex - 1]!, stagePadding)
      const end = toDiagramRoutePoint(segment.points[pointIndex]!, stagePadding)
      if (start.x === end.x && start.y === end.y) {
        continue
      }
      edges.push({
        id: `sld-import-connection:${sanitizeId(connection.id)}:${segmentIndex}:${pointIndex - 1}`,
        x1: start.x,
        y1: start.y,
        x2: end.x,
        y2: end.y,
        kind: isFeederArrowSegment(segment.terminalOwnerId, pointIndex, segment.points, elementsBySourceId) ? "arrow" : "line",
        weight: "normal",
        startBinding: null,
        endBinding: null,
      })
    }
    return edges
  })
}

function isFeederArrowSegment(
  terminalOwnerId: string,
  pointIndex: number,
  points: SldRoutePoint[],
  elementsBySourceId: Map<string, SldElement>,
): boolean {
  return elementsBySourceId.get(terminalOwnerId)?.kind === "feeder"
    && pointIndex === points.length - 1
}

function staticKindForElement(element: SldElement): DiagramStaticKind | null {
  if (element.kind === "transformer") {
    return "transformer"
  }
  if (element.kind === "ground") {
    return "ground"
  }
  return null
}

function isSwitchgearCandidateKind(kind: SldElement["kind"]): kind is SwitchgearSldImportCandidate["kind"] {
  return kind === "breaker" || kind === "disconnector"
}

function switchgearTypeForElement(element: SldElement): SwitchgearSldImportCandidate["switchgearType"] {
  if (element.kind === "breaker") {
    return "switchgear"
  }
  return element.grounded ? "earthing" : "disconnector"
}

function appendGroundedDisconnectorTerminators(
  positionedElements: PositionedSldElement[],
  edges: DiagramEdge[],
  staticElements: DiagramStaticElement[],
) {
  const positionedByBay = groupPositionedElementsByBay(positionedElements)

  for (const item of positionedElements) {
    if (item.element.kind !== "disconnector" || !item.element.grounded) {
      continue
    }

    const direction = resolveGroundTerminatorDirection(item, positionedByBay)
    const groundX = item.position.x + direction * GROUND_TERMINATOR_OFFSET
    const groundY = item.position.y
    edges.push({
      id: `sld-import-ground-connection:${sanitizeId(item.element.sourceId)}`,
      x1: item.position.x,
      y1: item.position.y,
      x2: groundX - direction * GROUND_TERMINATOR_CONNECTOR_OFFSET,
      y2: groundY,
      kind: "line",
      weight: "normal",
      startBinding: null,
      endBinding: null,
    })
    staticElements.push({
      id: `sld-import-static-ground:${sanitizeId(item.element.sourceId)}`,
      kind: "ground",
      size: GROUND_TERMINATOR_STATIC_SIZE,
      x: groundX,
      y: groundY,
      rotation: direction > 0 ? 0 : 180,
    })
  }
}

function groupPositionedElementsByBay(positionedElements: PositionedSldElement[]): Map<string, PositionedSldElement[]> {
  const groups = new Map<string, PositionedSldElement[]>()
  for (const item of positionedElements) {
    const key = item.element.bayName?.trim()
    if (!key) {
      continue
    }
    groups.set(key, [...(groups.get(key) ?? []), item])
  }
  return groups
}

function resolveGroundTerminatorDirection(
  item: PositionedSldElement,
  positionedByBay: Map<string, PositionedSldElement[]>,
): 1 | -1 {
  const bayItems = item.element.bayName
    ? positionedByBay.get(item.element.bayName.trim()) ?? []
    : []
  const peerPositions = bayItems
    .filter(candidate => candidate.element.sourceId !== item.element.sourceId)
    .map(candidate => candidate.position.x)

  if (peerPositions.length === 0) {
    return 1
  }

  const averageX = peerPositions.reduce((sum, value) => sum + value, 0) / peerPositions.length
  return item.position.x < averageX ? -1 : 1
}

function buildElementTextElement(element: SldElement, position: SldRoutePoint): DiagramTextElement {
  if (element.kind === "feeder") {
    return {
      id: buildElementId("feeder-label", element),
      text: element.bayName?.trim() || element.label,
      size: DEFAULT_TEXT_SIZE,
      x: position.x + FEEDER_EXIT_LABEL_OFFSET_X,
      y: position.y + FEEDER_EXIT_LABEL_OFFSET_Y,
    }
  }

  return {
    id: buildElementId("text", element),
    text: fallbackElementText(element),
    size: DEFAULT_TEXT_SIZE,
    x: position.x,
    y: position.y,
  }
}

function fallbackElementText(element: SldElement): string {
  return element.kind === "unknown"
    ? `${element.label} (${element.equipmentType || "unknown"})`
    : element.label
}

function toDiagramPoint(
  position: { x: number | null; y: number | null },
  stagePadding: number,
): SldRoutePoint | null {
  if (position.x === null || position.y === null || !Number.isFinite(position.x) || !Number.isFinite(position.y)) {
    return null
  }
  return {
    x: position.x + stagePadding,
    y: position.y + stagePadding,
  }
}

function toDiagramRoutePoint(point: SldRoutePoint, stagePadding: number): SldRoutePoint {
  return {
    x: point.x + stagePadding,
    y: point.y + stagePadding,
  }
}

function missingPositionDiagnostic(element: SldElement): SwitchgearSldImportAdapterDiagnostic {
  return {
    severity: "warning",
    code: "switchgear-sld-import.missing-element-position",
    message: `Generated SLD element "${element.label}" has no resolved position and was skipped by the diagram adapter.`,
    sourceId: element.sourceId,
    sourcePath: element.sourcePath,
    sourceLocation: element.sourceLocation,
  }
}

function buildElementId(prefix: string, element: SldElement): string {
  return `sld-import-${prefix}:${sanitizeId(element.sourceId)}`
}

function sanitizeId(value: string): string {
  return value.trim().replace(/[^a-zA-Z0-9_-]+/g, "_")
}

function normalizeStagePadding(value: number | undefined): number {
  return Number.isFinite(value) && value !== undefined ? value : DEFAULT_STAGE_PADDING
}

function candidateMatchKey(name: string, switchgearType: string): string {
  return `${normalizeCandidateName(name)}\u0000${switchgearType.trim().toLowerCase()}`
}

function normalizeCandidateName(name: string): string {
  return name.trim().toLowerCase()
}

function reserveUniqueCandidateName(name: string, usedNames: Set<string>): string {
  const baseName = name.trim() || "Imported switchgear"
  const normalizedBase = normalizeCandidateName(baseName)
  if (!usedNames.has(normalizedBase)) {
    usedNames.add(normalizedBase)
    return baseName
  }

  for (let index = 2; index < 10000; index += 1) {
    const nextName = `${baseName} ${index}`
    const normalized = normalizeCandidateName(nextName)
    if (!usedNames.has(normalized)) {
      usedNames.add(normalized)
      return nextName
    }
  }

  return `${baseName} ${Date.now()}`
}
