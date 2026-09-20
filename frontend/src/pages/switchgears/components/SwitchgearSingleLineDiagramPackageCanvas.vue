<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue"
import { useRouter } from "vue-router"
import { screenToWorld, zoomViewportAt, zoomViewportCentered } from "@affino/diagram-core"
import { getSvgEntityProps, useDiagramEngine, useDiagramPointerController, useDiagramSelection, useDiagramTextEditor, useDiagramViewport, useDiagramVisibleEntities } from "@affino/diagram-vue"
import type { DiagramEdge } from "@affino/diagram-core"

import SwitchgearControlToolbar from "./SwitchgearControlToolbar.vue"
import SwitchgearSldPackageToolbar from "./SwitchgearSldPackageToolbar.vue"
import SwitchgearSldSelectionPanel from "./SwitchgearSldSelectionPanel.vue"
import SldToolbarButton from "./SwitchgearSldToolbarButton.vue"
import SwitchgearSldObjectBrowser from "./SwitchgearSldObjectBrowser.vue"
import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import { useToastStore } from "@/stores/toastStore"
import { useSelectionStore } from "@/stores/selectionStore"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { localSettingsKeys, readLocalSetting, writeLocalSetting } from "@/services/localSettingsStorage"

import type { SwitchgearType } from "@/types/switchgear"
import type { DiagramStaticKind, DiagramStaticSize, StoredDiagramState } from "../utils/switchgearSldDiagramTypes"
import type { SwitchgearSldPackageSceneModel } from "../utils/switchgearSldPackageScene"
import { serializeSwitchgearSldPackageScene } from "../utils/switchgearSldPackageScene"

const GRID_STEP = 24
const COPY_PASTE_OFFSET = GRID_STEP
const NUDGE_FINE_STEP = 1
const NUDGE_LARGE_STEP = GRID_STEP * 4
const DIAGRAM_CLIPBOARD_KIND = "unitlab.switchgear-sld-selection"
const DEFAULT_TEXT_LABEL = "TEXT"
const EDGE_PORT_SNAP_RADIUS = 18
const ORTHOGONAL_SNAP_TOLERANCE_DEG = 5
const PERSIST_DEBOUNCE_MS = 500
const MINIMAP_WIDTH = 180
const MINIMAP_HEIGHT = 124
const TEXT_HIT_TOLERANCE_PX = 10
const LABEL_MIN_OFFSET = -220
const LABEL_MAX_OFFSET = 220
const STATIC_DIMENSIONS: Record<DiagramStaticKind, Record<DiagramStaticSize, { width: number; height: number }>> = {
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

type PackageTool = "select" | "pan" | "line"
type SldWorkMode = "edit" | "operate"
type EdgeStyle = "line" | "arrow"
type EdgeWeight = "normal" | "bold"
type DraftEndpoint = {
  point: { x: number; y: number }
  portId: string | null
}
type DraftLine = {
  start: DraftEndpoint
  current: DraftEndpoint
}
type EdgeDragState = {
  pointerId: number
  edgeId: string
  endpoint: "source" | "target"
  draft: DraftEndpoint
}
type EdgeMoveState = {
  pointerId: number
  edgeIds: string[]
  startPoint: { x: number; y: number }
  delta: { x: number; y: number }
}
type LabelDragState = {
  pointerId: number
  nodeId: string
  originX: number
  originY: number
  currentX: number
  currentY: number
  startX: number
  startY: number
}
type ContextMenuState = {
  x: number
  y: number
  kind: "edge" | "static" | "text" | "node"
  nodeId?: string
}
type DiagramClipboardEdge = {
  x1: number
  y1: number
  x2: number
  y2: number
  kind: EdgeStyle
  weight?: EdgeWeight
}
type DiagramClipboardStaticElement = {
  kind: DiagramStaticKind
  size: DiagramStaticSize
  x: number
  y: number
  rotation: 0 | 90 | 180 | 270
}
type DiagramClipboardTextElement = {
  text: string
  x: number
  y: number
}
type DiagramClipboardSelection = {
  edges: DiagramClipboardEdge[]
  staticElements: DiagramClipboardStaticElement[]
  textElements: DiagramClipboardTextElement[]
}
type BrowserObject = {
  id: string
  label: string
  kind: "line" | "symbol" | "text"
  selected: boolean
}
type MinimapDragState = {
  pointerId: number
  offsetX: number
  offsetY: number
}
type SelectionDragSnapState = {
  pointerId: number
  ids: string[]
  originCenter: { x: number; y: number }
  delta: { x: number; y: number }
}

const props = defineProps<{
  model: SwitchgearSldPackageSceneModel
  workspaceId: number
  storageKey: string
  initialStoredState: StoredDiagramState | null
  fitRequestKey?: number
  selectionRequestKey?: number
  requestedSelectionIds?: string[]
  persistDocument?: (state: StoredDiagramState) => Promise<unknown>
}>()

const emit = defineEmits<{
  (event: "editSwitchgearBindings", id: number): void
}>()

const router = useRouter()
const selectionStore = useSelectionStore()
const switchgearStore = useSwitchgearStore()
const toastStore = useToastStore()
const stageRef = ref<HTMLElement | null>(null)
const svgRef = ref<SVGSVGElement | null>(null)
const textEditorRef = ref<HTMLTextAreaElement | null>(null)
const editableText = ref("")
const lastStoredState = ref<StoredDiagramState | null>(props.initialStoredState)
const draftLine = ref<DraftLine | null>(null)
const draggedEdge = ref<EdgeDragState | null>(null)
const movedEdges = ref<EdgeMoveState | null>(null)
const labelDrag = ref<LabelDragState | null>(null)
const minimapDrag = ref<MinimapDragState | null>(null)
const selectionDragSnap = ref<SelectionDragSnapState | null>(null)
const panObjectPointerId = ref<number | null>(null)
const lineKind = ref<EdgeStyle>("line")
const lineWeight = ref<EdgeWeight>("normal")
const contextMenu = ref<ContextMenuState | null>(null)
const switchgearDeleteConfirmOpen = ref(false)
const pendingSwitchgearDeleteIds = ref<number[]>([])
const switchgearDeleteBusy = ref(false)
const objectBrowserOpen = ref(false)
const localClipboardSelection = ref<DiagramClipboardSelection | null>(null)
const clipboardPasteCount = ref(0)
let persistTimer: ReturnType<typeof setTimeout> | null = null
let entityIdSequence = 0
let hasLocalStateChanges = false
let persistenceArmed = false
let pendingPersistedState: StoredDiagramState | null = null
let persistedStateRequestInFlight = false
let lastPersistedStateFingerprint = persistedStateFingerprint(props.initialStoredState)
let pendingPersistedStateFingerprint: string | null = null
let suppressNextViewportPersistence = false
const initialFitDone = ref(false)
const EDGE_HIT_TOLERANCE_PX = 8

const diagram = useDiagramEngine(props.model.scene)
const viewport = useDiagramViewport(diagram, { element: stageRef })
const visible = useDiagramVisibleEntities(diagram, { overscan: 240 })
const selection = useDiagramSelection(diagram)
const pointer = useDiagramPointerController(diagram, {
  toWorldPoint: mapPointerToWorld,
  setPointerCapture: (event) => {
    const element = event.currentTarget as Element | null
    element?.setPointerCapture?.(event.pointerId)
  },
  releasePointerCapture: (event) => {
    const element = event.currentTarget as Element | null
    element?.releasePointerCapture?.(event.pointerId)
  },
})
const textEditor = useDiagramTextEditor(diagram, { viewport: viewport.viewport })

watch(
  () => props.model.scene.nodes.map(node => `${node.id}:${String(node.metadata?.switchgearType ?? "")}`).join("|"),
  () => {
    const nextTypes = new Map(props.model.scene.nodes.map(node => [node.id, node.metadata?.switchgearType]))
    const current = diagram.engine.serialize()
    let changed = false
    const nodes = current.nodes.map(node => {
      const nextType = nextTypes.get(node.id)
      if (nextType === undefined || node.metadata?.switchgearType === nextType) {
        return node
      }
      changed = true
      return {
        ...node,
        metadata: {
          ...node.metadata,
          switchgearType: nextType,
        },
      }
    })
    if (changed) {
      diagram.engine.transact(() => ({ ...current, nodes }))
    }
  },
)

const activeTool = ref<PackageTool>("select")
const editMode = ref(readLocalSetting<SldWorkMode>(
  localSettingsKeys.switchgearDiagramMode(props.workspaceId),
  "edit",
  { validate: (value) => value === "edit" || value === "operate" ? value : null },
) === "edit")
const selectionPanelExpanded = ref(false)
const svgPointerProps = pointer.getSvgPointerProps()
const renderedViewport = computed(() => {
  const current = viewport.viewport.value
  const snapshot = pointer.state.value
  if (snapshot.tool !== "pan" || !snapshot.active || !snapshot.previewDelta) {
    return current
  }
  return {
    ...current,
    x: current.x - snapshot.previewDelta.x,
    y: current.y - snapshot.previewDelta.y,
  }
})
const gridStepWorld = computed(() => {
  const zoom = renderedViewport.value.zoom > 0 ? renderedViewport.value.zoom : 1
  return GRID_STEP / zoom
})
const gridStrokeWidthWorld = computed(() => {
  const zoom = renderedViewport.value.zoom > 0 ? renderedViewport.value.zoom : 1
  return 1 / zoom
})
const viewportBox = computed(() => {
  const value = renderedViewport.value
  return {
    x: value.x,
    y: value.y,
    width: Math.max(1, value.width),
    height: Math.max(1, value.height),
  }
})
const zoomLabel = computed(() => `${Math.round((viewport.viewport.value.zoom > 0 ? viewport.viewport.value.zoom : 1) * 100)}%`)
const snapEnabled = computed(() => lastStoredState.value?.snapEnabled !== false)
const toolbarActions = {
  setTool,
  toggleEditMode: toggleEditMode,
  setLineKind: (kind: EdgeStyle) => activeTool.value === "line" ? lineKind.value = kind : setSelectedEdgesKind(kind),
  setLineWeight: (weight: EdgeWeight) => activeTool.value === "line" ? lineWeight.value = weight : setSelectedEdgesWeight(weight),
  rotateEdges: rotateSelectedEdges90,
  addStatic,
  addLine,
  addText,
  editText: () => {
    const id = selectedTextIds.value[0]
    if (id) beginTextEdit(id)
  },
  setStaticSize: setSelectedStaticSize,
  rotateStatic: rotateSelectedStatic,
  setSwitchgearType: (type: SwitchgearType) => {
    const nodeId = selectedNodeIds.value[0]
    const switchgearId = nodeId ? resolveSwitchgearId(nodeId) : null
    if (switchgearId == null) return
    void switchgearStore.updateField(switchgearId, { switchgear_type: type }).catch((error: unknown) => {
      toastStore.error(error instanceof Error ? error.message : "Unable to update switchgear type")
    })
  },
  rotateSwitchgear: rotateSelectedSwitchgear,
  align: (edge: "left" | "top" | "right" | "bottom") => {
    if (edge === "left") alignSelectedNodesLeft()
    if (edge === "top") alignSelectedNodesTop()
    if (edge === "right") alignSelectedNodesRight()
    if (edge === "bottom") alignSelectedNodesBottom()
  },
  bringToFront: () => changeSelectionLayer("front"),
  sendToBack: () => changeSelectionLayer("back"),
  toggleSnap: toggleSnapEnabled,
  zoom: zoomBy,
  undo,
  redo,
  fit: fitScene,
  clear: clearSelection,
  duplicate: duplicateSelection,
  delete: deleteSelection,
  toggleObjectBrowser: () => { objectBrowserOpen.value = !objectBrowserOpen.value },
}
const selectionPanelActions = {
  setSwitchgearType: toolbarActions.setSwitchgearType,
  rotateSwitchgear: toolbarActions.rotateSwitchgear,
  setLineKind: setSelectedEdgesKind,
  setLineWeight: setSelectedEdgesWeight,
  rotateEdges: rotateSelectedEdges90,
  setStaticSize: setSelectedStaticSize,
  rotateStatic: rotateSelectedStatic,
  rotateSelection: rotateSelectedObjects90,
  editText: toolbarActions.editText,
  toggleExpanded: () => { selectionPanelExpanded.value = !selectionPanelExpanded.value },
}
const selectedShapeIds = computed(() => selection.selection.value.ids.filter(id => diagram.scene.value.entities.shapesById.has(id)))
const selectedEdgeIds = computed(() => selection.selection.value.ids.filter(id => diagram.scene.value.entities.edgesById.has(id)))
const selectedNodeIds = computed(() => selection.selection.value.ids.filter(id => diagram.scene.value.entities.nodesById.has(id)))
const selectedTextIds = computed(() => selection.selection.value.ids.filter(id => diagram.scene.value.entities.textsById.has(id)))
const selectionPanelKind = computed<"node" | "edge" | "static" | "text" | "common" | null>(() => {
  const selectedIds = selection.selection.value.ids
  const selectedRotatableCount = selectedNodeIds.value.length + selectedEdgeIds.value.length + selectedShapeIds.value.length
  const hasMixedRotatableSelection = selectedIds.length > 1
    && selectedRotatableCount === selectedIds.length
    && [selectedNodeIds.value.length, selectedEdgeIds.value.length, selectedShapeIds.value.length].filter(Boolean).length > 1
  if (hasMixedRotatableSelection || (selectedNodeIds.value.length > 1 && selectedRotatableCount === selectedIds.length)) return "common"
  if (selectedNodeIds.value.length === 1 && selection.selection.value.ids.length === 1) return "node"
  if (selectedEdgeIds.value.length > 0 && selectedNodeIds.value.length === 0 && selectedShapeIds.value.length === 0 && selectedTextIds.value.length === 0) return "edge"
  if (selectedShapeIds.value.length > 0 && selectedNodeIds.value.length === 0 && selectedEdgeIds.value.length === 0 && selectedTextIds.value.length === 0) return "static"
  if (selectedTextIds.value.length === 1 && selection.selection.value.ids.length === 1) return "text"
  return null
})
const selectionPanelStyle = computed(() => {
  const id = selection.selection.value.primaryId
  const geometry = id ? diagram.engine.getGeometrySnapshot(id) : null
  const current = viewport.viewport.value
  if (!geometry || current.zoom <= 0) {
    return undefined
  }
  const centerX = geometry.bounds.x + geometry.bounds.width / 2
  return {
    left: `${(centerX - current.x) * current.zoom}px`,
    top: `${(geometry.bounds.y + geometry.bounds.height - current.y) * current.zoom + 10}px`,
  }
})
const selectedStaticCount = computed(() => selectedShapeIds.value.length)
const selectedEdgeCount = computed(() => selectedEdgeIds.value.length)
const selectedNodeCount = computed(() => selectedNodeIds.value.length)
const selectedSwitchgearType = computed<SwitchgearType | null>(() => {
  const nodeId = selectedNodeIds.value[0]
  const node = nodeId ? diagram.scene.value.entities.nodesById.get(nodeId) : null
  return typeof node?.metadata?.switchgearType === "string" ? node.metadata.switchgearType as SwitchgearType : null
})
const selectedTextCount = computed(() => selectedTextIds.value.length)
const maxSwitchgearZIndex = computed(() => Math.max(0, ...[...diagram.scene.value.entities.nodesById.values()].map(node => Number(node.metadata?.zIndex) || 0)))
const edgesBelowSwitchgears = computed(() => visible.projection.value.edges.filter(edge => !isEdgeAboveSwitchgears(edge.id)))
const edgesAboveSwitchgears = computed(() => visible.projection.value.edges.filter(edge => isEdgeAboveSwitchgears(edge.id)))
const shapesBelowSwitchgears = computed(() => visible.projection.value.shapes.filter(shape => !isShapeAboveSwitchgears(shape.id)))
const shapesAboveSwitchgears = computed(() => visible.projection.value.shapes.filter(shape => isShapeAboveSwitchgears(shape.id)))
const browserObjects = computed<BrowserObject[]>(() => [
  ...diagram.scene.value.order.edgeIds.map((id, index) => ({
    id,
    label: `Line ${index + 1}`,
    kind: "line" as const,
    selected: selection.isSelected(id),
  })),
  ...diagram.scene.value.order.shapeIds.map((id, index) => ({
    id,
    label: `${resolveStaticMeta(id).kind === "transformer" ? "Transformer" : "Ground"} ${index + 1}`,
    kind: "symbol" as const,
    selected: selection.isSelected(id),
  })),
  ...diagram.scene.value.order.textIds.map((id, index) => ({
    id,
    label: diagram.scene.value.entities.textsById.get(id)?.text?.trim() || `Text ${index + 1}`,
    kind: "text" as const,
    selected: selection.isSelected(id),
  })),
])
const singleSelectedSwitchgearId = computed(() => {
  if (selectedNodeIds.value.length !== 1 || selectedEdgeIds.value.length > 0 || selectedShapeIds.value.length > 0 || selectedTextIds.value.length > 0) {
    return null
  }
  const nodeId = selectedNodeIds.value[0]
  if (!nodeId) {
    return null
  }
  return resolveSwitchgearId(nodeId)
})
const singleSelectedSwitchgear = computed(() => {
  const switchgearId = singleSelectedSwitchgearId.value
  return switchgearId == null ? null : switchgearStore.getById(switchgearId)
})

watch(singleSelectedSwitchgearId, (id) => {
  if (id != null && selectionStore.lastSwitchgearId !== id) {
    selectionStore.selectSwitchgear(id)
  }
})

watch(
  () => selection.selection.value.ids.join(","),
  () => { selectionPanelExpanded.value = false },
)
const selectedStaticSize = computed<DiagramStaticSize | "mixed" | null>(() => {
  if (selectedShapeIds.value.length === 0) {
    return null
  }
  const sizes = new Set<DiagramStaticSize>()
  for (const id of selectedShapeIds.value) {
    sizes.add(inferStaticSize(id))
  }
  return sizes.size === 1 ? [...sizes][0] : "mixed"
})
const canShrinkSelectedStatic = computed(() => selectedStaticSize.value === "md" || selectedStaticSize.value === "lg")
const canGrowSelectedStatic = computed(() => selectedStaticSize.value === "sm" || selectedStaticSize.value === "md")
const selectedEdgeKind = computed<EdgeStyle | "mixed" | null>(() => {
  if (selectedEdgeIds.value.length === 0) {
    return null
  }
  const kinds = new Set<EdgeStyle>()
  for (const id of selectedEdgeIds.value) {
    kinds.add(resolveEdgeKind(id))
  }
  return kinds.size === 1 ? [...kinds][0] : "mixed"
})
const selectedEdgeWeight = computed<EdgeWeight | "mixed" | null>(() => {
  if (selectedEdgeIds.value.length === 0) {
    return null
  }
  const weights = new Set<EdgeWeight>()
  for (const id of selectedEdgeIds.value) {
    weights.add(resolveEdgeWeightValue(id))
  }
  return weights.size === 1 ? [...weights][0] : "mixed"
})
const canUndo = computed(() => {
  void diagram.scene.value.revision
  return diagram.engine.canUndo()
})
const canRedo = computed(() => {
  void diagram.scene.value.revision
  return diagram.engine.canRedo()
})
const canDelete = computed(() => diagram.engine.canDelete(selection.selection.value.ids))
const canDuplicateSelection = computed(() => selectedNodeIds.value.length === 0 && (selectedEdgeIds.value.length > 0 || selectedShapeIds.value.length > 0 || selectedTextIds.value.length > 0))
const marqueeRect = computed(() => {
  const rect = pointer.state.value.marquee
  if (!rect) {
    return null
  }
  return {
    x: rect.x,
    y: rect.y,
    width: rect.width,
    height: rect.height,
  }
})
const selectionPreviewDelta = computed(() => {
  const snapshot = pointer.state.value
  if (snapshot.tool !== "drag-selection" || !snapshot.active || !snapshot.previewDelta) {
    return null
  }
  return snapSelectionDelta(snapshot.previewDelta)
})
const canvasCursorClass = computed(() => {
  if (minimapDrag.value) return "is-minimap-dragging"
  if (movedEdges.value || draggedEdge.value || labelDrag.value) return "is-dragging"

  const snapshot = pointer.state.value
  if (snapshot.active && snapshot.tool === "pan") return "is-panning"
  if (snapshot.active && snapshot.tool === "drag-selection") return "is-dragging"
  if (activeTool.value === "pan") return "is-pan"
  if (activeTool.value === "line") return "is-crosshair"
  return "is-select"
})
const edgeContextLabel = computed(() => selectedEdgeIds.value.length > 1 ? "selected lines" : "line")
const staticContextLabel = computed(() => selectedShapeIds.value.length > 1 ? "selected symbols" : "symbol")
const textContextLabel = computed(() => selectedTextIds.value.length > 1 ? "selected text" : "text")
const switchgearDeleteMessage = computed(() => {
  const count = pendingSwitchgearDeleteIds.value.length
  return count === 1
    ? "The switchgear and its channel bindings will be deleted."
    : `${count} switchgears and their channel bindings will be deleted.`
})
const selectedEdgeHandles = computed(() => selectedEdgeIds.value.flatMap((id) => {
  const edge = diagram.scene.value.entities.edgesById.get(id)
  if (!edge) {
    return []
  }
  const source = resolveEdgeEndpointPosition(edge.source)
  const target = resolveEdgeEndpointPosition(edge.target)
  return [
    { id: `${id}:source`, edgeId: id, endpoint: "source" as const, point: source },
    { id: `${id}:target`, edgeId: id, endpoint: "target" as const, point: target },
  ]
}))
const edgePreview = computed(() => {
  const drag = draggedEdge.value
  if (!drag) {
    return null
  }
  const edge = diagram.scene.value.entities.edgesById.get(drag.edgeId)
  if (!edge) {
    return null
  }
  const source = drag.endpoint === "source" ? drag.draft.point : resolveEdgeEndpointPosition(edge.source)
  const target = drag.endpoint === "target" ? drag.draft.point : resolveEdgeEndpointPosition(edge.target)
  return {
    edgeId: drag.edgeId,
    source,
    target,
  }
})
const snapPreviewPoint = computed(() => {
  if (draftLine.value?.current.portId) {
    return draftLine.value.current.point
  }
  if (draggedEdge.value?.draft.portId) {
    return draggedEdge.value.draft.point
  }
  return null
})
const minimapModel = computed(() => {
  if (draggedEdge.value) {
    return null
  }
  const current = renderedViewport.value
  if (current.width <= 0 || current.height <= 0) {
    return null
  }

  const worldViewportX = current.x
  const worldViewportY = current.y
  const worldViewportWidth = current.width
  const worldViewportHeight = current.height

  const nodeRects = [...diagram.scene.value.entities.nodesById.values()].map(node => ({
    id: node.id,
    x: node.x,
    y: node.y,
    width: node.width,
    height: node.height,
    active: selectedNodeIds.value.includes(node.id),
  }))
  const staticRects = [...diagram.scene.value.entities.shapesById.values()].map(shape => ({
    id: shape.id,
    x: shape.x,
    y: shape.y,
    width: Number(shape.width ?? 0),
    height: Number(shape.height ?? 0),
    active: selectedShapeIds.value.includes(shape.id),
  }))
  const textRects = [...diagram.scene.value.entities.textsById.values()].map(item => ({
    id: item.id,
    x: item.x - Number(item.width ?? 0) / 2,
    y: item.y - Number(item.height ?? 0) / 2,
    width: Number(item.width ?? 0),
    height: Number(item.height ?? 0),
    active: selectedTextIds.value.includes(item.id),
  }))
  const lineSegments = [...diagram.scene.value.entities.edgesById.values()].map(edge => {
    const source = resolveEdgeEndpointPosition(edge.source)
    const target = resolveEdgeEndpointPosition(edge.target)
    return {
      id: edge.id,
      x1: source.x,
      y1: source.y,
      x2: target.x,
      y2: target.y,
      weight: resolveEdgeWeightValue(edge.id),
      active: selectedEdgeIds.value.includes(edge.id),
    }
  })

  const xs: number[] = [worldViewportX, worldViewportX + worldViewportWidth]
  const ys: number[] = [worldViewportY, worldViewportY + worldViewportHeight]
  for (const item of nodeRects) {
    xs.push(item.x, item.x + item.width)
    ys.push(item.y, item.y + item.height)
  }
  for (const item of staticRects) {
    xs.push(item.x, item.x + item.width)
    ys.push(item.y, item.y + item.height)
  }
  for (const item of textRects) {
    xs.push(item.x, item.x + item.width)
    ys.push(item.y, item.y + item.height)
  }
  for (const item of lineSegments) {
    xs.push(item.x1, item.x2)
    ys.push(item.y1, item.y2)
  }

  const contentMinX = Math.min(...xs) - 160
  const contentMinY = Math.min(...ys) - 160
  const contentMaxX = Math.max(...xs) + 160
  const contentMaxY = Math.max(...ys) + 160
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
    scale,
    offsetX,
    offsetY,
    nodes: nodeRects.map(item => ({
      ...item,
      x: offsetX + (item.x - contentMinX) * scale,
      y: offsetY + (item.y - contentMinY) * scale,
      width: Math.max(3, Number(item.width) * scale),
      height: Math.max(3, Number(item.height) * scale),
    })),
    statics: staticRects.map(item => ({
      ...item,
      x: offsetX + (item.x - contentMinX) * scale,
      y: offsetY + (item.y - contentMinY) * scale,
      width: Math.max(3, Number(item.width) * scale),
      height: Math.max(3, Number(item.height) * scale),
    })),
    texts: textRects.map(item => ({
      ...item,
      x: offsetX + (item.x - contentMinX) * scale,
      y: offsetY + (item.y - contentMinY) * scale,
      width: Math.max(5, Number(item.width) * scale),
      height: Math.max(3, Number(item.height) * scale),
    })),
    lines: lineSegments.map(item => ({
      ...item,
      x1: offsetX + (item.x1 - contentMinX) * scale,
      y1: offsetY + (item.y1 - contentMinY) * scale,
      x2: offsetX + (item.x2 - contentMinX) * scale,
      y2: offsetY + (item.y2 - contentMinY) * scale,
    })),
    viewport: {
      x: offsetX + (worldViewportX - contentMinX) * scale,
      y: offsetY + (worldViewportY - contentMinY) * scale,
      width: Math.max(12, worldViewportWidth * scale),
      height: Math.max(12, worldViewportHeight * scale),
    },
  }
})

