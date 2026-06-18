import type {
  DiagramEdge,
  DiagramPoint,
  DiagramPort,
  DiagramSceneInput,
  DiagramShape,
  DiagramText,
  SerializedDiagramScene,
} from "@affino/diagram-core"

import type { Switchgear } from "@/types/switchgear"

import type {
  DiagramBindablePortOwnerType,
  DiagramEdge as LegacyDiagramEdge,
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

type LegacyPortPoint = {
  ownerType: DiagramPortOwnerType
  ownerId: number | string
  portId: string
  x: number
  y: number
}

type SerializeOptions = {
  workspaceId?: number | null
  snapEnabled?: boolean
  labelOffsetById?: Record<string, { x: number; y: number }>
  baseState?: StoredDiagramState | null
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
  const ports = switchgears.flatMap((switchgear, index) => buildNodePortsForLayout(
    switchgear.id,
    resolveLayout(layoutById, switchgear.id, index),
  ))
  const staticPorts = staticElements.flatMap(buildStaticPorts)
  const edgePorts = [...ports, ...staticPorts]
  const scene: DiagramSceneInput = {
    nodes: switchgears.map((switchgear, index) => {
      const layout = resolveLayout(layoutById, switchgear.id, index)
      const labelOffset = labelOffsetById[String(switchgear.id)] ?? LABEL_DEFAULT_OFFSET
      return {
        id: toSwitchgearNodeId(switchgear.id),
        kind: "node" as const,
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
          labelOffsetX: labelOffset.x,
          labelOffsetY: labelOffset.y,
        },
      }
    }),
    ports: ports.map((port) => ({
      id: toPortId(port),
      kind: "port" as const,
      nodeId: toSwitchgearNodeId(Number(port.ownerId)),
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
    texts: textElements.map(createLooseText),
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

export function serializeSwitchgearSldPackageScene(
  scene: SerializedDiagramScene,
  options: SerializeOptions = {},
): StoredDiagramState {
  const portsById = new Map(scene.ports.map(port => [port.id, port]))
  const labelOffsetById = { ...(options.baseState?.labelOffsetById ?? {}), ...(options.labelOffsetById ?? {}) }

  for (const node of scene.nodes) {
    const switchgearId = Number(node.metadata?.switchgearId)
    if (!Number.isFinite(switchgearId)) {
      continue
    }
    labelOffsetById[String(switchgearId)] = {
      x: Number(node.metadata?.labelOffsetX ?? LABEL_DEFAULT_OFFSET.x),
      y: Number(node.metadata?.labelOffsetY ?? LABEL_DEFAULT_OFFSET.y),
    }
  }

  return {
    workspaceId: options.workspaceId ?? options.baseState?.workspaceId,
    layoutById: Object.fromEntries(scene.nodes.flatMap((node) => {
      const switchgearId = Number(node.metadata?.switchgearId)
      if (!Number.isFinite(switchgearId)) {
        return []
      }
      return [[String(switchgearId), {
        x: Math.round(node.x - SWITCHGEAR_SLD_STAGE_PADDING),
        y: Math.round(node.y - SWITCHGEAR_SLD_STAGE_PADDING),
      } satisfies DiagramNodeLayout]]
    })),
    labelOffsetById,
    edges: scene.edges.map((edge) => serializeEdge(edge, portsById)),
    lines: scene.edges.map((edge) => serializeEdge(edge, portsById)),
    staticElements: scene.shapes.flatMap((shape) => {
      const staticId = typeof shape.metadata?.staticId === "string" ? shape.metadata.staticId : null
      const staticKind = normalizeStaticKind(shape.metadata?.staticKind)
      const staticSize = normalizeStaticSize(shape.metadata?.staticSize)
      if (!staticId) {
        return []
      }
      const width = Number(shape.width)
      const height = Number(shape.height)
      return [{
        id: staticId,
        kind: staticKind,
        size: staticSize,
        x: Math.round(shape.x + width / 2),
        y: Math.round(shape.y + height / 2),
        rotation: normalizeRotation(shape.rotation),
      } satisfies DiagramStaticElement]
    }),
    textElements: scene.texts.flatMap((text) => {
      if (text.metadata?.entityType === "switchgear-label") {
        return []
      }
      return [{
        id: text.id,
        text: typeof text.text === "string" && text.text.trim() ? text.text.trim().slice(0, 80) : DEFAULT_TEXT_LABEL,
        size: "md",
        x: Math.round(text.x),
        y: Math.round(text.y),
      } satisfies DiagramTextElement]
    }),
    snapEnabled: options.snapEnabled ?? options.baseState?.snapEnabled ?? true,
    viewState: {
      x: Math.round(-(scene.viewport.x * scene.viewport.zoom)),
      y: Math.round(-(scene.viewport.y * scene.viewport.zoom)),
      zoom: scene.viewport.zoom,
    },
  }
}

export function normalizeStoredDiagramState(value: unknown): StoredDiagramState | null {
  return value && typeof value === "object" && !Array.isArray(value)
    ? value as StoredDiagramState
    : null
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
  edge: LegacyDiagramEdge,
  ports: ReadonlyArray<LegacyPortPoint>,
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
      startBinding: edge.startBinding ?? null,
      endBinding: edge.endBinding ?? null,
    },
  }
}

function serializeEdge(
  edge: DiagramEdge,
  portsById: ReadonlyMap<string, DiagramPort>,
): LegacyDiagramEdge {
  const start = resolveSerializedEndpoint(edge.source, portsById)
  const end = resolveSerializedEndpoint(edge.target, portsById)
  return {
    id: edge.id,
    x1: Math.round(start.point.x),
    y1: Math.round(start.point.y),
    x2: Math.round(end.point.x),
    y2: Math.round(end.point.y),
    kind: edge.metadata?.edgeKind === "arrow" ? "arrow" : "line",
    weight: edge.metadata?.edgeWeight === "bold" ? "bold" : "normal",
    startBinding: start.binding,
    endBinding: end.binding,
  }
}

function resolveSerializedEndpoint(
  endpoint: DiagramEdge["source"],
  portsById: ReadonlyMap<string, DiagramPort>,
): { point: DiagramPoint; binding: DiagramPortBinding | null } {
  if (endpoint.kind === "point") {
    return { point: endpoint.point, binding: null }
  }
  if (endpoint.kind === "port") {
    const port = portsById.get(endpoint.portId)
    const binding = port ? toBindingFromPort(port) : null
    return {
      point: port ? { x: port.x, y: port.y } : { x: 0, y: 0 },
      binding,
    }
  }
  return { point: { x: 0, y: 0 }, binding: null }
}

function toBindingFromPort(port: DiagramPort): DiagramPortBinding | null {
  if (port.metadata?.ownerType !== "node") {
    return null
  }
  const ownerId = Number(port.metadata?.ownerId)
  const portId = typeof port.metadata?.portId === "string" ? port.metadata.portId : ""
  if (!Number.isFinite(ownerId) || !portId) {
    return null
  }
  return {
    ownerType: "node",
    ownerId,
    portId,
  }
}

function resolveEdgeEndpoint(
  binding: DiagramPortBinding | null | undefined,
  point: DiagramPoint,
  ports: ReadonlyArray<LegacyPortPoint>,
) {
  const port = binding ? resolvePortBinding(binding, ports) : null
  return port && port.ownerType === "node"
    ? { kind: "port" as const, portId: toPortId(port) }
    : { kind: "point" as const, point: port ? { x: port.x, y: port.y } : point }
}

function resolvePortBinding(
  binding: DiagramPortBinding,
  ports: ReadonlyArray<LegacyPortPoint>,
): LegacyPortPoint | null {
  return ports.find((port) => (
    port.ownerType === binding.ownerType
    && String(port.ownerId) === String(binding.ownerId)
    && port.portId === binding.portId
  )) ?? null
}

function buildNodePortsForLayout(nodeId: number, layout: DiagramNodeLayout): LegacyPortPoint[] {
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

function buildStaticPorts(element: DiagramStaticElement): LegacyPortPoint[] {
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

  return source.filter((edge): edge is LegacyDiagramEdge => (
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

function normalizeStaticKind(value: unknown): DiagramStaticKind {
  return value === "ground" ? "ground" : "transformer"
}

function normalizeStaticSize(value: unknown): DiagramStaticSize {
  return value === "sm" || value === "lg" ? value : "md"
}

function normalizeRotation(value: unknown): 0 | 90 | 180 | 270 {
  return value === 90 || value === 180 || value === 270 ? value : 0
}

function toSwitchgearNodeId(id: number) {
  return `switchgear:${id}`
}

function toStaticShapeId(id: string) {
  return `static:${id}`
}

function toPortId(port: LegacyPortPoint) {
  return `${port.ownerType}:${String(port.ownerId)}:${port.portId}`
}
