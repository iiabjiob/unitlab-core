<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"
import UiButton from "@/components/ui/UiButton.vue"
import WorkspacePlaceholder from "@/components/ui/WorkspacePlaceholder.vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useChannelStore } from "@/stores/channelStore"
import { useDeviceStore } from "@/stores/deviceStore"
import { useSelectionStore } from "@/stores/selectionStore"
import { useToastStore } from "@/stores/toastStore"
import { useThemeStore } from "@/stores/themeStore"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"
import { localSettingsKeys, readLocalSetting, writeLocalSetting } from "@/services/localSettingsStorage"
import SwitchgearSingleLineDiagramNode from "./SwitchgearSingleLineDiagramNode.vue"
import SwitchgearSingleLineDiagramStaticElement from "./SwitchgearSingleLineDiagramStaticElement.vue"
import SwitchgearControlToolbar from "./SwitchgearControlToolbar.vue"

type DiagramNodeLayout = {
  x: number
  y: number
}

type DiagramEdge = {
  id: string
  x1: number
  y1: number
  x2: number
  y2: number
  kind: "line" | "arrow"
}

type DiagramStaticKind = "transformer" | "ground"

type DiagramStaticElement = {
  id: string
  kind: DiagramStaticKind
  x: number
  y: number
  rotation: 0 | 90 | 180 | 270
}

type DiagramViewState = {
  x: number
  y: number
  zoom: number
}

type DiagramLabelOffset = {
  x: number
  y: number
}

type DiagramHistorySnapshot = {
  layoutById: Record<string, DiagramNodeLayout>
  labelOffsetById: Record<string, DiagramLabelOffset>
  edges: DiagramEdge[]
  staticElements: DiagramStaticElement[]
  selectedNodeIds: number[]
  selectedEdgeIds: string[]
  selectedStaticIds: string[]
  selectedEdgeId: string | null
}

type DiagramClipboardEdge = {
  x1: number
  y1: number
  x2: number
  y2: number
  kind: "line" | "arrow"
}

type StoredDiagramState = {
  layoutById?: Record<string, DiagramNodeLayout>
  labelOffsetById?: Record<string, DiagramLabelOffset>
  edges?: DiagramEdge[]
  lines?: DiagramEdge[]
  staticElements?: DiagramStaticElement[]
  snapEnabled?: boolean
  viewState?: DiagramViewState
}

type DiagramClipboardPayload = {
  kind: "unitlab.switchgear-sld-selection"
  version: 1
  edges: DiagramClipboardEdge[]
}

type DiagramPort = {
  ownerType: "node" | "line"
  ownerId: number | string
  x: number
  y: number
}

type DragState =
  | {
      type: "pan"
      startX: number
      startY: number
      originX: number
      originY: number
    }
  | {
      type: "node"
      id: number
      startX: number
      startY: number
      originX: number
      originY: number
    }
  | {
      type: "label"
      id: number
      startX: number
      startY: number
      originX: number
      originY: number
    }
  | {
      type: "static"
      id: string
      startX: number
      startY: number
      originX: number
      originY: number
    }
  | {
      type: "line"
      edgeId: string
      mode: "move" | "start" | "end"
      startX: number
      startY: number
      origin: DiagramEdge
    }
  | {
      type: "new-line"
      startX: number
      startY: number
      currentX: number
      currentY: number
    }
  | {
      type: "marquee"
      startX: number
      startY: number
      currentX: number
      currentY: number
      additive: boolean
    }
  | {
      type: "group"
      startX: number
      startY: number
      originNodes: Array<{ id: number; x: number; y: number }>
      originEdges: Array<DiagramEdge>
      originStatics: Array<DiagramStaticElement>
    }

type InteractionTool = "hand" | "arrow" | "line"

const NODE_WIDTH = 40
const NODE_HEIGHT = 40
const LABEL_MIN_OFFSET = -220
const LABEL_MAX_OFFSET = 220
const LABEL_DEFAULT_OFFSET: DiagramLabelOffset = { x: 0, y: 22 }
const STAGE_PADDING = 50000
const DEFAULT_VIEW: DiagramViewState = { x: 96 - STAGE_PADDING, y: 72 - STAGE_PADDING, zoom: 1 }
const MIN_ZOOM = 0.45
const MAX_ZOOM = 2.2
const MINIMAP_WIDTH = 220
const MINIMAP_HEIGHT = 150
const GRID_STEP = 24
const NUDGE_FINE_STEP = 1
const NUDGE_LARGE_STEP = GRID_STEP * 4
const PORT_SNAP_DISTANCE = 18
const HISTORY_LIMIT = 80
const DIAGRAM_CLIPBOARD_KIND = "unitlab.switchgear-sld-selection"
const COPY_PASTE_OFFSET = GRID_STEP * 2
const STATIC_ROTATIONS = [0, 90, 180, 270] as const

const router = useRouter()
const route = useRoute()
const switchgearStore = useSwitchgearStore()
const workspaceStore = useWorkspaceStore()
const channelStore = useChannelStore()
const deviceStore = useDeviceStore()
const selectionStore = useSelectionStore()
const toastStore = useToastStore()
const themeStore = useThemeStore()

const viewportRef = ref<HTMLElement | null>(null)
const layoutById = ref<Record<string, DiagramNodeLayout>>({})
const labelOffsetById = ref<Record<string, DiagramLabelOffset>>({})
const edges = ref<DiagramEdge[]>([])
const staticElements = ref<DiagramStaticElement[]>([])
const viewState = ref<DiagramViewState>({ ...DEFAULT_VIEW })
const interactionTool = ref<InteractionTool>("hand")
const snapEnabled = ref(true)
const selectedEdgeId = ref<string | null>(null)
const selectedNodeIds = ref<number[]>([])
const selectedEdgeIds = ref<string[]>([])
const selectedStaticIds = ref<string[]>([])
const lineContextMenu = ref<{ x: number; y: number; edgeIds: string[] } | null>(null)
const staticContextMenu = ref<{ x: number; y: number; staticIds: string[] } | null>(null)
const alignMenuOpen = ref(false)
const hydrating = ref(false)
const dragState = ref<DragState | null>(null)
const viewportSize = ref({ width: 0, height: 0 })
const undoStack = ref<DiagramHistorySnapshot[]>([])
const redoStack = ref<DiagramHistorySnapshot[]>([])
const historyDragSnapshot = ref<DiagramHistorySnapshot | null>(null)
const localClipboardEdges = ref<DiagramClipboardEdge[]>([])
const clipboardPasteCount = ref(0)
let viewportResizeObserver: ResizeObserver | null = null

const workspaceId = computed(() => workspaceStore.activeWorkspaceId)
const switchgears = computed(() => switchgearStore.switchgears)
const selectedNodeId = computed<number | null>(() => {
  const parsed = Number(route.params.id)
  return Number.isFinite(parsed) ? parsed : null
})
const storageKey = computed(() => (
  workspaceId.value ? localSettingsKeys.switchgearDiagram(workspaceId.value) : null
))
const legacyStorageKey = computed(() => (
  workspaceId.value ? `unitlab.switchgears.sld.${workspaceId.value}` : null
))
const switchgearIdsSignature = computed(() => switchgears.value.map(item => item.id).join(","))
const stageSize = computed(() => {
  return {
    width: STAGE_PADDING * 2,
    height: STAGE_PADDING * 2,
  }
})
const stageTransformStyle = computed(() => ({
  transform: `translate(${viewState.value.x}px, ${viewState.value.y}px) scale(${viewState.value.zoom})`,
  transformOrigin: "0 0",
}))
const stageContentStyle = computed(() => ({
  width: `${stageSize.value.width}px`,
  height: `${stageSize.value.height}px`,
}))
const isDarkTheme = computed(() => themeStore.currentTheme === "dark")
const viewportSurfaceStyle = computed(() => ({
  backgroundColor: isDarkTheme.value ? "rgb(3 7 18)" : "rgb(245 245 245)",
}))
const viewportOverlayStyle = computed(() => ({
  backgroundImage: isDarkTheme.value
    ? "radial-gradient(circle_at_top, rgba(56,189,248,0.12), transparent 40%), linear-gradient(to_bottom, rgba(255,255,255,0.04), transparent 35%)"
    : "radial-gradient(circle_at_top, rgba(56,189,248,0.16), transparent 45%), linear-gradient(to_bottom, rgba(255,255,255,0.5), transparent 38%)",
}))
const stageGridStyle = computed(() => ({
  ...stageContentStyle.value,
  backgroundColor: isDarkTheme.value ? "rgba(2, 6, 23, 0.94)" : "rgb(249 250 251)",
  backgroundImage: isDarkTheme.value
    ? "linear-gradient(rgba(56, 189, 248, 0.08) 1px, transparent 1px), linear-gradient(90deg, rgba(56, 189, 248, 0.08) 1px, transparent 1px), linear-gradient(rgba(148, 163, 184, 0.06) 1px, transparent 1px), linear-gradient(90deg, rgba(148, 163, 184, 0.06) 1px, transparent 1px)"
    : "linear-gradient(rgba(56, 189, 248, 0.1) 1px, transparent 1px), linear-gradient(90deg, rgba(56, 189, 248, 0.1) 1px, transparent 1px), linear-gradient(rgba(71, 85, 105, 0.08) 1px, transparent 1px), linear-gradient(90deg, rgba(71, 85, 105, 0.08) 1px, transparent 1px)",
  backgroundPosition: "0 0, 0 0, 0 0, 0 0",
  backgroundSize: "120px 120px, 120px 120px, 24px 24px, 24px 24px",
}))
const zoomLabel = computed(() => `${Math.round(viewState.value.zoom * 100)}%`)
const selectedNodeIdSet = computed(() => new Set(effectiveSelectedNodeIds()))
const selectedEdgeIdSet = computed(() => new Set(selectedEdgeIds.value))
const selectedStaticIdSet = computed(() => new Set(selectedStaticIds.value))
const selectedLineCount = computed(() => {
  const ids = new Set<string>()
  if (selectedEdgeId.value) {
    ids.add(selectedEdgeId.value)
  }
  selectedEdgeIds.value.forEach(id => ids.add(id))
  return ids.size
})
const selectedStaticCount = computed(() => selectedStaticIds.value.length)
const selectedNodeCount = computed(() => effectiveSelectedNodeIds().length)
const selectedObjectCount = computed(() => (
  selectedNodeCount.value + selectedLineCount.value + selectedStaticCount.value
))
const activeToolLabel = computed(() => {
  if (interactionTool.value === "line") {
    return "Draw line"
  }
  return interactionTool.value === "hand" ? "Move" : "Select"
})
const snapStateLabel = computed(() => snapEnabled.value ? "Snap on" : "Snap off")
const selectionSummary = computed(() => {
  if (selectedObjectCount.value === 0) {
    return "No selection"
  }

  const parts = [
    selectedNodeCount.value > 0 ? `${selectedNodeCount.value} switchgear${selectedNodeCount.value > 1 ? "s" : ""}` : null,
    selectedLineCount.value > 0 ? `${selectedLineCount.value} line${selectedLineCount.value > 1 ? "s" : ""}` : null,
    selectedStaticCount.value > 0 ? `${selectedStaticCount.value} symbol${selectedStaticCount.value > 1 ? "s" : ""}` : null,
  ].filter((part): part is string => part !== null)

  return parts.join(" · ")
})
const diagramMetricItems = computed(() => [
  { label: "Switchgears", value: switchgears.value.length },
  { label: "Lines", value: edges.value.length },
  { label: "Symbols", value: staticElements.value.length },
])
const draftLinePreview = computed(() => {
  if (!dragState.value || dragState.value.type !== "new-line") {
    return null
  }

  return {
    x1: dragState.value.startX,
    y1: dragState.value.startY,
    x2: dragState.value.currentX,
    y2: dragState.value.currentY,
  }
})
const canUndo = computed(() => undoStack.value.length > 0)
const canRedo = computed(() => redoStack.value.length > 0)
const selectedLineKind = computed<"line" | "arrow" | "mixed" | null>(() => {
  const edgeIds = effectiveSelectedEdgeIds()
  if (edgeIds.length === 0) {
    return null
  }

  const kinds = new Set(
    edgeIds
      .map(edgeId => getEdgeById(edgeId)?.kind ?? "line"),
  )

  if (kinds.size === 1) {
    return kinds.has("arrow") ? "arrow" : "line"
  }

  return "mixed"
})
const staticElementCount = computed(() => staticElements.value.length)
const singleSelectedSwitchgear = computed(() => {
  const selectedId = selectedNodeIds.value.length === 1
    ? selectedNodeIds.value[0]
    : (selectedNodeIds.value.length === 0 && selectedEdgeIds.value.length === 0 && selectedStaticIds.value.length === 0 ? selectedNodeId.value : null)

  if (selectedId === null) {
    return null
  }

  return switchgears.value.find(item => item.id === selectedId) ?? null
})
const marqueeRect = computed(() => {
  if (!dragState.value || dragState.value.type !== "marquee") {
    return null
  }
  const x = Math.min(dragState.value.startX, dragState.value.currentX)
  const y = Math.min(dragState.value.startY, dragState.value.currentY)
  const width = Math.abs(dragState.value.currentX - dragState.value.startX)
  const height = Math.abs(dragState.value.currentY - dragState.value.startY)
  return { x, y, width, height }
})
const nodeWorldRects = computed(() => switchgears.value.map((item, index) => {
  const layout = resolvedLayout(item.id, index)
  return {
    id: item.id,
    x: layout.x + STAGE_PADDING,
    y: layout.y + STAGE_PADDING,
    width: NODE_WIDTH,
    height: NODE_HEIGHT,
  }
}))
const minimapModel = computed(() => {
  if (nodeWorldRects.value.length === 0 || viewportSize.value.width <= 0 || viewportSize.value.height <= 0) {
    return null
  }

  const nodeMinX = Math.min(...nodeWorldRects.value.map(item => item.x))
  const nodeMinY = Math.min(...nodeWorldRects.value.map(item => item.y))
  const nodeMaxX = Math.max(...nodeWorldRects.value.map(item => item.x + item.width))
  const nodeMaxY = Math.max(...nodeWorldRects.value.map(item => item.y + item.height))

  const worldViewportX = -viewState.value.x / viewState.value.zoom
  const worldViewportY = -viewState.value.y / viewState.value.zoom
  const worldViewportWidth = viewportSize.value.width / viewState.value.zoom
  const worldViewportHeight = viewportSize.value.height / viewState.value.zoom

  const contentMinX = Math.min(nodeMinX, worldViewportX) - 160
  const contentMinY = Math.min(nodeMinY, worldViewportY) - 160
  const contentMaxX = Math.max(nodeMaxX, worldViewportX + worldViewportWidth) + 160
  const contentMaxY = Math.max(nodeMaxY, worldViewportY + worldViewportHeight) + 160

  const contentWidth = Math.max(1, contentMaxX - contentMinX)
  const contentHeight = Math.max(1, contentMaxY - contentMinY)
  const scale = Math.min(MINIMAP_WIDTH / contentWidth, MINIMAP_HEIGHT / contentHeight)
  const drawWidth = contentWidth * scale
  const drawHeight = contentHeight * scale
  const offsetX = (MINIMAP_WIDTH - drawWidth) / 2
  const offsetY = (MINIMAP_HEIGHT - drawHeight) / 2

  return {
    contentMinX,
    contentMinY,
    contentWidth,
    contentHeight,
    drawWidth,
    drawHeight,
    offsetX,
    offsetY,
    scale,
    nodes: nodeWorldRects.value.map(item => ({
      id: item.id,
      x: offsetX + (item.x - contentMinX) * scale,
      y: offsetY + (item.y - contentMinY) * scale,
      width: Math.max(3, item.width * scale),
      height: Math.max(3, item.height * scale),
      active: selectedNodeIdSet.value.has(item.id),
    })),
    viewport: {
      x: offsetX + (worldViewportX - contentMinX) * scale,
      y: offsetY + (worldViewportY - contentMinY) * scale,
      width: worldViewportWidth * scale,
      height: worldViewportHeight * scale,
    },
  }
})

