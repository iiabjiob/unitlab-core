<script setup lang="ts">
import UiButton from "@/components/ui/UiButton.vue"

type PackageTool = "select" | "pan" | "line"
type EdgeStyle = "line" | "arrow"
type EdgeWeight = "normal" | "bold"
type DiagramStaticSize = "sm" | "md" | "lg"

const props = defineProps<{
  counts: { nodes: number; edges: number; statics: number; texts: number }
  activeTool: PackageTool
  lineKind: EdgeStyle
  lineWeight: EdgeWeight
  selectedEdgeCount: number
  selectedEdgeKind: EdgeStyle | "mixed" | null
  selectedEdgeWeight: EdgeWeight | "mixed" | null
  selectedStaticCount: number
  selectedStaticSize: DiagramStaticSize | "mixed" | null
  selectedNodeCount: number
  selectedTextCount: number
  snapEnabled: boolean
  snapStateLabel: string
  zoomLabel: string
  selectionLabel: string
  canUndo: boolean
  canRedo: boolean
  canDelete: boolean
  canDuplicate: boolean
  selectionCount: number
  actions: {
    setTool: (tool: PackageTool) => void
    setLineKind: (kind: EdgeStyle) => void
    setLineWeight: (weight: EdgeWeight) => void
    rotateEdges: () => void
    addStatic: (kind: "transformer" | "ground") => void
    addText: () => void
    editText: () => void
    setStaticSize: (size: DiagramStaticSize) => void
    rotateStatic: () => void
    align: (edge: "left" | "top" | "right" | "bottom") => void
    toggleSnap: () => void
    zoom: (delta: number) => void
    undo: () => void
    redo: () => void
    fit: () => void
    autoArrange: () => void
    clear: () => void
    duplicate: () => void
    delete: () => void
  }
}>()
</script>

