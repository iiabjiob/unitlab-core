import type {
  SclEquipmentKind,
  SldConnection,
  SldDocument,
  SldElement,
  SldRoutePoint,
} from "@/modules/scd-sld-core"
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
  switchgearType: "switchgear" | "disconnector"
  position: SldRoutePoint
}

export type SwitchgearSldImportAdapterDiagnostic = {
  severity: "warning"
  code: string
  message: string
  sourceId?: string
  sourcePath?: string
}

export type SwitchgearSldImportAdapterResult = {
  diagram: StoredDiagramState
  switchgearCandidates: SwitchgearSldImportCandidate[]
  diagnostics: SwitchgearSldImportAdapterDiagnostic[]
}

export type SwitchgearSldImportAdapterOptions = {
  stagePadding?: number
}

const DEFAULT_STAGE_PADDING = 50000
const DEFAULT_BUSBAR_WIDTH = 144
const DEFAULT_BUSBAR_HEIGHT = 24
const LABEL_OFFSET_Y = 32
const DEFAULT_TEXT_SIZE = "md" as const
const DEFAULT_STATIC_SIZE = "md" as const

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
  const textElementSourceIds = new Set<string>()

  for (const element of document.elements) {
    const position = toDiagramPoint(element.position, stagePadding)
    if (!position) {
      diagnostics.push(missingPositionDiagnostic(element))
      continue
    }

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
      switchgearCandidates.push({
        id: buildElementId("candidate", element),
        sourceId: element.sourceId,
        sourcePath: element.sourcePath,
        label: element.label,
        equipmentType: element.equipmentType,
        kind: element.kind,
        switchgearType: switchgearTypeForKind(element.kind),
        position,
      })
      continue
    }

    textElementSourceIds.add(element.sourceId)
    textElements.push({
      id: buildElementId("text", element),
      text: fallbackElementText(element),
      size: DEFAULT_TEXT_SIZE,
      x: position.x,
      y: position.y,
    })
  }

  for (const connection of document.connections) {
    edges.push(...buildConnectionEdges(connection, stagePadding))
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

function buildConnectionEdges(connection: SldConnection, stagePadding: number): DiagramEdge[] {
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
        kind: "line",
        weight: "normal",
        startBinding: null,
        endBinding: null,
      })
    }
    return edges
  })
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

function switchgearTypeForKind(kind: SwitchgearSldImportCandidate["kind"]): SwitchgearSldImportCandidate["switchgearType"] {
  return kind === "breaker" ? "switchgear" : "disconnector"
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
