<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useRoute } from "vue-router"
import { getSvgEntityProps, useDiagramEngine, useDiagramPointerController, useDiagramSelection, useDiagramTextEditor, useDiagramViewport, useDiagramVisibleEntities } from "@affino/diagram-vue"

import UiButton from "@/components/ui/UiButton.vue"
import { writeLocalSetting } from "@/services/localSettingsStorage"
import { useSwitchgearStore } from "@/stores/switchgearStore"

import type { DiagramStaticKind, DiagramStaticSize, StoredDiagramState } from "../utils/switchgearSldDiagramTypes"
import type { SwitchgearSldPackageSceneModel } from "../utils/switchgearSldPackageScene"
import { serializeSwitchgearSldPackageScene } from "../utils/switchgearSldPackageScene"

const GRID_STEP = 24
const DEFAULT_TEXT_LABEL = "TEXT"
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

const props = defineProps<{
  model: SwitchgearSldPackageSceneModel
  workspaceId: number
  storageKey: string
  initialStoredState: StoredDiagramState | null
}>()

const route = useRoute()
const switchgearStore = useSwitchgearStore()
const stageRef = ref<HTMLElement | null>(null)
const editableText = ref("")
const lastStoredState = ref<StoredDiagramState | null>(props.initialStoredState)

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

const activeTool = ref<"select" | "pan">("select")
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
const selectedStaticCount = computed(() => selectedShapeIds.value.length)
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
const canUndo = computed(() => diagram.engine.canUndo())
const canRedo = computed(() => diagram.engine.canRedo())
const canDelete = computed(() => diagram.engine.canDelete(selection.selection.value.ids))

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

function setTool(tool: "select" | "pan") {
  activeTool.value = tool
  pointer.setTool(tool)
}

function fitScene() {
  diagram.engine.fitScene(96)
}

function clearSelection() {
  selection.clearSelection()
}

function undo() {
  diagram.dispatch({ type: "undo" })
}

function redo() {
  diagram.dispatch({ type: "redo" })
}

function deleteSelection() {
  diagram.engine.dispatchKeyboardCommand("delete")
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
  if (event.key === "Delete" || event.key === "Backspace") {
    event.preventDefault()
    deleteSelection()
    return
  }
  if (event.key === "Escape") {
    event.preventDefault()
    if (textEditor.activeEditor.value) {
      textEditor.cancelTextEdit()
      return
    }
    clearSelection()
  }
}

function beginTextEdit(id: string) {
  if (!diagram.engine.canEditText(id)) {
    return
  }
  textEditor.beginTextEdit(id)
}

function commitTextEdit() {
  textEditor.commitTextEdit(editableText.value)
}

function cancelTextEdit() {
  textEditor.cancelTextEdit()
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

function resolveNodeLabelPosition(id: string) {
  const node = diagram.scene.value.entities.nodesById.get(id)
  if (!node) {
    return { x: 0, y: 0 }
  }
  return {
    x: node.x + node.width / 2 + Number(node.metadata?.labelOffsetX ?? 0),
    y: node.y + node.height / 2 + Number(node.metadata?.labelOffsetY ?? 22),
  }
}

function resolveTextClass(id: string) {
  const text = diagram.scene.value.entities.textsById.get(id)
  return text?.metadata?.entityType === "generated-label"
    ? "switchgear-sld-package-canvas__generated-label"
    : "switchgear-sld-package-canvas__text"
}

function resolveEdgeStroke(id: string) {
  const edge = diagram.scene.value.entities.edgesById.get(id)
  return edge?.metadata?.edgeKind === "arrow"
    ? "var(--color-blue-700)"
    : "var(--color-neutral-700)"
}

function resolveEdgeWidth(id: string) {
  const edge = diagram.scene.value.entities.edgesById.get(id)
  return edge?.metadata?.edgeWeight === "bold" ? 3 : 2
}

function resolveStaticMeta(id: string): { kind: DiagramStaticKind; rotation: number } {
  const shape = diagram.scene.value.entities.shapesById.get(id)
  const staticKind: DiagramStaticKind = shape?.metadata?.staticKind === "ground" ? "ground" : "transformer"
  const rotation = Number(shape?.rotation ?? shape?.metadata?.rotation ?? 0)
  return {
    kind: staticKind === "ground" ? "ground" : "transformer",
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
      <svg
        class="switchgear-sld-package-canvas__svg"
        :viewBox="`${viewportBox.x} ${viewportBox.y} ${viewportBox.width} ${viewportBox.height}`"
        v-bind="pointer.getSvgPointerProps()"
      >
        <defs>
          <pattern id="switchgear-sld-package-grid" :width="24" :height="24" patternUnits="userSpaceOnUse">
            <path d="M 24 0 L 0 0 0 24" fill="none" stroke="rgba(148,163,184,0.18)" stroke-width="1" />
          </pattern>
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
          :opacity="edge.selected ? 1 : 0.92"
        />

        <g v-for="shape in visible.projection.value.shapes" :key="shape.id">
          <g
            v-if="resolveStaticMeta(shape.id).kind === 'transformer'"
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
          pointer-events="none"
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
        >
          {{ diagram.scene.value.entities.textsById.get(text.id)?.text }}
        </text>

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

      <textarea
        v-if="textEditor.activeEditor.value"
        v-model="editableText"
        class="switchgear-sld-package-canvas__editor"
        :style="textEditor.activeEditor.value.style"
        @keydown.enter.exact.prevent="commitTextEdit"
        @keydown.esc.prevent="cancelTextEdit"
        @blur="commitTextEdit"
      />
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

:global(.dark .switchgear-sld-package-canvas__editor) {
  background: var(--color-neutral-950);
  color: var(--color-neutral-100);
}
</style>