pointer.setTool("select")

onMounted(() => {
  void nextTick(() => {
    requestAnimationFrame(() => {
      ensureSceneVisible()
      // Affino emits scene revisions while it hydrates the initial scene and viewport.
      // Those revisions describe loading, not an operator edit.
      requestAnimationFrame(() => {
        persistenceArmed = true
      })
    })
  })
})

watch(
  () => [viewport.viewport.value.width, viewport.viewport.value.height] as const,
  ([width, height]) => {
    if (width > 0 && height > 0) {
      ensureSceneVisible()
    }
  },
)

watch(() => textEditor.activeEditor.value, async (next) => {
  editableText.value = next?.text ?? ""
  if (!next) {
    return
  }
  await nextTick()
  textEditorRef.value?.focus({ preventScroll: true })
  textEditorRef.value?.select()
})

watch(() => props.initialStoredState, (next) => {
  if (!hasLocalStateChanges) {
    lastStoredState.value = next
    lastPersistedStateFingerprint = persistedStateFingerprint(next)
  }
})

watch(() => props.fitRequestKey, (next, previous) => {
  if (next == null || next === previous) {
    return
  }
  fitScene()
})

onBeforeUnmount(() => {
  flushPersistedState()
})

watch(() => props.selectionRequestKey, (next, previous) => {
  if (next == null || next === previous) {
    return
  }
  const nodeIds = (props.requestedSelectionIds ?? []).filter(id => diagram.scene.value.entities.nodesById.has(id))
  if (nodeIds.length === 0) {
    return
  }
  selection.setSelection(nodeIds, nodeIds[0] ?? null)
  centerEntityInViewport(nodeIds[0] ?? null)
}, { immediate: true })

diagram.engine.subscribe((scene) => {
  if (scene.revision === 0 || !persistenceArmed) {
    return
  }
  if (suppressNextViewportPersistence) {
    suppressNextViewportPersistence = false
    return
  }
  const nextState = serializeSwitchgearSldPackageScene(diagram.engine.serialize(), {
    workspaceId: props.workspaceId,
    snapEnabled: lastStoredState.value?.snapEnabled ?? true,
    labelOffsetById: lastStoredState.value?.labelOffsetById,
    baseState: lastStoredState.value,
  })
  hasLocalStateChanges = true
  lastStoredState.value = nextState
  schedulePersistedState(nextState)
})

function schedulePersistedState(state: StoredDiagramState) {
  const fingerprint = persistedStateFingerprint(state)
  if (fingerprint === lastPersistedStateFingerprint || fingerprint === pendingPersistedStateFingerprint) {
    return
  }
  pendingPersistedState = state
  pendingPersistedStateFingerprint = fingerprint
  if (persistTimer != null) clearTimeout(persistTimer)
  persistTimer = setTimeout(() => {
    persistTimer = null
    void persistLatestState()
  }, PERSIST_DEBOUNCE_MS)
}

function flushPersistedState() {
  if (!lastStoredState.value) return
  pendingPersistedState = lastStoredState.value
  pendingPersistedStateFingerprint = persistedStateFingerprint(lastStoredState.value)
  if (persistTimer != null) clearTimeout(persistTimer)
  persistTimer = null
  void persistLatestState()
}

async function persistLatestState() {
  if (persistedStateRequestInFlight || !props.persistDocument) {
    return
  }
  persistedStateRequestInFlight = true
  try {
    while (pendingPersistedState) {
      const state = pendingPersistedState
      pendingPersistedState = null
      pendingPersistedStateFingerprint = null
      try {
        await props.persistDocument(state)
        lastPersistedStateFingerprint = persistedStateFingerprint(state)
      } catch (error) {
        toastStore.error(error instanceof Error ? error.message : "Unable to save SLD")
        pendingPersistedState = null
      }
    }
  } finally {
    persistedStateRequestInFlight = false
  }
}

function persistedStateFingerprint(state: StoredDiagramState | null): string {
  return JSON.stringify({
    layoutById: state?.layoutById ?? {},
    labelOffsetById: state?.labelOffsetById ?? {},
    edges: state?.edges ?? state?.lines ?? [],
    staticElements: state?.staticElements ?? [],
    textElements: state?.textElements ?? [],
    snapEnabled: state?.snapEnabled ?? true,
    viewState: state?.viewState ?? null,
  })
}

function createEntityId(prefix: string) {
  entityIdSequence += 1
  const randomPart = typeof crypto !== "undefined" && typeof crypto.randomUUID === "function"
    ? crypto.randomUUID()
    : `${Date.now()}-${entityIdSequence}`
  return `${prefix}:${randomPart}`
}

function centerEntityInViewport(id: string | null) {
  if (!id) {
    return
  }
  void nextTick(() => {
    requestAnimationFrame(() => {
      const geometry = diagram.engine.getGeometrySnapshot(id)
      if (!geometry) {
        return
      }
      const current = viewport.viewport.value
      if (current.width <= 0 || current.height <= 0) {
        return
      }
      const isVisible = geometry.bounds.x < current.x + current.width
        && geometry.bounds.x + geometry.bounds.width > current.x
        && geometry.bounds.y < current.y + current.height
        && geometry.bounds.y + geometry.bounds.height > current.y
      if (isVisible) {
        return
      }
      suppressNextViewportPersistence = true
      viewport.setViewport({
        x: geometry.bounds.x + geometry.bounds.width / 2 - current.width / 2,
        y: geometry.bounds.y + geometry.bounds.height / 2 - current.height / 2,
      })
    })
  })
}

function setTool(tool: PackageTool) {
  activeTool.value = tool
  draftLine.value = null
  draggedEdge.value = null
  labelDrag.value = null
  selectionDragSnap.value = null
  panObjectPointerId.value = null
  closeContextMenu()
  if (tool === "line") {
    pointer.setTool("select")
    focusStage()
    return
  }
  pointer.setTool(tool)
  focusStage()
}

function toggleEditMode() {
  editMode.value = !editMode.value
  writeLocalSetting(
    localSettingsKeys.switchgearDiagramMode(props.workspaceId),
    editMode.value ? "edit" : "operate",
  )
  activeTool.value = "select"
  draftLine.value = null
  draggedEdge.value = null
  movedEdges.value = null
  labelDrag.value = null
  selectionDragSnap.value = null
  objectBrowserOpen.value = false
  pointer.setTool("select")
  selection.clearSelection()
  closeContextMenu()
  focusStage()
}

function fitScene() {
  closeContextMenu()
  const currentViewport = viewport.viewport.value
  if (currentViewport.width <= 0 || currentViewport.height <= 0) {
    requestAnimationFrame(fitScene)
    return
  }
  const scene = diagram.scene.value
  const primaryIds = [
    ...scene.order.nodeIds,
    ...scene.order.shapeIds,
    ...scene.order.textIds,
  ]
  const primaryBounds = unionEntityBounds(primaryIds)
  const edgeIds = scene.order.edgeIds.filter((id) => {
    const edge = scene.entities.edgesById.get(id)
    const metadata = edge?.metadata
    return !primaryBounds || (metadata?.startBindingValid !== false && metadata?.endBindingValid !== false)
  })
  const bounds = unionEntityBounds([...primaryIds, ...edgeIds])
  if (bounds) {
    diagram.engine.fitBounds(bounds, 96)
  } else {
    diagram.engine.fitScene(96)
  }
  focusStage()
}

function unionEntityBounds(ids: ReadonlyArray<string>) {
  const bounds = ids
    .map(id => diagram.engine.getGeometrySnapshot(id)?.bounds)
    .filter((value): value is { x: number; y: number; width: number; height: number } => Boolean(value))
  if (bounds.length === 0) {
    return null
  }
  const minX = Math.min(...bounds.map(value => value.x))
  const minY = Math.min(...bounds.map(value => value.y))
  const maxX = Math.max(...bounds.map(value => value.x + value.width))
  const maxY = Math.max(...bounds.map(value => value.y + value.height))
  return {
    x: minX,
    y: minY,
    width: Math.max(1, maxX - minX),
    height: Math.max(1, maxY - minY),
  }
}

function ensureSceneVisible() {
  if (initialFitDone.value || hasPersistedViewport()) {
    return
  }
  const currentViewport = viewport.viewport.value
  if (currentViewport.width <= 0 || currentViewport.height <= 0) {
    return
  }
  initialFitDone.value = true
  const visibleIds = new Set(diagram.engine.queryVisible(diagram.scene.value.viewport))
  const hasVisibleSwitchgear = [...visibleIds].some(id => diagram.scene.value.entities.nodesById.has(id))
  if (!hasVisibleSwitchgear) {
    fitScene()
  }
}

function hasPersistedViewport() {
  const viewState = props.initialStoredState?.viewState
  return Number.isFinite(viewState?.x) && Number.isFinite(viewState?.y) && Number.isFinite(viewState?.zoom) && Number(viewState?.zoom) > 0
}

function toggleSnapEnabled() {
  closeContextMenu()
  const nextState: StoredDiagramState = {
    ...(lastStoredState.value ?? { workspaceId: props.workspaceId }),
    snapEnabled: !snapEnabled.value,
  }
  hasLocalStateChanges = true
  lastStoredState.value = nextState
  schedulePersistedState(nextState)
  focusStage()
}

