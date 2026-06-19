<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"
import { getSvgEntityProps, useDiagramEngine, useDiagramPointerController, useDiagramSelection, useDiagramTextEditor, useDiagramViewport, useDiagramVisibleEntities } from "@affino/diagram-vue"
import type { DiagramEdge } from "@affino/diagram-core"

import UiButton from "@/components/ui/UiButton.vue"
import SwitchgearControlToolbar from "./SwitchgearControlToolbar.vue"
import { useToastStore } from "@/stores/toastStore"
import { writeLocalSetting } from "@/services/localSettingsStorage"
import { useSwitchgearStore } from "@/stores/switchgearStore"

import type { DiagramStaticKind, DiagramStaticSize, StoredDiagramState } from "../utils/switchgearSldDiagramTypes"
import type { SwitchgearSldPackageSceneModel } from "../utils/switchgearSldPackageScene"
import { buildDefaultSwitchgearSldLayout, serializeSwitchgearSldPackageScene } from "../utils/switchgearSldPackageScene"

const GRID_STEP = 24
const COPY_PASTE_OFFSET = GRID_STEP
const DIAGRAM_CLIPBOARD_KIND = "unitlab.switchgear-sld-selection"
const DEFAULT_TEXT_LABEL = "TEXT"
const EDGE_PORT_SNAP_RADIUS = 18
const MINIMAP_WIDTH = 180
const MINIMAP_HEIGHT = 124
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

const props = defineProps<{
  model: SwitchgearSldPackageSceneModel
  workspaceId: number
  storageKey: string
  initialStoredState: StoredDiagramState | null
  fitRequestKey?: number
  selectionRequestKey?: number
  requestedSelectionIds?: string[]
}>()

const emit = defineEmits<{
  (event: "editSwitchgearBindings", id: number): void
}>()

const route = useRoute()
const router = useRouter()
const switchgearStore = useSwitchgearStore()
const toastStore = useToastStore()
const stageRef = ref<HTMLElement | null>(null)
const editableText = ref("")
const lastStoredState = ref<StoredDiagramState | null>(props.initialStoredState)
const draftLine = ref<DraftLine | null>(null)
const draggedEdge = ref<EdgeDragState | null>(null)
const labelDrag = ref<LabelDragState | null>(null)
const lineKind = ref<EdgeStyle>("line")
const lineWeight = ref<EdgeWeight>("normal")
const contextMenu = ref<ContextMenuState | null>(null)
const localClipboardSelection = ref<DiagramClipboardSelection | null>(null)
const clipboardPasteCount = ref(0)

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

