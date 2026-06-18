<script setup lang="ts">
import { computed, ref } from "vue"
import { useRoute } from "vue-router"
import { getSvgEntityProps, useDiagramEngine, useDiagramSelection, useDiagramViewport, useDiagramVisibleEntities } from "@affino/diagram-vue"

import UiButton from "@/components/ui/UiButton.vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"

import type { SwitchgearSldPackageSceneModel } from "../utils/switchgearSldPackageScene"

const props = defineProps<{
  model: SwitchgearSldPackageSceneModel
}>()

const route = useRoute()
const switchgearStore = useSwitchgearStore()
const stageRef = ref<HTMLElement | null>(null)
const panState = ref<{ pointerId: number; clientX: number; clientY: number; x: number; y: number } | null>(null)

const diagram = useDiagramEngine(props.model.scene)
const viewport = useDiagramViewport(diagram, { element: stageRef })
const visible = useDiagramVisibleEntities(diagram, { overscan: 240 })
const selection = useDiagramSelection(diagram)

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
const selectionLabel = computed(() => {
  const ids = selection.selection.value.ids
  if (ids.length === 0) {
    return "No selection"
  }
  return ids.length === 1 ? ids[0] : `${ids.length} selected`
})

syncRouteSelection()

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

function fitScene() {
  diagram.engine.fitScene(96)
}

function clearSelection() {
  selection.clearSelection()
}

function selectEntity(id: string) {
  selection.setSelection([id], id)
}

function onViewportPointerDown(event: PointerEvent) {
  if (event.button !== 0) {
    return
  }
  const currentTarget = event.currentTarget as HTMLElement | null
  currentTarget?.setPointerCapture?.(event.pointerId)
  panState.value = {
    pointerId: event.pointerId,
    clientX: event.clientX,
    clientY: event.clientY,
    x: viewport.viewport.value.x,
    y: viewport.viewport.value.y,
  }
}

function onViewportPointerMove(event: PointerEvent) {
  const current = panState.value
  if (!current || current.pointerId !== event.pointerId) {
    return
  }
  const zoom = viewport.viewport.value.zoom > 0 ? viewport.viewport.value.zoom : 1
  viewport.setViewport({
    x: current.x - (event.clientX - current.clientX) / zoom,
    y: current.y - (event.clientY - current.clientY) / zoom,
  })
}

function finishPan(event: PointerEvent) {
  const current = panState.value
  if (!current || current.pointerId !== event.pointerId) {
    return
  }
  const currentTarget = event.currentTarget as HTMLElement | null
  currentTarget?.releasePointerCapture?.(event.pointerId)
  panState.value = null
}

function onWheel(event: WheelEvent) {
  const stage = stageRef.value
  if (!stage) {
    return
  }

  const current = viewport.viewport.value
  const rect = stage.getBoundingClientRect()
  const nextZoom = event.ctrlKey || event.metaKey
    ? clampZoom(current.zoom * (event.deltaY < 0 ? 1.1 : 0.9))
    : current.zoom

  if (nextZoom !== current.zoom) {
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

function clampZoom(value: number) {
  return Math.max(0.05, Math.min(2.2, value))
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

function resolveTextClass(id: string) {
  const text = diagram.scene.value.entities.textsById.get(id)
  return text?.metadata?.entityType === "switchgear-label"
    ? "switchgear-sld-package-canvas__switchgear-label"
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

function resolveStaticMeta(id: string) {
  const shape = diagram.scene.value.entities.shapesById.get(id)
  const staticKind = shape?.metadata?.staticKind
  const rotation = Number(shape?.metadata?.rotation ?? 0)
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
        <span>{{ model.stats.nodes }} switchgears</span>
        <span>{{ model.stats.edges }} lines</span>
        <span>{{ model.stats.statics }} symbols</span>
        <span>{{ model.stats.texts }} labels</span>
      </div>
      <div class="switchgear-sld-package-canvas__actions">
        <span class="switchgear-sld-package-canvas__selection">{{ selectionLabel }}</span>
        <UiButton size="sm" variant="secondary" @click="fitScene">
          Fit
        </UiButton>
        <UiButton size="sm" variant="secondary" :disabled="selection.selection.value.ids.length === 0" @click="clearSelection">
          Clear
        </UiButton>
      </div>
    </div>

    <div
      ref="stageRef"
      class="switchgear-sld-package-canvas__stage"
      @wheel.prevent="onWheel"
    >
      <svg
        class="switchgear-sld-package-canvas__svg"
        :viewBox="`${viewportBox.x} ${viewportBox.y} ${viewportBox.width} ${viewportBox.height}`"
        @pointerdown="onViewportPointerDown"
        @pointermove="onViewportPointerMove"
        @pointerup="finishPan"
        @pointercancel="finishPan"
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
          @pointerdown.stop
          @click.stop="selectEntity(edge.id)"
        />

        <g
          v-for="shape in visible.projection.value.shapes"
          :key="shape.id"
          @pointerdown.stop
          @click.stop="selectEntity(shape.id)"
        >
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
          @pointerdown.stop
          @click.stop="selectEntity(node.id)"
        />

        <text
          v-for="node in visible.projection.value.nodes"
          :key="`${node.id}:caption`"
          :x="node.geometry.bounds.x + node.geometry.bounds.width / 2"
          :y="node.geometry.bounds.y + node.geometry.bounds.height / 2 + 4"
          class="switchgear-sld-package-canvas__node-text"
          text-anchor="middle"
          @pointerdown.stop
          @click.stop="selectEntity(node.id)"
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
          @pointerdown.stop
          @click.stop="selectEntity(text.id)"
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
.switchgear-sld-package-canvas__actions {
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

.switchgear-sld-package-canvas__stage {
  min-height: 0;
  flex: 1 1 auto;
  border: 1px solid var(--color-neutral-200);
  border-radius: 0.5rem;
  background: linear-gradient(180deg, var(--color-white), color-mix(in srgb, var(--color-sky-50) 42%, var(--color-white)));
  overflow: hidden;
  touch-action: none;
}

.switchgear-sld-package-canvas__svg {
  display: block;
  width: 100%;
  height: 100%;
  cursor: grab;
}

.switchgear-sld-package-canvas__svg:active {
  cursor: grabbing;
}

.switchgear-sld-package-canvas__node-text {
  fill: var(--color-neutral-800);
  font-size: 8px;
  font-weight: 600;
  pointer-events: none;
}

.switchgear-sld-package-canvas__switchgear-label {
  fill: var(--color-neutral-700);
  font-size: 10px;
  font-weight: 600;
}

.switchgear-sld-package-canvas__text {
  fill: var(--color-neutral-600);
  font-size: 12px;
  font-weight: 500;
}

:global(.dark .switchgear-sld-package-canvas__status span),
:global(.dark .switchgear-sld-package-canvas__selection) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-900);
  color: var(--color-neutral-300);
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

:global(.dark .switchgear-sld-package-canvas__text) {
  fill: var(--color-neutral-300);
}
</style>