function zoomBy(delta: number) {
  closeContextMenu()
  const current = viewport.viewport.value
  const currentZoom = current.zoom > 0 ? current.zoom : 1
  const nextZoom = clampZoom(currentZoom + delta)
  viewport.setViewport(withZoomedWorldExtent(current, zoomViewportCentered(current, nextZoom), nextZoom))
  focusStage()
}

function resetZoom() {
  closeContextMenu()
  const current = viewport.viewport.value
  viewport.setViewport(withZoomedWorldExtent(current, zoomViewportCentered(current, 1), 1))
  focusStage()
}

function withZoomedWorldExtent(current: typeof viewport.viewport.value, next: typeof viewport.viewport.value, nextZoom: number) {
  const screenWidth = current.width * (current.zoom > 0 ? current.zoom : 1)
  const screenHeight = current.height * (current.zoom > 0 ? current.zoom : 1)
  return {
    ...next,
    width: Math.max(1, screenWidth / nextZoom),
    height: Math.max(1, screenHeight / nextZoom),
    zoom: nextZoom,
  }
}

function undo() {
  closeContextMenu()
  diagram.dispatch({ type: "undo" })
  focusStage()
}

function redo() {
  closeContextMenu()
  diagram.dispatch({ type: "redo" })
  focusStage()
}

function deleteSelection() {
  if (!editMode.value) {
    return
  }
  closeContextMenu()
  const switchgearIds = [...new Set(
    selectedNodeIds.value
      .map(resolveSwitchgearId)
      .filter((id): id is number => id != null),
  )]
  if (switchgearIds.length > 0) {
    pendingSwitchgearDeleteIds.value = switchgearIds
    switchgearDeleteConfirmOpen.value = true
    return
  }
  diagram.engine.dispatchKeyboardCommand("delete")
  focusStage()
}

function cancelSwitchgearDeletion() {
  if (switchgearDeleteBusy.value) {
    return
  }
  switchgearDeleteConfirmOpen.value = false
  pendingSwitchgearDeleteIds.value = []
}

async function confirmSwitchgearDeletion() {
  const ids = [...pendingSwitchgearDeleteIds.value]
  if (ids.length === 0 || switchgearDeleteBusy.value) {
    return
  }

  switchgearDeleteBusy.value = true
  try {
    await switchgearStore.removeMany(ids)
    diagram.engine.dispatchKeyboardCommand("delete")
    selection.clearSelection()
    if (selectionStore.lastSwitchgearId != null && ids.includes(selectionStore.lastSwitchgearId)) {
      selectionStore.selectSwitchgear(null)
    }
    toastStore.success(ids.length === 1 ? "Switchgear deleted" : `${ids.length} switchgears deleted`)
    switchgearDeleteConfirmOpen.value = false
    pendingSwitchgearDeleteIds.value = []
  } catch (error) {
    toastStore.error(error instanceof Error ? error.message : "Failed to delete switchgears")
  } finally {
    switchgearDeleteBusy.value = false
  }
}

function duplicateSelection() {
  closeContextMenu()
  if (!canDuplicateSelection.value) {
    if (selectedNodeIds.value.length > 0) {
      toastStore.info('Switchgear duplicate is not supported yet. Select lines, symbols, or text to duplicate.')
      return
    }
    toastStore.info('Select at least one line, symbol, or text to duplicate')
    return
  }
  diagram.dispatch({
    type: "duplicateSelection",
    offset: { x: COPY_PASTE_OFFSET, y: COPY_PASTE_OFFSET },
    historyKey: "duplicate-selection",
  })
  focusStage()
}

function clearSelection() {
  closeContextMenu()
  selection.clearSelection()
  focusStage()
}

function closeContextMenu() {
  contextMenu.value = null
}

function hitTestCanvasEntity(point: { x: number; y: number }, radius: number) {
  const candidates = (["node", "edge", "shape", "text"] as const)
    .map((kind) => diagram.engine.hitTest(point, { radius, kinds: [kind] }))
    .filter((hit): hit is NonNullable<typeof hit> => Boolean(hit))

  return candidates.sort((left, right) => {
    const getMetadata = (hit: NonNullable<typeof candidates[number]>) => {
      if (hit.kind === "node") return diagram.scene.value.entities.nodesById.get(hit.id)?.metadata
      if (hit.kind === "edge") return diagram.scene.value.entities.edgesById.get(hit.id)?.metadata
      if (hit.kind === "shape") return diagram.scene.value.entities.shapesById.get(hit.id)?.metadata
      return diagram.scene.value.entities.textsById.get(hit.id)?.metadata
    }
    const leftMetadata = getMetadata(left)
    const rightMetadata = getMetadata(right)
    const leftZIndex = typeof leftMetadata?.zIndex === "number" ? leftMetadata.zIndex : 0
    const rightZIndex = typeof rightMetadata?.zIndex === "number" ? rightMetadata.zIndex : 0
    return rightZIndex - leftZIndex || left.distance - right.distance
  })[0] ?? null
}

function openContextMenu(event: MouseEvent, kind: ContextMenuState["kind"], nodeId?: string) {
  const stage = stageRef.value
  if (!stage) {
    return
  }
  const rect = stage.getBoundingClientRect()
  contextMenu.value = {
    x: Math.max(12, Math.min(rect.width - 188, event.clientX - rect.left)),
    y: Math.max(12, Math.min(rect.height - 176, event.clientY - rect.top)),
    kind,
    nodeId,
  }
}

function handleStagePointerDownCapture(event: PointerEvent) {
  const target = event.target as HTMLElement | null
  if (target?.closest(".switchgear-sld-object-browser, .switchgear-sld-package-canvas__selected-controls, .switchgear-sld-selection-panel, .switchgear-sld-package-canvas__canvas-controls, .switchgear-sld-package-canvas__context-menu, .switchgear-sld-package-canvas__minimap, .switchgear-sld-package-canvas__edge-handle")) {
    return
  }
  if (target?.closest(".switchgear-sld-package-canvas__context-menu")) {
    return
  }
  if (contextMenu.value) {
    closeContextMenu()
  }
  focusStage()
  if (!editMode.value) {
    const hit = diagram.engine.hitTest(mapPointerToWorld(event), {
      radius: 2,
      kinds: ["node"],
    })
    if (hit?.kind === "node") {
      if (event.shiftKey || event.metaKey || event.ctrlKey) {
        diagram.dispatch({
          type: "setSelection",
          selection: { ids: [hit.id], primaryId: hit.id },
          mode: "toggle",
        })
      } else {
        selection.setSelection([hit.id], hit.id)
      }
    } else {
      selection.clearSelection()
    }
    event.preventDefault()
    event.stopPropagation()
    return
  }
  if (activeTool.value === "pan") {
    const hit = diagram.engine.hitTest(mapPointerToWorld(event), { radius: 2 })
    if (hit && hit.kind !== "port") {
      panObjectPointerId.value = event.pointerId
      pointer.setTool("select")
    }
    return
  }
  if (activeTool.value !== "select") {
    return
  }
  const zoom = viewport.viewport.value.zoom > 0 ? viewport.viewport.value.zoom : 1
  const hit = hitTestCanvasEntity(mapPointerToWorld(event), EDGE_HIT_TOLERANCE_PX / zoom)
  if (!hit || hit.kind !== "edge") {
    return
  }
  const edge = diagram.scene.value.entities.edgesById.get(hit.id)
  const selectedIds = selectedEdgeIds.value.includes(hit.id) ? selectedEdgeIds.value : [hit.id]
  const movableIds = selectedIds.filter((id) => {
    const candidate = diagram.scene.value.entities.edgesById.get(id)
    return candidate?.source.kind === "point" && candidate.target.kind === "point"
  })
  if (event.shiftKey || event.metaKey || event.ctrlKey) {
    diagram.dispatch({
      type: "setSelection",
      selection: { ids: [hit.id], primaryId: hit.id },
      mode: "toggle",
    })
  } else {
    selection.setSelection(selectedIds, hit.id)
  }
  if (movableIds.length === 0 || !edge) {
    return
  }
  movedEdges.value = {
    pointerId: event.pointerId,
    edgeIds: movableIds,
    startPoint: mapPointerToWorld(event),
    delta: { x: 0, y: 0 },
  }
  stageRef.value?.setPointerCapture?.(event.pointerId)
  event.preventDefault()
  event.stopPropagation()
}

function onStagePointerMove(event: PointerEvent) {
  const snapshot = pointer.state.value
  if (snapshot.active && snapshot.tool === "drag-selection" && snapshot.previewDelta) {
    const primaryId = selection.selection.value.ids[0]
    const bounds = primaryId ? diagram.engine.getGeometrySnapshot(primaryId)?.bounds : null
    if (bounds) {
      if (!selectionDragSnap.value) {
        selectionDragSnap.value = {
          pointerId: event.pointerId,
          ids: [...selection.selection.value.ids],
          originCenter: getBoundsCenter(bounds),
          delta: snapshot.previewDelta,
        }
      } else if (selectionDragSnap.value.pointerId === event.pointerId) {
        selectionDragSnap.value.delta = snapshot.previewDelta
      }
    }
  }
  const drag = movedEdges.value
  if (!drag || drag.pointerId !== event.pointerId) {
    return
  }
  const rawDelta = {
    x: mapPointerToWorld(event).x - drag.startPoint.x,
    y: mapPointerToWorld(event).y - drag.startPoint.y,
  }
  drag.delta = snapEdgeMoveDelta(drag.edgeIds, rawDelta)
}

function snapSelectionDelta(delta: { x: number; y: number }) {
  if (!snapEnabled.value) {
    return delta
  }
  const id = selection.selection.value.ids[0]
  const bounds = id ? diagram.engine.getGeometrySnapshot(id)?.bounds : null
  if (!bounds) {
    return delta
  }
  const currentCenter = getBoundsCenter(bounds)
  const snapped = snapWorldPoint({ x: currentCenter.x + delta.x, y: currentCenter.y + delta.y })
  return {
    x: delta.x + snapped.x - (currentCenter.x + delta.x),
    y: delta.y + snapped.y - (currentCenter.y + delta.y),
  }
}

function commitSnappedSelection(drag: SelectionDragSnapState) {
  const snapped = snapWorldPoint({
    x: drag.originCenter.x + drag.delta.x,
    y: drag.originCenter.y + drag.delta.y,
  })
  const snappedDelta = {
    x: drag.delta.x + snapped.x - (drag.originCenter.x + drag.delta.x),
    y: drag.delta.y + snapped.y - (drag.originCenter.y + drag.delta.y),
  }
  if (snappedDelta.x === drag.delta.x && snappedDelta.y === drag.delta.y) {
    return
  }
  // The diagram pointer controller has already committed the raw drag by the
  // time the stage receives pointerup. Replace that commit with the snapped
  // delta so one gesture produces one history entry.
  diagram.dispatch({ type: "undo" })
  diagram.dispatch({
    type: "moveEntities",
    ids: drag.ids,
    delta: snappedDelta,
    historyKey: "drag-selection",
  })
}

function snapEdgeMoveDelta(ids: string[], delta: { x: number; y: number }) {
  if (!snapEnabled.value) {
    return delta
  }
  const edge = ids.map(id => diagram.scene.value.entities.edgesById.get(id)).find(candidate => candidate?.source.kind === "point")
  const bounds = edge ? diagram.engine.getGeometrySnapshot(edge.id)?.bounds : null
  if (!edge || !bounds) {
    return delta
  }
  const currentCenter = getBoundsCenter(bounds)
  const snapped = snapWorldPoint({
    x: currentCenter.x + delta.x,
    y: currentCenter.y + delta.y,
  })
  return {
    x: delta.x + snapped.x - (currentCenter.x + delta.x),
    y: delta.y + snapped.y - (currentCenter.y + delta.y),
  }
}

function onStagePointerUp(event: PointerEvent) {
  const selectedIdsBeforeRelease = [...selection.selection.value.ids]
  const primaryIdBeforeRelease = selection.selection.value.primaryId
  const drag = movedEdges.value
  if (drag && drag.pointerId === event.pointerId) {
    stageRef.value?.releasePointerCapture?.(event.pointerId)
    if (drag.delta.x !== 0 || drag.delta.y !== 0) {
      moveEdgesWithHistory(drag.edgeIds, drag.delta)
    }
    movedEdges.value = null
  }
  const selectionDrag = selectionDragSnap.value
  if (selectionDrag?.pointerId === event.pointerId) {
    if (snapEnabled.value) {
      commitSnappedSelection(selectionDrag)
    }
    selectionDragSnap.value = null
  }
  if (panObjectPointerId.value === event.pointerId) {
    pointer.setTool("pan")
    panObjectPointerId.value = null
  }
  restoreSelectionAfterPointerRelease(selectedIdsBeforeRelease, primaryIdBeforeRelease)
}

function moveEdgesWithHistory(ids: string[], delta: { x: number; y: number }) {
  const edges = ids
    .map(id => diagram.scene.value.entities.edgesById.get(id))
    .filter((edge): edge is NonNullable<typeof edge> => Boolean(edge))

  for (const edge of edges) {
    if (edge.source.kind === "point") {
      diagram.dispatch({
        type: "moveEdgeEndpoint",
        id: edge.id,
        endpoint: "source",
        point: { x: edge.source.point.x + delta.x, y: edge.source.point.y + delta.y },
        historyKey: "move-edges",
      })
    }
    if (edge.target.kind === "point") {
      diagram.dispatch({
        type: "moveEdgeEndpoint",
        id: edge.id,
        endpoint: "target",
        point: { x: edge.target.point.x + delta.x, y: edge.target.point.y + delta.y },
        historyKey: "move-edges",
      })
    }
    edge.points?.forEach((point, index) => {
      diagram.dispatch({
        type: "moveEdgeWaypoint",
        id: edge.id,
        index,
        point: { x: point.x + delta.x, y: point.y + delta.y },
        historyKey: "move-edges",
      })
    })
  }
}

function onStagePointerCancel(event: PointerEvent) {
  if (movedEdges.value?.pointerId !== event.pointerId) {
    return
  }
  stageRef.value?.releasePointerCapture?.(event.pointerId)
  movedEdges.value = null
  selectionDragSnap.value = null
  if (panObjectPointerId.value === event.pointerId) {
    pointer.setTool("pan")
    panObjectPointerId.value = null
  }
}

function handleBrowserSelect(id: string, event: MouseEvent) {
  if (event.metaKey || event.ctrlKey) {
    diagram.dispatch({
      type: "setSelection",
      selection: { ids: [id], primaryId: id },
      mode: "toggle",
    })
  } else if (event.shiftKey) {
    diagram.dispatch({
      type: "setSelection",
      selection: { ids: [id], primaryId: id },
      mode: "add",
    })
  } else {
    selection.setSelection([id], id)
  }
  centerEntityInViewport(id)
  focusStage()
}

function openEdgeContextMenu(event: MouseEvent, edgeId: string) {
  event.preventDefault()
  event.stopPropagation()
  if (!selectedEdgeIds.value.includes(edgeId)) {
    selection.setSelection([edgeId], edgeId)
  }
  closeTextEditorIfNeeded()
  openContextMenu(event, "edge")
}

function openStaticContextMenu(event: MouseEvent, shapeId: string) {
  event.preventDefault()
  event.stopPropagation()
  if (!selectedShapeIds.value.includes(shapeId)) {
    selection.setSelection([shapeId], shapeId)
  }
  closeTextEditorIfNeeded()
  openContextMenu(event, "static")
}

function openTextContextMenu(event: MouseEvent, textId: string) {
  event.preventDefault()
  event.stopPropagation()
  if (!selectedTextIds.value.includes(textId)) {
    selection.setSelection([textId], textId)
  }
  closeTextEditorIfNeeded()
  openContextMenu(event, "text")
}

function openNodeContextMenu(event: MouseEvent, nodeId: string) {
  event.preventDefault()
  event.stopPropagation()
  if (!selectedNodeIds.value.includes(nodeId)) {
    selection.setSelection([nodeId], nodeId)
  }
  closeTextEditorIfNeeded()
  openContextMenu(event, "node", nodeId)
}

function openNodeDetail(nodeId: string) {
  const switchgearId = resolveSwitchgearId(nodeId)
  closeContextMenu()
  if (switchgearId == null) {
    return
  }
  void router.push({ name: "switchgears.detail", params: { id: switchgearId } })
}

function requestSwitchgearBindingsEdit(nodeId: string) {
  const switchgearId = resolveSwitchgearId(nodeId)
  closeContextMenu()
  if (switchgearId == null) {
    return
  }
  emit("editSwitchgearBindings", switchgearId)
}

function closeTextEditorIfNeeded() {
  if (textEditor.activeEditor.value) {
    textEditor.cancelTextEdit()
  }
}

function buildDiagramClipboardPayload(selection: DiagramClipboardSelection) {
  return JSON.stringify({
    kind: DIAGRAM_CLIPBOARD_KIND,
    version: 2,
    edges: selection.edges,
    staticElements: selection.staticElements,
    textElements: selection.textElements,
  }, null, 2)
}