const activeTool = ref<PackageTool>("select")
const viewportBox = computed(() => {
  const value = viewport.viewport.value
  const zoom = value.zoom > 0 ? value.zoom : 1
  return {
    x: value.x,
    y: value.y,
    width: Math.max(1, value.width / zoom),
    height: Math.max(1, value.height / zoom),
  }
})
const zoomLabel = computed(() => `${Math.round((viewport.viewport.value.zoom > 0 ? viewport.viewport.value.zoom : 1) * 100)}%`)
const snapEnabled = computed(() => lastStoredState.value?.snapEnabled !== false)
const snapStateLabel = computed(() => snapEnabled.value ? "Snap on" : "Snap off")
const sceneCounts = computed(() => ({
  nodes: diagram.scene.value.order.nodeIds.length,
  edges: diagram.scene.value.order.edgeIds.length,
  statics: diagram.scene.value.order.shapeIds.length,
  texts: diagram.scene.value.order.textIds.length,
}))
const selectionLabel = computed(() => {
  const ids = selection.selection.value.ids
  if (ids.length === 0) {
    return "No selection"
  }
  if (ids.length === 1) {
    const id = ids[0]
    if (id.startsWith("switchgear:")) {
      return resolveNodeLabel(id) ?? id
    }
    return id
  }
  return `${ids.length} selected`
})
const selectedShapeIds = computed(() => selection.selection.value.ids.filter(id => diagram.scene.value.entities.shapesById.has(id)))
const selectedEdgeIds = computed(() => selection.selection.value.ids.filter(id => diagram.scene.value.entities.edgesById.has(id)))
const selectedNodeIds = computed(() => selection.selection.value.ids.filter(id => diagram.scene.value.entities.nodesById.has(id)))
const selectedTextIds = computed(() => selection.selection.value.ids.filter(id => diagram.scene.value.entities.textsById.has(id)))
const selectedStaticCount = computed(() => selectedShapeIds.value.length)
const selectedEdgeCount = computed(() => selectedEdgeIds.value.length)
const selectedNodeCount = computed(() => selectedNodeIds.value.length)
const selectedTextCount = computed(() => selectedTextIds.value.length)
const singleSelectedSwitchgear = computed(() => {
  if (selectedNodeIds.value.length !== 1 || selectedEdgeIds.value.length > 0 || selectedShapeIds.value.length > 0 || selectedTextIds.value.length > 0) {
    return null
  }
  const nodeId = selectedNodeIds.value[0]
  if (!nodeId) {
    return null
  }
  const switchgearId = resolveSwitchgearId(nodeId)
  return switchgearId == null ? null : switchgearStore.getById(switchgearId)
})
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
const canUndo = computed(() => diagram.engine.canUndo())
const canRedo = computed(() => diagram.engine.canRedo())
const canDelete = computed(() => diagram.engine.canDelete(selection.selection.value.ids))
const svgPointerProps = computed(() => activeTool.value === "line" ? {} : pointer.getSvgPointerProps())
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
const minimapModel = computed(() => {
  const current = viewport.viewport.value
  const zoom = current.zoom > 0 ? current.zoom : 1
  if (current.width <= 0 || current.height <= 0) {
    return null
  }

  const worldViewportX = current.x
  const worldViewportY = current.y
  const worldViewportWidth = current.width / zoom
  const worldViewportHeight = current.height / zoom

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
syncRouteSelection()

watch(() => route.params.id, () => {
  syncRouteSelection()
})

watch(() => textEditor.activeEditor.value, (next) => {
  editableText.value = next?.text ?? ""
})

watch(() => props.initialStoredState, (next) => {
  lastStoredState.value = next
})

watch(() => props.fitRequestKey, (next, previous) => {
  if (next == null || next === previous) {
    return
  }
  fitScene()
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
})

diagram.engine.subscribe((scene) => {
  if (scene.revision === 0) {
    return
  }
  const nextState = serializeSwitchgearSldPackageScene(diagram.engine.serialize(), {
    workspaceId: props.workspaceId,
    snapEnabled: lastStoredState.value?.snapEnabled ?? true,
    labelOffsetById: lastStoredState.value?.labelOffsetById,
    baseState: lastStoredState.value,
  })
  lastStoredState.value = nextState
  writeLocalSetting(props.storageKey, nextState, {
    legacyKeys: [`unitlab.switchgears.sld.${props.workspaceId}`],
  })
})

function syncRouteSelection() {
  const switchgearId = Number(route.params.id)
  if (!Number.isFinite(switchgearId)) {
    return
  }
  const nodeId = `switchgear:${switchgearId}`
  if (diagram.scene.value.entities.nodesById.has(nodeId)) {
    selection.setSelection([nodeId], nodeId)
  }
}

function setTool(tool: PackageTool) {
  activeTool.value = tool
  draftLine.value = null
  draggedEdge.value = null
  labelDrag.value = null
  closeContextMenu()
  if (tool === "line") {
    pointer.setTool("select")
    return
  }
  pointer.setTool(tool)
}

function fitScene() {
  diagram.engine.fitScene(96)
}

function toggleSnapEnabled() {
  const nextState: StoredDiagramState = {
    ...(lastStoredState.value ?? { workspaceId: props.workspaceId }),
    snapEnabled: !snapEnabled.value,
  }
  lastStoredState.value = nextState
  writeLocalSetting(props.storageKey, nextState, {
    legacyKeys: [`unitlab.switchgears.sld.${props.workspaceId}`],
  })
}

function zoomBy(delta: number) {
  const current = viewport.viewport.value
  const currentZoom = current.zoom > 0 ? current.zoom : 1
  const nextZoom = clampZoom(currentZoom + delta)
  const centerX = current.x + current.width / currentZoom / 2
  const centerY = current.y + current.height / currentZoom / 2
  viewport.setViewport({
    x: centerX - current.width / nextZoom / 2,
    y: centerY - current.height / nextZoom / 2,
    zoom: nextZoom,
  })
}

function autoArrange() {
  const nodeIds = [...diagram.scene.value.order.nodeIds]
  if (nodeIds.length === 0) {
    return
  }
  diagram.engine.transact(() => {
    const serialized = diagram.engine.serialize()
    const indexById = new Map(nodeIds.map((id, index) => [id, index]))
    return {
      ...serialized,
      nodes: serialized.nodes.map((node) => {
        const index = indexById.get(node.id)
        if (index == null) {
          return node
        }
        const layout = buildDefaultSwitchgearSldLayout(index)
        return {
          ...node,
          x: layout.x,
          y: layout.y,
        }
      }),
    }
  })
  fitScene()
}

function undo() {
  diagram.dispatch({ type: "undo" })
}

function redo() {
  diagram.dispatch({ type: "redo" })
}

function deleteSelection() {
  closeContextMenu()
  diagram.engine.dispatchKeyboardCommand("delete")
}

function clearSelection() {
  closeContextMenu()
  selection.clearSelection()
}

function closeContextMenu() {
  contextMenu.value = null
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
  openContextMenu(event, "text")
}

function openNodeContextMenu(event: MouseEvent, nodeId: string) {
  event.preventDefault()
  event.stopPropagation()
  selection.setSelection([nodeId], nodeId)
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
    id: `edge:paste:${Date.now()}:${index}`,
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
      id: `shape:paste:${Date.now()}:${index}`,
      kind: 'shape' as const,
      x: Math.round(center.x - dims.width / 2),
      y: Math.round(center.y - dims.height / 2),
      width: dims.width,
      height: dims.height,
      rotation: normalizeRotation(item.rotation),
      shape: item.kind,
      metadata: {
        entityType: 'static',
        staticId: `paste-${item.kind}-${Date.now()}-${index}`,
        staticKind: item.kind,
        staticSize: item.size,
        rotation: normalizeRotation(item.rotation),
      },
    }
  })
  const textEntries = source.textElements.map((item, index) => {
    const point = snapWorldPoint({ x: item.x + offset, y: item.y + offset })
    return {
      id: `text:paste:${Date.now()}:${index}`,
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
}

function addText() {
  const center = getViewportCenter()
  diagram.dispatch({
    type: "pasteClipboard",
    clipboard: {
      nodes: [],
      edges: [],
      texts: [{
        id: "text:new",
        kind: "text",
        x: center.x,
        y: center.y,
        text: DEFAULT_TEXT_LABEL,
        width: 96,
        height: 28,
        fontSize: 12,
        metadata: { entityType: "text" },
      }],
      shapes: [],
      ports: [],
      selection: { ids: ["text:new"], primaryId: "text:new" },
      viewport: diagram.scene.value.viewport,
    },
    offset: { x: 0, y: 0 },
    historyKey: "add-text",
  })
}

function addStatic(kind: DiagramStaticKind) {
  const center = getViewportCenter()
  const dims = STATIC_DIMENSIONS[kind].md
  const seed = `package-${kind}-${diagram.scene.value.revision}-${Math.round(center.x)}-${Math.round(center.y)}`
  diagram.dispatch({
    type: "pasteClipboard",
    clipboard: {
      nodes: [],
      edges: [],
      texts: [],
      shapes: [{
        id: `shape:${kind}`,
        kind: "shape",
        x: center.x - dims.width / 2,
        y: center.y - dims.height / 2,
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
      selection: { ids: [`shape:${kind}`], primaryId: `shape:${kind}` },
      viewport: diagram.scene.value.viewport,
    },
    offset: { x: 0, y: 0 },
    historyKey: `add-${kind}`,
  })
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
}

function setSelectedEdgesWeight(weight: EdgeWeight) {
  updateSelectedEdges((edge) => ({
    ...edge,
    metadata: {
      ...edge.metadata,
      edgeWeight: weight,
    },
  }))
}

function rotateSelectedEdges90() {
  if (selectedEdgeIds.value.length === 0) {
    return
  }
  updateSelectedEdges((edge) => {
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
    return {
      ...edge,
      source: { kind: "point", point: nextSource },
      target: { kind: "point", point: nextTarget },
      metadata: {
        ...edge.metadata,
        startBinding: null,
        endBinding: null,
      },
    }
  })
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
}

function onWheel(event: WheelEvent) {
  const current = viewport.viewport.value
  if (event.ctrlKey || event.metaKey) {
    const stage = stageRef.value
    if (!stage) {
      return
    }
    const rect = stage.getBoundingClientRect()
    const nextZoom = clampZoom(current.zoom * (event.deltaY < 0 ? 1.1 : 0.9))
    const relativeX = rect.width > 0 ? (event.clientX - rect.left) / rect.width : 0.5
    const relativeY = rect.height > 0 ? (event.clientY - rect.top) / rect.height : 0.5
    const worldWidth = current.width / current.zoom
    const worldHeight = current.height / current.zoom
    const focusX = current.x + worldWidth * relativeX
    const focusY = current.y + worldHeight * relativeY
    const nextWorldWidth = current.width / nextZoom
    const nextWorldHeight = current.height / nextZoom

    viewport.setViewport({
      x: focusX - nextWorldWidth * relativeX,
      y: focusY - nextWorldHeight * relativeY,
      zoom: nextZoom,
    })
    return
  }

  const zoom = current.zoom > 0 ? current.zoom : 1
  viewport.setViewport({
    x: current.x + event.deltaX / zoom,
    y: current.y + event.deltaY / zoom,
  })
}

function onStageKeydown(event: KeyboardEvent) {
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
    draftLine.value = null
    draggedEdge.value = null
    labelDrag.value = null
    clearSelection()
  }
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
}

function cancelTextEdit() {
  closeContextMenu()
  textEditor.cancelTextEdit()
}

function onSvgClick(event: MouseEvent) {
  closeContextMenu()
  if (activeTool.value !== "line") {
    return
  }
  const endpoint = snapDraftEndpoint(mapPointerToWorld(event as unknown as PointerEvent))
  if (!draftLine.value) {
    draftLine.value = { start: endpoint, current: endpoint }
    return
  }
  createLine(draftLine.value.start, endpoint)
  draftLine.value = null
}

function onSvgPointerMove(event: PointerEvent) {
  if (activeTool.value === "line" && draftLine.value) {
    draftLine.value = {
      ...draftLine.value,
      current: snapDraftEndpoint(mapPointerToWorld(event)),
    }
  }

  const drag = draggedEdge.value
  if (!drag || drag.pointerId !== event.pointerId) {
    return
  }
  draggedEdge.value = {
    ...drag,
    draft: snapDraftEndpoint(mapPointerToWorld(event)),
  }
}

function onSvgPointerUp(event: PointerEvent) {
  const drag = draggedEdge.value
  if (!drag || drag.pointerId !== event.pointerId) {
    return
  }
  updateEdgeEndpoint(drag.edgeId, drag.endpoint, drag.draft)
  draggedEdge.value = null
}

function startEdgeEndpointDrag(event: PointerEvent, edgeId: string, endpoint: "source" | "target") {
  event.stopPropagation()
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
  event.stopPropagation()
  selection.setSelection([nodeId], nodeId)
  const target = event.currentTarget as Element | null
  target?.setPointerCapture?.(event.pointerId)
  labelDrag.value = {
    pointerId: event.pointerId,
    nodeId,
    originX: Number(node.metadata?.labelOffsetX ?? 0),
    originY: Number(node.metadata?.labelOffsetY ?? 22),
    currentX: Number(node.metadata?.labelOffsetX ?? 0),
    currentY: Number(node.metadata?.labelOffsetY ?? 22),
    startX: event.clientX,
    startY: event.clientY,
  }
}

function onLabelPointerMove(event: PointerEvent) {
  const drag = labelDrag.value
  if (!drag || drag.pointerId !== event.pointerId) {
    return
  }
  labelDrag.value = {
    ...drag,
    currentX: clampLabelOffset(drag.originX + event.clientX - drag.startX),
    currentY: clampLabelOffset(drag.originY + event.clientY - drag.startY),
  }
}

function finishLabelDrag(event: PointerEvent) {
  const drag = labelDrag.value
  const target = event.currentTarget as Element | null
  target?.releasePointerCapture?.(event.pointerId)
  if (!drag || drag.pointerId !== event.pointerId) {
    return
  }
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
}

function createLine(start: DraftEndpoint, end: DraftEndpoint) {
  const seed = `edge-${diagram.scene.value.revision}-${Math.round(start.point.x)}-${Math.round(start.point.y)}`
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
}

function updateEdgeEndpoint(edgeId: string, endpoint: "source" | "target", draft: DraftEndpoint) {
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

function snapDraftEndpoint(point: { x: number; y: number }): DraftEndpoint {
  const port = diagram.engine.nearestPort(point, EDGE_PORT_SNAP_RADIUS)
  if (!port || port.kind !== "port") {
    return { point, portId: null }
  }
  const entity = diagram.scene.value.entities.portsById.get(port.id)
  return entity
    ? { point: { x: entity.x, y: entity.y }, portId: port.id }
    : { point, portId: null }
}

function resolveEdgeEndpointPosition(endpoint: { kind: "point"; point: { x: number; y: number } } | { kind: "node"; nodeId: string } | { kind: "port"; portId: string }) {
  if (endpoint.kind === "point") {
    return endpoint.point
  }
  if (endpoint.kind === "port") {
    const port = diagram.scene.value.entities.portsById.get(endpoint.portId)
    return port ? { x: port.x, y: port.y } : { x: 0, y: 0 }
  }
  const node = diagram.scene.value.entities.nodesById.get(endpoint.nodeId)
  return node ? { x: node.x + node.width / 2, y: node.y + node.height / 2 } : { x: 0, y: 0 }
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
  const stage = stageRef.value
  const current = viewport.viewport.value
  if (!stage || current.width <= 0 || current.height <= 0) {
    return { x: current.x, y: current.y }
  }
  const rect = stage.getBoundingClientRect()
  const relativeX = rect.width > 0 ? (event.clientX - rect.left) / rect.width : 0
  const relativeY = rect.height > 0 ? (event.clientY - rect.top) / rect.height : 0
  return {
    x: current.x + (current.width / current.zoom) * relativeX,
    y: current.y + (current.height / current.zoom) * relativeY,
  }
}

function centerViewportAtWorldPoint(worldX: number, worldY: number) {
  const current = viewport.viewport.value
  const zoom = current.zoom > 0 ? current.zoom : 1
  viewport.setViewport({
    x: worldX - current.width / zoom / 2,
    y: worldY - current.height / zoom / 2,
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
  centerViewportAtWorldPoint(worldX, worldY)
}

function getViewportCenter() {
  const current = viewport.viewport.value
  return {
    x: current.x + current.width / current.zoom / 2,
    y: current.y + current.height / current.zoom / 2,
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

function resolveNodeFill(id: string) {
  const node = diagram.scene.value.entities.nodesById.get(id)
  const switchgearId = Number(node?.metadata?.switchgearId)
  const switchgear = Number.isFinite(switchgearId) ? switchgearStore.getById(switchgearId) : null
  const state = switchgear ? switchgearStore.resolveSwitchgearState(switchgear) : "UNKNOWN"
  if (state === "CLOSED") return "var(--color-emerald-100)"
  if (state === "OPEN") return "var(--color-amber-100)"
  if (state === "INTERMEDIATE") return "var(--color-orange-100)"
  return "var(--color-white)"
}

function resolveNodeStroke(id: string, selected: boolean) {
  if (selected) return "var(--color-blue-500)"
  return "var(--color-blue-300)"
}

function resolveNodeLabel(id: string) {
  return diagram.scene.value.entities.nodesById.get(id)?.metadata?.name as string | undefined
}

function resolveSwitchgearId(id: string) {
  const value = Number(diagram.scene.value.entities.nodesById.get(id)?.metadata?.switchgearId)
  return Number.isFinite(value) ? value : null
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
  return resolveEdgeKind(id) === "arrow"
    ? "var(--color-blue-700)"
    : "var(--color-neutral-700)"
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
</script>

<template>
  <section class="switchgear-sld-package-canvas">
    <div class="switchgear-sld-package-canvas__toolbar">
      <div class="switchgear-sld-package-canvas__status">
        <span>{{ sceneCounts.nodes }} switchgears</span>
        <span>{{ sceneCounts.edges }} lines</span>
        <span>{{ sceneCounts.statics }} symbols</span>
        <span>{{ sceneCounts.texts }} texts</span>
      </div>
      <div class="switchgear-sld-package-canvas__actions">
        <div class="switchgear-sld-package-canvas__tool-tabs">
          <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': activeTool === 'select' }" @click="setTool('select')">
            Select
          </button>
          <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': activeTool === 'pan' }" @click="setTool('pan')">
            Pan
          </button>
          <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': activeTool === 'line' }" @click="setTool('line')">
            Line
          </button>
        </div>
        <div v-if="activeTool === 'line' || selectedEdgeCount > 0" class="switchgear-sld-package-canvas__tool-tabs">
          <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (activeTool === 'line' ? lineKind : selectedEdgeKind) === 'line' }" @click="activeTool === 'line' ? lineKind = 'line' : setSelectedEdgesKind('line')">
            Plain
          </button>
          <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (activeTool === 'line' ? lineKind : selectedEdgeKind) === 'arrow' }" @click="activeTool === 'line' ? lineKind = 'arrow' : setSelectedEdgesKind('arrow')">
            Arrow
          </button>
          <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (activeTool === 'line' ? lineWeight : selectedEdgeWeight) === 'normal' }" @click="activeTool === 'line' ? lineWeight = 'normal' : setSelectedEdgesWeight('normal')">
            Normal
          </button>
          <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (activeTool === 'line' ? lineWeight : selectedEdgeWeight) === 'bold' }" @click="activeTool === 'line' ? lineWeight = 'bold' : setSelectedEdgesWeight('bold')">
            Bold
          </button>
          <UiButton v-if="selectedEdgeCount > 0" size="sm" variant="secondary" @click="rotateSelectedEdges90">
            Rotate
          </UiButton>
        </div>
        <UiButton size="sm" variant="secondary" @click="addStatic('transformer')">
          Add transformer
        </UiButton>
        <UiButton size="sm" variant="secondary" @click="addStatic('ground')">
          Add ground
        </UiButton>
        <UiButton size="sm" variant="secondary" @click="addText">
          Add text
        </UiButton>
        <UiButton v-if="selectedTextCount === 1" size="sm" variant="secondary" @click="beginTextEdit(selectedTextIds[0]!)">
          Edit text
        </UiButton>
        <div v-if="selectedStaticCount > 0" class="switchgear-sld-package-canvas__tool-tabs">
          <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': selectedStaticSize === 'sm' }" @click="setSelectedStaticSize('sm')">
            S
          </button>
          <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': selectedStaticSize === 'md' }" @click="setSelectedStaticSize('md')">
            M
          </button>
          <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': selectedStaticSize === 'lg' }" @click="setSelectedStaticSize('lg')">
            L
          </button>
          <UiButton size="sm" variant="secondary" @click="rotateSelectedStatic">
            Rotate
          </UiButton>
        </div>
        <div v-if="selectedNodeCount > 1" class="switchgear-sld-package-canvas__tool-tabs">
          <UiButton size="sm" variant="secondary" @click="alignSelectedNodesLeft">
            Align left
          </UiButton>
          <UiButton size="sm" variant="secondary" @click="alignSelectedNodesTop">
            Align top
          </UiButton>
          <UiButton size="sm" variant="secondary" @click="alignSelectedNodesRight">
            Align right
          </UiButton>
          <UiButton size="sm" variant="secondary" @click="alignSelectedNodesBottom">
            Align bottom
          </UiButton>
        </div>
        <div class="switchgear-sld-package-canvas__tool-tabs">
          <UiButton
            size="sm"
            variant="secondary"
            :class="{ 'switchgear-sld-package-canvas__snap-toggle--active': snapEnabled }"
            :title="snapEnabled ? 'Disable magnetic snap' : 'Enable magnetic snap'"
            :aria-label="snapEnabled ? 'Disable magnetic snap' : 'Enable magnetic snap'"
            @click="toggleSnapEnabled"
          >
            Snap
          </UiButton>
          <UiButton size="sm" variant="secondary" class="switchgear-sld-package-canvas__icon-action" title="Zoom out" aria-label="Zoom out" @click="zoomBy(-0.1)">
            <svg viewBox="0 0 16 16" class="switchgear-sld-package-canvas__icon" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">
              <circle cx="7" cy="7" r="4.5" />
              <path d="M10.5 10.5 14 14" />
              <path d="M5 7h4" />
            </svg>
          </UiButton>
          <span class="switchgear-sld-package-canvas__zoom-label">{{ zoomLabel }}</span>
          <UiButton size="sm" variant="secondary" class="switchgear-sld-package-canvas__icon-action" title="Zoom in" aria-label="Zoom in" @click="zoomBy(0.1)">
            <svg viewBox="0 0 16 16" class="switchgear-sld-package-canvas__icon" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">
              <circle cx="7" cy="7" r="4.5" />
              <path d="M10.5 10.5 14 14" />
              <path d="M7 5v4" />
              <path d="M5 7h4" />
            </svg>
          </UiButton>
        </div>
        <span class="switchgear-sld-package-canvas__selection">{{ snapStateLabel }}</span>
        <span class="switchgear-sld-package-canvas__selection">{{ selectionLabel }}</span>
        <UiButton size="sm" variant="secondary" :disabled="!canUndo" @click="undo">
          Undo
        </UiButton>
        <UiButton size="sm" variant="secondary" :disabled="!canRedo" @click="redo">
          Redo
        </UiButton>
        <UiButton size="sm" variant="secondary" @click="fitScene">
          Fit
        </UiButton>
        <UiButton size="sm" variant="secondary" @click="autoArrange">
          Auto layout
        </UiButton>
        <UiButton size="sm" variant="secondary" :disabled="selection.selection.value.ids.length === 0" @click="clearSelection">
          Clear
        </UiButton>
        <UiButton size="sm" variant="secondary" :disabled="!canDelete" @click="deleteSelection">
          Delete
        </UiButton>
      </div>
    </div>

    <div
      ref="stageRef"
      class="switchgear-sld-package-canvas__stage"
      tabindex="0"
      @wheel.prevent="onWheel"
      @keydown="onStageKeydown"
    >
      <div
        v-if="singleSelectedSwitchgear"
        class="switchgear-sld-package-canvas__selected-controls"
        @pointerdown.stop
      >
        <SwitchgearControlToolbar
          :switchgear="singleSelectedSwitchgear"
          compact
        />
      </div>

      <svg
        class="switchgear-sld-package-canvas__svg"
        :viewBox="`${viewportBox.x} ${viewportBox.y} ${viewportBox.width} ${viewportBox.height}`"
        v-bind="svgPointerProps"
        @click="onSvgClick"
        @pointermove="onSvgPointerMove"
        @pointerup="onSvgPointerUp"
        @contextmenu.prevent="closeContextMenu"
      >
        <defs>
          <pattern id="switchgear-sld-package-grid" :width="24" :height="24" patternUnits="userSpaceOnUse">
            <path d="M 24 0 L 0 0 0 24" fill="none" stroke="rgba(148,163,184,0.18)" stroke-width="1" />
          </pattern>
          <marker id="switchgear-sld-package-arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
            <path d="M 0 0 L 10 5 L 0 10 z" fill="currentColor" />
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
          v-for="edge in visible.projection.value.edges"
          :key="edge.id"
          v-bind="getSvgEntityProps(edge)"
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

        <g v-for="shape in visible.projection.value.shapes" :key="shape.id">
          <g
            v-if="resolveStaticMeta(shape.id).kind === 'transformer'"
            @contextmenu.stop.prevent="openStaticContextMenu($event, shape.id)"
            :transform="`translate(${shape.geometry.bounds.x + shape.geometry.bounds.width / 2} ${shape.geometry.bounds.y + shape.geometry.bounds.height / 2}) rotate(${resolveStaticMeta(shape.id).rotation})`"
          >
            <ellipse
              :cx="-shape.geometry.bounds.width * 0.18"
              cy="0"
              :rx="shape.geometry.bounds.width * 0.22"
              :ry="shape.geometry.bounds.height * 0.3"
              fill="none"
              stroke="var(--color-neutral-700)"
              stroke-width="2"
            />
            <ellipse
              :cx="shape.geometry.bounds.width * 0.18"
              cy="0"
              :rx="shape.geometry.bounds.width * 0.22"
              :ry="shape.geometry.bounds.height * 0.3"
              fill="none"
              stroke="var(--color-neutral-700)"
              stroke-width="2"
            />
          </g>
          <g
            v-else
            @contextmenu.stop.prevent="openStaticContextMenu($event, shape.id)"
            :transform="`translate(${shape.geometry.bounds.x + shape.geometry.bounds.width / 2} ${shape.geometry.bounds.y + shape.geometry.bounds.height / 2}) rotate(${resolveStaticMeta(shape.id).rotation})`"
          >
            <line x1="0" :y1="-shape.geometry.bounds.height * 0.5" x2="0" y2="0" stroke="var(--color-neutral-700)" stroke-width="2" />
            <line :x1="-shape.geometry.bounds.width * 0.4" y1="0" :x2="shape.geometry.bounds.width * 0.4" y2="0" stroke="var(--color-neutral-700)" stroke-width="2" />
            <line :x1="-shape.geometry.bounds.width * 0.26" :y1="shape.geometry.bounds.height * 0.18" :x2="shape.geometry.bounds.width * 0.26" :y2="shape.geometry.bounds.height * 0.18" stroke="var(--color-neutral-700)" stroke-width="2" />
            <line :x1="-shape.geometry.bounds.width * 0.14" :y1="shape.geometry.bounds.height * 0.34" :x2="shape.geometry.bounds.width * 0.14" :y2="shape.geometry.bounds.height * 0.34" stroke="var(--color-neutral-700)" stroke-width="2" />
          </g>
        </g>

        <rect
          v-for="node in visible.projection.value.nodes"
          :key="node.id"
          v-bind="getSvgEntityProps(node)"
          rx="8"
          :fill="resolveNodeFill(node.id)"
          :stroke="resolveNodeStroke(node.id, node.selected)"
          :stroke-width="node.selected ? 2.5 : 1.5"
          @dblclick.stop="openNodeDetail(node.id)"
          @contextmenu.stop.prevent="openNodeContextMenu($event, node.id)"
        />

        <text
          v-for="node in visible.projection.value.nodes"
          :key="`${node.id}:caption`"
          :x="node.geometry.bounds.x + node.geometry.bounds.width / 2"
          :y="node.geometry.bounds.y + node.geometry.bounds.height / 2 + 4"
          class="switchgear-sld-package-canvas__node-text"
          text-anchor="middle"
          pointer-events="none"
        >
          {{ resolveNodeLabel(node.id) }}
        </text>

        <text
          v-for="node in visible.projection.value.nodes"
          :key="`${node.id}:label`"
          :x="resolveNodeLabelPosition(node.id).x"
          :y="resolveNodeLabelPosition(node.id).y"
          class="switchgear-sld-package-canvas__switchgear-label"
          text-anchor="middle"
          dominant-baseline="middle"
          @pointerdown="beginLabelDrag($event, node.id)"
          @pointermove="onLabelPointerMove"
          @pointerup="finishLabelDrag"
          @dblclick.stop="openNodeDetail(node.id)"
          @contextmenu.stop.prevent="openNodeContextMenu($event, node.id)"
        >
          {{ resolveNodeLabel(node.id) }}
        </text>

        <text
          v-for="text in visible.projection.value.texts"
          :key="text.id"
          v-bind="getSvgEntityProps(text)"
          :class="resolveTextClass(text.id)"
          text-anchor="middle"
          dominant-baseline="middle"
          @dblclick.stop="beginTextEdit(text.id)"
          @contextmenu.stop.prevent="openTextContextMenu($event, text.id)"
        >
          {{ diagram.scene.value.entities.textsById.get(text.id)?.text }}
        </text>

        <circle
          v-for="handle in selectedEdgeHandles"
          :key="handle.id"
          :cx="handle.point.x"
          :cy="handle.point.y"
          r="6"
          fill="var(--color-white)"
          stroke="var(--color-blue-500)"
          stroke-width="2"
          @pointerdown="startEdgeEndpointDrag($event, handle.edgeId, handle.endpoint)"
          @pointerup="finishEdgeEndpointDrag"
        />

        <circle
          v-for="handle in visible.projection.value.activeHandles"
          :key="handle.id"
          :cx="handle.point.x"
          :cy="handle.point.y"
          r="4"
          fill="var(--color-blue-500)"
          stroke="var(--color-white)"
          stroke-width="1.5"
        />
      </svg>

      <div
        v-if="contextMenu"
        class="switchgear-sld-package-canvas__context-menu"
        :style="{ left: `${contextMenu.x}px`, top: `${contextMenu.y}px` }"
        @pointerdown.stop
      >
        <template v-if="contextMenu.kind === 'edge'">
          <button type="button" class="switchgear-sld-package-canvas__context-item" @click="rotateSelectedEdges90(); closeContextMenu()">
            Rotate selected lines 90deg
          </button>
          <button type="button" class="switchgear-sld-package-canvas__context-item switchgear-sld-package-canvas__context-item--danger" @click="deleteSelection()">
            Remove selected lines
          </button>
        </template>
        <template v-else-if="contextMenu.kind === 'static'">
          <button type="button" class="switchgear-sld-package-canvas__context-item" :disabled="!canShrinkSelectedStatic" @click="setSelectedStaticSize(selectedStaticSize === 'lg' ? 'md' : 'sm'); closeContextMenu()">
            Shrink selected symbols
          </button>
          <button type="button" class="switchgear-sld-package-canvas__context-item" :disabled="!canGrowSelectedStatic" @click="setSelectedStaticSize(selectedStaticSize === 'sm' ? 'md' : 'lg'); closeContextMenu()">
            Grow selected symbols
          </button>
          <button type="button" class="switchgear-sld-package-canvas__context-item" @click="rotateSelectedStatic(); closeContextMenu()">
            Rotate selected symbols 90deg
          </button>
          <button type="button" class="switchgear-sld-package-canvas__context-item switchgear-sld-package-canvas__context-item--danger" @click="deleteSelection()">
            Remove selected symbols
          </button>
        </template>
        <template v-else-if="contextMenu.kind === 'text'">
          <button type="button" class="switchgear-sld-package-canvas__context-item" @click="selectedTextIds[0] && beginTextEdit(selectedTextIds[0]); closeContextMenu()">
            Edit text
          </button>
          <button type="button" class="switchgear-sld-package-canvas__context-item switchgear-sld-package-canvas__context-item--danger" @click="deleteSelection()">
            Remove selected text
          </button>
        </template>
        <template v-else>
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
        v-model="editableText"
        class="switchgear-sld-package-canvas__editor"
        :style="textEditor.activeEditor.value.style"
        @keydown.enter.exact.prevent="commitTextEdit"
        @keydown.esc.prevent="cancelTextEdit"
        @blur="commitTextEdit"
      />

      <div v-if="minimapModel" class="switchgear-sld-package-canvas__minimap">
        <svg
          :width="MINIMAP_WIDTH"
          :height="MINIMAP_HEIGHT"
          class="switchgear-sld-package-canvas__minimap-svg"
          @pointerdown.stop.prevent="handleMinimapPointerDown"
        >
          <rect
            x="0"
            y="0"
            :width="MINIMAP_WIDTH"
            :height="MINIMAP_HEIGHT"
            rx="8"
            fill="rgba(148,163,184,0.18)"
          />
          <g>
            <line
              v-for="line in minimapModel.lines"
              :key="line.id"
              :x1="line.x1"
              :y1="line.y1"
              :x2="line.x2"
              :y2="line.y2"
              :stroke="line.active ? '#38bdf8' : '#475569'"
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
              :fill="item.active ? '#38bdf8' : '#64748b'"
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
              :fill="item.active ? '#38bdf8' : '#64748b'"
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
              :fill="item.active ? '#38bdf8' : '#334155'"
              :opacity="item.active ? 1 : 0.65"
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
  border-radius: 999px;
  background: var(--color-white);
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
  font-weight: 500;
}

.switchgear-sld-package-canvas__tool-tab {
  padding: 0.45rem 0.7rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: 0.5rem;
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
  border-radius: 0.5rem;
  background: linear-gradient(180deg, var(--color-white), color-mix(in srgb, var(--color-sky-50) 42%, var(--color-white)));
  overflow: hidden;
  outline: none;
}

.switchgear-sld-package-canvas__stage:focus-visible {
  box-shadow: 0 0 0 2px color-mix(in srgb, var(--color-blue-500) 35%, transparent);
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
  border-radius: 0.75rem;
  background: color-mix(in srgb, var(--color-white) 90%, transparent);
  box-shadow: 0 14px 30px rgb(15 23 42 / 0.14);
  backdrop-filter: blur(10px);
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
  .switchgear-sld-package-canvas__selected-controls {
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

.switchgear-sld-package-canvas__node-text {
  fill: var(--color-neutral-800);
  font-size: 8px;
  font-weight: 600;
}

.switchgear-sld-package-canvas__switchgear-label {
  fill: var(--color-neutral-700);
  font-size: 10px;
  font-weight: 600;
}

.switchgear-sld-package-canvas__generated-label,
.switchgear-sld-package-canvas__text {
  fill: var(--color-neutral-600);
  font-size: 12px;
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
  border-radius: 0.625rem;
  background: color-mix(in srgb, var(--color-white) 94%, transparent);
  box-shadow: var(--shadow-lg);
  backdrop-filter: blur(12px);
}

.switchgear-sld-package-canvas__context-item {
  padding: 0.55rem 0.7rem;
  border: 0;
  border-radius: 0.45rem;
  background: transparent;
  color: var(--color-neutral-700);
  font: inherit;
  font-size: var(--text-sm);
  text-align: left;
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
  border-radius: 0.625rem;
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
  border-radius: 0.375rem;
  background: var(--color-white);
  color: var(--color-neutral-900);
  font: inherit;
  font-size: 12px;
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

:global(.dark .switchgear-sld-package-canvas__stage) {
  border-color: var(--color-neutral-800);
  background: linear-gradient(180deg, rgb(10 15 28), rgb(3 7 18));
}

:global(.dark .switchgear-sld-package-canvas__node-text) {
  fill: var(--color-neutral-950);
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