function clampZoom(value: number): number {
  return Math.min(MAX_ZOOM, Math.max(MIN_ZOOM, value))
}

function cloneLayoutById(source: Record<string, DiagramNodeLayout>) {
  return Object.fromEntries(
    Object.entries(source).map(([key, value]) => [key, { x: value.x, y: value.y }]),
  )
}

function cloneLabelOffsetById(source: Record<string, DiagramLabelOffset>) {
  return Object.fromEntries(
    Object.entries(source).map(([key, value]) => [key, { x: value.x, y: value.y }]),
  )
}

function cloneEdges(source: DiagramEdge[]) {
  return source.map(edge => ({ ...edge }))
}

function cloneStaticElements(source: DiagramStaticElement[]) {
  return source.map(element => ({ ...element }))
}

function normalizeEdgeKind(value: unknown): "line" | "arrow" {
  return value === "arrow" ? "arrow" : "line"
}

function normalizeStaticKind(value: unknown): DiagramStaticKind {
  return value === "ground" ? "ground" : "transformer"
}

function normalizeRotation(value: unknown): 0 | 90 | 180 | 270 {
  const numeric = Number(value)
  if (STATIC_ROTATIONS.includes(numeric as 0 | 90 | 180 | 270)) {
    return numeric as 0 | 90 | 180 | 270
  }
  return 0
}

function getStaticElementById(id: string): DiagramStaticElement | null {
  return staticElements.value.find(element => element.id === id) ?? null
}

function getStaticElementBaseSize(kind: DiagramStaticKind) {
  if (kind === "transformer") {
    return { width: 80, height: 80 }
  }

  return { width: 40, height: 40 }
}

function getStaticElementBounds(element: DiagramStaticElement) {
  const base = getStaticElementBaseSize(element.kind)
  const swap = element.rotation === 90 || element.rotation === 270
  const width = swap ? base.height : base.width
  const height = swap ? base.width : base.height

  return {
    width,
    height,
    x1: element.x - width / 2,
    y1: element.y - height / 2,
    x2: element.x + width / 2,
    y2: element.y + height / 2,
  }
}

function nextQuarterRotation(rotation: 0 | 90 | 180 | 270): 0 | 90 | 180 | 270 {
  return (((rotation + 90) % 360) || 0) as 0 | 90 | 180 | 270
}

function buildDiagramClipboardPayload(sourceEdges: DiagramClipboardEdge[]) {
  return JSON.stringify({
    kind: DIAGRAM_CLIPBOARD_KIND,
    version: 1,
    edges: sourceEdges,
  } as DiagramClipboardPayload, null, 2)
}

function parseDiagramClipboardPayload(rawText: string): DiagramClipboardEdge[] | null {
  try {
    const parsed = JSON.parse(rawText) as Partial<DiagramClipboardPayload>
    if (parsed.kind !== DIAGRAM_CLIPBOARD_KIND || parsed.version !== 1 || !Array.isArray(parsed.edges)) {
      return null
    }

    const nextEdges = parsed.edges.filter(edge => (
      Number.isFinite(edge?.x1)
      && Number.isFinite(edge?.y1)
      && Number.isFinite(edge?.x2)
      && Number.isFinite(edge?.y2)
    )).map(edge => ({
      x1: edge.x1,
      y1: edge.y1,
      x2: edge.x2,
      y2: edge.y2,
      kind: normalizeEdgeKind(edge.kind),
    }))

    return nextEdges.length > 0 ? nextEdges : null
  } catch {
    return null
  }
}

function snapshotDiagramState(): DiagramHistorySnapshot {
  return {
    layoutById: cloneLayoutById(layoutById.value),
    labelOffsetById: cloneLabelOffsetById(labelOffsetById.value),
    edges: cloneEdges(edges.value),
    staticElements: cloneStaticElements(staticElements.value),
    selectedNodeIds: [...selectedNodeIds.value],
    selectedEdgeIds: [...selectedEdgeIds.value],
    selectedStaticIds: [...selectedStaticIds.value],
    selectedEdgeId: selectedEdgeId.value,
  }
}

function applyDiagramSnapshot(snapshot: DiagramHistorySnapshot) {
  layoutById.value = cloneLayoutById(snapshot.layoutById)
  labelOffsetById.value = cloneLabelOffsetById(snapshot.labelOffsetById)
  edges.value = cloneEdges(snapshot.edges)
  staticElements.value = cloneStaticElements(snapshot.staticElements)
  selectedNodeIds.value = [...snapshot.selectedNodeIds]
  selectedEdgeIds.value = [...snapshot.selectedEdgeIds]
  selectedStaticIds.value = [...snapshot.selectedStaticIds]
  selectedEdgeId.value = snapshot.selectedEdgeId
  closeLineContextMenu()
}

function areSnapshotsEqual(left: DiagramHistorySnapshot, right: DiagramHistorySnapshot): boolean {
  return JSON.stringify(left) === JSON.stringify(right)
}

function pushUndoSnapshot(snapshot: DiagramHistorySnapshot) {
  undoStack.value = [...undoStack.value.slice(-(HISTORY_LIMIT - 1)), snapshot]
}

function recordHistoryChange(before: DiagramHistorySnapshot, after: DiagramHistorySnapshot) {
  if (areSnapshotsEqual(before, after)) {
    return
  }
  pushUndoSnapshot(before)
  redoStack.value = []
}

function commitHistoryMutation(mutator: () => void): boolean {
  const before = snapshotDiagramState()
  mutator()
  const after = snapshotDiagramState()
  recordHistoryChange(before, after)
  return !areSnapshotsEqual(before, after)
}

function beginDragHistorySession() {
  if (!historyDragSnapshot.value) {
    historyDragSnapshot.value = snapshotDiagramState()
  }
}

function finishDragHistorySession() {
  if (!historyDragSnapshot.value) {
    return
  }
  const before = historyDragSnapshot.value
  historyDragSnapshot.value = null
  recordHistoryChange(before, snapshotDiagramState())
}

function clearHistory() {
  undoStack.value = []
  redoStack.value = []
  historyDragSnapshot.value = null
}

function undo() {
  const previous = undoStack.value.length > 0
    ? undoStack.value[undoStack.value.length - 1]
    : null
  if (!previous) {
    return
  }
  const current = snapshotDiagramState()
  undoStack.value = undoStack.value.slice(0, -1)
  redoStack.value = [...redoStack.value.slice(-(HISTORY_LIMIT - 1)), current]
  applyDiagramSnapshot(previous)
}

function redo() {
  const next = redoStack.value.length > 0
    ? redoStack.value[redoStack.value.length - 1]
    : null
  if (!next) {
    return
  }
  const current = snapshotDiagramState()
  redoStack.value = redoStack.value.slice(0, -1)
  pushUndoSnapshot(current)
  applyDiagramSnapshot(next)
}

function updateViewportMetrics() {
  const viewport = viewportRef.value
  if (!viewport) {
    viewportSize.value = { width: 0, height: 0 }
    return
  }
  viewportSize.value = {
    width: viewport.clientWidth,
    height: viewport.clientHeight,
  }
}

function defaultLayout(index: number): DiagramNodeLayout {
  const columns = 4
  return {
    x: 120 + (index % columns) * 320,
    y: 120 + Math.floor(index / columns) * 220,
  }
}

function resolvedLayout(id: number, index: number): DiagramNodeLayout {
  return layoutById.value[String(id)] ?? defaultLayout(index)
}

function setNodeLayout(id: number, patch: DiagramNodeLayout) {
  layoutById.value = {
    ...layoutById.value,
    [String(id)]: patch,
  }
}

function snapWorldValue(value: number): number {
  if (!snapEnabled.value) {
    return value
  }
  return Math.round(value / GRID_STEP) * GRID_STEP
}

function snapWorldPoint(point: { x: number; y: number }) {
  return {
    x: snapWorldValue(point.x),
    y: snapWorldValue(point.y),
  }
}

function snapNodeLayoutToGrid(layout: DiagramNodeLayout): DiagramNodeLayout {
  const snappedWorld = snapWorldPoint({
    x: layout.x + STAGE_PADDING,
    y: layout.y + STAGE_PADDING,
  })
  return {
    x: snappedWorld.x - STAGE_PADDING,
    y: snappedWorld.y - STAGE_PADDING,
  }
}

function buildNodePortsForLayout(nodeId: number, layout: DiagramNodeLayout): DiagramPort[] {
  const worldX = layout.x + STAGE_PADDING
  const worldY = layout.y + STAGE_PADDING
  const halfWidth = NODE_WIDTH / 2
  const halfHeight = NODE_HEIGHT / 2

  return [
    { ownerType: "node", ownerId: nodeId, x: worldX + halfWidth, y: worldY },
    { ownerType: "node", ownerId: nodeId, x: worldX + NODE_WIDTH, y: worldY + halfHeight },
    { ownerType: "node", ownerId: nodeId, x: worldX + halfWidth, y: worldY + NODE_HEIGHT },
    { ownerType: "node", ownerId: nodeId, x: worldX, y: worldY + halfHeight },
  ]
}

function buildLinePorts(edge: DiagramEdge): DiagramPort[] {
  return [
    { ownerType: "line", ownerId: edge.id, x: edge.x1, y: edge.y1 },
    { ownerType: "line", ownerId: edge.id, x: edge.x2, y: edge.y2 },
  ]
}

function findNearestPort(point: { x: number; y: number }, ports: DiagramPort[]): DiagramPort | null {
  let nearest: { port: DiagramPort; distance: number } | null = null

  for (const port of ports) {
    const distance = Math.hypot(port.x - point.x, port.y - point.y)
    if (!nearest || distance < nearest.distance) {
      nearest = { port, distance }
    }
  }

  return nearest?.port ?? null
}

function collectStationaryPorts(
  excludedNodeIds: Set<number> = new Set<number>(),
  excludedEdgeIds: Set<string> = new Set<string>(),
): DiagramPort[] {
  const ports: DiagramPort[] = []

  switchgears.value.forEach((item, index) => {
    if (excludedNodeIds.has(item.id)) {
      return
    }
    ports.push(...buildNodePortsForLayout(item.id, resolvedLayout(item.id, index)))
  })

  edges.value.forEach((edge) => {
    if (excludedEdgeIds.has(edge.id)) {
      return
    }
    ports.push(...buildLinePorts(edge))
  })

  return ports
}

function findBestPortCorrection(movingPorts: DiagramPort[], stationaryPorts: DiagramPort[]) {
  let best: { dx: number; dy: number; distance: number } | null = null

  for (const movingPort of movingPorts) {
    for (const stationaryPort of stationaryPorts) {
      const dx = stationaryPort.x - movingPort.x
      const dy = stationaryPort.y - movingPort.y
      const distance = Math.hypot(dx, dy)
      if (distance > PORT_SNAP_DISTANCE) {
        continue
      }
      if (!best || distance < best.distance) {
        best = { dx, dy, distance }
      }
    }
  }

  if (best === null) {
    return null
  }

  return { dx: best.dx, dy: best.dy }
}

function applyPortSnapToNodeLayout(
  nodeId: number,
  layout: DiagramNodeLayout,
  excludedNodeIds: Set<number> = new Set<number>([nodeId]),
  excludedEdgeIds: Set<string> = new Set<string>(),
): DiagramNodeLayout {
  if (!snapEnabled.value) {
    return layout
  }

  const correction = findBestPortCorrection(
    buildNodePortsForLayout(nodeId, layout),
    collectStationaryPorts(excludedNodeIds, excludedEdgeIds),
  )

  if (!correction) {
    return layout
  }

  return {
    x: layout.x + correction.dx,
    y: layout.y + correction.dy,
  }
}

function applyPortSnapToLinePoint(point: { x: number; y: number }, excludedEdgeIds: Set<string> = new Set<string>()) {
  if (!snapEnabled.value) {
    return point
  }

  const correction = findBestPortCorrection(
    [{ ownerType: "line", ownerId: "moving", x: point.x, y: point.y }],
    collectStationaryPorts(new Set<number>(), excludedEdgeIds),
  )

  if (!correction) {
    return point
  }

  return {
    x: point.x + correction.dx,
    y: point.y + correction.dy,
  }
}

function computeSnappedTranslation(
  anchor: { x: number; y: number },
  deltaX: number,
  deltaY: number,
  movingPorts: DiagramPort[],
  excludedNodeIds: Set<number> = new Set<number>(),
  excludedEdgeIds: Set<string> = new Set<string>(),
) {
  if (!snapEnabled.value) {
    return { dx: deltaX, dy: deltaY }
  }

  let snappedDeltaX = snapWorldValue(anchor.x + deltaX) - anchor.x
  let snappedDeltaY = snapWorldValue(anchor.y + deltaY) - anchor.y

  const correctedPorts = movingPorts.map(port => ({
    ...port,
    x: port.x + snappedDeltaX,
    y: port.y + snappedDeltaY,
  }))

  const correction = findBestPortCorrection(
    correctedPorts,
    collectStationaryPorts(excludedNodeIds, excludedEdgeIds),
  )

  if (correction) {
    snappedDeltaX += correction.dx
    snappedDeltaY += correction.dy
  }

  return { dx: snappedDeltaX, dy: snappedDeltaY }
}

