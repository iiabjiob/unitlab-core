import type {
  DiagramEdge,
  DiagramPoint,
  DiagramPort,
  DiagramSceneInput,
  DiagramShape,
  DiagramText,
} from "@affino/diagram-core"

import type { Switchgear } from "@/types/switchgear"

import type {
  DiagramBindablePortOwnerType,
  DiagramNodeLayout,
  DiagramPortBinding,
  DiagramStaticElement,
  DiagramStaticKind,
  DiagramStaticSize,
  DiagramTextElement,
  StoredDiagramState,
} from "./switchgearSldDiagramTypes"

export const SWITCHGEAR_SLD_NODE_WIDTH = 40
export const SWITCHGEAR_SLD_NODE_HEIGHT = 40
export const SWITCHGEAR_SLD_STAGE_PADDING = 50000
export const SWITCHGEAR_SLD_DEFAULT_VIEW = Object.freeze({
  x: 96 - SWITCHGEAR_SLD_STAGE_PADDING,
  y: 72 - SWITCHGEAR_SLD_STAGE_PADDING,
  zoom: 1,
})

const GRID_STEP = 24
const LABEL_DEFAULT_OFFSET = Object.freeze({ x: 0, y: 22 })
const DEFAULT_TEXT_LABEL = "TEXT"
const GENERATED_IMPORT_LABEL_ID_PREFIX = "sld-import-label:"
const TEXT_WIDTH_BY_SIZE = {
  md: 96,
} as const
const TEXT_HEIGHT_BY_SIZE = {
  md: 28,
} as const
const STATIC_SIZE_DIMENSIONS: Record<DiagramStaticKind, Record<DiagramStaticSize, { width: number; height: number }>> = {
  transformer: {
    sm: { width: GRID_STEP * 3, height: GRID_STEP * 3 },
    md: { width: GRID_STEP * 4, height: GRID_STEP * 4 },
    lg: { width: GRID_STEP * 6, height: GRID_STEP * 6 },
  },
  ground: {
    sm: { width: GRID_STEP, height: GRID_STEP },
    md: { width: GRID_STEP * 2, height: GRID_STEP * 2 },
    lg: { width: GRID_STEP * 3, height: GRID_STEP * 3 },
  },
}

type DiagramPortOwnerType = DiagramBindablePortOwnerType | "line"

type LegacyDiagramPort = {
  ownerType: DiagramPortOwnerType
  ownerId: number | string
  portId: string
  x: number
  y: number
}

export type SwitchgearSldPackageSceneModel = Readonly<{
  scene: DiagramSceneInput
  sceneKey: string
  stats: {
    nodes: number
    edges: number
    statics: number
    texts: number
  }
}>