<template>
  <div class="switchgear-sld-package-canvas__toolbar" role="toolbar" aria-label="Single line diagram editor">
    <div class="switchgear-sld-package-canvas__status" aria-live="polite">
      <span>{{ props.counts.nodes }} switchgears</span>
      <span>{{ props.counts.edges }} lines</span>
      <span>{{ props.counts.statics }} symbols</span>
      <span>{{ props.counts.texts }} texts</span>
    </div>
    <div class="switchgear-sld-package-canvas__actions">
      <div class="switchgear-sld-package-canvas__tool-tabs" role="group" aria-label="Canvas tool">
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.activeTool === 'select' }" :aria-pressed="props.activeTool === 'select'" title="Select and move objects (V)" @click="props.actions.setTool('select')">Select</button>
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.activeTool === 'pan' }" :aria-pressed="props.activeTool === 'pan'" title="Pan the canvas (H)" @click="props.actions.setTool('pan')">Pan</button>
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.activeTool === 'line' }" :aria-pressed="props.activeTool === 'line'" title="Draw a line (L)" @click="props.actions.setTool('line')">Line</button>
      </div>
      <div v-if="props.activeTool === 'line' || props.selectedEdgeCount > 0" class="switchgear-sld-package-canvas__tool-tabs" role="group" aria-label="Line style">
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'line' }" :aria-pressed="(props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'line'" title="Plain line" @click="props.actions.setLineKind('line')">Plain</button>
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'arrow' }" :aria-pressed="(props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'arrow'" title="Arrow line" @click="props.actions.setLineKind('arrow')">Arrow</button>
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (props.activeTool === 'line' ? props.lineWeight : props.selectedEdgeWeight) === 'normal' }" :aria-pressed="(props.activeTool === 'line' ? props.lineWeight : props.selectedEdgeWeight) === 'normal'" title="Normal line weight" @click="props.actions.setLineWeight('normal')">Normal</button>
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (props.activeTool === 'line' ? props.lineWeight : props.selectedEdgeWeight) === 'bold' }" :aria-pressed="(props.activeTool === 'line' ? props.lineWeight : props.selectedEdgeWeight) === 'bold'" title="Bold line weight" @click="props.actions.setLineWeight('bold')">Bold</button>
        <UiButton v-if="props.selectedEdgeCount > 0" size="sm" variant="secondary" title="Rotate selected lines 90 degrees" @click="props.actions.rotateEdges">Rotate</UiButton>
      </div>
      <UiButton size="sm" variant="secondary" title="Add transformer symbol" @click="props.actions.addStatic('transformer')">Add transformer</UiButton>
      <UiButton size="sm" variant="secondary" title="Add ground symbol" @click="props.actions.addStatic('ground')">Add ground</UiButton>
      <UiButton size="sm" variant="secondary" title="Add text label" @click="props.actions.addText">Add text</UiButton>
      <UiButton v-if="props.selectedTextCount === 1" size="sm" variant="secondary" title="Edit selected text" @click="props.actions.editText">Edit text</UiButton>
      <div v-if="props.selectedStaticCount > 0" class="switchgear-sld-package-canvas__tool-tabs" role="group" aria-label="Symbol size">
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.selectedStaticSize === 'sm' }" :aria-pressed="props.selectedStaticSize === 'sm'" title="Small symbol" @click="props.actions.setStaticSize('sm')">S</button>
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.selectedStaticSize === 'md' }" :aria-pressed="props.selectedStaticSize === 'md'" title="Medium symbol" @click="props.actions.setStaticSize('md')">M</button>
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.selectedStaticSize === 'lg' }" :aria-pressed="props.selectedStaticSize === 'lg'" title="Large symbol" @click="props.actions.setStaticSize('lg')">L</button>
        <UiButton size="sm" variant="secondary" title="Rotate selected symbols 90 degrees" @click="props.actions.rotateStatic">Rotate</UiButton>
      </div>
      <div v-if="props.selectedNodeCount > 1" class="switchgear-sld-package-canvas__tool-tabs">
        <UiButton size="sm" variant="secondary" title="Align selected switchgears to the left" @click="props.actions.align('left')">Align left</UiButton>
        <UiButton size="sm" variant="secondary" title="Align selected switchgears to the top" @click="props.actions.align('top')">Align top</UiButton>
        <UiButton size="sm" variant="secondary" title="Align selected switchgears to the right" @click="props.actions.align('right')">Align right</UiButton>
        <UiButton size="sm" variant="secondary" title="Align selected switchgears to the bottom" @click="props.actions.align('bottom')">Align bottom</UiButton>
      </div>
      <div class="switchgear-sld-package-canvas__tool-tabs">
        <UiButton size="sm" variant="secondary" :class="{ 'switchgear-sld-package-canvas__snap-toggle--active': props.snapEnabled }" :title="props.snapEnabled ? 'Disable magnetic snap' : 'Enable magnetic snap'" :aria-label="props.snapEnabled ? 'Disable magnetic snap' : 'Enable magnetic snap'" @click="props.actions.toggleSnap">Snap</UiButton>
        <UiButton size="sm" variant="secondary" class="switchgear-sld-package-canvas__icon-action" title="Zoom out" aria-label="Zoom out" @click="props.actions.zoom(-0.1)">−</UiButton>
        <span class="switchgear-sld-package-canvas__zoom-label">{{ props.zoomLabel }}</span>
        <UiButton size="sm" variant="secondary" class="switchgear-sld-package-canvas__icon-action" title="Zoom in" aria-label="Zoom in" @click="props.actions.zoom(0.1)">+</UiButton>
      </div>
      <span class="switchgear-sld-package-canvas__selection">{{ props.snapStateLabel }}</span>
      <span class="switchgear-sld-package-canvas__selection">{{ props.selectionLabel }}</span>
      <UiButton size="sm" variant="secondary" :disabled="!props.canUndo" title="Undo last change (Ctrl/Cmd+Z)" @click="props.actions.undo">Undo</UiButton>
      <UiButton size="sm" variant="secondary" :disabled="!props.canRedo" title="Redo last change (Ctrl/Cmd+Shift+Z)" @click="props.actions.redo">Redo</UiButton>
      <UiButton size="sm" variant="secondary" title="Fit all objects in view" @click="props.actions.fit">Fit</UiButton>
      <UiButton size="sm" variant="secondary" title="Arrange switchgears automatically" @click="props.actions.autoArrange">Auto layout</UiButton>
      <UiButton size="sm" variant="secondary" :disabled="props.selectionCount === 0" title="Clear selection (Esc)" @click="props.actions.clear">Clear</UiButton>
      <UiButton size="sm" variant="secondary" :disabled="!props.canDuplicate" title="Duplicate selected symbols, lines, or text (Ctrl/Cmd+D)" @click="props.actions.duplicate">Duplicate</UiButton>
      <UiButton size="sm" variant="secondary" :disabled="!props.canDelete" title="Delete selection (Delete/Backspace)" @click="props.actions.delete">Delete</UiButton>
    </div>
  </div>
</template>