function resolvedLabelOffset(id: number): DiagramLabelOffset {
  return labelOffsetById.value[String(id)] ?? LABEL_DEFAULT_OFFSET
}

function clampLabelOffset(value: number): number {
  return Math.max(LABEL_MIN_OFFSET, Math.min(LABEL_MAX_OFFSET, Math.round(value)))
}

function setLabelOffset(id: number, patch: DiagramLabelOffset) {
  labelOffsetById.value = {
    ...labelOffsetById.value,
    [String(id)]: {
      x: clampLabelOffset(patch.x),
      y: clampLabelOffset(patch.y),
    },
  }
}

function ensureLayoutDefaults(): boolean {
  const nextLayout: Record<string, DiagramNodeLayout> = {}
  let layoutChanged = false

  switchgears.value.forEach((item, index) => {
    const existing = layoutById.value[String(item.id)]
    if (existing) {
      nextLayout[String(item.id)] = existing
      return
    }
    nextLayout[String(item.id)] = defaultLayout(index)
    layoutChanged = true
  })

  if (Object.keys(nextLayout).length !== Object.keys(layoutById.value).length) {
    layoutChanged = true
  }

  if (selectedEdgeId.value && !edges.value.some(edge => edge.id === selectedEdgeId.value)) {
    selectedEdgeId.value = null
  }

  if (layoutChanged) {
    layoutById.value = nextLayout
  }
  return layoutChanged
}

function restoreDiagramState() {
  hydrating.value = true
  clearHistory()
  snapEnabled.value = true
  selectedEdgeId.value = null
  selectedNodeIds.value = []
  selectedEdgeIds.value = []
  selectedStaticIds.value = []
  layoutById.value = {}
  labelOffsetById.value = {}
  edges.value = []
  staticElements.value = []
  viewState.value = { ...DEFAULT_VIEW }

  let hadSavedState = false

  if (storageKey.value) {
    const parsed = readLocalSetting<StoredDiagramState | null>(storageKey.value, null, {
      legacyKeys: legacyStorageKey.value ? [legacyStorageKey.value] : [],
      validate: normalizeStoredDiagramState,
    })
    if (parsed) {
      if (parsed.layoutById && typeof parsed.layoutById === "object") {
        layoutById.value = parsed.layoutById
      }
      if (parsed.labelOffsetById && typeof parsed.labelOffsetById === "object") {
        labelOffsetById.value = parsed.labelOffsetById
      }
      const savedLines = Array.isArray(parsed.lines) ? parsed.lines : parsed.edges
      if (Array.isArray(savedLines)) {
        edges.value = savedLines.filter(edge =>
          Number.isFinite(edge?.x1) &&
          Number.isFinite(edge?.y1) &&
          Number.isFinite(edge?.x2) &&
          Number.isFinite(edge?.y2) &&
          typeof edge?.id === "string",
        ).map(edge => ({
          id: edge.id,
          x1: edge.x1,
          y1: edge.y1,
          x2: edge.x2,
          y2: edge.y2,
          kind: normalizeEdgeKind(edge.kind),
        }))
      }
      if (Array.isArray(parsed.staticElements)) {
        staticElements.value = parsed.staticElements.filter(element =>
          Number.isFinite(element?.x)
          && Number.isFinite(element?.y)
          && typeof element?.id === "string",
        ).map(element => ({
          id: element.id,
          kind: normalizeStaticKind(element.kind),
          x: element.x,
          y: element.y,
          rotation: normalizeRotation(element.rotation),
        }))
      }
      snapEnabled.value = parsed.snapEnabled !== false
      if (parsed.viewState) {
        const parsedZoom = Number(parsed.viewState.zoom)
        viewState.value = {
          x: Number.isFinite(parsed.viewState.x) ? parsed.viewState.x : DEFAULT_VIEW.x,
          y: Number.isFinite(parsed.viewState.y) ? parsed.viewState.y : DEFAULT_VIEW.y,
          zoom: Number.isFinite(parsedZoom) ? clampZoom(parsedZoom) : DEFAULT_VIEW.zoom,
        }
      }
      hadSavedState = true
    }
  }

  ensureLayoutDefaults()
  hydrating.value = false

  if (!hadSavedState && switchgears.value.length > 0) {
    void nextTick(() => {
      fitToContent()
    })
  }
}

function persistDiagramState() {
  if (hydrating.value || !storageKey.value) {
    return
  }
  writeLocalSetting(storageKey.value, {
    layoutById: layoutById.value,
    labelOffsetById: labelOffsetById.value,
    edges: edges.value,
    lines: edges.value,
    staticElements: staticElements.value,
    snapEnabled: snapEnabled.value,
    viewState: viewState.value,
  }, {
    legacyKeys: legacyStorageKey.value ? [legacyStorageKey.value] : [],
  })
}

function normalizeStoredDiagramState(value: unknown): StoredDiagramState | null {
  return value && typeof value === "object" && !Array.isArray(value)
    ? value as StoredDiagramState
    : null
}

function toggleSnapEnabled() {
  snapEnabled.value = !snapEnabled.value
}

function buildEdgeId(): string {
  return `line-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`
}

function buildStaticElementId(): string {
  return `static-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`
}

function addEdge(start: { x: number; y: number }, end: { x: number; y: number }) {
  const snappedStart = snapWorldPoint(start)
  const snappedEnd = snapWorldPoint(end)
  if (Math.hypot(snappedEnd.x - snappedStart.x, snappedEnd.y - snappedStart.y) < 8) {
    toastStore.warning("Line is too short")
    return
  }
  const nextEdge: DiagramEdge = {
    id: buildEdgeId(),
    x1: snappedStart.x,
    y1: snappedStart.y,
    x2: snappedEnd.x,
    y2: snappedEnd.y,
    kind: "line",
  }
  commitHistoryMutation(() => {
    edges.value = [...edges.value, nextEdge]
    selectedEdgeId.value = nextEdge.id
    selectedNodeIds.value = []
    selectedEdgeIds.value = [nextEdge.id]
    closeLineContextMenu()
  })
}

function addStaticElementInViewport(kind: DiagramStaticKind) {
  if (viewportSize.value.width <= 0 || viewportSize.value.height <= 0) {
    return
  }

  const worldCenterX = (-viewState.value.x + viewportSize.value.width / 2) / viewState.value.zoom
  const worldCenterY = (-viewState.value.y + viewportSize.value.height / 2) / viewState.value.zoom
  const point = snapWorldPoint({ x: worldCenterX, y: worldCenterY })
  const nextElement: DiagramStaticElement = {
    id: buildStaticElementId(),
    kind,
    x: point.x,
    y: point.y,
    rotation: 0,
  }

  commitHistoryMutation(() => {
    staticElements.value = [...staticElements.value, nextElement]
    selectedNodeIds.value = []
    selectedEdgeIds.value = []
    selectedEdgeId.value = null
    selectedStaticIds.value = [nextElement.id]
    closeLineContextMenu()
  })
}

async function handleCopySelection() {
  const edgeIds = effectiveSelectedEdgeIds()
  const sourceEdges = edgeIds
    .map(edgeId => getEdgeById(edgeId))
    .filter((edge): edge is DiagramEdge => edge !== null)
    .map(edge => ({ x1: edge.x1, y1: edge.y1, x2: edge.x2, y2: edge.y2, kind: edge.kind }))

  if (!sourceEdges.length) {
    if (effectiveSelectedNodeIds().length > 0) {
      toastStore.info("Switchgear copy is not supported yet. Select at least one line to copy.")
      return
    }
    toastStore.info("Select at least one line to copy")
    return
  }

  localClipboardEdges.value = sourceEdges
  clipboardPasteCount.value = 0

  try {
    if (typeof navigator !== "undefined" && navigator.clipboard?.writeText) {
      await navigator.clipboard.writeText(buildDiagramClipboardPayload(sourceEdges))
    }
    toastStore.success(`Copied ${sourceEdges.length} line${sourceEdges.length > 1 ? "s" : ""}`)
  } catch {
    toastStore.success(`Copied ${sourceEdges.length} line${sourceEdges.length > 1 ? "s" : ""}`)
  }
}

async function resolveClipboardEdges() {
  try {
    if (typeof navigator !== "undefined" && navigator.clipboard?.readText) {
      const rawText = await navigator.clipboard.readText()
      const parsed = parseDiagramClipboardPayload(rawText)
      if (parsed?.length) {
        localClipboardEdges.value = parsed
        return parsed
      }
    }
  } catch {
  }

  if (localClipboardEdges.value.length) {
    return localClipboardEdges.value
  }

  return null
}

async function handlePasteSelection() {
  const sourceEdges = await resolveClipboardEdges()
  if (!sourceEdges?.length) {
    toastStore.info("Nothing to paste")
    return
  }

  const offset = COPY_PASTE_OFFSET * (clipboardPasteCount.value + 1)
  const insertedEdges: DiagramEdge[] = []

  commitHistoryMutation(() => {
    insertedEdges.push(...sourceEdges.map((edge) => {
      const start = snapWorldPoint({ x: edge.x1 + offset, y: edge.y1 + offset })
      const end = snapWorldPoint({ x: edge.x2 + offset, y: edge.y2 + offset })
      return {
        id: buildEdgeId(),
        x1: start.x,
        y1: start.y,
        x2: end.x,
        y2: end.y,
        kind: edge.kind,
      }
    }))

    edges.value = [...edges.value, ...insertedEdges]
    selectedNodeIds.value = []
    selectedEdgeIds.value = insertedEdges.map(edge => edge.id)
    selectedEdgeId.value = insertedEdges[0]?.id ?? null
    closeLineContextMenu()
  })

  clipboardPasteCount.value += 1
  toastStore.success(`Pasted ${insertedEdges.length} line${insertedEdges.length > 1 ? "s" : ""}`)
}

function getEdgeById(edgeId: string): DiagramEdge | null {
  return edges.value.find(edge => edge.id === edgeId) ?? null
}

function updateEdge(edgeId: string, patch: Partial<DiagramEdge>) {
  edges.value = edges.value.map(edge => (
    edge.id === edgeId
      ? {
          ...edge,
          ...patch,
        }
      : edge
  ))
}

function snapToEightDirections(anchorX: number, anchorY: number, targetX: number, targetY: number) {
  const dx = targetX - anchorX
  const dy = targetY - anchorY
  const length = Math.hypot(dx, dy)
  if (length < 0.0001) {
    return { x: targetX, y: targetY }
  }

  const step = Math.PI / 4
  const angle = Math.atan2(dy, dx)
  const snappedAngle = Math.round(angle / step) * step

  return {
    x: Math.round(anchorX + Math.cos(snappedAngle) * length),
    y: Math.round(anchorY + Math.sin(snappedAngle) * length),
  }
}

function removeSelectedEdge() {
  const toDelete = new Set<string>()
  if (selectedEdgeId.value) {
    toDelete.add(selectedEdgeId.value)
  }
  selectedEdgeIds.value.forEach(id => toDelete.add(id))
  if (toDelete.size === 0) {
    return
  }
  commitHistoryMutation(() => {
    edges.value = edges.value.filter(edge => !toDelete.has(edge.id))
    selectedEdgeId.value = null
    selectedEdgeIds.value = []
    closeLineContextMenu()
  })
}

function removeSelectedStaticElements() {
  const staticIds = new Set(selectedStaticIds.value)
  if (staticIds.size === 0) {
    return
  }

  commitHistoryMutation(() => {
    staticElements.value = staticElements.value.filter(element => !staticIds.has(element.id))
    selectedStaticIds.value = []
    closeLineContextMenu()
  })
}

function clearSelection() {
  selectedEdgeId.value = null
  selectedEdgeIds.value = []
  selectedNodeIds.value = []
  selectedStaticIds.value = []
  if (selectedNodeId.value !== null) {
    selectionStore.selectSwitchgear(null)
    void router.push({ name: "switchgears.list" })
  }
}

function appendUnique<T>(items: T[], value: T): T[] {
  return items.includes(value) ? items : [...items, value]
}

function effectiveSelectedNodeIds(): number[] {
  if (selectedNodeIds.value.length > 0) {
    return [...selectedNodeIds.value]
  }
  if (selectedEdgeId.value || selectedEdgeIds.value.length > 0 || selectedStaticIds.value.length > 0) {
    return []
  }
  return selectedNodeId.value !== null ? [selectedNodeId.value] : []
}

function effectiveSelectedEdgeIds(): string[] {
  if (selectedEdgeIds.value.length > 0) {
    return [...selectedEdgeIds.value]
  }
  return selectedEdgeId.value ? [selectedEdgeId.value] : []
}

function effectiveSelectedStaticIds(): string[] {
  return [...selectedStaticIds.value]
}

function closeLineContextMenu() {
  lineContextMenu.value = null
  staticContextMenu.value = null
  alignMenuOpen.value = false
}

function setSelectedEdgesKind(kind: "line" | "arrow") {
  const edgeIds = new Set(effectiveSelectedEdgeIds())
  if (edgeIds.size === 0) {
    return
  }

  commitHistoryMutation(() => {
    edges.value = edges.value.map(edge => (
      edgeIds.has(edge.id)
        ? { ...edge, kind }
        : edge
    ))
    closeLineContextMenu()
  })
}

function rotateSelectedEdges90() {
  const edgeIds = new Set(effectiveSelectedEdgeIds())
  if (edgeIds.size === 0) {
    return
  }

  commitHistoryMutation(() => {
    edges.value = edges.value.map((edge) => {
      if (!edgeIds.has(edge.id)) {
        return edge
      }

      const centerX = (edge.x1 + edge.x2) / 2
      const centerY = (edge.y1 + edge.y2) / 2
      const deltaX = (edge.x2 - edge.x1) / 2
      const deltaY = (edge.y2 - edge.y1) / 2

      const start = snapWorldPoint({
        x: centerX + deltaY,
        y: centerY - deltaX,
      })
      const end = snapWorldPoint({
        x: centerX - deltaY,
        y: centerY + deltaX,
      })

      return {
        ...edge,
        x1: start.x,
        y1: start.y,
        x2: end.x,
        y2: end.y,
      }
    })
    closeLineContextMenu()
  })
}