export function buildSwitchgearSldPackageSceneModel(
  switchgears: ReadonlyArray<Switchgear>,
  storedState: StoredDiagramState | null,
): SwitchgearSldPackageSceneModel {
  const layoutById = storedState?.layoutById ?? {}
  const labelOffsetById = storedState?.labelOffsetById ?? {}
  const staticElements = normalizeStaticElements(storedState?.staticElements)
  const textElements = normalizeTextElements(storedState?.textElements)
  const ports = [
    ...switchgears.flatMap((switchgear, index) => buildNodePortsForLayout(
      switchgear.id,
      resolveLayout(layoutById, switchgear.id, index),
    )),
  ]
  const staticPorts = staticElements.flatMap(buildStaticPorts)
  const edgePorts = [...ports, ...staticPorts]
  const scene: DiagramSceneInput = {
    nodes: switchgears.map((switchgear, index) => {
      const layout = resolveLayout(layoutById, switchgear.id, index)
      return {
        id: toSwitchgearNodeId(switchgear.id),
        kind: "node",
        x: layout.x + SWITCHGEAR_SLD_STAGE_PADDING,
        y: layout.y + SWITCHGEAR_SLD_STAGE_PADDING,
        width: SWITCHGEAR_SLD_NODE_WIDTH,
        height: SWITCHGEAR_SLD_NODE_HEIGHT,
        portIds: buildNodePortsForLayout(switchgear.id, layout).map((port) => toPortId(port)),
        metadata: {
          entityType: "switchgear",
          switchgearId: switchgear.id,
          switchgearType: switchgear.switchgear_type,
          name: switchgear.name,
        },
      }
    }),
    ports: ports.map((port) => ({
      id: toPortId(port),
      kind: "port",
      nodeId: resolvePortNodeId(port),
      x: port.x,
      y: port.y,
      radius: 4,
      metadata: {
        entityType: "port",
        ownerType: port.ownerType,
        ownerId: String(port.ownerId),
        portId: port.portId,
      },
    })),
    edges: normalizeEdges(storedState).map((edge) => createDiagramEdge(edge, edgePorts)),
    shapes: staticElements.map((element) => createStaticShape(element)),
    texts: [
      ...switchgears.map((switchgear, index) => createSwitchgearLabelText(
        switchgear,
        resolveLayout(layoutById, switchgear.id, index),
        labelOffsetById[String(switchgear.id)] ?? LABEL_DEFAULT_OFFSET,
      )),
      ...textElements.map(createLooseText),
    ],
    viewport: {
      x: resolveViewportX(storedState),
      y: resolveViewportY(storedState),
      zoom: resolveViewportZoom(storedState),
    },
  }

  return {
    scene,
    sceneKey: JSON.stringify({
      switchgears: switchgears.map(item => ({
        id: item.id,
        name: item.name,
        type: item.switchgear_type,
      })),
      layoutById,
      labelOffsetById,
      edges: storedState?.edges ?? storedState?.lines ?? [],
      staticElements,
      textElements,
      viewState: storedState?.viewState ?? null,
    }),
    stats: {
      nodes: switchgears.length,
      edges: scene.edges?.length ?? 0,
      statics: staticElements.length,
      texts: scene.texts?.length ?? 0,
    },
  }
}

export function normalizeStoredDiagramState(value: unknown): StoredDiagramState | null {
  return value && typeof value === "object" && !Array.isArray(value)
    ? value as StoredDiagramState
    : null
}

function createSwitchgearLabelText(
  switchgear: Switchgear,
  layout: DiagramNodeLayout,
  offset: { x: number; y: number },
): DiagramText {
  const nodeX = layout.x + SWITCHGEAR_SLD_STAGE_PADDING
  const nodeY = layout.y + SWITCHGEAR_SLD_STAGE_PADDING

  return {
    id: toSwitchgearLabelId(switchgear.id),
    kind: "text",
    x: nodeX + SWITCHGEAR_SLD_NODE_WIDTH / 2 + offset.x,
    y: nodeY + SWITCHGEAR_SLD_NODE_HEIGHT / 2 + offset.y,
    text: switchgear.name,
    width: Math.max(64, switchgear.name.length * 8 + 18),
    height: 20,
    fontSize: 10,
    metadata: {
      entityType: "switchgear-label",
      switchgearId: switchgear.id,
    },
  }
}

function createLooseText(element: DiagramTextElement): DiagramText {
  return {
    id: element.id,
    kind: "text",
    x: element.x,
    y: element.y,
    text: element.text,
    width: Math.max(
      TEXT_WIDTH_BY_SIZE[element.size],
      Math.min(288, Math.max(64, element.text.length * 8 + 24)),
    ),
    height: TEXT_HEIGHT_BY_SIZE[element.size],
    fontSize: 12,
    metadata: {
      entityType: element.id.startsWith(GENERATED_IMPORT_LABEL_ID_PREFIX) ? "generated-label" : "text",
    },
  }
}

function createStaticShape(element: DiagramStaticElement): DiagramShape {
  const bounds = getStaticElementBounds(element)
  return {
    id: toStaticShapeId(element.id),
    kind: "shape",
    x: bounds.x1,
    y: bounds.y1,
    width: bounds.width,
    height: bounds.height,
    rotation: element.rotation,
    shape: element.kind,
    metadata: {
      entityType: "static",
      staticId: element.id,
      staticKind: element.kind,
      staticSize: element.size,
      rotation: element.rotation,
    },
  }
}