function parseDiagramClipboardPayload(rawText: string): DiagramClipboardSelection | null {
  try {
    const parsed = JSON.parse(rawText) as {
      kind?: unknown
      version?: unknown
      edges?: unknown
      staticElements?: unknown
      textElements?: unknown
    }
    if (parsed.kind !== DIAGRAM_CLIPBOARD_KIND || (parsed.version !== 1 && parsed.version !== 2)) {
      return null
    }

    const edges = (Array.isArray(parsed.edges) ? parsed.edges : []).flatMap((value): DiagramClipboardEdge[] => {
      if (!value || typeof value !== 'object') {
        return []
      }
      const edge = value as Partial<DiagramClipboardEdge>
      const x1 = Number(edge.x1)
      const y1 = Number(edge.y1)
      const x2 = Number(edge.x2)
      const y2 = Number(edge.y2)
      if (!Number.isFinite(x1) || !Number.isFinite(y1) || !Number.isFinite(x2) || !Number.isFinite(y2)) {
        return []
      }
      return [{
        x1,
        y1,
        x2,
        y2,
        kind: edge.kind === 'arrow' ? 'arrow' : 'line',
        weight: edge.weight === 'bold' ? 'bold' : 'normal',
      }]
    })

    const staticElements = (Array.isArray(parsed.staticElements) ? parsed.staticElements : []).flatMap((value): DiagramClipboardStaticElement[] => {
      if (!value || typeof value !== 'object') {
        return []
      }
      const item = value as Partial<DiagramClipboardStaticElement>
      const x = Number(item.x)
      const y = Number(item.y)
      if (!Number.isFinite(x) || !Number.isFinite(y)) {
        return []
      }
      return [{
        kind: item.kind === 'ground' ? 'ground' : 'transformer',
        size: item.size === 'sm' || item.size === 'lg' ? item.size : 'md',
        x,
        y,
        rotation: normalizeRotation(item.rotation),
      }]
    })

    const textElements = (Array.isArray(parsed.textElements) ? parsed.textElements : []).flatMap((value): DiagramClipboardTextElement[] => {
      if (!value || typeof value !== 'object') {
        return []
      }
      const item = value as Partial<DiagramClipboardTextElement>
      const x = Number(item.x)
      const y = Number(item.y)
      if (!Number.isFinite(x) || !Number.isFinite(y)) {
        return []
      }
      return [{
        text: typeof item.text === 'string' && item.text.trim() ? item.text.trim().slice(0, 80) : DEFAULT_TEXT_LABEL,
        x,
        y,
      }]
    })

    return edges.length > 0 || staticElements.length > 0 || textElements.length > 0
      ? { edges, staticElements, textElements }
      : null
  } catch {
    return null
  }
}

function formatClipboardSelectionLabel(selection: DiagramClipboardSelection) {
  const parts = [
    selection.edges.length > 0 ? `${selection.edges.length} line${selection.edges.length > 1 ? 's' : ''}` : null,
    selection.staticElements.length > 0 ? `${selection.staticElements.length} symbol${selection.staticElements.length > 1 ? 's' : ''}` : null,
    selection.textElements.length > 0 ? `${selection.textElements.length} text` : null,
  ].filter((value): value is string => Boolean(value))
  return parts.join(', ')
}

async function handleCopySelection() {
  const selectionData: DiagramClipboardSelection = {
    edges: selectedEdgeIds.value.flatMap((id) => {
      const edge = diagram.scene.value.entities.edgesById.get(id)
      if (!edge) {
        return []
      }
      const source = resolveEdgeEndpointPosition(edge.source)
      const target = resolveEdgeEndpointPosition(edge.target)
      return [{
        x1: source.x,
        y1: source.y,
        x2: target.x,
        y2: target.y,
        kind: resolveEdgeKind(id),
        weight: resolveEdgeWeightValue(id),
      }]
    }),
    staticElements: selectedShapeIds.value.flatMap((id) => {
      const shape = diagram.scene.value.entities.shapesById.get(id)
      if (!shape) {
        return []
      }
      return [{
        kind: resolveStaticMeta(id).kind,
        size: inferStaticSize(id),
        x: shape.x + shape.width / 2,
        y: shape.y + shape.height / 2,
        rotation: normalizeRotation(shape.rotation),
      }]
    }),
    textElements: selectedTextIds.value.flatMap((id) => {
      const item = diagram.scene.value.entities.textsById.get(id)
      if (!item) {
        return []
      }
      return [{
        text: item.text,
        x: item.x,
        y: item.y,
      }]
    }),
  }

  if (selectionData.edges.length === 0 && selectionData.staticElements.length === 0 && selectionData.textElements.length === 0) {
    if (selectedNodeIds.value.length > 0) {
      toastStore.info('Switchgear copy is not supported yet. Select lines, symbols, or text to copy.')
      return
    }
    toastStore.info('Select at least one line, symbol, or text to copy')
    return
  }

  localClipboardSelection.value = selectionData
  clipboardPasteCount.value = 0
  try {
    if (typeof navigator !== 'undefined' && navigator.clipboard?.writeText) {
      await navigator.clipboard.writeText(buildDiagramClipboardPayload(selectionData))
    }
    toastStore.success(`Copied ${formatClipboardSelectionLabel(selectionData)}`)
  } catch {
    toastStore.success(`Copied ${formatClipboardSelectionLabel(selectionData)}`)
  }
}

async function resolveClipboardSelection() {
  try {
    if (typeof navigator !== 'undefined' && navigator.clipboard?.readText) {
      const rawText = await navigator.clipboard.readText()
      const parsed = parseDiagramClipboardPayload(rawText)
      if (parsed) {
        localClipboardSelection.value = parsed
        return parsed
      }
    }
  } catch {
  }
  return localClipboardSelection.value
}

async function handlePasteSelection() {
  const source = await resolveClipboardSelection()
  if (!source || (source.edges.length === 0 && source.staticElements.length === 0 && source.textElements.length === 0)) {
    toastStore.info('Nothing to paste')
    return
  }
  const offset = COPY_PASTE_OFFSET * (clipboardPasteCount.value + 1)
  const edgeEntries = source.edges.map((edge, index) => ({
    id: createEntityId(`edge-paste-${index}`),
    kind: 'edge' as const,
    source: { kind: 'point' as const, point: snapWorldPoint({ x: edge.x1 + offset, y: edge.y1 + offset }) },
    target: { kind: 'point' as const, point: snapWorldPoint({ x: edge.x2 + offset, y: edge.y2 + offset }) },
    metadata: {
      entityType: 'edge',
      edgeKind: edge.kind,
      edgeWeight: edge.weight === 'bold' ? 'bold' : 'normal',
      startBinding: null,
      endBinding: null,
    },
  }))
  const shapeEntries = source.staticElements.map((item, index) => {
    const dims = STATIC_DIMENSIONS[item.kind][item.size]
    const center = snapWorldPoint({ x: item.x + offset, y: item.y + offset })
    return {
      id: createEntityId(`shape-paste-${index}`),
      kind: 'shape' as const,
      x: Math.round(center.x - dims.width / 2),
      y: Math.round(center.y - dims.height / 2),
      width: dims.width,
      height: dims.height,
      rotation: normalizeRotation(item.rotation),
      shape: item.kind,
      metadata: {
        entityType: 'static',
        staticId: createEntityId(`static-paste-${item.kind}-${index}`),
        staticKind: item.kind,
        staticSize: item.size,
        rotation: normalizeRotation(item.rotation),
      },
    }
  })
  const textEntries = source.textElements.map((item, index) => {
    const point = snapWorldPoint({ x: item.x + offset, y: item.y + offset })
    return {
      id: createEntityId(`text-paste-${index}`),
      kind: 'text' as const,
      x: point.x,
      y: point.y,
      text: item.text,
      width: Math.max(96, Math.min(288, item.text.length * 8 + 24)),
      height: 28,
      fontSize: 12,
      metadata: { entityType: 'text' },
    }
  })
  const selectionIds = [...edgeEntries.map(item => item.id), ...shapeEntries.map(item => item.id), ...textEntries.map(item => item.id)]
  diagram.dispatch({
    type: 'pasteClipboard',
    clipboard: {
      nodes: [],
      edges: edgeEntries,
      shapes: shapeEntries,
      texts: textEntries,
      ports: [],
      selection: { ids: selectionIds, primaryId: selectionIds[0] ?? null },
      viewport: diagram.scene.value.viewport,
    },
    offset: { x: 0, y: 0 },
    historyKey: 'paste-selection',
  })
  clipboardPasteCount.value += 1
  toastStore.success(`Pasted ${formatClipboardSelectionLabel(source)}`)
  focusStage()
}

function addText() {
  const center = getViewportCenter()
  const id = createEntityId("text")
  diagram.dispatch({
    type: "pasteClipboard",
    clipboard: {
      nodes: [],
      edges: [],
      texts: [{
        id,
        kind: "text",
        x: center.x - 48,
        y: center.y - 14,
        text: DEFAULT_TEXT_LABEL,
        width: 96,
        height: 28,
        fontSize: 12,
        metadata: { entityType: "text" },
      }],
      shapes: [],
      ports: [],
      selection: { ids: [id], primaryId: id },
      viewport: diagram.scene.value.viewport,
    },
    offset: { x: 0, y: 0 },
    historyKey: "add-text",
  })
  editableText.value = DEFAULT_TEXT_LABEL
  beginTextEdit(id)
  focusStage()
}

function addStatic(kind: DiagramStaticKind) {
  const center = getViewportCenter()
  const dims = STATIC_DIMENSIONS[kind].md
  const snappedCenter = snapWorldPoint(center)
  const seed = createEntityId(`static-${kind}`)
  const id = createEntityId("shape")
  diagram.dispatch({
    type: "pasteClipboard",
    clipboard: {
      nodes: [],
      edges: [],
      texts: [],
      shapes: [{
        id,
        kind: "shape",
        x: snappedCenter.x - dims.width / 2,
        y: snappedCenter.y - dims.height / 2,
        width: dims.width,
        height: dims.height,
        rotation: 0,
        shape: kind,
        metadata: {
          entityType: "static",
          staticId: seed,
          staticKind: kind,
          staticSize: "md",
          rotation: 0,
        },
      }],
      ports: [],
      selection: { ids: [id], primaryId: id },
      viewport: diagram.scene.value.viewport,
    },
    offset: { x: 0, y: 0 },
    historyKey: `add-${kind}`,
  })
  focusStage()
}

function rotateSelectedStatic() {
  if (selectedShapeIds.value.length === 0) {
    return
  }
  diagram.dispatch({
    type: "rotateEntities",
    entries: selectedShapeIds.value.flatMap((id) => {
      const shape = diagram.scene.value.entities.shapesById.get(id)
      if (!shape) {
        return []
      }
      const current = Number(shape.rotation ?? 0)
      const next = (((current + 90) % 360) || 0) as 0 | 90 | 180 | 270
      return [{ id, rotation: next }]
    }),
    historyKey: "rotate-static",
  })
  focusStage()
}

function setSelectedStaticSize(size: DiagramStaticSize) {
  if (selectedShapeIds.value.length === 0) {
    return
  }
  diagram.dispatch({
    type: "resizeEntities",
    entries: selectedShapeIds.value.flatMap((id) => {
      const shape = diagram.scene.value.entities.shapesById.get(id)
      const meta = resolveStaticMeta(id)
      if (!shape) {
        return []
      }
      const dims = STATIC_DIMENSIONS[meta.kind][size]
      const centerX = shape.x + shape.width / 2
      const centerY = shape.y + shape.height / 2
      return [{
        id,
        x: Math.round(centerX - dims.width / 2),
        y: Math.round(centerY - dims.height / 2),
        width: dims.width,
        height: dims.height,
      }]
    }),
    historyKey: `resize-static-${size}`,
  })
  focusStage()
}

function updateSelectedEdges(update: (edge: DiagramEdge) => DiagramEdge) {
  if (selectedEdgeIds.value.length === 0) {
    return
  }
  const selectedIds = new Set(selectedEdgeIds.value)
  diagram.engine.transact(() => {
    const serialized = diagram.engine.serialize()
    return {
      ...serialized,
      edges: serialized.edges.map(edge => (selectedIds.has(edge.id) ? update(edge) : edge)),
    }
  })
}

function setSelectedEdgesKind(kind: EdgeStyle) {
  updateSelectedEdges((edge) => ({
    ...edge,
    metadata: {
      ...edge.metadata,
      edgeKind: kind,
    },
  }))
  focusStage()
}

function setSelectedEdgesWeight(weight: EdgeWeight) {
  updateSelectedEdges((edge) => ({
    ...edge,
    metadata: {
      ...edge.metadata,
      edgeWeight: weight,
    },
  }))
  focusStage()
}

function rotateEdges90(edgeIds: ReadonlyArray<string>, historyKey: string) {
  if (edgeIds.length === 0) {
    return false
  }
  const hasBindings = edgeIds.some((id) => {
    const edge = diagram.scene.value.entities.edgesById.get(id)
    return Boolean(edge?.metadata?.startBinding || edge?.metadata?.endBinding)
  })
  if (hasBindings) {
    toastStore.info("Unbind selected lines before rotating them")
    return false
  }
  for (const edgeId of edgeIds) {
    const edge = diagram.scene.value.entities.edgesById.get(edgeId)
    if (!edge) {
      continue
    }
    const source = resolveEdgeEndpointPosition(edge.source)
    const target = resolveEdgeEndpointPosition(edge.target)
    const centerX = (source.x + target.x) / 2
    const centerY = (source.y + target.y) / 2
    const deltaX = (target.x - source.x) / 2
    const deltaY = (target.y - source.y) / 2
    const nextSource = {
      x: Math.round(centerX + deltaY),
      y: Math.round(centerY - deltaX),
    }
    const nextTarget = {
      x: Math.round(centerX - deltaY),
      y: Math.round(centerY + deltaX),
    }
    diagram.dispatch({
      type: "moveEdgeEndpoint",
      id: edgeId,
      endpoint: "source",
      point: nextSource,
      historyKey,
    })
    diagram.dispatch({
      type: "moveEdgeEndpoint",
      id: edgeId,
      endpoint: "target",
      point: nextTarget,
      historyKey,
    })
  }
  return true
}

function rotateSelectedEdges90() {
  if (!rotateEdges90(selectedEdgeIds.value, "rotate-edges")) {
    return
  }
  focusStage()
}

function rotateSelectedObjects90() {
  const selectedIds = selection.selection.value.ids
  const nodeAndShapeEntries = [
    ...selectedNodeIds.value.map((id) => {
      const node = diagram.scene.value.entities.nodesById.get(id)
      return node ? { id, rotation: ((Number(node.rotation ?? 0) + 90) % 360 + 360) % 360 } : null
    }),
    ...selectedShapeIds.value.map((id) => {
      const shape = diagram.scene.value.entities.shapesById.get(id)
      return shape ? { id, rotation: ((Number(shape.rotation ?? 0) + 90) % 360 + 360) % 360 } : null
    }),
  ].filter((entry): entry is { id: string; rotation: number } => Boolean(entry))
  const hasEdges = selectedEdgeIds.value.length > 0
  if (hasEdges && !rotateEdges90(selectedEdgeIds.value, "rotate-selection")) {
    return
  }
  if (nodeAndShapeEntries.length > 0) {
    diagram.dispatch({
      type: "rotateEntities",
      entries: nodeAndShapeEntries,
      historyKey: "rotate-selection",
    })
  }
  if (selectedIds.length > 0) {
    focusStage()
  }
}

function alignSelectedNodesLeft() {
  if (selectedNodeIds.value.length < 2) {
    return
  }
  diagram.dispatch({
    type: "alignEntities",
    ids: selectedNodeIds.value,
    edge: "left",
    historyKey: "align-nodes-left",
  })
  focusStage()
}

function alignSelectedNodesTop() {
  if (selectedNodeIds.value.length < 2) {
    return
  }
  diagram.dispatch({
    type: "alignEntities",
    ids: selectedNodeIds.value,
    edge: "top",
    historyKey: "align-nodes-top",
  })
  focusStage()
}

function alignSelectedNodesRight() {
  if (selectedNodeIds.value.length < 2) {
    return
  }
  diagram.dispatch({
    type: "alignEntities",
    ids: selectedNodeIds.value,
    edge: "right",
    historyKey: "align-nodes-right",
  })
  focusStage()
}

function alignSelectedNodesBottom() {
  if (selectedNodeIds.value.length < 2) {
    return
  }
  diagram.dispatch({
    type: "alignEntities",
    ids: selectedNodeIds.value,
    edge: "bottom",
    historyKey: "align-nodes-bottom",
  })
  focusStage()
}

function changeSelectionLayer(direction: "front" | "back") {
  const ids = selection.selection.value.ids
  if (ids.length === 0) {
    return
  }
  diagram.dispatch({
    type: direction === "front" ? "bringToFront" : "sendToBack",
    ids,
    historyKey: `layer-${direction}`,
  })
  focusStage()
}

function rotateSelectedSwitchgear() {
  const entries = selectedNodeIds.value.map((id) => {
    const node = diagram.scene.value.entities.nodesById.get(id)
    return {
      id,
      rotation: ((Number(node?.rotation ?? 0) + 90) % 360 + 360) % 360,
    }
  })
  if (entries.length === 0) {
    return
  }
  diagram.dispatch({
    type: "rotateEntities",
    entries,
    historyKey: "rotate-switchgear",
  })
  focusStage()
}