function rotateSelectedStaticElements90() {
  const staticIds = new Set(effectiveSelectedStaticIds())
  if (staticIds.size === 0) {
    return
  }

  commitHistoryMutation(() => {
    staticElements.value = staticElements.value.map(element => (
      staticIds.has(element.id)
        ? { ...element, rotation: nextQuarterRotation(element.rotation) }
        : element
    ))
    closeLineContextMenu()
  })
}

function alignSelectedNodesLeft() {
  const nodeIds = effectiveSelectedNodeIds()
  if (nodeIds.length < 2) {
    return
  }

  const layouts = nodeIds.map((id) => {
    const index = switchgears.value.findIndex(item => item.id === id)
    return { id, layout: resolvedLayout(id, index) }
  })
  const targetX = Math.min(...layouts.map(item => item.layout.x))
  commitHistoryMutation(() => {
    const nextLayout = { ...layoutById.value }
    layouts.forEach(({ id, layout }) => {
      nextLayout[String(id)] = { x: targetX, y: layout.y }
    })
    layoutById.value = nextLayout
    closeLineContextMenu()
  })
}

function alignSelectedNodesTop() {
  const nodeIds = effectiveSelectedNodeIds()
  if (nodeIds.length < 2) {
    return
  }

  const layouts = nodeIds.map((id) => {
    const index = switchgears.value.findIndex(item => item.id === id)
    return { id, layout: resolvedLayout(id, index) }
  })
  const targetY = Math.min(...layouts.map(item => item.layout.y))
  commitHistoryMutation(() => {
    const nextLayout = { ...layoutById.value }
    layouts.forEach(({ id, layout }) => {
      nextLayout[String(id)] = { x: layout.x, y: targetY }
    })
    layoutById.value = nextLayout
    closeLineContextMenu()
  })
}

function alignSelectedNodesRight() {
  const nodeIds = effectiveSelectedNodeIds()
  if (nodeIds.length < 2) {
    return
  }

  const layouts = nodeIds.map((id) => {
    const index = switchgears.value.findIndex(item => item.id === id)
    return { id, layout: resolvedLayout(id, index) }
  })
  const targetRight = Math.max(...layouts.map(item => item.layout.x + NODE_WIDTH))
  commitHistoryMutation(() => {
    const nextLayout = { ...layoutById.value }
    layouts.forEach(({ id, layout }) => {
      nextLayout[String(id)] = { x: targetRight - NODE_WIDTH, y: layout.y }
    })
    layoutById.value = nextLayout
    closeLineContextMenu()
  })
}

function alignSelectedNodesBottom() {
  const nodeIds = effectiveSelectedNodeIds()
  if (nodeIds.length < 2) {
    return
  }

  const layouts = nodeIds.map((id) => {
    const index = switchgears.value.findIndex(item => item.id === id)
    return { id, layout: resolvedLayout(id, index) }
  })
  const targetBottom = Math.max(...layouts.map(item => item.layout.y + NODE_HEIGHT))
  commitHistoryMutation(() => {
    const nextLayout = { ...layoutById.value }
    layouts.forEach(({ id, layout }) => {
      nextLayout[String(id)] = { x: layout.x, y: targetBottom - NODE_HEIGHT }
    })
    layoutById.value = nextLayout
    closeLineContextMenu()
  })
}

function keyboardNudgeDelta(event: KeyboardEvent): { dx: number; dy: number } | null {
  if (event.ctrlKey || event.metaKey) {
    return null
  }

  const step = event.altKey
    ? NUDGE_FINE_STEP
    : event.shiftKey
      ? NUDGE_LARGE_STEP
      : GRID_STEP

  switch (event.key) {
    case "ArrowLeft":
      return { dx: -step, dy: 0 }
    case "ArrowRight":
      return { dx: step, dy: 0 }
    case "ArrowUp":
      return { dx: 0, dy: -step }
    case "ArrowDown":
      return { dx: 0, dy: step }
    default:
      return null
  }
}

function nudgeSelection(dx: number, dy: number): boolean {
  const nodeIds = effectiveSelectedNodeIds()
    .filter(id => switchgears.value.some(item => item.id === id))
  const edgeIds = new Set(effectiveSelectedEdgeIds())
  const staticIds = new Set(effectiveSelectedStaticIds())

  if (nodeIds.length === 0 && edgeIds.size === 0 && staticIds.size === 0) {
    return false
  }

  return commitHistoryMutation(() => {
    if (nodeIds.length > 0) {
      const nextLayout = { ...layoutById.value }
      nodeIds.forEach((id) => {
        const index = switchgears.value.findIndex(item => item.id === id)
        const layout = resolvedLayout(id, index)
        nextLayout[String(id)] = {
          x: Math.round(layout.x + dx),
          y: Math.round(layout.y + dy),
        }
      })
      layoutById.value = nextLayout
    }

    if (edgeIds.size > 0) {
      edges.value = edges.value.map(edge => (
        edgeIds.has(edge.id)
          ? {
              ...edge,
              x1: Math.round(edge.x1 + dx),
              y1: Math.round(edge.y1 + dy),
              x2: Math.round(edge.x2 + dx),
              y2: Math.round(edge.y2 + dy),
            }
          : edge
      ))
    }

    if (staticIds.size > 0) {
      staticElements.value = staticElements.value.map(element => (
        staticIds.has(element.id)
          ? {
              ...element,
              x: Math.round(element.x + dx),
              y: Math.round(element.y + dy),
            }
          : element
      ))
    }

    closeLineContextMenu()
  })
}

function openLineContextMenu(edgeId: string, event: MouseEvent) {
  const viewport = viewportRef.value
  if (!viewport) {
    return
  }

  const effectiveEdges = effectiveSelectedEdgeIds()
  const selection = selectedEdgeIdSet.value.has(edgeId) && effectiveEdges.length > 0
    ? [...effectiveEdges]
    : [edgeId]

  selectedEdgeId.value = selection[0] ?? edgeId
  selectedEdgeIds.value = selection
  selectedNodeIds.value = []
  selectedStaticIds.value = []

  const rect = viewport.getBoundingClientRect()
  lineContextMenu.value = {
    x: event.clientX - rect.left,
    y: event.clientY - rect.top,
    edgeIds: selection,
  }
}

function openStaticContextMenu(staticId: string, event: MouseEvent) {
  const viewport = viewportRef.value
  if (!viewport) {
    return
  }

  const effectiveStatics = effectiveSelectedStaticIds()
  const selection = selectedStaticIdSet.value.has(staticId) && effectiveStatics.length > 0
    ? [...effectiveStatics]
    : [staticId]

  selectedEdgeId.value = null
  selectedEdgeIds.value = []
  selectedNodeIds.value = []
  selectedStaticIds.value = selection
  if (selectedNodeId.value !== null) {
    selectionStore.selectSwitchgear(null)
    void router.push({ name: "switchgears.list" })
  }

  const rect = viewport.getBoundingClientRect()
  staticContextMenu.value = {
    x: event.clientX - rect.left,
    y: event.clientY - rect.top,
    staticIds: selection,
  }
}

function handleWindowKeyDown(event: KeyboardEvent) {
  const active = document.activeElement as HTMLElement | null
  const tagName = active?.tagName?.toLowerCase() ?? ""
  const isEditable = active?.isContentEditable || ["input", "textarea", "select"].includes(tagName)
  if (isEditable) {
    return
  }

  if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "c") {
    event.preventDefault()
    if (dragState.value) {
      return
    }
    void handleCopySelection()
    return
  }

  if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "v") {
    event.preventDefault()
    if (dragState.value) {
      return
    }
    void handlePasteSelection()
    return
  }

  if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "z") {
    event.preventDefault()
    if (dragState.value) {
      return
    }
    if (event.shiftKey) {
      redo()
      return
    }
    undo()
    return
  }

  if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "y") {
    event.preventDefault()
    if (dragState.value) {
      return
    }
    redo()
    return
  }

  const nudgeDelta = keyboardNudgeDelta(event)
  if (nudgeDelta) {
    if (dragState.value) {
      return
    }
    if (nudgeSelection(nudgeDelta.dx, nudgeDelta.dy)) {
      event.preventDefault()
    }
    return
  }

  if ((event.key === "Delete" || event.key === "Backspace") && (selectedLineCount.value > 0 || selectedStaticCount.value > 0)) {
    event.preventDefault()
    if (selectedLineCount.value > 0) {
      removeSelectedEdge()
    }
    if (selectedStaticCount.value > 0) {
      removeSelectedStaticElements()
    }
    return
  }

  if (event.key === "Escape") {
    if (dragState.value) {
      stopDragSession()
    }
    if (interactionTool.value === "line") {
      interactionTool.value = "hand"
    }
    closeLineContextMenu()
  }
}

function selectNode(id: number, event?: MouseEvent) {
  closeLineContextMenu()
  if (event?.shiftKey) {
    selectedNodeIds.value = appendUnique(effectiveSelectedNodeIds(), id)
    return
  }

  selectedEdgeId.value = null
  selectedEdgeIds.value = []
  selectedStaticIds.value = []
  selectedNodeIds.value = [id]
  void router.push({ name: "switchgears.detail", params: { id } })
}

function selectStaticElement(id: string, event?: MouseEvent) {
  closeLineContextMenu()
  if (event?.shiftKey) {
    selectedStaticIds.value = appendUnique(effectiveSelectedStaticIds(), id)
    return
  }

  selectedEdgeId.value = null
  selectedEdgeIds.value = []
  selectedNodeIds.value = []
  selectedStaticIds.value = [id]
  if (selectedNodeId.value !== null) {
    selectionStore.selectSwitchgear(null)
    void router.push({ name: "switchgears.list" })
  }
}

function openDetail(id: number) {
  closeLineContextMenu()
  selectedEdgeId.value = null
  void router.push({ name: "switchgears.detail", params: { id } })
}

function setInteractionTool(tool: InteractionTool) {
  closeLineContextMenu()
  interactionTool.value = tool
}

function worldPointFromViewportEvent(event: PointerEvent) {
  const viewport = viewportRef.value
  if (!viewport) {
    return null
  }
  const rect = viewport.getBoundingClientRect()
  return {
    x: (event.clientX - rect.left - viewState.value.x) / viewState.value.zoom,
    y: (event.clientY - rect.top - viewState.value.y) / viewState.value.zoom,
  }
}

function zoomAt(clientX: number, clientY: number, nextZoom: number) {
  const viewport = viewportRef.value
  if (!viewport) {
    return
  }
  const zoom = clampZoom(nextZoom)
  const rect = viewport.getBoundingClientRect()
  const localX = clientX - rect.left
  const localY = clientY - rect.top
  const worldX = (localX - viewState.value.x) / viewState.value.zoom
  const worldY = (localY - viewState.value.y) / viewState.value.zoom

  viewState.value = {
    x: localX - worldX * zoom,
    y: localY - worldY * zoom,
    zoom,
  }
}

function zoomBy(delta: number) {
  const viewport = viewportRef.value
  if (!viewport) {
    return
  }
  const rect = viewport.getBoundingClientRect()
  const nextZoom = clampZoom(viewState.value.zoom + delta)
  zoomAt(rect.left + rect.width / 2, rect.top + rect.height / 2, nextZoom)
}

function fitToContent() {
  const viewport = viewportRef.value
  if (!viewport || switchgears.value.length === 0) {
    return
  }
  const rect = viewport.getBoundingClientRect()
  const positions = switchgears.value.map((item, index) => resolvedLayout(item.id, index))
  const minX = Math.min(...positions.map(item => item.x + STAGE_PADDING))
  const minY = Math.min(...positions.map(item => item.y + STAGE_PADDING))
  const maxX = Math.max(...positions.map(item => item.x + STAGE_PADDING + NODE_WIDTH))
  const maxY = Math.max(...positions.map(item => item.y + STAGE_PADDING + NODE_HEIGHT))
  const contentWidth = maxX - minX + 160
  const contentHeight = maxY - minY + 160
  const zoom = clampZoom(Math.min(
    (rect.width - 80) / contentWidth,
    (rect.height - 80) / contentHeight,
    1.2,
  ))

  viewState.value = {
    zoom,
    x: (rect.width - contentWidth * zoom) / 2 - (minX - 80) * zoom,
    y: (rect.height - contentHeight * zoom) / 2 - (minY - 80) * zoom,
  }
}

function autoArrange() {
  const changed = commitHistoryMutation(() => {
    const nextLayout: Record<string, DiagramNodeLayout> = {}
    switchgears.value.forEach((item, index) => {
      nextLayout[String(item.id)] = defaultLayout(index)
    })
    layoutById.value = nextLayout
    selectedEdgeId.value = null
  })

  if (changed) {
    void nextTick(() => {
      fitToContent()
    })
  }
}

function handleWheel(event: WheelEvent) {
  if (event.ctrlKey || event.metaKey || event.altKey) {
    const multiplier = event.deltaY > 0 ? 0.92 : 1.08
    zoomAt(event.clientX, event.clientY, clampZoom(viewState.value.zoom * multiplier))
    return
  }

  viewState.value = {
    ...viewState.value,
    x: viewState.value.x - event.deltaX,
    y: viewState.value.y - event.deltaY,
  }
}

function stopDragSession() {
  finishDragHistorySession()
  dragState.value = null
  if (typeof window === "undefined") {
    return
  }
  window.removeEventListener("pointermove", onWindowPointerMove)
  window.removeEventListener("pointerup", onWindowPointerUp)
  window.removeEventListener("pointercancel", onWindowPointerUp)
}

