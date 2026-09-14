<script setup lang="ts">
import UiButton from "@/components/ui/UiButton.vue"

type PackageTool = "select" | "pan" | "line"
type EdgeStyle = "line" | "arrow"
type EdgeWeight = "normal" | "bold"
type DiagramStaticSize = "sm" | "md" | "lg"

const props = defineProps<{
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
  zoomLabel: string
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
    <div class="switchgear-sld-package-canvas__actions">
      <div class="switchgear-sld-package-canvas__tool-tabs" role="group" aria-label="Canvas tool">
        <UiButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.activeTool === 'select' }" :aria-pressed="props.activeTool === 'select'" title="Select and move objects (V)" aria-label="Select tool" @click="props.actions.setTool('select')">↖</UiButton>
        <UiButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.activeTool === 'pan' }" :aria-pressed="props.activeTool === 'pan'" title="Pan the canvas (H)" aria-label="Pan tool" @click="props.actions.setTool('pan')">✋</UiButton>
        <UiButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.activeTool === 'line' }" :aria-pressed="props.activeTool === 'line'" title="Draw a line (L)" aria-label="Line tool" @click="props.actions.setTool('line')">╱</UiButton>
      </div>
      <div v-if="props.activeTool === 'line' || props.selectedEdgeCount > 0" class="switchgear-sld-package-canvas__tool-tabs" role="group" aria-label="Line style">
        <UiButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'line' }" :aria-pressed="(props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'line'" title="Plain line" aria-label="Plain line" @click="props.actions.setLineKind('line')">━</UiButton>
        <UiButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'arrow' }" :aria-pressed="(props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'arrow'" title="Arrow line" aria-label="Arrow line" @click="props.actions.setLineKind('arrow')">➞</UiButton>
        <UiButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (props.activeTool === 'line' ? props.lineWeight : props.selectedEdgeWeight) === 'normal' }" :aria-pressed="(props.activeTool === 'line' ? props.lineWeight : props.selectedEdgeWeight) === 'normal'" title="Normal line weight" aria-label="Normal line weight" @click="props.actions.setLineWeight('normal')">─</UiButton>
        <UiButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (props.activeTool === 'line' ? props.lineWeight : props.selectedEdgeWeight) === 'bold' }" :aria-pressed="(props.activeTool === 'line' ? props.lineWeight : props.selectedEdgeWeight) === 'bold'" title="Bold line weight" aria-label="Bold line weight" @click="props.actions.setLineWeight('bold')">━</UiButton>
        <UiButton v-if="props.selectedEdgeCount > 0" size="xs" variant="toolbar" title="Rotate selected lines 90 degrees" aria-label="Rotate selected lines" @click="props.actions.rotateEdges">↻</UiButton>
      </div>
      <UiButton size="xs" variant="toolbar" title="Add transformer symbol" aria-label="Add transformer symbol" @click="props.actions.addStatic('transformer')">＋</UiButton>
      <UiButton size="xs" variant="toolbar" title="Add ground symbol" aria-label="Add ground symbol" @click="props.actions.addStatic('ground')">⏚</UiButton>
      <UiButton size="xs" variant="toolbar" title="Add text label" aria-label="Add text label" @click="props.actions.addText">T</UiButton>
      <UiButton v-if="props.selectedTextCount === 1" size="xs" variant="toolbar" title="Edit selected text" aria-label="Edit selected text" @click="props.actions.editText">✎</UiButton>
      <div v-if="props.selectedStaticCount > 0" class="switchgear-sld-package-canvas__tool-tabs" role="group" aria-label="Symbol size">
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.selectedStaticSize === 'sm' }" :aria-pressed="props.selectedStaticSize === 'sm'" title="Small symbol" @click="props.actions.setStaticSize('sm')">S</button>
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.selectedStaticSize === 'md' }" :aria-pressed="props.selectedStaticSize === 'md'" title="Medium symbol" @click="props.actions.setStaticSize('md')">M</button>
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.selectedStaticSize === 'lg' }" :aria-pressed="props.selectedStaticSize === 'lg'" title="Large symbol" @click="props.actions.setStaticSize('lg')">L</button>
        <UiButton size="xs" variant="toolbar" title="Rotate selected symbols 90 degrees" aria-label="Rotate selected symbols" @click="props.actions.rotateStatic">↻</UiButton>
      </div>
      <div v-if="props.selectedNodeCount > 1" class="switchgear-sld-package-canvas__tool-tabs">
        <UiButton size="xs" variant="toolbar" title="Align selected switchgears to the left" aria-label="Align left" @click="props.actions.align('left')">⇐</UiButton>
        <UiButton size="xs" variant="toolbar" title="Align selected switchgears to the top" aria-label="Align top" @click="props.actions.align('top')">⇑</UiButton>
        <UiButton size="xs" variant="toolbar" title="Align selected switchgears to the right" aria-label="Align right" @click="props.actions.align('right')">⇒</UiButton>
        <UiButton size="xs" variant="toolbar" title="Align selected switchgears to the bottom" aria-label="Align bottom" @click="props.actions.align('bottom')">⇓</UiButton>
      </div>
      <div class="switchgear-sld-package-canvas__tool-tabs">
        <UiButton size="xs" variant="toolbar" :class="{ 'switchgear-sld-package-canvas__snap-toggle--active': props.snapEnabled }" :title="props.snapEnabled ? 'Disable magnetic snap' : 'Enable magnetic snap'" :aria-label="props.snapEnabled ? 'Disable magnetic snap' : 'Enable magnetic snap'" @click="props.actions.toggleSnap">⌁</UiButton>
        <UiButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__icon-action" title="Zoom out" aria-label="Zoom out" @click="props.actions.zoom(-0.1)">−</UiButton>
        <span class="switchgear-sld-package-canvas__zoom-label">{{ props.zoomLabel }}</span>
        <UiButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__icon-action" title="Zoom in" aria-label="Zoom in" @click="props.actions.zoom(0.1)">+</UiButton>
      </div>
      <UiButton size="xs" variant="toolbar" :disabled="!props.canUndo" title="Undo last change (Ctrl/Cmd+Z)" aria-label="Undo" @click="props.actions.undo">↶</UiButton>
      <UiButton size="xs" variant="toolbar" :disabled="!props.canRedo" title="Redo last change (Ctrl/Cmd+Shift+Z)" aria-label="Redo" @click="props.actions.redo">↷</UiButton>
      <UiButton size="xs" variant="toolbar" title="Fit all objects in view" aria-label="Fit all objects" @click="props.actions.fit">⌗</UiButton>
      <UiButton size="xs" variant="toolbar" title="Arrange switchgears automatically" aria-label="Auto layout" @click="props.actions.autoArrange">⤢</UiButton>
      <UiButton size="xs" variant="toolbar" :disabled="props.selectionCount === 0" title="Clear selection (Esc)" aria-label="Clear selection" @click="props.actions.clear">×</UiButton>
      <UiButton size="xs" variant="toolbar" :disabled="!props.canDuplicate" title="Duplicate selected symbols, lines, or text (Ctrl/Cmd+D)" aria-label="Duplicate selection" @click="props.actions.duplicate">⧉</UiButton>
      <UiButton size="xs" variant="toolbar" :disabled="!props.canDelete" title="Delete selection (Delete/Backspace)" aria-label="Delete selection" @click="props.actions.delete">⌫</UiButton>
    </div>
  </div>