function onWheel(event: WheelEvent) {
  closeContextMenu()
  const current = viewport.viewport.value
  if (event.ctrlKey || event.metaKey) {
    const stage = stageRef.value
    if (!stage) {
      return
    }
    const rect = stage.getBoundingClientRect()
    const nextZoom = clampZoom(current.zoom * (event.deltaY < 0 ? 1.1 : 0.9))
    viewport.setViewport(withZoomedWorldExtent(current, zoomViewportAt(current, nextZoom, {
      x: event.clientX - rect.left,
      y: event.clientY - rect.top,
    }), nextZoom))
    return
  }

  const zoom = current.zoom > 0 ? current.zoom : 1
  viewport.setViewport({
    x: current.x + event.deltaX / zoom,
    y: current.y + event.deltaY / zoom,
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
  const ids = selection.selection.value.ids
  if (ids.length === 0) {
    return false
  }
  diagram.dispatch({
    type: "moveEntities",
    ids,
    delta: { x: dx, y: dy },
    historyKey: "nudge-selection",
  })
  return true
}

function shouldIgnoreStageKeydown() {
  const active = document.activeElement as HTMLElement | null
  const tagName = active?.tagName?.toLowerCase() ?? ""
  return active?.isContentEditable || ["input", "textarea", "select"].includes(tagName)
}

function onStageKeydown(event: KeyboardEvent) {
  if (shouldIgnoreStageKeydown()) {
    return
  }
  if (!editMode.value) {
    return
  }
  if (!event.metaKey && !event.ctrlKey && !event.altKey) {
    if (event.key.toLowerCase() === "v") {
      event.preventDefault()
      setTool("select")
      return
    }
    if (event.key.toLowerCase() === "h") {
      event.preventDefault()
      setTool("pan")
      return
    }
    if (event.key.toLowerCase() === "l") {
      event.preventDefault()
      setTool("line")
      return
    }
  }
  if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "c") {
    event.preventDefault()
    if (!textEditor.activeEditor.value) {
      void handleCopySelection()
    }
    return
  }
  if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "v") {
    event.preventDefault()
    if (!textEditor.activeEditor.value) {
      void handlePasteSelection()
    }
    return
  }
  if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "d") {
    event.preventDefault()
    if (!textEditor.activeEditor.value) {
      duplicateSelection()
    }
    return
  }
  if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "z") {
    event.preventDefault()
    if (event.shiftKey) {
      redo()
      return
    }
    undo()
    return
  }
  if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "y") {
    event.preventDefault()
    redo()
    return
  }
  const nudgeDelta = keyboardNudgeDelta(event)
  if (nudgeDelta && !textEditor.activeEditor.value) {
    if (nudgeSelection(nudgeDelta.dx, nudgeDelta.dy)) {
      event.preventDefault()
    }
    return
  }

  if (event.key === "Enter" && selectedTextCount.value === 1 && !textEditor.activeEditor.value) {
    event.preventDefault()
    const textId = selectedTextIds.value[0]
    if (textId) {
      beginTextEdit(textId)
    }
    return
  }
  if (event.key === "Delete" || event.key === "Backspace") {
    event.preventDefault()
    deleteSelection()
    return
  }
  if (event.key === "Escape") {
    event.preventDefault()
    if (contextMenu.value) {
      closeContextMenu()
      return
    }
    if (textEditor.activeEditor.value) {
      textEditor.cancelTextEdit()
      return
    }
    if (draggedEdge.value || labelDrag.value) {
      draftLine.value = null
      draggedEdge.value = null
      labelDrag.value = null
      return
    }
    if (activeTool.value === "line") {
      setTool("select")
      return
    }
    draftLine.value = null
    clearSelection()
  }
}

function focusStage() {
  stageRef.value?.focus({ preventScroll: true })
}

function beginTextEdit(id: string) {
  closeContextMenu()
  if (!diagram.engine.canEditText(id)) {
    return
  }
  textEditor.beginTextEdit(id)
}

function commitTextEdit() {
  closeContextMenu()
  textEditor.commitTextEdit(editableText.value)
  focusStage()
}

function cancelTextEdit() {
  closeContextMenu()
  textEditor.cancelTextEdit()
  focusStage()
}

function onSvgClick(event: MouseEvent) {
  closeContextMenu()
  if (activeTool.value !== "line") {
    return
  }
  const pointerPoint = mapPointerToWorld(event as unknown as PointerEvent)
  const endpoint = !draftLine.value
    ? snapDraftEndpoint(pointerPoint)
    : snapDraftEndpoint(resolveConstrainedLinePoint(draftLine.value.start.point, pointerPoint, event.shiftKey))
  if (!draftLine.value) {
    draftLine.value = { start: endpoint, current: endpoint }
    return
  }
  createLine(draftLine.value.start, endpoint)
  draftLine.value = null
}

function onSvgDoubleClick(event: MouseEvent) {
  if (!editMode.value || activeTool.value !== "select") {
    return
  }
  const zoom = viewport.viewport.value.zoom > 0 ? viewport.viewport.value.zoom : 1
  const hit = diagram.engine.hitTest(mapPointerToWorld(event as unknown as PointerEvent), {
    radius: TEXT_HIT_TOLERANCE_PX / zoom,
    kinds: ["text"],
  })
  if (hit?.kind === "text") {
    event.preventDefault()
    beginTextEdit(hit.id)
  }
}

function onSvgPointerMove(event: PointerEvent) {
  if (activeTool.value === "line" && draftLine.value) {
    draftLine.value = {
      ...draftLine.value,
      current: snapDraftEndpoint(resolveConstrainedLinePoint(draftLine.value.start.point, mapPointerToWorld(event), event.shiftKey)),
    }
  }

  const drag = draggedEdge.value
  if (!drag || drag.pointerId !== event.pointerId) {
    return
  }
  const edge = diagram.scene.value.entities.edgesById.get(drag.edgeId)
  const anchor = edge
    ? resolveEdgeEndpointPosition(drag.endpoint === "source" ? edge.target : edge.source)
    : null
  const nextPoint = anchor
    ? resolveConstrainedLinePoint(anchor, mapPointerToWorld(event), event.shiftKey)
    : mapPointerToWorld(event)
  draggedEdge.value = {
    ...drag,
    draft: snapDraftEndpoint(nextPoint),
  }
}

function onSvgPointerUp(event: PointerEvent) {
  const drag = draggedEdge.value
  if (!drag || drag.pointerId !== event.pointerId) {
    return
  }
  const selectedIdsBeforeRelease = [...selection.selection.value.ids]
  const primaryIdBeforeRelease = selection.selection.value.primaryId
  updateEdgeEndpoint(drag.edgeId, drag.endpoint, drag.draft)
  draggedEdge.value = null
  restoreSelectionAfterPointerRelease(selectedIdsBeforeRelease, primaryIdBeforeRelease)
}

function cancelPointerInteraction(event: PointerEvent) {
  const target = event.currentTarget as Element | null
  target?.releasePointerCapture?.(event.pointerId)
  if (draggedEdge.value?.pointerId === event.pointerId) {
    draggedEdge.value = null
  }
  if (labelDrag.value?.pointerId === event.pointerId) {
    labelDrag.value = null
  }
  if (minimapDrag.value?.pointerId === event.pointerId) {
    minimapDrag.value = null
  }
  if (movedEdges.value?.pointerId === event.pointerId) {
    movedEdges.value = null
  }
  if (selectionDragSnap.value?.pointerId === event.pointerId) {
    selectionDragSnap.value = null
  }
  if (panObjectPointerId.value === event.pointerId) {
    pointer.setTool("pan")
    panObjectPointerId.value = null
  }
  if (activeTool.value === "line") {
    draftLine.value = null
  }
}

function startEdgeEndpointDrag(event: PointerEvent, edgeId: string, endpoint: "source" | "target") {
  event.stopPropagation()
  event.preventDefault()
  const target = event.currentTarget as Element | null
  target?.setPointerCapture?.(event.pointerId)
  draggedEdge.value = {
    pointerId: event.pointerId,
    edgeId,
    endpoint,
    draft: snapDraftEndpoint(mapPointerToWorld(event)),
  }
}

function finishEdgeEndpointDrag(event: PointerEvent) {
  const target = event.currentTarget as Element | null
  target?.releasePointerCapture?.(event.pointerId)
  onSvgPointerUp(event)
}

function beginLabelDrag(event: PointerEvent, nodeId: string) {
  if (activeTool.value !== "select") {
    return
  }
  const node = diagram.scene.value.entities.nodesById.get(nodeId)
  if (!node) {
    return
  }
  if (event.shiftKey || event.metaKey || event.ctrlKey) {
    event.preventDefault()
    event.stopPropagation()
    diagram.dispatch({
      type: "setSelection",
      selection: { ids: [nodeId], primaryId: nodeId },
      mode: "toggle",
    })
    return
  }
  event.stopPropagation()
  selection.setSelection([nodeId], nodeId)
  const target = event.currentTarget as Element | null
  target?.setPointerCapture?.(event.pointerId)
  const pointerWorld = mapPointerToWorld(event)
  labelDrag.value = {
    pointerId: event.pointerId,
    nodeId,
    originX: Number(node.metadata?.labelOffsetX ?? 0),
    originY: Number(node.metadata?.labelOffsetY ?? 22),
    currentX: Number(node.metadata?.labelOffsetX ?? 0),
    currentY: Number(node.metadata?.labelOffsetY ?? 22),
    startX: pointerWorld.x,
    startY: pointerWorld.y,
  }
}

function onLabelPointerMove(event: PointerEvent) {
  const drag = labelDrag.value
  if (!drag || drag.pointerId !== event.pointerId) {
    return
  }
  const pointerWorld = mapPointerToWorld(event)
  labelDrag.value = {
    ...drag,
    currentX: clampLabelOffset(drag.originX + pointerWorld.x - drag.startX),
    currentY: clampLabelOffset(drag.originY + pointerWorld.y - drag.startY),
  }
}

function finishLabelDrag(event: PointerEvent) {
  const drag = labelDrag.value
  const target = event.currentTarget as Element | null
  target?.releasePointerCapture?.(event.pointerId)
  if (!drag || drag.pointerId !== event.pointerId) {
    return
  }
  const selectedIdsBeforeRelease = [...selection.selection.value.ids]
  const primaryIdBeforeRelease = selection.selection.value.primaryId
  diagram.engine.transact(() => {
    const serialized = diagram.engine.serialize()
    return {
      ...serialized,
      nodes: serialized.nodes.map((node) => node.id === drag.nodeId
        ? {
            ...node,
            metadata: {
              ...node.metadata,
              labelOffsetX: clampLabelOffset(drag.currentX),
              labelOffsetY: clampLabelOffset(drag.currentY),
            },
          }
        : node),
    }
  })
  labelDrag.value = null
  restoreSelectionAfterPointerRelease(selectedIdsBeforeRelease, primaryIdBeforeRelease)
}

function restoreSelectionAfterPointerRelease(ids: string[], primaryId: string | null) {
  if (ids.length === 0) {
    return
  }
  requestAnimationFrame(() => {
    const validIds = ids.filter((id) => (
      diagram.scene.value.entities.nodesById.has(id)
      || diagram.scene.value.entities.edgesById.has(id)
      || diagram.scene.value.entities.shapesById.has(id)
      || diagram.scene.value.entities.textsById.has(id)
    ))
    if (validIds.length === 0) {
      return
    }
    const validPrimaryId = primaryId && validIds.includes(primaryId) ? primaryId : validIds[0]
    selection.setSelection(validIds, validPrimaryId)
  })
}

function createLine(start: DraftEndpoint, end: DraftEndpoint) {
  if (Math.hypot(end.point.x - start.point.x, end.point.y - start.point.y) < 1) {
    toastStore.info("Line needs two different points")
    return null
  }
  const seed = createEntityId("edge")
  diagram.dispatch({
    type: "createEdge",
    edge: {
      id: seed,
      kind: "edge",
      source: toEdgeEndpoint(start),
      target: toEdgeEndpoint(end),
      metadata: {
        entityType: "edge",
        edgeKind: lineKind.value,
        edgeWeight: lineWeight.value,
      },
    },
    historyKey: "create-edge",
  })
  selection.setSelection([seed], seed)
  return seed
}

function addLine() {
  const center = getViewportCenter()
  const start = snapDraftEndpoint({ x: center.x - 60, y: center.y })
  const end = snapDraftEndpoint({ x: center.x + 60, y: center.y })
  createLine(start, end)
  focusStage()
}

function updateEdgeEndpoint(edgeId: string, endpoint: "source" | "target", draft: DraftEndpoint) {
  const edge = diagram.scene.value.entities.edgesById.get(edgeId)
  const point = draft.portId
    ? diagram.scene.value.entities.portsById.get(draft.portId)
    : null
  const nextPoint = point ? { x: point.x, y: point.y } : draft.point
  diagram.dispatch({
    type: "moveEdgeEndpoint",
    id: edgeId,
    endpoint,
    point: nextPoint,
    historyKey: "move-edge-endpoint",
  })

  const previousBinding = edge?.metadata?.[endpoint === "source" ? "startBinding" : "endBinding"]
  const nextBinding = draft.portId ? resolvePortBinding(draft.portId) : null
  if (JSON.stringify(previousBinding ?? null) === JSON.stringify(nextBinding)) {
    return
  }

  diagram.engine.transact(() => {
    const serialized = diagram.engine.serialize()
    return {
      ...serialized,
      edges: serialized.edges.map((edge) => {
        if (edge.id !== edgeId) {
          return edge
        }
        return {
          ...edge,
          [endpoint]: toEdgeEndpoint(draft),
          metadata: {
            ...edge.metadata,
            [endpoint === "source" ? "startBinding" : "endBinding"]: draft.portId
              ? resolvePortBinding(draft.portId)
              : null,
          },
        }
      }),
    }
  })
}

function toEdgeEndpoint(draft: DraftEndpoint) {
  return draft.portId
    ? { kind: "port" as const, portId: draft.portId }
    : { kind: "point" as const, point: draft.point }
}

function resolvePortBinding(portId: string) {
  const port = diagram.scene.value.entities.portsById.get(portId)
  if (port?.metadata?.ownerType !== "node") {
    return null
  }
  const ownerId = Number(port.metadata.ownerId)
  const boundPortId = typeof port.metadata.portId === "string" ? port.metadata.portId : ""
  return Number.isFinite(ownerId) && boundPortId
    ? { ownerType: "node" as const, ownerId, portId: boundPortId }
    : null
}

function snapDraftEndpoint(point: { x: number; y: number }): DraftEndpoint {
  const port = diagram.engine.nearestPort(point, EDGE_PORT_SNAP_RADIUS)
  if (!port || port.kind !== "port") {
    return { point, portId: null }
  }
  const entity = diagram.scene.value.entities.portsById.get(port.id)
  return entity
    ? { point: { x: entity.x, y: entity.y }, portId: port.id }
    : { point: snapWorldPoint(point), portId: null }
}

function resolveConstrainedLinePoint(anchor: { x: number; y: number }, point: { x: number; y: number }, constrain: boolean) {
  const constrainedPoint = constrain
    ? snapToEightDirections(anchor.x, anchor.y, point.x, point.y)
    : point
  return snapToOrthogonalLine(anchor, constrainedPoint)
}