function onWindowPointerMove(event: PointerEvent) {
  if (!dragState.value) {
    return
  }
  if (dragState.value.type === "pan") {
    viewState.value = {
      ...viewState.value,
      x: dragState.value.originX + (event.clientX - dragState.value.startX),
      y: dragState.value.originY + (event.clientY - dragState.value.startY),
    }
    return
  }

  if (dragState.value.type === "marquee") {
    const world = worldPointFromViewportEvent(event)
    if (!world) {
      return
    }
    dragState.value = {
      ...dragState.value,
      currentX: world.x,
      currentY: world.y,
    }
    return
  }

  if (dragState.value.type === "new-line") {
    const world = worldPointFromViewportEvent(event)
    if (!world) {
      return
    }
    const snapped = event.shiftKey
      ? snapToEightDirections(dragState.value.startX, dragState.value.startY, world.x, world.y)
      : world
    const current = applyPortSnapToLinePoint(snapWorldPoint(snapped))
    dragState.value = {
      ...dragState.value,
      currentX: current.x,
      currentY: current.y,
    }
    return
  }

  const deltaX = (event.clientX - dragState.value.startX) / viewState.value.zoom
  const deltaY = (event.clientY - dragState.value.startY) / viewState.value.zoom

  if (dragState.value.type === "group") {
    const anchor = dragState.value.originNodes.length > 0
      ? {
          x: dragState.value.originNodes[0].x + STAGE_PADDING,
          y: dragState.value.originNodes[0].y + STAGE_PADDING,
        }
      : dragState.value.originEdges.length > 0
        ? {
          x: dragState.value.originEdges[0]?.x1 ?? 0,
          y: dragState.value.originEdges[0]?.y1 ?? 0,
        }
        : {
            x: dragState.value.originStatics[0]?.x ?? 0,
            y: dragState.value.originStatics[0]?.y ?? 0,
          }
    const movingPorts = [
      ...dragState.value.originNodes.flatMap(item => buildNodePortsForLayout(item.id, { x: item.x, y: item.y })),
      ...dragState.value.originEdges.flatMap(edge => buildLinePorts(edge)),
    ]
    const snappedTranslation = computeSnappedTranslation(
      anchor,
      deltaX,
      deltaY,
      movingPorts,
      new Set<number>(dragState.value.originNodes.map(item => item.id)),
      new Set<string>(dragState.value.originEdges.map(edge => edge.id)),
    )
    for (const orig of dragState.value.originNodes) {
      setNodeLayout(orig.id, {
        x: Math.round(orig.x + snappedTranslation.dx),
        y: Math.round(orig.y + snappedTranslation.dy),
      })
    }
    for (const orig of dragState.value.originEdges) {
      updateEdge(orig.id, {
        x1: Math.round(orig.x1 + snappedTranslation.dx),
        y1: Math.round(orig.y1 + snappedTranslation.dy),
        x2: Math.round(orig.x2 + snappedTranslation.dx),
        y2: Math.round(orig.y2 + snappedTranslation.dy),
      })
    }
    staticElements.value = staticElements.value.map((element) => {
      const original = dragState.value?.type === "group"
        ? dragState.value.originStatics.find(item => item.id === element.id)
        : null
      if (!original) {
        return element
      }
      return {
        ...element,
        x: Math.round(original.x + snappedTranslation.dx),
        y: Math.round(original.y + snappedTranslation.dy),
      }
    })
    return
  }
  if (dragState.value.type === "line") {
    if (!getEdgeById(dragState.value.edgeId)) {
      return
    }
    if (dragState.value.mode === "move") {
      const snappedTranslation = computeSnappedTranslation(
        { x: dragState.value.origin.x1, y: dragState.value.origin.y1 },
        deltaX,
        deltaY,
        buildLinePorts(dragState.value.origin),
        new Set<number>(),
        new Set<string>([dragState.value.edgeId]),
      )
      updateEdge(dragState.value.edgeId, {
        x1: Math.round(dragState.value.origin.x1 + snappedTranslation.dx),
        y1: Math.round(dragState.value.origin.y1 + snappedTranslation.dy),
        x2: Math.round(dragState.value.origin.x2 + snappedTranslation.dx),
        y2: Math.round(dragState.value.origin.y2 + snappedTranslation.dy),
      })
      return
    }

    if (dragState.value.mode === "start") {
      const rawX = dragState.value.origin.x1 + deltaX
      const rawY = dragState.value.origin.y1 + deltaY
      let snapped = event.shiftKey
        ? snapToEightDirections(dragState.value.origin.x2, dragState.value.origin.y2, rawX, rawY)
        : { x: rawX, y: rawY }
      snapped = snapWorldPoint(snapped)
      snapped = applyPortSnapToLinePoint(snapped, new Set<string>([dragState.value.edgeId]))
      updateEdge(dragState.value.edgeId, {
        x1: snapped.x,
        y1: snapped.y,
      })
      return
    }

    const rawX = dragState.value.origin.x2 + deltaX
    const rawY = dragState.value.origin.y2 + deltaY
    let snapped = event.shiftKey
      ? snapToEightDirections(dragState.value.origin.x1, dragState.value.origin.y1, rawX, rawY)
      : { x: rawX, y: rawY }
    snapped = snapWorldPoint(snapped)
    snapped = applyPortSnapToLinePoint(snapped, new Set<string>([dragState.value.edgeId]))
    updateEdge(dragState.value.edgeId, {
      x2: snapped.x,
      y2: snapped.y,
    })
    return
  }

  if (dragState.value.type === "node") {
    const snappedLayout = applyPortSnapToNodeLayout(
      dragState.value.id,
      snapNodeLayoutToGrid({
        x: dragState.value.originX + deltaX,
        y: dragState.value.originY + deltaY,
      }),
    )
    setNodeLayout(dragState.value.id, snappedLayout)
    return
  }

  if (dragState.value.type === "static") {
    const staticDragState = dragState.value
    const point = snapWorldPoint({
      x: staticDragState.originX + deltaX,
      y: staticDragState.originY + deltaY,
    })
    staticElements.value = staticElements.value.map(element => (
      element.id === staticDragState.id
        ? { ...element, x: point.x, y: point.y }
        : element
    ))
    return
  }

  setLabelOffset(dragState.value.id, {
    x: dragState.value.originX + deltaX,
    y: dragState.value.originY + deltaY,
  })
}

function onWindowPointerUp() {
  if (dragState.value?.type === "new-line") {
    const distance = Math.hypot(
      dragState.value.currentX - dragState.value.startX,
      dragState.value.currentY - dragState.value.startY,
    )
    if (distance >= 8) {
      addEdge(
        { x: dragState.value.startX, y: dragState.value.startY },
        { x: dragState.value.currentX, y: dragState.value.currentY },
      )
    }
    stopDragSession()
    return
  }

  if (dragState.value?.type === "marquee") {
    const x1 = Math.min(dragState.value.startX, dragState.value.currentX)
    const y1 = Math.min(dragState.value.startY, dragState.value.currentY)
    const x2 = Math.max(dragState.value.startX, dragState.value.currentX)
    const y2 = Math.max(dragState.value.startY, dragState.value.currentY)

    const nodes = switchgears.value
      .filter((item, index) => {
        const layout = resolvedLayout(item.id, index)
        const nx1 = layout.x + STAGE_PADDING
        const ny1 = layout.y + STAGE_PADDING
        const nx2 = nx1 + NODE_WIDTH
        const ny2 = ny1 + NODE_HEIGHT
        return nx1 <= x2 && nx2 >= x1 && ny1 <= y2 && ny2 >= y1
      })
      .map(item => item.id)

    const lines = edges.value
      .filter(edge => {
        const ex1 = Math.min(edge.x1, edge.x2)
        const ey1 = Math.min(edge.y1, edge.y2)
        const ex2 = Math.max(edge.x1, edge.x2)
        const ey2 = Math.max(edge.y1, edge.y2)
        return ex1 <= x2 && ex2 >= x1 && ey1 <= y2 && ey2 >= y1
      })
      .map(edge => edge.id)

    const statics = staticElements.value
      .filter((element) => {
        const bounds = getStaticElementBounds(element)
        return bounds.x1 <= x2 && bounds.x2 >= x1 && bounds.y1 <= y2 && bounds.y2 >= y1
      })
      .map(element => element.id)

    const isClickOnly = Math.abs(dragState.value.currentX - dragState.value.startX) < 1
      && Math.abs(dragState.value.currentY - dragState.value.startY) < 1

    if (isClickOnly && !dragState.value.additive) {
      clearSelection()
      stopDragSession()
      return
    }

    if (dragState.value.additive) {
      selectedNodeIds.value = [...new Set([...effectiveSelectedNodeIds(), ...nodes])]
      selectedEdgeIds.value = [...new Set([...effectiveSelectedEdgeIds(), ...lines])]
      selectedStaticIds.value = [...new Set([...effectiveSelectedStaticIds(), ...statics])]
    } else {
      selectedNodeIds.value = nodes
      selectedEdgeIds.value = lines
      selectedStaticIds.value = statics
    }
    selectedEdgeId.value = selectedEdgeIds.value[0] ?? null
  }
  stopDragSession()
}

function beginViewportPan(event: PointerEvent) {
  closeLineContextMenu()
  if (event.button !== 0) {
    return
  }
  const target = event.target as HTMLElement | null
  if (target?.closest("[data-node-root]") || target?.closest("[data-edge-hitbox]") || target?.closest("[data-edge-handle]") || target?.closest("[data-static-root]")) {
    return
  }

  if (interactionTool.value === "line") {
    const point = worldPointFromViewportEvent(event)
    if (!point) {
      return
    }
    beginLineDraftFromPoint(point, event)
    return
  }

  if (interactionTool.value === "arrow") {
    const point = worldPointFromViewportEvent(event)
    if (!point) {
      return
    }
    dragState.value = {
      type: "marquee",
      startX: point.x,
      startY: point.y,
      currentX: point.x,
      currentY: point.y,
      additive: event.shiftKey,
    }
    event.preventDefault()
    window.addEventListener("pointermove", onWindowPointerMove)
    window.addEventListener("pointerup", onWindowPointerUp)
    window.addEventListener("pointercancel", onWindowPointerUp)
    return
  }

  clearSelection()
  dragState.value = {
    type: "pan",
    startX: event.clientX,
    startY: event.clientY,
    originX: viewState.value.x,
    originY: viewState.value.y,
  }
  event.preventDefault()
  window.addEventListener("pointermove", onWindowPointerMove)
  window.addEventListener("pointerup", onWindowPointerUp)
  window.addEventListener("pointercancel", onWindowPointerUp)
}

function beginLineDraftFromPoint(point: { x: number; y: number }, event: PointerEvent) {
  const snapped = applyPortSnapToLinePoint(snapWorldPoint(point))
  clearSelection()
  dragState.value = {
    type: "new-line",
    startX: snapped.x,
    startY: snapped.y,
    currentX: snapped.x,
    currentY: snapped.y,
  }
  event.preventDefault()
  window.addEventListener("pointermove", onWindowPointerMove)
  window.addEventListener("pointerup", onWindowPointerUp)
  window.addEventListener("pointercancel", onWindowPointerUp)
}

function beginNodeDrag(id: number, event: PointerEvent) {
  closeLineContextMenu()
  if (event.button !== 0) {
    return
  }
  if (interactionTool.value === "line") {
    const pointer = worldPointFromViewportEvent(event)
    if (!pointer) {
      return
    }
    const index = switchgears.value.findIndex(item => item.id === id)
    const port = findNearestPort(pointer, buildNodePortsForLayout(id, resolvedLayout(id, index)))
    beginLineDraftFromPoint(port ?? pointer, event)
    return
  }

  const selectedNodeGroup = effectiveSelectedNodeIds()
  const selectedEdgeGroup = effectiveSelectedEdgeIds()
  const selectedStaticGroup = effectiveSelectedStaticIds()
  const nodeIsSelected = selectedNodeGroup.includes(id)
  const hasGroupSelection = selectedNodeGroup.length + selectedEdgeGroup.length + selectedStaticGroup.length > 1

  if (interactionTool.value === "arrow") {
    const dragNodeIds = nodeIsSelected ? selectedNodeGroup : [id]
    const originNodes = dragNodeIds.map(nid => {
      const nindex = switchgears.value.findIndex(item => item.id === nid)
      const layout = resolvedLayout(nid, nindex)
      return { id: nid, x: layout.x, y: layout.y }
    })
    const originEdges = edges.value
      .filter(edge => nodeIsSelected && selectedEdgeGroup.includes(edge.id))
      .map(edge => ({ ...edge }))
    const originStatics = staticElements.value
      .filter(element => nodeIsSelected && selectedStaticGroup.includes(element.id))
      .map(element => ({ ...element }))

    selectedEdgeId.value = null
    selectedEdgeIds.value = nodeIsSelected ? [...selectedEdgeGroup] : []
    selectedStaticIds.value = nodeIsSelected ? [...selectedStaticGroup] : []
    selectedNodeIds.value = [...dragNodeIds]

    dragState.value = {
      type: "group",
      startX: event.clientX,
      startY: event.clientY,
      originNodes,
      originEdges,
      originStatics,
    }
    beginDragHistorySession()
    event.preventDefault()
    window.addEventListener("pointermove", onWindowPointerMove)
    window.addEventListener("pointerup", onWindowPointerUp)
    window.addEventListener("pointercancel", onWindowPointerUp)
    return
  }

  if (nodeIsSelected && hasGroupSelection) {
    const originNodes = selectedNodeGroup.map(nid => {
      const nindex = switchgears.value.findIndex(item => item.id === nid)
      const layout = resolvedLayout(nid, nindex)
      return { id: nid, x: layout.x, y: layout.y }
    })
    const originEdges = edges.value
      .filter(edge => selectedEdgeGroup.includes(edge.id))
      .map(edge => ({ ...edge }))
    const originStatics = staticElements.value
      .filter(element => selectedStaticGroup.includes(element.id))
      .map(element => ({ ...element }))
    dragState.value = {
      type: "group",
      startX: event.clientX,
      startY: event.clientY,
      originNodes,
      originEdges,
      originStatics,
    }
    beginDragHistorySession()
    event.preventDefault()
    window.addEventListener("pointermove", onWindowPointerMove)
    window.addEventListener("pointerup", onWindowPointerUp)
    window.addEventListener("pointercancel", onWindowPointerUp)
    return
  }

  const index = switchgears.value.findIndex(item => item.id === id)
  const current = resolvedLayout(id, index)
  selectedEdgeId.value = null
  selectedNodeIds.value = []
  selectedEdgeIds.value = []
  selectedStaticIds.value = []
  dragState.value = {
    type: "node",
    id,
    startX: event.clientX,
    startY: event.clientY,
    originX: current.x,
    originY: current.y,
  }
  beginDragHistorySession()
  event.preventDefault()
  window.addEventListener("pointermove", onWindowPointerMove)
  window.addEventListener("pointerup", onWindowPointerUp)
  window.addEventListener("pointercancel", onWindowPointerUp)
}