function createDiagramEdge(
  edge: NonNullable<StoredDiagramState["edges"]>[number],
  ports: ReadonlyArray<LegacyDiagramPort>,
): DiagramEdge {
  return {
    id: edge.id,
    kind: "edge",
    source: resolveEdgeEndpoint(edge.startBinding, { x: edge.x1, y: edge.y1 }, ports),
    target: resolveEdgeEndpoint(edge.endBinding, { x: edge.x2, y: edge.y2 }, ports),
    metadata: {
      entityType: "edge",
      edgeKind: edge.kind,
      edgeWeight: edge.weight ?? "normal",
    },
  }
}

function resolveEdgeEndpoint(
  binding: DiagramPortBinding | null | undefined,
  point: DiagramPoint,
  ports: ReadonlyArray<LegacyDiagramPort>,
) {
  const port = binding ? resolvePortBinding(binding, ports) : null
  return port
    ? { kind: "port" as const, portId: toPortId(port) }
    : { kind: "point" as const, point }
}

function resolvePortBinding(
  binding: DiagramPortBinding,
  ports: ReadonlyArray<LegacyDiagramPort>,
): LegacyDiagramPort | null {
  return ports.find((port) => (
    port.ownerType === binding.ownerType
    && String(port.ownerId) === String(binding.ownerId)
    && port.portId === binding.portId
  )) ?? null
}

function buildNodePortsForLayout(nodeId: number, layout: DiagramNodeLayout): LegacyDiagramPort[] {
  const worldX = layout.x + SWITCHGEAR_SLD_STAGE_PADDING
  const worldY = layout.y + SWITCHGEAR_SLD_STAGE_PADDING
  const halfWidth = SWITCHGEAR_SLD_NODE_WIDTH / 2
  const halfHeight = SWITCHGEAR_SLD_NODE_HEIGHT / 2

  return [
    { ownerType: "node", ownerId: nodeId, portId: "top", x: worldX + halfWidth, y: worldY },
    { ownerType: "node", ownerId: nodeId, portId: "right", x: worldX + SWITCHGEAR_SLD_NODE_WIDTH, y: worldY + halfHeight },
    { ownerType: "node", ownerId: nodeId, portId: "bottom", x: worldX + halfWidth, y: worldY + SWITCHGEAR_SLD_NODE_HEIGHT },
    { ownerType: "node", ownerId: nodeId, portId: "left", x: worldX, y: worldY + halfHeight },
  ]
}

function buildStaticPorts(element: DiagramStaticElement): LegacyDiagramPort[] {
  const base = getStaticElementDimensions(element)
  const localPorts = element.kind === "transformer"
    ? [
        { portId: "primary", x: 0, y: -base.height / 2 },
        { portId: "secondary", x: 0, y: base.height / 2 },
      ]
    : [{ portId: "terminal", x: -base.width / 2, y: 0 }]

  return localPorts.map((local) => {
    const rotated = rotateLocalPoint(local, element.rotation)
    return {
      ownerType: "static" as const,
      ownerId: element.id,
      portId: local.portId,
      x: element.x + rotated.x,
      y: element.y + rotated.y,
    }
  })
}

function rotateLocalPoint(point: { x: number; y: number }, rotation: 0 | 90 | 180 | 270) {
  const radians = rotation * Math.PI / 180
  const cos = Math.cos(radians)
  const sin = Math.sin(radians)

  return {
    x: Math.round(point.x * cos - point.y * sin),
    y: Math.round(point.x * sin + point.y * cos),
  }
}

function getStaticElementDimensions(element: Pick<DiagramStaticElement, "kind" | "size">) {
  return STATIC_SIZE_DIMENSIONS[element.kind][element.size]
}

function getStaticElementBounds(element: DiagramStaticElement) {
  const base = getStaticElementDimensions(element)
  const swap = element.rotation === 90 || element.rotation === 270
  const width = swap ? base.height : base.width
  const height = swap ? base.width : base.height

  return {
    width,
    height,
    x1: element.x - width / 2,
    y1: element.y - height / 2,
  }
}

function resolveLayout(
  layoutById: Record<string, DiagramNodeLayout>,
  id: number,
  index: number,
): DiagramNodeLayout {
  return layoutById[String(id)] ?? defaultLayout(index)
}