function snapToOrthogonalLine(anchor: { x: number; y: number }, point: { x: number; y: number }) {
  if (!snapEnabled.value) {
    return point
  }
  const dx = point.x - anchor.x
  const dy = point.y - anchor.y
  if (Math.hypot(dx, dy) < 0.0001) {
    return point
  }

  const angle = Math.atan2(dy, dx)
  const quarterTurn = Math.PI / 2
  const nearestOrthogonal = Math.round(angle / quarterTurn) * quarterTurn
  const angularDistance = Math.abs(Math.atan2(
    Math.sin(angle - nearestOrthogonal),
    Math.cos(angle - nearestOrthogonal),
  ))
  if (angularDistance > ORTHOGONAL_SNAP_TOLERANCE_DEG * Math.PI / 180) {
    return point
  }

  const isHorizontal = Math.abs(Math.cos(nearestOrthogonal)) > 0.5
  return isHorizontal
    ? { x: point.x, y: anchor.y }
    : { x: anchor.x, y: point.y }
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

function applySelectionPreview(point: { x: number; y: number }, entityId?: string) {
  const delta = selectionPreviewDelta.value
  if (!delta || !entityId || !selection.isSelected(entityId)) {
    return point
  }

  return {
    x: point.x + delta.x,
    y: point.y + delta.y,
  }
}

function resolveEdgeEndpointPosition(endpoint: { kind: "point"; point: { x: number; y: number } } | { kind: "node"; nodeId: string } | { kind: "port"; portId: string }) {
  if (endpoint.kind === "point") {
    return endpoint.point
  }
  if (endpoint.kind === "port") {
    const port = diagram.scene.value.entities.portsById.get(endpoint.portId)
    return port ? applySelectionPreview({ x: port.x, y: port.y }, port.nodeId) : { x: 0, y: 0 }
  }
  const node = diagram.scene.value.entities.nodesById.get(endpoint.nodeId)
  return node ? applySelectionPreview({ x: node.x + node.width / 2, y: node.y + node.height / 2 }, node.id) : { x: 0, y: 0 }
}

function snapWorldValue(value: number) {
  const snapEnabled = lastStoredState.value?.snapEnabled !== false
  return snapEnabled ? Math.round(value / GRID_STEP) * GRID_STEP : value
}

function snapWorldPoint(point: { x: number; y: number }) {
  return {
    x: snapWorldValue(point.x),
    y: snapWorldValue(point.y),
  }
}

function normalizeRotation(value: unknown): 0 | 90 | 180 | 270 {
  const numeric = Number(value)
  if (numeric === 90 || numeric === 180 || numeric === 270) {
    return numeric
  }
  return 0
}

function clampZoom(value: number) {
  return Math.max(0.05, Math.min(2.2, value))
}

function mapPointerToWorld(event: PointerEvent) {
  const current = viewport.viewport.value
  const svg = svgRef.value
  if (svg) {
    const matrix = svg.getScreenCTM()
    if (matrix) {
      const point = new DOMPoint(event.clientX, event.clientY).matrixTransform(matrix.inverse())
      return { x: point.x, y: point.y }
    }
  }
  const stage = stageRef.value
  if (!stage || current.width <= 0 || current.height <= 0) {
    return { x: current.x, y: current.y }
  }
  const rect = stage.getBoundingClientRect()
  return screenToWorld(current, {
    x: event.clientX - rect.left,
    y: event.clientY - rect.top,
  })
}

function handleMinimapPointerDown(event: PointerEvent) {
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
  const current = viewport.viewport.value
  const insideViewport = localX >= model.viewport.x
    && localX <= model.viewport.x + model.viewport.width
    && localY >= model.viewport.y
    && localY <= model.viewport.y + model.viewport.height
  minimapDrag.value = {
    pointerId: event.pointerId,
    offsetX: insideViewport ? worldX - current.x : current.width / 2,
    offsetY: insideViewport ? worldY - current.y : current.height / 2,
  }
  target.setPointerCapture?.(event.pointerId)
  moveViewportFromMinimapPointer(event, model, rect)
  focusStage()
}

function moveViewportFromMinimapPointer(event: PointerEvent, model: NonNullable<typeof minimapModel.value>, rect: DOMRect) {
  const drag = minimapDrag.value
  if (!drag || drag.pointerId !== event.pointerId || model.scale <= 0) {
    return
  }
  const localX = event.clientX - rect.left
  const localY = event.clientY - rect.top
  const worldX = model.contentMinX + (localX - model.offsetX) / model.scale
  const worldY = model.contentMinY + (localY - model.offsetY) / model.scale
  viewport.setViewport({
    x: worldX - drag.offsetX,
    y: worldY - drag.offsetY,
  })
}

function handleMinimapPointerMove(event: PointerEvent) {
  const target = event.currentTarget as SVGElement | null
  const model = minimapModel.value
  if (!target || !model || !minimapDrag.value) {
    return
  }
  moveViewportFromMinimapPointer(event, model, target.getBoundingClientRect())
}

function handleMinimapPointerUp(event: PointerEvent) {
  const target = event.currentTarget as SVGElement | null
  if (minimapDrag.value?.pointerId !== event.pointerId) {
    return
  }
  target?.releasePointerCapture?.(event.pointerId)
  minimapDrag.value = null
  focusStage()
}

function getViewportCenter() {
  const current = viewport.viewport.value
  return {
    x: current.x + current.width / 2,
    y: current.y + current.height / 2,
  }
}

function getBoundsCenter(bounds: { x: number; y: number; width: number; height: number }) {
  return {
    x: bounds.x + bounds.width / 2,
    y: bounds.y + bounds.height / 2,
  }
}

function inferStaticSize(id: string): DiagramStaticSize {
  const shape = diagram.scene.value.entities.shapesById.get(id)
  const meta = resolveStaticMeta(id)
  if (!shape) {
    return "md"
  }
  const currentMax = Math.max(shape.width, shape.height)
  let best: { size: DiagramStaticSize; distance: number } | null = null
  for (const size of ["sm", "md", "lg"] as const) {
    const dims = STATIC_DIMENSIONS[meta.kind][size]
    const distance = Math.abs(currentMax - Math.max(dims.width, dims.height))
    if (!best || distance < best.distance) {
      best = { size, distance }
    }
  }
  return best?.size ?? "md"
}

function resolveSelectionPreviewTransform(id: string) {
  const edgeMove = movedEdges.value
  if (edgeMove?.edgeIds.includes(id)) {
    return `translate(${edgeMove.delta.x} ${edgeMove.delta.y})`
  }
  const delta = selectionPreviewDelta.value
  if (!delta || !selection.isSelected(id)) {
    return undefined
  }
  return `translate(${delta.x} ${delta.y})`
}

function resolveNodeSymbolTransform(id: string, bounds: { x: number; y: number; width: number; height: number }) {
  const transforms: string[] = []
  const preview = resolveSelectionPreviewTransform(id)
  if (preview) {
    transforms.push(preview)
  }
  const node = diagram.scene.value.entities.nodesById.get(id)
  const rotation = Number(node?.rotation ?? 0)
  if (rotation !== 0) {
    transforms.push(`rotate(${rotation} ${bounds.x + bounds.width / 2} ${bounds.y + bounds.height / 2})`)
  }
  return transforms.length > 0 ? transforms.join(" ") : undefined
}

function resolveHandlePreviewPoint(handle: { ownerId: string; point: { x: number; y: number } }) {
  const delta = selectionPreviewDelta.value
  const ownerId = handle.ownerId
  if (!delta || (ownerId !== "__selection__" && !selection.isSelected(ownerId))) {
    return handle.point
  }
  return {
    x: handle.point.x + delta.x,
    y: handle.point.y + delta.y,
  }
}

function resolveNodeFill(id: string) {
  if (isNodeOffline(id)) {
    return "var(--sld-node-fill-offline)"
  }
  const node = diagram.scene.value.entities.nodesById.get(id)
  const switchgearId = Number(node?.metadata?.switchgearId)
  const switchgear = Number.isFinite(switchgearId) ? switchgearStore.getById(switchgearId) : null
  const state = switchgear ? switchgearStore.resolveSwitchgearState(switchgear) : "UNKNOWN"
  if (state === "CLOSED") return "var(--sld-node-fill-closed)"
  if (state === "OPEN") return "var(--sld-node-fill-open)"
  if (state === "INTERMEDIATE") return "var(--sld-node-fill-intermediate)"
  return "var(--sld-node-fill-default)"
}

function resolveNodeStroke(id: string, selected: boolean) {
  if (selected) return "var(--color-blue-500)"
  if (isNodeOffline(id)) return "var(--sld-node-stroke-offline)"
  return "var(--sld-node-stroke-default)"
}

function isNodeOffline(id: string) {
  const switchgearId = Number(diagram.scene.value.entities.nodesById.get(id)?.metadata?.switchgearId)
  const switchgear = Number.isFinite(switchgearId) ? switchgearStore.getById(switchgearId) : null
  return Boolean(switchgear && !switchgearStore.isUnitOnline(switchgear))
}

function resolveNodeLabel(id: string) {
  return diagram.scene.value.entities.nodesById.get(id)?.metadata?.name as string | undefined
}

function resolveSwitchgearId(id: string) {
  const value = Number(diagram.scene.value.entities.nodesById.get(id)?.metadata?.switchgearId)
  return Number.isFinite(value) ? value : null
}

function resolveSwitchgearType(id: string): SwitchgearType {
  const value = diagram.scene.value.entities.nodesById.get(id)?.metadata?.switchgearType
  return value === "disconnector" || value === "earthing" ? value : "switchgear"
}

function resolveNodeLabelPosition(id: string) {
  const node = diagram.scene.value.entities.nodesById.get(id)
  if (!node) {
    return { x: 0, y: 0 }
  }
  const drag = labelDrag.value
  const offsetX = drag?.nodeId === id ? drag.currentX : Number(node.metadata?.labelOffsetX ?? 0)
  const offsetY = drag?.nodeId === id ? drag.currentY : Number(node.metadata?.labelOffsetY ?? 22)
  return {
    x: node.x + node.width / 2 + offsetX,
    y: node.y + node.height / 2 + offsetY,
  }
}

function resolveTextClass(id: string) {
  const text = diagram.scene.value.entities.textsById.get(id)
  return text?.metadata?.entityType === "generated-label"
    ? "switchgear-sld-package-canvas__generated-label"
    : "switchgear-sld-package-canvas__text"
}

function resolveEdgeKind(id: string): EdgeStyle {
  const edge = diagram.scene.value.entities.edgesById.get(id)
  return edge?.metadata?.edgeKind === "arrow" ? "arrow" : "line"
}

function resolveEdgeWeightValue(id: string): EdgeWeight {
  const edge = diagram.scene.value.entities.edgesById.get(id)
  return edge?.metadata?.edgeWeight === "bold" ? "bold" : "normal"
}

function resolveEdgeStroke(id: string) {
  if (isBrokenEdge(id)) {
    return "var(--color-rose-600)"
  }
  return "var(--sld-edge-stroke)"
}

function isEdgeAboveSwitchgears(id: string) {
  const edge = diagram.scene.value.entities.edgesById.get(id)
  return Number(edge?.metadata?.zIndex) > maxSwitchgearZIndex.value
}

function isShapeAboveSwitchgears(id: string) {
  const shape = diagram.scene.value.entities.shapesById.get(id)
  return Number(shape?.metadata?.zIndex) > maxSwitchgearZIndex.value
}

function isBrokenEdge(id: string) {
  const edge = diagram.scene.value.entities.edgesById.get(id)
  return edge?.metadata?.startBindingValid === false || edge?.metadata?.endBindingValid === false
}

function resolveEdgeWidth(id: string) {
  return resolveEdgeWeightValue(id) === "bold" ? 3 : 2
}

function clampLabelOffset(value: number) {
  return Math.max(LABEL_MIN_OFFSET, Math.min(LABEL_MAX_OFFSET, Math.round(value)))
}

function resolveStaticMeta(id: string): { kind: DiagramStaticKind; rotation: number } {
  const shape = diagram.scene.value.entities.shapesById.get(id)
  const staticKind: DiagramStaticKind = shape?.metadata?.staticKind === "ground" ? "ground" : "transformer"
  const rotation = Number(shape?.rotation ?? shape?.metadata?.rotation ?? 0)
  return {
    kind: staticKind,
    rotation,
  }
}

function resolveTransformerCircleRadius(id: string): number {
  const shape = diagram.scene.value.entities.shapesById.get(id)
  if (!shape) {
    return 0
  }
  return Math.min(Number(shape.width ?? 0), Number(shape.height ?? 0)) * 0.25
}

function resolveTransformerCircleOffset(id: string): number {
  return resolveTransformerCircleRadius(id) * 0.75
}
</script>

<template>
  <section class="switchgear-sld-package-canvas">
      <SwitchgearSldPackageToolbar
        :active-tool="activeTool"
        :edit-mode="editMode"
      :line-kind="lineKind"
      :line-weight="lineWeight"
      :selected-edge-count="selectedEdgeCount"
      :selected-edge-kind="selectedEdgeKind"
      :selected-edge-weight="selectedEdgeWeight"
      :selected-static-count="selectedStaticCount"
      :selected-static-size="selectedStaticSize"
      :selected-node-count="selectedNodeCount"
      :selected-switchgear-type="selectedSwitchgearType"
      :selected-text-count="selectedTextCount"
      :snap-enabled="snapEnabled"
      :zoom-label="zoomLabel"
      :can-undo="canUndo"
      :can-redo="canRedo"
      :can-delete="canDelete"
      :can-duplicate="canDuplicateSelection"
      :selection-count="selection.selection.value.ids.length"
      :object-browser-open="objectBrowserOpen"
      :actions="toolbarActions"
    />

    <div
      ref="stageRef"
      class="switchgear-sld-package-canvas__stage"
        :class="[canvasCursorClass, { 'is-operator-mode': !editMode }]"
      tabindex="0"
      @keydown="onStageKeydown"
      @pointerdown.capture="handleStagePointerDownCapture"
      @pointermove="onStagePointerMove"
      @pointerup="onStagePointerUp"
      @pointercancel="onStagePointerCancel"
      @wheel.prevent="onWheel"
    >
      <SwitchgearSldObjectBrowser
        v-if="objectBrowserOpen"
        :objects="browserObjects"
        :selection-count="selection.selection.value.ids.length"
        :can-delete="canDelete"
        @select="handleBrowserSelect"
        @delete="deleteSelection"
        @close="objectBrowserOpen = false"
      />
      <div class="switchgear-sld-package-canvas__canvas-controls switchgear-sld-package-canvas__canvas-controls--top-right" @pointerdown.stop>
        <SldToolbarButton
          size="xs"
          variant="toolbar"
          :class="{ 'switchgear-sld-package-canvas__snap-toggle--active': snapEnabled }"
          :aria-pressed="snapEnabled"
          :title="snapEnabled ? 'Disable magnetic snap' : 'Enable magnetic snap'"
          :aria-label="snapEnabled ? 'Disable magnetic snap' : 'Enable magnetic snap'"
          @click="toolbarActions.toggleSnap"
        >
          <span aria-hidden="true">🧲</span>
        </SldToolbarButton>
      </div>
      <div class="switchgear-sld-package-canvas__canvas-controls switchgear-sld-package-canvas__canvas-controls--bottom-left" @pointerdown.stop>
        <SldToolbarButton size="xs" variant="toolbar" title="Fit all objects in view" aria-label="Fit all objects" @click="toolbarActions.fit">
          <span aria-hidden="true">⛶</span>
        </SldToolbarButton>
        <SldToolbarButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__icon-action" title="Zoom out" aria-label="Zoom out" @click="toolbarActions.zoom(-0.1)">−</SldToolbarButton>
        <span class="switchgear-sld-package-canvas__zoom-label" title="Double-click to reset zoom" @dblclick.stop.prevent="resetZoom">{{ zoomLabel }}</span>
        <SldToolbarButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__icon-action" title="Zoom in" aria-label="Zoom in" @click="toolbarActions.zoom(0.1)">+</SldToolbarButton>
      </div>
      <SwitchgearSldSelectionPanel
        v-if="editMode && selectionPanelKind && !pointer.state.value.active && !movedEdges && !draggedEdge && !labelDrag"
        class="switchgear-sld-package-canvas__selection-panel-anchor"
        :style="selectionPanelStyle"
        :kind="selectionPanelKind"
        :switchgear-type="selectedSwitchgearType"
        :edge-kind="selectedEdgeKind"
        :edge-weight="selectedEdgeWeight"
        :static-size="selectedStaticSize"
        :expanded="selectionPanelExpanded"
        :actions="selectionPanelActions"
      />
      <div
        v-if="singleSelectedSwitchgear"
        class="switchgear-sld-package-canvas__selected-controls switchgear-sld-package-canvas__selected-controls--canvas-fixed"
        @pointerdown.stop
      >
        <SwitchgearControlToolbar
          :switchgear="singleSelectedSwitchgear"
          compact
          @open-settings="openNodeDetail(selectedNodeIds[0] ?? '')"
        />
      </div>

      <svg
        ref="svgRef"
        class="switchgear-sld-package-canvas__svg"
        :viewBox="`${viewportBox.x} ${viewportBox.y} ${viewportBox.width} ${viewportBox.height}`"
        v-bind="editMode && activeTool !== 'line' ? svgPointerProps : {}"
        @click="onSvgClick"
        @dblclick="onSvgDoubleClick"
        @pointermove="onSvgPointerMove"
        @pointerup="onSvgPointerUp"
        @pointercancel="cancelPointerInteraction"
        @lostpointercapture="cancelPointerInteraction"
        @contextmenu.prevent="closeContextMenu"
      >
        <defs>
          <pattern
            id="switchgear-sld-package-grid"
            :width="gridStepWorld"
            :height="gridStepWorld"
            patternUnits="userSpaceOnUse"
          >
            <path
              :d="`M ${gridStepWorld} 0 L 0 0 0 ${gridStepWorld}`"
              fill="none"
              stroke="rgb(var(--color-slate-400-rgb) / 0.18)"
              :stroke-width="gridStrokeWidthWorld"
            />
          </pattern>
          <marker id="switchgear-sld-package-arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
            <path d="M 0 0 L 10 5 L 0 10 z" fill="currentColor" pointer-events="none" />
          </marker>
        </defs>
        <rect
          :x="viewportBox.x"
          :y="viewportBox.y"
          :width="viewportBox.width"
          :height="viewportBox.height"
          fill="url(#switchgear-sld-package-grid)"
        />

        <polyline
          v-for="edge in edgesBelowSwitchgears"
          :key="edge.id"
          class="switchgear-sld-package-canvas__edge"
          v-bind="getSvgEntityProps(edge)"
          :transform="resolveSelectionPreviewTransform(edge.id)"
          fill="none"
          stroke-linecap="round"
          stroke-linejoin="round"
          :stroke="resolveEdgeStroke(edge.id)"
          :stroke-width="resolveEdgeWidth(edge.id)"
          :opacity="edgePreview?.edgeId === edge.id ? 0.2 : edge.selected ? 1 : 0.92"
          :marker-end="resolveEdgeKind(edge.id) === 'arrow' ? 'url(#switchgear-sld-package-arrow)' : undefined"
          :style="resolveEdgeKind(edge.id) === 'arrow' ? { color: resolveEdgeStroke(edge.id) } : undefined"
          @contextmenu.stop.prevent="openEdgeContextMenu($event, edge.id)"
        />

        <polyline
          v-if="edgePreview"
          :points="`${edgePreview.source.x},${edgePreview.source.y} ${edgePreview.target.x},${edgePreview.target.y}`"
          fill="none"
          stroke="var(--color-blue-500)"
          stroke-width="3"
          stroke-linecap="round"
          stroke-linejoin="round"
        />

        <polyline
          v-if="draftLine"
          :points="`${draftLine.start.point.x},${draftLine.start.point.y} ${draftLine.current.point.x},${draftLine.current.point.y}`"
          fill="none"
          stroke="var(--color-blue-500)"
          :stroke-width="lineWeight === 'bold' ? 3 : 2"
          stroke-dasharray="6 4"
          stroke-linecap="round"
          stroke-linejoin="round"
          :marker-end="lineKind === 'arrow' ? 'url(#switchgear-sld-package-arrow)' : undefined"
          style="color: var(--color-blue-500)"
        />

        <circle
          v-if="snapPreviewPoint"
          :cx="snapPreviewPoint.x"
          :cy="snapPreviewPoint.y"
          r="10"
          fill="color-mix(in srgb, var(--color-blue-400) 18%, transparent)"
          stroke="var(--color-blue-500)"
          stroke-width="2"
          stroke-dasharray="3 2"
          pointer-events="none"
        />

        <g v-for="shape in shapesBelowSwitchgears" :key="shape.id" class="switchgear-sld-package-canvas__static" :transform="resolveSelectionPreviewTransform(shape.id)">
          <g
            v-if="resolveStaticMeta(shape.id).kind === 'transformer'"
            @contextmenu.stop.prevent="openStaticContextMenu($event, shape.id)"
            :transform="`translate(${shape.geometry.bounds.x + shape.geometry.bounds.width / 2} ${shape.geometry.bounds.y + shape.geometry.bounds.height / 2}) rotate(${resolveStaticMeta(shape.id).rotation})`"
          >
            <circle
              :cx="-resolveTransformerCircleOffset(shape.id)"
              cy="0"
              :r="resolveTransformerCircleRadius(shape.id)"
              fill="none"
              stroke="var(--sld-symbol-stroke)"
              stroke-width="2"
            />
            <circle
              :cx="resolveTransformerCircleOffset(shape.id)"
              cy="0"
              :r="resolveTransformerCircleRadius(shape.id)"
              fill="none"
              stroke="var(--sld-symbol-stroke)"
              stroke-width="2"
            />
          </g>
          <g
            v-else
            @contextmenu.stop.prevent="openStaticContextMenu($event, shape.id)"
            :transform="`translate(${shape.geometry.bounds.x + shape.geometry.bounds.width / 2} ${shape.geometry.bounds.y + shape.geometry.bounds.height / 2}) rotate(${resolveStaticMeta(shape.id).rotation})`"
          >
            <line x1="0" :y1="-shape.geometry.bounds.height * 0.5" x2="0" y2="0" stroke="var(--sld-symbol-stroke)" stroke-width="2" />
            <line :x1="-shape.geometry.bounds.width * 0.4" y1="0" :x2="shape.geometry.bounds.width * 0.4" y2="0" stroke="var(--sld-symbol-stroke)" stroke-width="2" />
            <line :x1="-shape.geometry.bounds.width * 0.26" :y1="shape.geometry.bounds.height * 0.18" :x2="shape.geometry.bounds.width * 0.26" :y2="shape.geometry.bounds.height * 0.18" stroke="var(--sld-symbol-stroke)" stroke-width="2" />
            <line :x1="-shape.geometry.bounds.width * 0.14" :y1="shape.geometry.bounds.height * 0.34" :x2="shape.geometry.bounds.width * 0.14" :y2="shape.geometry.bounds.height * 0.34" stroke="var(--sld-symbol-stroke)" stroke-width="2" />
          </g>
        </g>

        <rect
          v-for="node in visible.projection.value.nodes"
          :key="node.id"
          v-bind="getSvgEntityProps(node)"
          class="switchgear-sld-package-canvas__node"
          :transform="resolveSelectionPreviewTransform(node.id)"
          rx="8"
          :fill="resolveNodeFill(node.id)"
          :stroke="resolveNodeStroke(node.id, node.selected)"
          :stroke-width="node.selected ? 2.5 : 1.5"
          @dblclick.stop="openNodeDetail(node.id)"
          @contextmenu.stop.prevent="openNodeContextMenu($event, node.id)"
        />

        <g
          v-for="node in visible.projection.value.nodes"
          :key="`${node.id}:symbol`"
          class="switchgear-sld-package-canvas__node-symbol"
          :class="`switchgear-sld-package-canvas__node-symbol--${resolveSwitchgearType(node.id)}`"
          :transform="resolveNodeSymbolTransform(node.id, node.geometry.bounds)"
          pointer-events="none"
        >
          <template v-if="resolveSwitchgearType(node.id) === 'disconnector'">
            <circle :cx="node.geometry.bounds.x + 12" :cy="node.geometry.bounds.y + node.geometry.bounds.height / 2" r="2" />
            <circle :cx="node.geometry.bounds.x + node.geometry.bounds.width - 12" :cy="node.geometry.bounds.y + node.geometry.bounds.height / 2" r="2" />
            <line :x1="node.geometry.bounds.x + 14" :y1="node.geometry.bounds.y + node.geometry.bounds.height / 2" :x2="node.geometry.bounds.x + node.geometry.bounds.width - 14" :y2="node.geometry.bounds.y + node.geometry.bounds.height / 2 - 9" />
          </template>
          <template v-else-if="resolveSwitchgearType(node.id) === 'earthing'">
            <line :x1="node.geometry.bounds.x + node.geometry.bounds.width / 2" :y1="node.geometry.bounds.y + 9" :x2="node.geometry.bounds.x + node.geometry.bounds.width / 2" :y2="node.geometry.bounds.y + 25" />
            <line :x1="node.geometry.bounds.x + 11" :y1="node.geometry.bounds.y + 26" :x2="node.geometry.bounds.x + node.geometry.bounds.width - 11" :y2="node.geometry.bounds.y + 26" />
            <line :x1="node.geometry.bounds.x + 14" :y1="node.geometry.bounds.y + 30" :x2="node.geometry.bounds.x + node.geometry.bounds.width - 14" :y2="node.geometry.bounds.y + 30" />
            <line :x1="node.geometry.bounds.x + 17" :y1="node.geometry.bounds.y + 34" :x2="node.geometry.bounds.x + node.geometry.bounds.width - 17" :y2="node.geometry.bounds.y + 34" />
          </template>
          <template v-else>
            <line :x1="node.geometry.bounds.x + 11" :y1="node.geometry.bounds.y + node.geometry.bounds.height / 2" :x2="node.geometry.bounds.x + 17" :y2="node.geometry.bounds.y + node.geometry.bounds.height / 2" />
            <rect :x="node.geometry.bounds.x + 17" :y="node.geometry.bounds.y + node.geometry.bounds.height / 2 - 5" width="6" height="10" rx="1" />
            <line :x1="node.geometry.bounds.x + 23" :y1="node.geometry.bounds.y + node.geometry.bounds.height / 2" :x2="node.geometry.bounds.x + node.geometry.bounds.width - 11" :y2="node.geometry.bounds.y + node.geometry.bounds.height / 2" />
          </template>
        </g>

        <text
          v-for="node in visible.projection.value.nodes"
          :key="`${node.id}:label`"
          :x="resolveNodeLabelPosition(node.id).x"
          :transform="resolveSelectionPreviewTransform(node.id)"
          :y="resolveNodeLabelPosition(node.id).y"
          class="switchgear-sld-package-canvas__switchgear-label"
          :class="{ 'switchgear-sld-package-canvas__switchgear-label--offline': isNodeOffline(node.id) }"
          text-anchor="middle"
          dominant-baseline="middle"
          @pointerdown="beginLabelDrag($event, node.id)"
          @pointermove="onLabelPointerMove"
          @pointerup="finishLabelDrag"
          @pointercancel="cancelPointerInteraction"
          @lostpointercapture="cancelPointerInteraction"
          @dblclick.stop="openNodeDetail(node.id)"
          @contextmenu.stop.prevent="openNodeContextMenu($event, node.id)"
        >
          {{ resolveNodeLabel(node.id) }}
        </text>

        <g v-for="shape in shapesAboveSwitchgears" :key="`${shape.id}:above-switchgears`" class="switchgear-sld-package-canvas__static" :transform="resolveSelectionPreviewTransform(shape.id)">
          <g
            v-if="resolveStaticMeta(shape.id).kind === 'transformer'"
            @contextmenu.stop.prevent="openStaticContextMenu($event, shape.id)"
            :transform="`translate(${shape.geometry.bounds.x + shape.geometry.bounds.width / 2} ${shape.geometry.bounds.y + shape.geometry.bounds.height / 2}) rotate(${resolveStaticMeta(shape.id).rotation})`"
          >
            <circle
              :cx="-resolveTransformerCircleOffset(shape.id)"
              cy="0"
              :r="resolveTransformerCircleRadius(shape.id)"
              fill="none"
              stroke="var(--sld-symbol-stroke)"
              stroke-width="2"
            />
            <circle
              :cx="resolveTransformerCircleOffset(shape.id)"
              cy="0"
              :r="resolveTransformerCircleRadius(shape.id)"
              fill="none"
              stroke="var(--sld-symbol-stroke)"
              stroke-width="2"
            />
          </g>
          <g
            v-else
            @contextmenu.stop.prevent="openStaticContextMenu($event, shape.id)"
            :transform="`translate(${shape.geometry.bounds.x + shape.geometry.bounds.width / 2} ${shape.geometry.bounds.y + shape.geometry.bounds.height / 2}) rotate(${resolveStaticMeta(shape.id).rotation})`"
          >
            <line x1="0" :y1="-shape.geometry.bounds.height * 0.5" x2="0" y2="0" stroke="var(--sld-symbol-stroke)" stroke-width="2" />
            <line :x1="-shape.geometry.bounds.width * 0.4" y1="0" :x2="shape.geometry.bounds.width * 0.4" y2="0" stroke="var(--sld-symbol-stroke)" stroke-width="2" />
            <line :x1="-shape.geometry.bounds.width * 0.26" :y1="shape.geometry.bounds.height * 0.18" :x2="shape.geometry.bounds.width * 0.26" :y2="shape.geometry.bounds.height * 0.18" stroke="var(--sld-symbol-stroke)" stroke-width="2" />
            <line :x1="-shape.geometry.bounds.width * 0.14" :y1="shape.geometry.bounds.height * 0.34" :x2="shape.geometry.bounds.width * 0.14" :y2="shape.geometry.bounds.height * 0.34" stroke="var(--sld-symbol-stroke)" stroke-width="2" />
          </g>
        </g>

        <polyline
          v-for="edge in edgesAboveSwitchgears"
          :key="`${edge.id}:above-switchgears`"
          class="switchgear-sld-package-canvas__edge"
          v-bind="getSvgEntityProps(edge)"
          :transform="resolveSelectionPreviewTransform(edge.id)"
          fill="none"
          stroke-linecap="round"
          stroke-linejoin="round"
          :stroke="resolveEdgeStroke(edge.id)"
          :stroke-width="resolveEdgeWidth(edge.id)"
          :opacity="edgePreview?.edgeId === edge.id ? 0.2 : edge.selected ? 1 : 0.92"
          :marker-end="resolveEdgeKind(edge.id) === 'arrow' ? 'url(#switchgear-sld-package-arrow)' : undefined"
          :style="resolveEdgeKind(edge.id) === 'arrow' ? { color: resolveEdgeStroke(edge.id) } : undefined"
          @contextmenu.stop.prevent="openEdgeContextMenu($event, edge.id)"
        />

        <text
          v-for="text in visible.projection.value.texts"
          :key="text.id"
          v-bind="getSvgEntityProps(text)"
          :x="text.geometry.bounds.x + text.geometry.bounds.width / 2"
          :y="text.geometry.bounds.y + text.geometry.bounds.height / 2"
          class="switchgear-sld-package-canvas__text-entity"
          :transform="resolveSelectionPreviewTransform(text.id)"
          :class="resolveTextClass(text.id)"
          text-anchor="middle"
          dominant-baseline="middle"
          @contextmenu.stop.prevent="openTextContextMenu($event, text.id)"
        >
          {{ diagram.scene.value.entities.textsById.get(text.id)?.text }}
        </text>

        <circle
          v-for="handle in selectedEdgeHandles"
          :key="handle.id"
          class="switchgear-sld-package-canvas__edge-handle"
          :cx="handle.point.x"
          :transform="resolveSelectionPreviewTransform(handle.edgeId)"
          :cy="handle.point.y"
          r="6"
          fill="var(--color-white)"
          stroke="var(--color-blue-500)"
          stroke-width="2"
          @pointerdown.capture="startEdgeEndpointDrag($event, handle.edgeId, handle.endpoint)"
          @pointerup="finishEdgeEndpointDrag"
          @pointercancel="cancelPointerInteraction"
          @lostpointercapture="cancelPointerInteraction"
        />

        <rect
          v-if="marqueeRect"
          :x="marqueeRect.x"
          :y="marqueeRect.y"
          :width="marqueeRect.width"
          :height="marqueeRect.height"
          fill="color-mix(in srgb, var(--color-blue-400) 16%, transparent)"
          stroke="var(--color-blue-500)"
          stroke-width="1.5"
          stroke-dasharray="6 4"
          pointer-events="none"
        />

        <circle
          v-for="handle in visible.projection.value.activeHandles"
          :key="handle.id"
          :cx="resolveHandlePreviewPoint(handle).x"
          :cy="resolveHandlePreviewPoint(handle).y"
          r="4"
          fill="var(--color-blue-500)"
          stroke="var(--color-white)"
          stroke-width="1.5"
        />
      </svg>

      <div
        v-if="contextMenu"
        class="switchgear-sld-package-canvas__context-menu"
        :class="{ 'switchgear-sld-package-canvas__context-menu--wide': contextMenu.kind === 'static' }"
        :style="{ left: `${contextMenu.x}px`, top: `${contextMenu.y}px` }"
        @pointerdown.stop
      >
        <template v-if="contextMenu.kind === 'edge'">
          <button type="button" class="switchgear-sld-package-canvas__context-item" @click="rotateSelectedEdges90(); closeContextMenu()">
            <span>Rotate {{ edgeContextLabel }} 90°</span>
            <span class="switchgear-sld-package-canvas__context-shortcut">R</span>
          </button>
          <button type="button" class="switchgear-sld-package-canvas__context-item" @click="duplicateSelection()">
            Duplicate {{ edgeContextLabel }}
          </button>
          <button type="button" class="switchgear-sld-package-canvas__context-item switchgear-sld-package-canvas__context-item--danger" @click="deleteSelection()">
            <span>Remove {{ edgeContextLabel }}</span>
            <span class="switchgear-sld-package-canvas__context-shortcut">Del</span>
          </button>
        </template>
        <template v-else-if="contextMenu.kind === 'static'">
          <button type="button" class="switchgear-sld-package-canvas__context-item" :disabled="!canShrinkSelectedStatic" @click="setSelectedStaticSize(selectedStaticSize === 'lg' ? 'md' : 'sm'); closeContextMenu()">
            <span>Shrink {{ staticContextLabel }}</span>
            <span class="switchgear-sld-package-canvas__context-shortcut">S</span>
          </button>
          <button type="button" class="switchgear-sld-package-canvas__context-item" :disabled="!canGrowSelectedStatic" @click="setSelectedStaticSize(selectedStaticSize === 'sm' ? 'md' : 'lg'); closeContextMenu()">
            <span>Grow {{ staticContextLabel }}</span>
            <span class="switchgear-sld-package-canvas__context-shortcut">L</span>
          </button>
          <button type="button" class="switchgear-sld-package-canvas__context-item" @click="rotateSelectedStatic(); closeContextMenu()">
            <span>Rotate {{ staticContextLabel }} 90°</span>
            <span class="switchgear-sld-package-canvas__context-shortcut">R</span>
          </button>
          <button type="button" class="switchgear-sld-package-canvas__context-item" @click="duplicateSelection()">
            Duplicate {{ staticContextLabel }}
          </button>
          <button type="button" class="switchgear-sld-package-canvas__context-item switchgear-sld-package-canvas__context-item--danger" @click="deleteSelection()">
            <span>Remove {{ staticContextLabel }}</span>
            <span class="switchgear-sld-package-canvas__context-shortcut">Del</span>
          </button>
        </template>
        <template v-else-if="contextMenu.kind === 'text'">
          <button type="button" class="switchgear-sld-package-canvas__context-item" @click="selectedTextIds[0] && beginTextEdit(selectedTextIds[0]); closeContextMenu()">
            <span>Edit text</span>
            <span class="switchgear-sld-package-canvas__context-shortcut">Enter</span>
          </button>
          <button type="button" class="switchgear-sld-package-canvas__context-item" @click="duplicateSelection()">
            Duplicate {{ textContextLabel }}
          </button>
          <button type="button" class="switchgear-sld-package-canvas__context-item switchgear-sld-package-canvas__context-item--danger" @click="deleteSelection()">
            <span>Remove {{ textContextLabel }}</span>
            <span class="switchgear-sld-package-canvas__context-shortcut">Del</span>
          </button>
        </template>
        <template v-else>
          <button v-if="selectedNodeCount > 1" type="button" class="switchgear-sld-package-canvas__context-item" @click="alignSelectedNodesLeft(); closeContextMenu()">
            Align selected left
          </button>
          <button v-if="selectedNodeCount > 1" type="button" class="switchgear-sld-package-canvas__context-item" @click="alignSelectedNodesTop(); closeContextMenu()">
            Align selected top
          </button>
          <button v-if="selectedNodeCount > 1" type="button" class="switchgear-sld-package-canvas__context-item" @click="alignSelectedNodesRight(); closeContextMenu()">
            Align selected right
          </button>
          <button v-if="selectedNodeCount > 1" type="button" class="switchgear-sld-package-canvas__context-item" @click="alignSelectedNodesBottom(); closeContextMenu()">
            Align selected bottom
          </button>
          <button type="button" class="switchgear-sld-package-canvas__context-item" @click="contextMenu.nodeId && requestSwitchgearBindingsEdit(contextMenu.nodeId)">
            Edit bindings
          </button>
          <button type="button" class="switchgear-sld-package-canvas__context-item" @click="contextMenu.nodeId && openNodeDetail(contextMenu.nodeId)">
            Open detail
          </button>
        </template>
      </div>

      <textarea
        v-if="textEditor.activeEditor.value"
        ref="textEditorRef"
        v-model="editableText"
        class="switchgear-sld-package-canvas__editor"
        :style="textEditor.activeEditor.value.style"
        @pointerdown.stop
        @keydown.enter.exact.prevent.stop="commitTextEdit"
        @keydown.esc.prevent="cancelTextEdit"
        @blur="commitTextEdit"
      />

      <div v-if="minimapModel" class="switchgear-sld-package-canvas__minimap">
        <svg
          :width="MINIMAP_WIDTH"
          :height="MINIMAP_HEIGHT"
          class="switchgear-sld-package-canvas__minimap-svg"
          @pointerdown.stop.prevent="handleMinimapPointerDown"
          @pointermove.stop.prevent="handleMinimapPointerMove"
          @pointerup.stop.prevent="handleMinimapPointerUp"
          @pointercancel.stop.prevent="handleMinimapPointerUp"
          @lostpointercapture="handleMinimapPointerUp"
        >
          <rect
            x="0"
            y="0"
            :width="MINIMAP_WIDTH"
            :height="MINIMAP_HEIGHT"
            rx="8"
            fill="rgb(var(--color-slate-400-rgb) / 0.18)"
          />
          <g>
            <line
              v-for="line in minimapModel.lines"
              :key="line.id"
              :x1="line.x1"
              :y1="line.y1"
              :x2="line.x2"
              :y2="line.y2"
              :stroke="line.active ? 'var(--color-sky-400)' : 'var(--color-slate-600)'"
              :stroke-width="line.active ? (line.weight === 'bold' ? 2.4 : 1.8) : (line.weight === 'bold' ? 1.8 : 1.2)"
              stroke-linecap="round"
              :opacity="line.active ? 1 : 0.62"
            />
            <rect
              v-for="item in minimapModel.statics"
              :key="item.id"
              :x="item.x"
              :y="item.y"
              :width="item.width"
              :height="item.height"
              rx="1.5"
              :fill="item.active ? 'var(--color-sky-400)' : 'var(--color-slate-500)'"
              :opacity="item.active ? 1 : 0.58"
            />
            <rect
              v-for="item in minimapModel.texts"
              :key="item.id"
              :x="item.x"
              :y="item.y"
              :width="item.width"
              :height="item.height"
              rx="1.5"
              :fill="item.active ? 'var(--color-sky-400)' : 'var(--color-slate-500)'"
              :opacity="item.active ? 1 : 0.5"
            />
            <rect
              v-for="item in minimapModel.nodes"
              :key="item.id"
              :x="item.x"
              :y="item.y"
              :width="item.width"
              :height="item.height"
              rx="1.5"
              :fill="item.active ? 'var(--color-sky-400)' : 'var(--color-slate-700)'"
              :opacity="item.active ? 1 : 0.65"
            />
          </g>
          <rect
            :x="minimapModel.viewport.x"
            :y="minimapModel.viewport.y"
            :width="minimapModel.viewport.width"
            :height="minimapModel.viewport.height"
            rx="2"
            fill="rgb(var(--color-sky-500-rgb) / 0.15)"
            stroke="var(--color-sky-500)"
            stroke-width="1.4"
          />
        </svg>
      </div>
    </div>
  </section>
  <ConfirmModal
    :open="switchgearDeleteConfirmOpen"
    title="Delete switchgear"
    :message="switchgearDeleteMessage"
    confirm-label="Delete"
    cancel-label="Cancel"
    @cancel="cancelSwitchgearDeletion"
    @confirm="confirmSwitchgearDeletion"
  />
</template>

<style scoped>
.switchgear-sld-package-canvas {
  display: flex;
  min-height: 0;
  flex: 1 1 auto;
  flex-direction: column;
  gap: 0.75rem;
}

.switchgear-sld-package-canvas__toolbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.75rem;
  flex-wrap: wrap;
}