function beginLabelDrag(id: number, event: PointerEvent) {
  closeLineContextMenu()
  if (event.button !== 0) {
    return
  }
  if (interactionTool.value === "line") {
    const pointer = worldPointFromViewportEvent(event)
    if (!pointer) {
      return
    }
    const index = switchgears.value.findIndex(item => item.id === id)
    const port = findNearestPort(pointer, buildNodePortsForLayout(id, resolvedLayout(id, index)))
    beginLineDraftFromPoint(port ?? pointer, event)
    return
  }

  if (interactionTool.value === "arrow") {
    const isSelected = selectedNodeIdSet.value.has(id) || selectedNodeId.value === id
    if (!isSelected) {
      selectedEdgeId.value = null
      selectedEdgeIds.value = []
      selectedStaticIds.value = []
      selectedNodeIds.value = [id]
    }
  }

  const current = resolvedLabelOffset(id)
  selectedEdgeId.value = null
  dragState.value = {
    type: "label",
    id,
    startX: event.clientX,
    startY: event.clientY,
    originX: current.x,
    originY: current.y,
  }
  beginDragHistorySession()
  event.preventDefault()
  window.addEventListener("pointermove", onWindowPointerMove)
  window.addEventListener("pointerup", onWindowPointerUp)
  window.addEventListener("pointercancel", onWindowPointerUp)
}

function beginStaticDrag(id: string, event: PointerEvent) {
  closeLineContextMenu()
  if (event.button !== 0) {
    return
  }
  if (interactionTool.value === "line") {
    const pointer = worldPointFromViewportEvent(event)
    const element = getStaticElementById(id)
    if (!element && !pointer) {
      return
    }
    beginLineDraftFromPoint(element ? { x: element.x, y: element.y } : pointer!, event)
    return
  }

  const selectedNodeGroup = effectiveSelectedNodeIds()
  const selectedEdgeGroup = effectiveSelectedEdgeIds()
  const selectedStaticGroup = effectiveSelectedStaticIds()
  const staticIsSelected = selectedStaticGroup.includes(id)
  const hasGroupSelection = selectedNodeGroup.length + selectedEdgeGroup.length + selectedStaticGroup.length > 1

  if (staticIsSelected && (interactionTool.value === "arrow" || hasGroupSelection)) {
    const originNodes = selectedNodeGroup.map((nodeId) => {
      const index = switchgears.value.findIndex(item => item.id === nodeId)
      const layout = resolvedLayout(nodeId, index)
      return { id: nodeId, x: layout.x, y: layout.y }
    })
    const originEdges = edges.value
      .filter(edge => selectedEdgeGroup.includes(edge.id))
      .map(edge => ({ ...edge }))
    const originStatics = staticElements.value
      .filter(element => selectedStaticGroup.includes(element.id))
      .map(element => ({ ...element }))

    dragState.value = {
      type: "group",
      startX: event.clientX,
      startY: event.clientY,
      originNodes,
      originEdges,
      originStatics,
    }
    beginDragHistorySession()
    event.preventDefault()
    window.addEventListener("pointermove", onWindowPointerMove)
    window.addEventListener("pointerup", onWindowPointerUp)
    window.addEventListener("pointercancel", onWindowPointerUp)
    return
  }

  if (interactionTool.value === "arrow") {
    return
  }

  const element = getStaticElementById(id)
  if (!element) {
    return
  }

  selectedEdgeId.value = null
  selectedNodeIds.value = []
  selectedEdgeIds.value = []
  selectedStaticIds.value = [id]
  if (selectedNodeId.value !== null) {
    selectionStore.selectSwitchgear(null)
    void router.push({ name: "switchgears.list" })
  }
  dragState.value = {
    type: "static",
    id,
    startX: event.clientX,
    startY: event.clientY,
    originX: element.x,
    originY: element.y,
  }
  beginDragHistorySession()
  event.preventDefault()
  window.addEventListener("pointermove", onWindowPointerMove)
  window.addEventListener("pointerup", onWindowPointerUp)
  window.addEventListener("pointercancel", onWindowPointerUp)
}

function beginEdgeDrag(edgeId: string, mode: "move" | "start" | "end", event: PointerEvent) {
  closeLineContextMenu()
  if (event.button !== 0) {
    return
  }
  if (interactionTool.value === "line") {
    const edge = getEdgeById(edgeId)
    const pointer = worldPointFromViewportEvent(event)
    if (!edge || !pointer) {
      return
    }
    const start = mode === "start"
      ? { x: edge.x1, y: edge.y1 }
      : mode === "end"
        ? { x: edge.x2, y: edge.y2 }
        : findNearestPort(pointer, buildLinePorts(edge)) ?? pointer
    beginLineDraftFromPoint(start, event)
    return
  }

  if (event.shiftKey) {
    return
  }

  const selectedNodeGroup = effectiveSelectedNodeIds()
  const selectedEdgeGroup = effectiveSelectedEdgeIds()
  const selectedStaticGroup = effectiveSelectedStaticIds()
  const edgeIsSelected = selectedEdgeGroup.includes(edgeId)
  const hasGroupSelection = selectedNodeGroup.length + selectedEdgeGroup.length + selectedStaticGroup.length > 1

  if (mode === "move" && edgeIsSelected && (interactionTool.value === "arrow" || hasGroupSelection)) {
    const originNodes = selectedNodeGroup.map((id) => {
      const index = switchgears.value.findIndex(item => item.id === id)
      const layout = resolvedLayout(id, index)
      return { id, x: layout.x, y: layout.y }
    })
    const originEdges = edges.value
      .filter(edge => selectedEdgeGroup.includes(edge.id))
      .map(edge => ({ ...edge }))
    const originStatics = staticElements.value
      .filter(element => selectedStaticGroup.includes(element.id))
      .map(element => ({ ...element }))

    dragState.value = {
      type: "group",
      startX: event.clientX,
      startY: event.clientY,
      originNodes,
      originEdges,
      originStatics,
    }
    beginDragHistorySession()
    event.preventDefault()
    window.addEventListener("pointermove", onWindowPointerMove)
    window.addEventListener("pointerup", onWindowPointerUp)
    window.addEventListener("pointercancel", onWindowPointerUp)
    return
  }

  if (interactionTool.value === "arrow" && !edgeIsSelected) {
    return
  }

  const edge = getEdgeById(edgeId)
  if (!edge) {
    return
  }
  selectedEdgeId.value = edgeId
  selectedEdgeIds.value = [edgeId]
  selectedNodeIds.value = []
  selectedStaticIds.value = []
  dragState.value = {
    type: "line",
    edgeId,
    mode,
    startX: event.clientX,
    startY: event.clientY,
    origin: { ...edge },
  }
  beginDragHistorySession()
  event.preventDefault()
  window.addEventListener("pointermove", onWindowPointerMove)
  window.addEventListener("pointerup", onWindowPointerUp)
  window.addEventListener("pointercancel", onWindowPointerUp)
}

function centerViewportAtWorldPoint(worldX: number, worldY: number) {
  if (viewportSize.value.width <= 0 || viewportSize.value.height <= 0) {
    return
  }
  viewState.value = {
    ...viewState.value,
    x: viewportSize.value.width / 2 - worldX * viewState.value.zoom,
    y: viewportSize.value.height / 2 - worldY * viewState.value.zoom,
  }
}

function handleMinimapPointerDown(event: PointerEvent) {
  closeLineContextMenu()
  const model = minimapModel.value
  const target = event.currentTarget as SVGElement | null
  if (!model || !target) {
    return
  }
  const rect = target.getBoundingClientRect()
  const localX = event.clientX - rect.left
  const localY = event.clientY - rect.top
  const worldX = model.contentMinX + (localX - model.offsetX) / model.scale
  const worldY = model.contentMinY + (localY - model.offsetY) / model.scale
  centerViewportAtWorldPoint(worldX, worldY)
}

function edgePath(edge: DiagramEdge): string {
  return `M ${edge.x1} ${edge.y1} L ${edge.x2} ${edge.y2}`
}

function selectEdge(edgeId: string, event?: MouseEvent) {
  closeLineContextMenu()
  if (event?.shiftKey) {
    selectedEdgeId.value = edgeId
    selectedEdgeIds.value = appendUnique(effectiveSelectedEdgeIds(), edgeId)
    return
  }

  selectedEdgeId.value = edgeId
  selectedEdgeIds.value = [edgeId]
  selectedNodeIds.value = []
  selectedStaticIds.value = []
}

watch(
  () => workspaceId.value,
  async () => {
    if (workspaceId.value) {
      await runStoreBootstrap(
        ["switchgear-sld", workspaceId.value],
        [
          () => switchgearStore.ensureLoaded(),
          () => deviceStore.ensureLoaded(),
          () => channelStore.ensureLoaded(),
        ],
        { mode: "settled" },
      )
    }
    restoreDiagramState()
  },
  { immediate: true },
)

watch(switchgearIdsSignature, () => {
  const changed = ensureLayoutDefaults()
  if (changed) {
    persistDiagramState()
  }
})

watch(
  [layoutById, labelOffsetById, edges, staticElements, viewState, snapEnabled],
  () => {
    persistDiagramState()
  },
  { deep: true },
)

onMounted(() => {
  updateViewportMetrics()
  if (typeof window !== "undefined") {
    window.addEventListener("resize", updateViewportMetrics)
    window.addEventListener("keydown", handleWindowKeyDown)
  }
  if (typeof ResizeObserver !== "undefined" && viewportRef.value) {
    viewportResizeObserver = new ResizeObserver(() => {
      updateViewportMetrics()
    })
    viewportResizeObserver.observe(viewportRef.value)
  }
})

onBeforeUnmount(() => {
  stopDragSession()
  if (typeof window !== "undefined") {
    window.removeEventListener("resize", updateViewportMetrics)
    window.removeEventListener("keydown", handleWindowKeyDown)
  }
  if (viewportResizeObserver) {
    viewportResizeObserver.disconnect()
    viewportResizeObserver = null
  }
})
</script>