function defaultLayout(index: number): DiagramNodeLayout {
  const columns = 4
  return snapNodeCenterToGrid({
    x: 120 + (index % columns) * 320,
    y: 120 + Math.floor(index / columns) * 220,
  })
}

function snapNodeCenterToGrid(layout: DiagramNodeLayout): DiagramNodeLayout {
  const centerX = gridSnapWorldValue(layout.x + SWITCHGEAR_SLD_STAGE_PADDING + SWITCHGEAR_SLD_NODE_WIDTH / 2)
  const centerY = gridSnapWorldValue(layout.y + SWITCHGEAR_SLD_STAGE_PADDING + SWITCHGEAR_SLD_NODE_HEIGHT / 2)

  return {
    x: centerX - SWITCHGEAR_SLD_STAGE_PADDING - SWITCHGEAR_SLD_NODE_WIDTH / 2,
    y: centerY - SWITCHGEAR_SLD_STAGE_PADDING - SWITCHGEAR_SLD_NODE_HEIGHT / 2,
  }
}

function gridSnapWorldValue(value: number): number {
  return Math.round(value / GRID_STEP) * GRID_STEP
}

function resolveViewportX(storedState: StoredDiagramState | null): number {
  const x = storedState?.viewState?.x
  const zoom = resolveViewportZoom(storedState)
  return Number.isFinite(x) ? -Number(x) / zoom : SWITCHGEAR_SLD_DEFAULT_VIEW.x
}

function resolveViewportY(storedState: StoredDiagramState | null): number {
  const y = storedState?.viewState?.y
  const zoom = resolveViewportZoom(storedState)
  return Number.isFinite(y) ? -Number(y) / zoom : SWITCHGEAR_SLD_DEFAULT_VIEW.y
}

function resolveViewportZoom(storedState: StoredDiagramState | null): number {
  const zoom = Number(storedState?.viewState?.zoom)
  return Number.isFinite(zoom) && zoom > 0 ? Math.max(0.05, Math.min(2.2, zoom)) : SWITCHGEAR_SLD_DEFAULT_VIEW.zoom
}

function normalizeEdges(storedState: StoredDiagramState | null) {
  const source = Array.isArray(storedState?.lines)
    ? storedState?.lines
    : Array.isArray(storedState?.edges)
      ? storedState?.edges
      : []

  return source.filter((edge): edge is NonNullable<StoredDiagramState["edges"]>[number] => (
    typeof edge?.id === "string"
    && Number.isFinite(edge?.x1)
    && Number.isFinite(edge?.y1)
    && Number.isFinite(edge?.x2)
    && Number.isFinite(edge?.y2)
  ))
}

function normalizeStaticElements(value: StoredDiagramState["staticElements"] | undefined): DiagramStaticElement[] {
  return Array.isArray(value)
    ? value.filter((element): element is DiagramStaticElement => (
      typeof element?.id === "string"
      && Number.isFinite(element?.x)
      && Number.isFinite(element?.y)
    ))
    : []
}

function normalizeTextElements(value: StoredDiagramState["textElements"] | undefined): DiagramTextElement[] {
  return Array.isArray(value)
    ? value.filter((element): element is DiagramTextElement => (
      typeof element?.id === "string"
      && Number.isFinite(element?.x)
      && Number.isFinite(element?.y)
    )).map(element => ({
      ...element,
      text: typeof element.text === "string" && element.text.trim()
        ? element.text.trim().slice(0, 80)
        : DEFAULT_TEXT_LABEL,
    }))
    : []
}

function toSwitchgearNodeId(id: number) {
  return `switchgear:${id}`
}

function toSwitchgearLabelId(id: number) {
  return `switchgear-label:${id}`
}

function toStaticShapeId(id: string) {
  return `static:${id}`
}

function toPortId(port: LegacyDiagramPort) {
  return `${port.ownerType}:${String(port.ownerId)}:${port.portId}`
}

function resolvePortNodeId(port: LegacyDiagramPort) {
  return port.ownerType === "node"
    ? toSwitchgearNodeId(Number(port.ownerId))
    : toStaticShapeId(String(port.ownerId))
}