.switchgear-sld-package-canvas__status,
.switchgear-sld-package-canvas__actions,
.switchgear-sld-package-canvas__tool-tabs {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  flex-wrap: wrap;
}

.switchgear-sld-package-canvas__status span,
.switchgear-sld-package-canvas__selection {
  padding: 0.25rem 0.5rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-pill);
  background: var(--color-white);
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
  font-weight: 500;
}

.switchgear-sld-package-canvas__tool-tab {
  padding: 0.45rem 0.7rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-lg);
  background: var(--color-white);
  color: var(--color-neutral-600);
  font: inherit;
  font-size: var(--text-sm);
  font-weight: 500;
}

.switchgear-sld-package-canvas__tool-tab.is-active {
  border-color: var(--color-blue-300);
  background: var(--color-blue-50);
  color: var(--color-blue-800);
}

.switchgear-sld-package-canvas__stage {
  position: relative;
  min-height: 0;
  flex: 1 1 auto;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-lg);
  background: linear-gradient(180deg, var(--color-white), color-mix(in srgb, var(--color-sky-50) 42%, var(--color-white)));
  overflow: hidden;
  outline: none;
  touch-action: none;
  user-select: none;
  --sld-symbol-stroke: var(--color-neutral-700);
  --sld-edge-stroke: var(--color-neutral-700);
  --sld-edge-stroke-arrow: var(--color-blue-700);
  --sld-node-fill-default: var(--color-white);
  --sld-node-fill-offline: var(--color-neutral-100);
  --sld-node-fill-closed: var(--color-emerald-100);
  --sld-node-fill-open: var(--color-amber-100);
  --sld-node-fill-intermediate: var(--color-orange-100);
  --sld-node-stroke-default: var(--color-blue-300);
  --sld-node-stroke-offline: var(--color-neutral-400);
}