<template>
  <section class="switchgear-sld">
    <div class="switchgear-sld__toolbar">
      <div class="switchgear-sld__tool-group">
        <button
          type="button"
          class="switchgear-sld__tool-button switchgear-sld__tool-button--split"
          :class="{ 'switchgear-sld__tool-button--active': interactionTool === 'hand' }"
          title="Pan / Move (Hand)"
          @click="setInteractionTool('hand')"
        >
          <svg viewBox="0 0 16 16" class="switchgear-sld__icon" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round">
            <path d="M6 2v6M4.5 4.5V4a1 1 0 0 0-2 0v4.5l-.5.5V11a3 3 0 0 0 3 3h2a3 3 0 0 0 3-3V7a1 1 0 0 0-2 0v-.5a1 1 0 0 0-2 0V2a1 1 0 0 0-2 0Z"/>
          </svg>
        </button>
        <button
          type="button"
          class="switchgear-sld__tool-button"
          :class="{ 'switchgear-sld__tool-button--active': interactionTool === 'arrow' }"
          title="Select / Marquee (Arrow)"
          @click="setInteractionTool('arrow')"
        >
          <svg viewBox="0 0 16 16" class="switchgear-sld__icon" fill="currentColor">
            <path d="M3 2l10 6-5.5 1.5L6 15z"/>
          </svg>
        </button>
        <button
          type="button"
          class="switchgear-sld__tool-button"
          :class="{ 'switchgear-sld__tool-button--active': interactionTool === 'line' }"
          title="Draw line"
          aria-label="Draw line"
          @click="setInteractionTool('line')"
        >
          <svg viewBox="0 0 16 16" class="switchgear-sld__icon" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">
            <path d="M3 11 11 3"/>
            <path d="M10.5 11.5h2.5V9"/>
            <path d="M3 5.5V3h2.5"/>
          </svg>
        </button>
      </div>

      <button
        type="button"
        class="switchgear-sld__tool-button switchgear-sld__tool-button--standalone"
        :class="{ 'switchgear-sld__tool-button--active': snapEnabled }"
        :title="snapEnabled ? 'Disable magnetic snap' : 'Enable magnetic snap'"
        :aria-label="snapEnabled ? 'Disable magnetic snap' : 'Enable magnetic snap'"
        @click="toggleSnapEnabled()"
      >
        <svg viewBox="0 0 16 16" class="switchgear-sld__icon" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
          <path d="M5 2v3" />
          <path d="M11 2v3" />
          <path d="M5 5H3.5A1.5 1.5 0 0 0 2 6.5V9a3 3 0 0 0 3 3h6a3 3 0 0 0 3-3V6.5A1.5 1.5 0 0 0 12.5 5H11" />
          <path d="M5 8h6" />
        </svg>
      </button>

      <UiButton size="sm" variant="secondary" class="switchgear-sld__icon-action" title="Undo" aria-label="Undo" :disabled="!canUndo" @click="undo()">
        <svg viewBox="0 0 16 16" class="switchgear-sld__icon" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">
          <path d="M6 4 2.5 7.5 6 11"/>
          <path d="M3 7.5h5.25a4.25 4.25 0 1 1 0 8.5H7"/>
        </svg>
      </UiButton>
      <UiButton size="sm" variant="secondary" class="switchgear-sld__icon-action" title="Redo" aria-label="Redo" :disabled="!canRedo" @click="redo()">
        <svg viewBox="0 0 16 16" class="switchgear-sld__icon" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">
          <path d="m10 4 3.5 3.5L10 11"/>
          <path d="M13 7.5H7.75a4.25 4.25 0 1 0 0 8.5H9"/>
        </svg>
      </UiButton>
      <UiButton
        size="sm"
        variant="secondary"
        class="switchgear-sld__icon-action"
        title="Add transformer"
        aria-label="Add transformer"
        @click="addStaticElementInViewport('transformer')"
      >
        <svg viewBox="0 0 16 16" class="switchgear-sld__icon" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round">
          <circle cx="8" cy="5.5" r="3.5" />
          <circle cx="8" cy="10.5" r="3.5" />
        </svg>
      </UiButton>
      <UiButton
        size="sm"
        variant="secondary"
        class="switchgear-sld__icon-action"
        title="Add ground"
        aria-label="Add ground"
        @click="addStaticElementInViewport('ground')"
      >
        <svg viewBox="0 0 16 16" class="switchgear-sld__icon" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="square" stroke-linejoin="round">
          <path d="M2 8h8" />
          <path d="M10 3v10" />
          <path d="M13 4.5v7" />
          <path d="M15 6v4" />
        </svg>
      </UiButton>

      <div class="switchgear-sld__metrics" aria-label="Diagram metrics">
        <span
          v-for="item in diagramMetricItems"
          :key="item.label"
          class="switchgear-sld__metric"
        >
          <span class="switchgear-sld__metric-value">{{ item.value }}</span>
          <span class="switchgear-sld__metric-label">{{ item.label }}</span>
        </span>
      </div>

      <div
        v-if="selectedLineCount > 0"
        class="switchgear-sld__tool-group"
      >
        <button
          type="button"
          class="switchgear-sld__kind-button switchgear-sld__kind-button--split"
          :class="{ 'switchgear-sld__kind-button--active': selectedLineKind === 'line' }"
          @click="setSelectedEdgesKind('line')"
        >
          Line
        </button>
        <button
          type="button"
          class="switchgear-sld__kind-button"
          :class="{ 'switchgear-sld__kind-button--active': selectedLineKind === 'arrow' }"
          @click="setSelectedEdgesKind('arrow')"
        >
          Arrow
        </button>
      </div>
      <div v-if="selectedNodeCount > 1" class="switchgear-sld__toolbar-menu-anchor">
        <UiButton
          size="sm"
          variant="secondary"
          class="switchgear-sld__icon-action"
          title="Align nodes"
          aria-label="Align nodes"
          @click="alignMenuOpen = !alignMenuOpen"
        >
          <svg viewBox="0 0 16 16" class="switchgear-sld__icon" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">
            <path d="M3 2v12" />
            <path d="M2 3h12" />
            <path d="M6 6h6" />
            <path d="M6 10h4" />
          </svg>
        </UiButton>
        <div
          v-if="alignMenuOpen"
          class="switchgear-sld__dropdown"
          @pointerdown.stop
        >
          <button
            type="button"
            class="switchgear-sld__dropdown-item"
            @click="alignSelectedNodesLeft()"
          >
            <span>Align left</span>
            <svg viewBox="0 0 16 16" class="switchgear-sld__icon" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">
              <path d="M3 2v12" />
              <path d="M6 4h6" />
              <path d="M6 8h4" />
              <path d="M6 12h7" />
            </svg>
          </button>
          <button
            type="button"
            class="switchgear-sld__dropdown-item"
            @click="alignSelectedNodesTop()"
          >
            <span>Align top</span>
            <svg viewBox="0 0 16 16" class="switchgear-sld__icon" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">
              <path d="M2 3h12" />
              <path d="M4 6v6" />
              <path d="M8 6v4" />
              <path d="M12 6v7" />
            </svg>
          </button>
          <button
            type="button"
            class="switchgear-sld__dropdown-item"
            @click="alignSelectedNodesRight()"
          >
            <span>Align right</span>
            <svg viewBox="0 0 16 16" class="switchgear-sld__icon" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">
              <path d="M13 2v12" />
              <path d="M4 4h6" />
              <path d="M6 8h4" />
              <path d="M3 12h7" />
            </svg>
          </button>
          <button
            type="button"
            class="switchgear-sld__dropdown-item"
            @click="alignSelectedNodesBottom()"
          >
            <span>Align bottom</span>
            <svg viewBox="0 0 16 16" class="switchgear-sld__icon" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">
              <path d="M2 13h12" />
              <path d="M4 4v6" />
              <path d="M8 6v4" />
              <path d="M12 3v7" />
            </svg>
          </button>
        </div>
      </div>

      <div class="switchgear-sld__toolbar-spacer">
        <SwitchgearControlToolbar
          v-if="singleSelectedSwitchgear"
          :switchgear="singleSelectedSwitchgear"
          compact
        />
      </div>
    </div>

    <WorkspacePlaceholder
      v-if="!workspaceId"
      tag="Switchgears"
      title="Select a workspace"
      description="Pick a workspace to open its single line diagram and arrange the switchgear layout."
    />

    <WorkspacePlaceholder
      v-else-if="switchgears.length === 0"
      tag="Single Line Diagram"
      title="No switchgears yet"
      description="Create switchgears from the sidebar first, then place them on the canvas and connect them with lines."
    />

    <div
      v-else
      ref="viewportRef"
      class="switchgear-sld__viewport"
      :class="{
        'switchgear-sld__viewport--select': interactionTool === 'arrow',
        'switchgear-sld__viewport--draw-line': interactionTool === 'line',
      }"
      :style="viewportSurfaceStyle"
      @pointerdown="beginViewportPan"
      @wheel.prevent="handleWheel"
    >
      <div class="switchgear-sld__viewport-overlay" :style="viewportOverlayStyle"></div>

      <div class="switchgear-sld__status-strip" aria-live="polite">
        <span class="switchgear-sld__status-pill switchgear-sld__status-pill--primary">
          {{ activeToolLabel }}
        </span>
        <span
          class="switchgear-sld__status-pill"
          :class="{ 'switchgear-sld__status-pill--muted': !snapEnabled }"
        >
          {{ snapStateLabel }}
        </span>
        <span class="switchgear-sld__status-selection">
          {{ selectionSummary }}
        </span>
      </div>

      <div class="switchgear-sld__stage" :style="stageTransformStyle">
        <div class="switchgear-sld__grid" :style="stageGridStyle">
          <svg
            class="switchgear-sld__edge-layer"
            :viewBox="`0 0 ${stageSize.width} ${stageSize.height}`"
            preserveAspectRatio="none"
          >
            <defs>
              <marker
                id="switchgear-sld-arrowhead"
                markerWidth="10"
                markerHeight="10"
                refX="8"
                refY="3.5"
                orient="auto"
                markerUnits="strokeWidth"
              >
                <path d="M0 0 8 3.5 0 7z" fill="#2563eb" />
              </marker>
            </defs>
            <g v-for="edge in edges" :key="edge.id">
              <path
                :d="edgePath(edge)"
                fill="none"
                stroke-linecap="round"
                stroke-linejoin="round"
                :stroke="selectedEdgeId === edge.id ? '#2563eb' : '#2563eb'"
                :stroke-width="selectedEdgeIdSet.has(edge.id) ? 3 : 2"
                :marker-end="edge.kind === 'arrow' ? 'url(#switchgear-sld-arrowhead)' : undefined"
              />
              <path
                :d="edgePath(edge)"
                data-edge-hitbox
                fill="none"
                stroke="transparent"
                stroke-width="18"
                stroke-linecap="round"
                stroke-linejoin="round"
                class="switchgear-sld__edge-hitbox"
                @pointerdown.stop="beginEdgeDrag(edge.id, 'move', $event)"
                @click.stop="selectEdge(edge.id, $event)"
                @contextmenu.stop.prevent="openLineContextMenu(edge.id, $event)"
              />
              <g v-if="selectedEdgeIdSet.has(edge.id)">
                <circle
                  :cx="edge.x1"
                  :cy="edge.y1"
                  r="6"
                  fill="#ffffff"
                  stroke="#0ea5e9"
                  stroke-width="2"
                  data-edge-handle
                  class="switchgear-sld__edge-handle"
                  @pointerdown.stop="beginEdgeDrag(edge.id, 'start', $event)"
                  @contextmenu.stop.prevent="openLineContextMenu(edge.id, $event)"
                />
                <circle
                  :cx="edge.x2"
                  :cy="edge.y2"
                  r="6"
                  fill="#ffffff"
                  stroke="#0ea5e9"
                  stroke-width="2"
                  data-edge-handle
                  class="switchgear-sld__edge-handle"
                  @pointerdown.stop="beginEdgeDrag(edge.id, 'end', $event)"
                  @contextmenu.stop.prevent="openLineContextMenu(edge.id, $event)"
                />
              </g>
            </g>

            <g v-if="draftLinePreview">
              <path
                :d="`M ${draftLinePreview.x1} ${draftLinePreview.y1} L ${draftLinePreview.x2} ${draftLinePreview.y2}`"
                fill="none"
                stroke="#0ea5e9"
                stroke-width="2.5"
                stroke-linecap="round"
                stroke-dasharray="10 7"
                class="switchgear-sld__edge-draft"
              />
              <circle
                :cx="draftLinePreview.x1"
                :cy="draftLinePreview.y1"
                r="5"
                class="switchgear-sld__edge-draft-handle"
              />
              <circle
                :cx="draftLinePreview.x2"
                :cy="draftLinePreview.y2"
                r="5"
                class="switchgear-sld__edge-draft-handle"
              />
            </g>

            <rect
              v-if="marqueeRect"
              :x="marqueeRect.x"
              :y="marqueeRect.y"
              :width="marqueeRect.width"
              :height="marqueeRect.height"
              fill="rgba(14,165,233,0.12)"
              stroke="#0ea5e9"
              stroke-width="1.5"
              stroke-dasharray="6 4"
            />
          </svg>

          <SwitchgearSingleLineDiagramStaticElement
            v-for="element in staticElements"
            :key="element.id"
            :id="element.id"
            :kind="element.kind"
            :x="element.x"
            :y="element.y"
            :rotation="element.rotation"
            :selected="selectedStaticIdSet.has(element.id)"
            @drag-start="beginStaticDrag(element.id, $event)"
            @select="selectStaticElement(element.id, $event)"
            @context-menu="openStaticContextMenu(element.id, $event)"
          />

          <SwitchgearSingleLineDiagramNode
            v-for="(switchgear, index) in switchgears"
            :key="switchgear.id"
            :switchgear="switchgear"
            :x="resolvedLayout(switchgear.id, index).x + STAGE_PADDING"
            :y="resolvedLayout(switchgear.id, index).y + STAGE_PADDING"
            :label-offset-x="resolvedLabelOffset(switchgear.id).x"
            :label-offset-y="resolvedLabelOffset(switchgear.id).y"
            :selected="selectedNodeIdSet.has(switchgear.id)"
            @drag-start="beginNodeDrag(switchgear.id, $event)"
            @label-drag-start="beginLabelDrag(switchgear.id, $event)"
            @select="selectNode(switchgear.id, $event)"
            @open-detail="openDetail(switchgear.id)"
          />
        </div>
      </div>

      <div class="switchgear-sld__zoom-panel">
        <UiButton size="sm" variant="secondary" class="switchgear-sld__icon-action" title="Zoom out" aria-label="Zoom out" @click="zoomBy(-0.1)">
          <svg viewBox="0 0 16 16" class="switchgear-sld__icon" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">
            <circle cx="7" cy="7" r="4.5"/>
            <path d="M10.5 10.5 14 14"/>
            <path d="M5 7h4"/>
          </svg>
        </UiButton>
        <span class="switchgear-sld__zoom-label">{{ zoomLabel }}</span>
        <UiButton size="sm" variant="secondary" class="switchgear-sld__icon-action" title="Zoom in" aria-label="Zoom in" @click="zoomBy(0.1)">
          <svg viewBox="0 0 16 16" class="switchgear-sld__icon" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">
            <circle cx="7" cy="7" r="4.5"/>
            <path d="M10.5 10.5 14 14"/>
            <path d="M7 5v4"/>
            <path d="M5 7h4"/>
          </svg>
        </UiButton>
        <UiButton size="sm" variant="secondary" @click="fitToContent">
          Fit
        </UiButton>
        <UiButton size="sm" variant="secondary" @click="autoArrange">
          Auto layout
        </UiButton>
      </div>

      <div
        v-if="lineContextMenu"
        class="switchgear-sld__context-menu"
        :style="{ left: `${lineContextMenu.x}px`, top: `${lineContextMenu.y}px` }"
        @pointerdown.stop
      >
        <button
          type="button"
          class="switchgear-sld__context-item"
          @click="rotateSelectedEdges90()"
        >
          <span>{{ lineContextMenu.edgeIds.length > 1 ? 'Rotate selected lines 90°' : 'Rotate line 90°' }}</span>
          <span class="switchgear-sld__context-shortcut">R</span>
        </button>
        <button
          type="button"
          class="switchgear-sld__context-item switchgear-sld__context-item--danger"
          @click="removeSelectedEdge()"
        >
          <span>{{ lineContextMenu.edgeIds.length > 1 ? 'Remove selected lines' : 'Remove line' }}</span>
          <span class="switchgear-sld__context-shortcut">Del</span>
        </button>
      </div>

      <div
        v-if="staticContextMenu"
        class="switchgear-sld__context-menu switchgear-sld__context-menu--wide"
        :style="{ left: `${staticContextMenu.x}px`, top: `${staticContextMenu.y}px` }"
        @pointerdown.stop
      >
        <button
          type="button"
          class="switchgear-sld__context-item"
          @click="rotateSelectedStaticElements90()"
        >
          <span>{{ staticContextMenu.staticIds.length > 1 ? 'Rotate selected symbols 90°' : 'Rotate symbol 90°' }}</span>
          <span class="switchgear-sld__context-shortcut">R</span>
        </button>
        <button
          type="button"
          class="switchgear-sld__context-item switchgear-sld__context-item--danger"
          @click="removeSelectedStaticElements()"
        >
          <span>{{ staticContextMenu.staticIds.length > 1 ? 'Remove selected symbols' : 'Remove symbol' }}</span>
          <span class="switchgear-sld__context-shortcut">Del</span>
        </button>
      </div>

      <div
        v-if="minimapModel"
        class="switchgear-sld__minimap"
      >
        <svg
          :width="MINIMAP_WIDTH"
          :height="MINIMAP_HEIGHT"
          class="switchgear-sld__minimap-svg"
          @pointerdown.stop.prevent="handleMinimapPointerDown"
        >
          <rect
            x="0"
            y="0"
            :width="MINIMAP_WIDTH"
            :height="MINIMAP_HEIGHT"
            rx="8"
            :fill="isDarkTheme ? 'rgba(15, 23, 42, 0.86)' : 'rgba(148,163,184,0.18)'"
          />
          <g>
            <rect
              v-for="item in minimapModel.nodes"
              :key="item.id"
              :x="item.x"
              :y="item.y"
              :width="item.width"
              :height="item.height"
              rx="1.5"
              :fill="item.active ? '#38bdf8' : (isDarkTheme ? '#e2e8f0' : '#334155')"
              :opacity="item.active ? 1 : (isDarkTheme ? 0.92 : 0.65)"
            />
          </g>
          <rect
            :x="minimapModel.viewport.x"
            :y="minimapModel.viewport.y"
            :width="minimapModel.viewport.width"
            :height="minimapModel.viewport.height"
            rx="2"
            fill="rgba(14,165,233,0.15)"
            stroke="#0ea5e9"
            stroke-width="1.4"
          />
        </svg>
      </div>
    </div>
  </section>
</template>

<style scoped>
.switchgear-sld {
  display: flex;
  min-height: 0;
  height: 100%;
  flex-direction: column;
  padding: 0.75rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 82%, transparent);
  border-radius: 1rem;
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-white) 94%, transparent), color-mix(in srgb, var(--color-neutral-50) 86%, transparent));
  box-shadow:
    0 18px 40px rgb(15 23 42 / 0.08),
    inset 0 1px 0 rgb(255 255 255 / 0.72);
}