</template>

<style scoped>
.switchgear-sld-package-canvas__toolbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.75rem;
  flex-wrap: wrap;
  min-width: 0;
}

.switchgear-sld-package-canvas__actions,
.switchgear-sld-package-canvas__tool-tabs {
  display: flex;
  align-items: center;
  gap: 0.35rem;
  flex-wrap: wrap;
}

.switchgear-sld-package-canvas__actions {
  flex: 1 1 32rem;
  min-width: 0;
}

.switchgear-sld-package-canvas__tool-tab {
  width: 2.25rem;
  height: 2.25rem;
  min-width: 2.25rem;
  padding: 0;
  border: 1px solid var(--color-neutral-200);
  border-radius: 0.5rem;
  background: var(--color-white);
  color: var(--color-neutral-600);
  font: inherit;
  font-size: 1.05rem;
  line-height: 1;
  font-weight: 600;
  cursor: pointer;
}

.switchgear-sld-package-canvas__actions :deep(.btn-toolbar) {
  width: 2.25rem;
  height: 2.25rem;
  min-width: 2.25rem;
  padding: 0;
  font-size: 1.05rem;
  line-height: 1;
}

.switchgear-sld-package-canvas__tool-tab.is-active {
  border-color: var(--color-blue-300);
  background: var(--color-blue-50);
  color: var(--color-blue-800);
}

.switchgear-sld-package-canvas__snap-toggle--active {
  border-color: var(--color-blue-300);
  background: var(--color-blue-50);
  color: var(--color-blue-800);
}

.switchgear-sld-package-canvas__icon-action {
  min-width: 1.9rem;
  padding-inline: 0.35rem;
}

.switchgear-sld-package-canvas__zoom-label {
  min-width: 2.8rem;
  text-align: center;
  font-size: var(--text-xs);
  color: var(--color-neutral-600);
}

@media (max-width: 900px) {
  .switchgear-sld-package-canvas__actions {
    flex-basis: 100%;
  }
}

:global(.dark) .switchgear-sld-package-canvas__tool-tab {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-900);
  color: var(--color-neutral-300);
}

:global(.dark) .switchgear-sld-package-canvas__tool-tab.is-active,
:global(.dark) .switchgear-sld-package-canvas__snap-toggle--active {
  border-color: var(--color-blue-500);
  background: color-mix(in srgb, var(--color-blue-900) 75%, transparent);
  color: var(--color-blue-100);
}
</style>