.switchgear-sld-package-canvas__stage.is-select,
.switchgear-sld-package-canvas__stage.is-select .switchgear-sld-package-canvas__node,
.switchgear-sld-package-canvas__stage.is-select .switchgear-sld-package-canvas__static,
.switchgear-sld-package-canvas__stage.is-select .switchgear-sld-package-canvas__edge,
.switchgear-sld-package-canvas__stage.is-select .switchgear-sld-package-canvas__text-entity,
.switchgear-sld-package-canvas__stage.is-select .switchgear-sld-package-canvas__switchgear-label {
  cursor: grab;
}

.switchgear-sld-package-canvas__stage.is-select {
  cursor: default;
}

.switchgear-sld-package-canvas__stage.is-pan,
.switchgear-sld-package-canvas__stage.is-pan .switchgear-sld-package-canvas__svg {
  cursor: grab;
}

.switchgear-sld-package-canvas__stage.is-panning,
.switchgear-sld-package-canvas__stage.is-panning .switchgear-sld-package-canvas__svg,
.switchgear-sld-package-canvas__stage.is-dragging,
.switchgear-sld-package-canvas__stage.is-dragging .switchgear-sld-package-canvas__svg,
.switchgear-sld-package-canvas__stage.is-minimap-dragging,
.switchgear-sld-package-canvas__stage.is-minimap-dragging .switchgear-sld-package-canvas__minimap-svg,
.switchgear-sld-package-canvas__stage.is-select .switchgear-sld-package-canvas__node:active,
.switchgear-sld-package-canvas__stage.is-select .switchgear-sld-package-canvas__static:active,
.switchgear-sld-package-canvas__stage.is-select .switchgear-sld-package-canvas__edge:active,
.switchgear-sld-package-canvas__stage.is-select .switchgear-sld-package-canvas__text-entity:active,
.switchgear-sld-package-canvas__stage.is-select .switchgear-sld-package-canvas__switchgear-label:active {
  cursor: grabbing;
}

.switchgear-sld-package-canvas__stage.is-crosshair,
.switchgear-sld-package-canvas__stage.is-crosshair .switchgear-sld-package-canvas__svg {
  cursor: crosshair;
}

.switchgear-sld-package-canvas__edge-handle {
  cursor: crosshair;
}

.switchgear-sld-package-canvas__minimap-svg {
  cursor: pointer;
}

.switchgear-sld-package-canvas__stage.is-minimap-dragging .switchgear-sld-package-canvas__minimap-svg {
  cursor: grabbing;
}

.switchgear-sld-package-canvas__stage:focus,
.switchgear-sld-package-canvas__stage:focus-visible {
  outline: none;
  box-shadow: none;
}

.switchgear-sld-package-canvas__svg {
  touch-action: none;
  user-select: none;
}

.switchgear-sld-package-canvas__stage.is-operator-mode {
  cursor: default;
}

.switchgear-sld-package-canvas__stage.is-operator-mode .switchgear-sld-package-canvas__svg,
.switchgear-sld-package-canvas__stage.is-operator-mode .switchgear-sld-package-canvas__svg * {
  cursor: default;
}

.switchgear-sld-package-canvas__stage.is-operator-mode .switchgear-sld-package-canvas__svg .switchgear-sld-package-canvas__node {
  cursor: pointer;
}

.switchgear-sld-package-canvas__selection-panel-anchor {
  position: absolute;
  z-index: 12;
  transform: translateX(-50%);
}

.switchgear-sld-package-canvas__canvas-controls {
  position: absolute;
  z-index: 13;
  display: flex;
  align-items: center;
  gap: 0.3rem;
  padding: 0.25rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 75%, transparent);
  border-radius: var(--radius-xl);
  background: color-mix(in srgb, var(--color-white) 92%, transparent);
  box-shadow: 0 10px 24px rgb(var(--color-slate-900-rgb) / 0.14);
  backdrop-filter: blur(10px);
}

.switchgear-sld-package-canvas__canvas-controls--top-right {
  top: 0.75rem;
  right: 0.75rem;
  padding: 0;
  border: 0;
  background: transparent;
  box-shadow: none;
  backdrop-filter: none;
}

.switchgear-sld-package-canvas__canvas-controls--top-right :deep(.switchgear-sld-package-canvas__snap-toggle--active) {
  border-color: var(--color-blue-300);
  background: var(--color-blue-50);
  color: var(--color-blue-800);
}

.switchgear-sld-package-canvas__canvas-controls--bottom-left {
  bottom: 0.75rem;
  left: 0.75rem;
}

.switchgear-sld-package-canvas__selected-controls {
  position: absolute;
  top: 0.75rem;
  right: 0.75rem;
  z-index: 12;
  max-width: min(45rem, calc(100% - 1.5rem));
  overflow-x: auto;
  padding: 0.375rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 70%, transparent);
  border-radius: var(--radius-xl);
  background: color-mix(in srgb, var(--color-white) 90%, transparent);
  box-shadow: 0 14px 30px rgb(var(--color-slate-900-rgb) / 0.14);
  backdrop-filter: blur(10px);
}

.switchgear-sld-package-canvas__selected-controls--anchored {
  top: auto;
  right: auto;
  transform: translate(-50%, calc(-100% - 0.5rem));
}

.switchgear-sld-package-canvas__selected-controls--canvas-fixed {
  top: 0.75rem;
  right: 3.35rem;
  left: auto;
  transform: none;
}

.switchgear-sld-package-canvas__selected-controls :deep(.switchgear-control-toolbar__row),
.switchgear-sld-package-canvas__selected-controls :deep(.switchgear-control-toolbar__commands) {
  flex-wrap: nowrap;
}

.switchgear-sld-package-canvas__selected-controls :deep(.switchgear-control-toolbar__compact-title),
.switchgear-sld-package-canvas__selected-controls :deep(.switchgear-control-toolbar__warning),
.switchgear-sld-package-canvas__selected-controls :deep(.switchgear-control-toolbar__state) {
  white-space: nowrap;
}

.switchgear-sld-package-canvas__selected-controls :deep(.switchgear-control-toolbar__command-button--compact) {
  min-width: 78px;
}

@media (max-width: 960px) {
  .switchgear-sld-package-canvas__selected-controls:not(.switchgear-sld-package-canvas__selected-controls--anchored):not(.switchgear-sld-package-canvas__selected-controls--canvas-fixed) {
    top: 3.625rem;
    right: 0.75rem;
    left: 0.75rem;
    max-width: none;
  }
}

.switchgear-sld-package-canvas__svg {
  display: block;
  width: 100%;
  height: 100%;
}

.switchgear-sld-package-canvas__switchgear-label {
  fill: var(--color-neutral-700);
  font-size: var(--text-2xs);
  font-weight: 600;
}

.switchgear-sld-package-canvas__node-symbol {
  fill: none;
  stroke: var(--sld-symbol-stroke);
  stroke-linecap: round;
  stroke-linejoin: round;
  stroke-width: 1.8;
}

.switchgear-sld-package-canvas__switchgear-label--offline {
  fill: var(--color-neutral-500);
}

.switchgear-sld-package-canvas__generated-label,
.switchgear-sld-package-canvas__text {
  fill: var(--color-neutral-600);
  font-size: var(--text-xs);
  font-weight: 500;
}

.switchgear-sld-package-canvas__context-menu {
  position: absolute;
  z-index: 3;
  display: grid;
  min-width: 13rem;
  gap: 0.125rem;
  padding: 0.35rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 82%, transparent);
  border-radius: var(--radius-card);
  background: color-mix(in srgb, var(--color-white) 94%, transparent);
  box-shadow: var(--shadow-lg);
  backdrop-filter: blur(12px);
}

.switchgear-sld-package-canvas__context-menu--wide {
  min-width: 15rem;
}

.switchgear-sld-package-canvas__context-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.75rem;
  padding: 0.55rem 0.7rem;
  border: 0;
  border-radius: var(--radius-control);
  background: transparent;
  color: var(--color-neutral-700);
  font: inherit;
  font-size: var(--text-sm);
  text-align: left;
}

.switchgear-sld-package-canvas__context-shortcut {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.switchgear-sld-package-canvas__context-item:hover:not(:disabled) {
  background: var(--color-neutral-100);
}

.switchgear-sld-package-canvas__context-item:disabled {
  opacity: 0.45;
}

.switchgear-sld-package-canvas__context-item--danger {
  color: var(--color-rose-600);
}

.switchgear-sld-package-canvas__minimap {
  position: absolute;
  right: 1rem;
  bottom: 1rem;
  z-index: 2;
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 78%, transparent);
  border-radius: var(--radius-card);
  background: color-mix(in srgb, var(--color-white) 88%, transparent);
  box-shadow: var(--shadow-md);
  backdrop-filter: blur(8px);
}

.switchgear-sld-package-canvas__minimap-svg {
  display: block;
  cursor: pointer;
}

.switchgear-sld-package-canvas__editor {
  position: absolute;
  padding: 0.25rem 0.375rem;
  border: 1px solid var(--color-blue-400);
  border-radius: var(--radius-md);
  background: var(--color-white);
  color: var(--color-neutral-900);
  font: inherit;
  font-size: var(--text-xs);
  resize: none;
  outline: none;
  box-shadow: var(--shadow-md);
}

:global(.dark .switchgear-sld-package-canvas__status span),
:global(.dark .switchgear-sld-package-canvas__selection),
:global(.dark .switchgear-sld-package-canvas__tool-tab) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-900);
  color: var(--color-neutral-300);
}

:global(.dark .switchgear-sld-package-canvas__tool-tab.is-active) {
  border-color: var(--color-blue-500);
  background: color-mix(in srgb, var(--color-blue-900) 75%, transparent);
  color: var(--color-blue-100);
}

:global(.dark .switchgear-sld-package-canvas__selected-controls) {
  border-color: color-mix(in srgb, var(--color-neutral-700) 88%, transparent);
  background: color-mix(in srgb, var(--color-neutral-950) 82%, transparent);
}

:global(.dark .switchgear-sld-package-canvas__canvas-controls) {
  border-color: var(--color-neutral-700);
  background: color-mix(in srgb, var(--color-neutral-900) 94%, transparent);
}

:global(.dark .switchgear-sld-package-canvas__canvas-controls--top-right) {
  border: 0;
  background: transparent;
  box-shadow: none;
  backdrop-filter: none;
}

:global(.dark .switchgear-sld-package-canvas__canvas-controls--top-right) :deep(.switchgear-sld-package-canvas__snap-toggle--active) {
  border-color: var(--color-blue-500);
  background: color-mix(in srgb, var(--color-blue-900) 75%, transparent);
  color: var(--color-blue-100);
}

:global(.dark .switchgear-sld-package-canvas__stage) {
  border-color: var(--color-neutral-800);
  background: linear-gradient(180deg, rgb(var(--color-diagram-surface-rgb)), rgb(var(--color-diagram-background-rgb)));
  --sld-symbol-stroke: var(--color-neutral-200);
  --sld-edge-stroke: var(--color-neutral-200);
  --sld-edge-stroke-arrow: var(--color-blue-300);
  --sld-node-fill-default: var(--color-neutral-800);
  --sld-node-fill-offline: var(--color-neutral-900);
  --sld-node-fill-closed: color-mix(in srgb, var(--color-emerald-900) 72%, var(--color-neutral-800));
  --sld-node-fill-open: color-mix(in srgb, var(--color-amber-900) 72%, var(--color-neutral-800));
  --sld-node-fill-intermediate: color-mix(in srgb, var(--color-orange-900) 72%, var(--color-neutral-800));
  --sld-node-stroke-default: var(--color-blue-400);
  --sld-node-stroke-offline: var(--color-neutral-500);
}

:global(.dark .switchgear-sld-package-canvas__switchgear-label) {
  fill: var(--color-neutral-200);
}

:global(.dark .switchgear-sld-package-canvas__generated-label),
:global(.dark .switchgear-sld-package-canvas__text) {
  fill: var(--color-neutral-300);
}

:global(.dark .switchgear-sld-package-canvas__context-menu) {
  border-color: color-mix(in srgb, var(--color-neutral-700) 88%, transparent);
  background: color-mix(in srgb, var(--color-neutral-950) 90%, transparent);
}

:global(.dark .switchgear-sld-package-canvas__context-item) {
  color: var(--color-neutral-200);
}

:global(.dark .switchgear-sld-package-canvas__context-item:hover:not(:disabled)) {
  background: color-mix(in srgb, var(--color-neutral-800) 82%, transparent);
}

:global(.dark .switchgear-sld-package-canvas__context-item--danger) {
  color: var(--color-rose-300);
}

:global(.dark .switchgear-sld-package-canvas__snap-toggle--active) {
  border-color: var(--color-blue-500);
  background: color-mix(in srgb, var(--color-blue-900) 75%, transparent);
  color: var(--color-blue-100);
}

:global(.dark .switchgear-sld-package-canvas__minimap) {
  border-color: color-mix(in srgb, var(--color-neutral-700) 88%, transparent);
  background: color-mix(in srgb, var(--color-neutral-950) 78%, transparent);
}

:global(.dark .switchgear-sld-package-canvas__editor) {
  background: var(--color-neutral-950);
  color: var(--color-neutral-100);
}

.switchgear-sld-package-canvas__snap-toggle--active {
  border-color: var(--color-blue-300);
  background: var(--color-blue-50);
  color: var(--color-blue-800);
}

.switchgear-sld-package-canvas__icon-action {
  min-width: 2rem;
  padding-inline: 0.4rem;
}

.switchgear-sld-package-canvas__icon {
  width: 0.95rem;
  height: 0.95rem;
}

.switchgear-sld-package-canvas__zoom-label {
  min-width: 3rem;
  text-align: center;
  font-size: var(--text-xs);
  color: var(--color-neutral-600);
}

</style>