.switchgear-sld__toolbar {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.5rem;
  margin-bottom: 0.75rem;
  padding: 0.5rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 84%, transparent);
  border-radius: 0.875rem;
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-white) 92%, transparent), color-mix(in srgb, var(--color-neutral-50) 80%, transparent));
  box-shadow:
    0 10px 24px rgb(15 23 42 / 0.06),
    inset 0 1px 0 rgb(255 255 255 / 0.78);
}

.switchgear-sld__tool-group {
  display: inline-flex;
  overflow: hidden;
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 82%, transparent);
  border-radius: 0.5rem;
  background: color-mix(in srgb, var(--color-white) 70%, transparent);
  box-shadow: inset 0 1px 0 rgb(255 255 255 / 0.72);
}

.switchgear-sld__tool-button,
.switchgear-sld__kind-button {
  display: flex;
  height: 1.75rem;
  align-items: center;
  justify-content: center;
  border: 0;
  background: transparent;
  color: var(--color-neutral-600);
  cursor: pointer;
  font: inherit;
  outline: none;
  transition: background-color 0.15s ease, color 0.15s ease, box-shadow 0.15s ease;
}

.switchgear-sld__tool-button {
  width: 2rem;
}

.switchgear-sld__tool-button--standalone {
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 82%, transparent);
  border-radius: 0.5rem;
  background: color-mix(in srgb, var(--color-white) 70%, transparent);
  box-shadow: inset 0 1px 0 rgb(255 255 255 / 0.72);
}

.switchgear-sld__kind-button {
  padding: 0 0.75rem;
  font-size: var(--text-xs);
  font-weight: 500;
}

.switchgear-sld__tool-button--split,
.switchgear-sld__kind-button--split {
  border-right: 1px solid color-mix(in srgb, var(--color-neutral-300) 82%, transparent);
}

.switchgear-sld__tool-button:hover,
.switchgear-sld__kind-button:hover {
  background: color-mix(in srgb, var(--color-neutral-200) 76%, transparent);
  color: var(--color-neutral-900);
}

.switchgear-sld__tool-button:focus-visible,
.switchgear-sld__kind-button:focus-visible {
  box-shadow: 0 0 0 2px color-mix(in srgb, var(--color-blue-500) 40%, transparent);
}

.switchgear-sld__tool-button--active,
.switchgear-sld__kind-button--active,
.switchgear-sld__tool-button--active:hover,
.switchgear-sld__kind-button--active:hover {
  background: var(--color-blue-600);
  color: var(--color-white);
  box-shadow:
    inset 0 1px 0 rgb(255 255 255 / 0.22),
    0 0 0 1px color-mix(in srgb, var(--color-blue-400) 34%, transparent);
}

.switchgear-sld__icon {
  width: 0.875rem;
  height: 0.875rem;
}

.switchgear-sld__icon-action {
  width: 2rem;
  justify-content: center;
  padding-right: 0;
  padding-left: 0;
}

.switchgear-sld__toolbar-menu-anchor {
  position: relative;
}

.switchgear-sld__dropdown,
.switchgear-sld__context-menu {
  position: absolute;
  z-index: 20;
  min-width: 168px;
  overflow: hidden;
  padding: 0.25rem 0;
  border: 1px solid var(--color-neutral-300);
  border-radius: 0.5rem;
  background: var(--color-white);
  box-shadow: 0 20px 25px -5px rgb(0 0 0 / 10%), 0 8px 10px -6px rgb(0 0 0 / 10%);
}

.switchgear-sld__dropdown {
  top: 100%;
  left: 0;
  margin-top: 0.5rem;
}

.switchgear-sld__dropdown-item,
.switchgear-sld__context-item {
  display: flex;
  width: 100%;
  align-items: center;
  justify-content: space-between;
  padding: 0.5rem 0.75rem;
  border: 0;
  background: transparent;
  color: var(--color-neutral-700);
  font: inherit;
  font-size: var(--text-sm);
  text-align: left;
  transition: background-color 0.15s ease;
}

.switchgear-sld__dropdown-item:hover,
.switchgear-sld__context-item:hover {
  background: var(--color-neutral-100);
}

.switchgear-sld__toolbar-spacer {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  margin-left: auto;
  min-width: 0;
}

.switchgear-sld__metrics {
  display: inline-flex;
  align-items: center;
  gap: 0.375rem;
  padding-left: 0.25rem;
}

.switchgear-sld__metric {
  display: inline-flex;
  min-height: 1.75rem;
  align-items: center;
  gap: 0.375rem;
  padding: 0.25rem 0.5rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 82%, transparent);
  border-radius: 0.5rem;
  background: color-mix(in srgb, var(--color-white) 70%, transparent);
  color: var(--color-neutral-500);
  font-size: 0.6875rem;
  font-weight: 600;
}

.switchgear-sld__metric-value {
  color: var(--color-neutral-900);
  font-family: var(--font-mono);
}

.switchgear-sld__metric-label {
  text-transform: uppercase;
}

.switchgear-sld__viewport {
  position: relative;
  min-height: 460px;
  flex: 1 1 auto;
  overflow: hidden;
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 82%, transparent);
  border-radius: 0.875rem;
  background: var(--color-neutral-100);
  cursor: grab;
  box-shadow:
    inset 0 0 0 1px rgb(255 255 255 / 0.4),
    inset 0 18px 60px rgb(15 23 42 / 0.08);
}

.switchgear-sld__viewport:active {
  cursor: grabbing;
}

.switchgear-sld__viewport--select {
  cursor: crosshair;
}

.switchgear-sld__viewport--draw-line {
  cursor: crosshair;
}

.switchgear-sld__viewport--draw-line :deep(.switchgear-sld-node),
.switchgear-sld__viewport--draw-line :deep(.switchgear-sld-node__button),
.switchgear-sld__viewport--draw-line :deep(.switchgear-sld-node__label),
.switchgear-sld__viewport--draw-line :deep(.switchgear-sld-static-element),
.switchgear-sld__viewport--draw-line .switchgear-sld__edge-hitbox,
.switchgear-sld__viewport--draw-line .switchgear-sld__edge-handle {
  cursor: crosshair;
}

.switchgear-sld__viewport-overlay {
  position: absolute;
  inset: 0;
  pointer-events: none;
}

.switchgear-sld__status-strip {
  position: absolute;
  top: 0.75rem;
  left: 0.75rem;
  z-index: 10;
  display: inline-flex;
  max-width: calc(100% - 1.5rem);
  align-items: center;
  gap: 0.375rem;
  padding: 0.375rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 70%, transparent);
  border-radius: 0.75rem;
  background: color-mix(in srgb, var(--color-white) 88%, transparent);
  box-shadow: 0 12px 28px rgb(15 23 42 / 0.12);
  backdrop-filter: blur(10px);
  pointer-events: none;
}

.switchgear-sld__status-pill,
.switchgear-sld__status-selection {
  display: inline-flex;
  min-height: 1.375rem;
  align-items: center;
  border-radius: 999px;
  font-family: var(--font-mono);
  font-size: 0.6875rem;
  font-weight: 600;
  white-space: nowrap;
}

.switchgear-sld__status-pill {
  padding: 0 0.5rem;
  background: color-mix(in srgb, var(--color-neutral-100) 86%, transparent);
  color: var(--color-neutral-600);
}

.switchgear-sld__status-pill--primary {
  background: color-mix(in srgb, var(--color-blue-600) 12%, transparent);
  color: var(--color-blue-700);
}

.switchgear-sld__status-pill--muted {
  color: var(--color-neutral-400);
}

.switchgear-sld__status-selection {
  overflow: hidden;
  max-width: min(28rem, 45vw);
  padding: 0 0.25rem;
  color: var(--color-neutral-600);
  text-overflow: ellipsis;
}

.switchgear-sld__stage {
  position: absolute;
  top: 0;
  left: 0;
}

.switchgear-sld__grid {
  position: relative;
}

.switchgear-sld__edge-layer {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  overflow: visible;
}

.switchgear-sld__edge-hitbox {
  cursor: move;
}

.switchgear-sld__edge-handle {
  cursor: pointer;
}

.switchgear-sld__edge-draft {
  opacity: 0.9;
  pointer-events: none;
}

.switchgear-sld__edge-draft-handle {
  fill: color-mix(in srgb, var(--color-white) 90%, transparent);
  stroke: var(--color-blue-500);
  stroke-width: 2;
  pointer-events: none;
}

.switchgear-sld__zoom-panel,
.switchgear-sld__minimap {
  position: absolute;
  z-index: 10;
  padding: 0.5rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 80%, transparent);
  border-radius: 0.75rem;
  background: color-mix(in srgb, var(--color-white) 88%, transparent);
  box-shadow:
    0 20px 32px -12px rgb(15 23 42 / 0.24),
    inset 0 1px 0 rgb(255 255 255 / 0.72);
  backdrop-filter: blur(10px);
}

.switchgear-sld__zoom-panel {
  bottom: 0.75rem;
  left: 0.75rem;
  display: flex;
  align-items: center;
  gap: 0.5rem;
}

.switchgear-sld__zoom-label {
  min-width: 3rem;
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
  font-weight: 500;
  text-align: center;
}

.switchgear-sld__context-menu {
  min-width: 156px;
}

.switchgear-sld__context-menu--wide {
  min-width: 176px;
}

.switchgear-sld__context-item--danger {
  color: var(--color-rose-600);
}

.switchgear-sld__context-shortcut {
  color: var(--color-neutral-400);
  font-size: 0.6875rem;
  letter-spacing: 0;
  text-transform: uppercase;
}

.switchgear-sld__minimap {
  right: 0.75rem;
  bottom: 0.75rem;
  overflow: hidden;
}

.switchgear-sld__minimap-svg {
  display: block;
  cursor: crosshair;
}

:global(.dark .switchgear-sld) {
  border-color: color-mix(in srgb, var(--color-neutral-700) 70%, transparent);
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-neutral-900) 86%, transparent), color-mix(in srgb, var(--color-neutral-950) 82%, transparent));
  box-shadow:
    0 18px 42px rgb(0 0 0 / 0.28),
    inset 0 1px 0 rgb(255 255 255 / 0.06);
}

:global(.dark .switchgear-sld__toolbar) {
  border-color: color-mix(in srgb, var(--color-neutral-700) 76%, transparent);
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-neutral-800) 72%, transparent), color-mix(in srgb, var(--color-neutral-900) 86%, transparent));
  box-shadow:
    0 14px 28px rgb(0 0 0 / 0.22),
    inset 0 1px 0 rgb(255 255 255 / 0.06);
}

:global(.dark .switchgear-sld__tool-group),
:global(.dark .switchgear-sld__tool-button--standalone),
:global(.dark .switchgear-sld__tool-button--split),
:global(.dark .switchgear-sld__kind-button--split),
:global(.dark .switchgear-sld__dropdown),
:global(.dark .switchgear-sld__context-menu) {
  border-color: var(--color-neutral-700);
}

:global(.dark .switchgear-sld__tool-group),
:global(.dark .switchgear-sld__tool-button--standalone) {
  background: color-mix(in srgb, var(--color-neutral-800) 76%, transparent);
  box-shadow: inset 0 1px 0 rgb(255 255 255 / 0.05);
}

:global(.dark .switchgear-sld__tool-button),
:global(.dark .switchgear-sld__kind-button) {
  background: transparent;
  color: var(--color-neutral-400);
}

:global(.dark .switchgear-sld__tool-button:hover),
:global(.dark .switchgear-sld__kind-button:hover) {
  background: var(--color-neutral-700);
}

:global(.dark .switchgear-sld__tool-button--active),
:global(.dark .switchgear-sld__kind-button--active),
:global(.dark .switchgear-sld__tool-button--active:hover),
:global(.dark .switchgear-sld__kind-button--active:hover) {
  background: var(--color-blue-600);
  color: var(--color-white);
  box-shadow:
    inset 0 1px 0 rgb(255 255 255 / 0.16),
    0 0 0 1px color-mix(in srgb, var(--color-blue-400) 28%, transparent);
}

:global(.dark .switchgear-sld__metric) {
  border-color: color-mix(in srgb, var(--color-neutral-700) 72%, transparent);
  background: color-mix(in srgb, var(--color-neutral-900) 70%, transparent);
  color: var(--color-neutral-500);
}

:global(.dark .switchgear-sld__metric-value) {
  color: var(--color-neutral-100);
}

:global(.dark .switchgear-sld__dropdown),
:global(.dark .switchgear-sld__context-menu) {
  background: var(--color-neutral-900);
}

:global(.dark .switchgear-sld__dropdown-item),
:global(.dark .switchgear-sld__context-item) {
  color: var(--color-neutral-200);
}

:global(.dark .switchgear-sld__dropdown-item:hover),
:global(.dark .switchgear-sld__context-item:hover) {
  background: var(--color-neutral-800);
}

:global(.dark .switchgear-sld__viewport) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-950);
  box-shadow:
    inset 0 0 0 1px rgb(255 255 255 / 0.03),
    inset 0 22px 80px rgb(0 0 0 / 0.32);
}

:global(.dark .switchgear-sld__status-strip) {
  border-color: color-mix(in srgb, var(--color-neutral-700) 74%, transparent);
  background: color-mix(in srgb, var(--color-neutral-950) 84%, transparent);
  box-shadow: 0 16px 32px rgb(0 0 0 / 0.3);
}

:global(.dark .switchgear-sld__status-pill) {
  background: color-mix(in srgb, var(--color-neutral-800) 78%, transparent);
  color: var(--color-neutral-300);
}

:global(.dark .switchgear-sld__status-pill--primary) {
  background: color-mix(in srgb, var(--color-blue-500) 18%, transparent);
  color: var(--color-blue-200);
}

:global(.dark .switchgear-sld__status-pill--muted) {
  color: var(--color-neutral-500);
}

:global(.dark .switchgear-sld__status-selection) {
  color: var(--color-neutral-300);
}

:global(.dark .switchgear-sld__zoom-panel),
:global(.dark .switchgear-sld__minimap) {
  border-color: color-mix(in srgb, var(--color-neutral-700) 80%, transparent);
  background: color-mix(in srgb, var(--color-neutral-900) 85%, transparent);
  box-shadow:
    0 20px 36px -12px rgb(0 0 0 / 0.42),
    inset 0 1px 0 rgb(255 255 255 / 0.05);
}

:global(.dark .switchgear-sld__zoom-label) {
  color: var(--color-neutral-300);
}

:global(.dark .switchgear-sld__context-item--danger) {
  color: var(--color-rose-400);
}

:global(.dark .switchgear-sld__context-shortcut) {
  color: var(--color-neutral-500);
}
</style>
