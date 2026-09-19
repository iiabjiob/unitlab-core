<script setup lang="ts">
import SldToolbarButton from "./SwitchgearSldToolbarButton.vue"
import UiAffinoListbox from "../../../components/ui/UiAffinoListbox.vue"

type PackageTool = "select" | "pan" | "line"
type EdgeStyle = "line" | "arrow"
type EdgeWeight = "normal" | "bold"
type DiagramStaticSize = "sm" | "md" | "lg"
type Alignment = "left" | "top" | "right" | "bottom"

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
  objectBrowserOpen: boolean
  actions: {
    setTool: (tool: PackageTool) => void
    setLineKind: (kind: EdgeStyle) => void
    setLineWeight: (weight: EdgeWeight) => void
    rotateEdges: () => void
    addStatic: (kind: "transformer" | "ground") => void
    addLine: () => void
    addText: () => void
    editText: () => void
    setStaticSize: (size: DiagramStaticSize) => void
    rotateStatic: () => void
    align: (edge: Alignment) => void
    toggleSnap: () => void
    zoom: (delta: number) => void
    undo: () => void
    redo: () => void
    fit: () => void
    clear: () => void
    duplicate: () => void
    delete: () => void
    toggleObjectBrowser: () => void
  }
}>()

const alignmentOptions = [
  { value: "left", label: "⇐  Align left" },
  { value: "top", label: "⇑  Align top" },
  { value: "right", label: "⇒  Align right" },
  { value: "bottom", label: "⇓  Align bottom" },
]

function onAlignmentChange(value: string | number | null) {
  if (value === "left" || value === "top" || value === "right" || value === "bottom") {
    props.actions.align(value)
  }
}
</script>

<template>
  <div class="switchgear-sld-package-canvas__toolbar" role="toolbar" aria-label="Single line diagram editor">
    <div class="switchgear-sld-package-canvas__actions">
      <div class="switchgear-sld-package-canvas__tool-tabs" role="group" aria-label="Canvas tool">
        <SldToolbarButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.activeTool === 'select' }" :aria-pressed="props.activeTool === 'select'" title="Select and move objects (V)" aria-label="Select tool" @click="props.actions.setTool('select')">↖</SldToolbarButton>
        <SldToolbarButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.activeTool === 'pan' }" :aria-pressed="props.activeTool === 'pan'" title="Pan the canvas (H)" aria-label="Pan tool" @click="props.actions.setTool('pan')">✋</SldToolbarButton>
      </div>
      <span class="switchgear-sld-package-canvas__toolbar-divider" role="separator" aria-orientation="vertical" />
      <div v-if="props.activeTool === 'line' || props.selectedEdgeCount > 0" class="switchgear-sld-package-canvas__tool-tabs" role="group" aria-label="Line style">
        <SldToolbarButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'line' }" :aria-pressed="(props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'line'" title="Plain line" aria-label="Plain line" @click="props.actions.setLineKind('line')">━</SldToolbarButton>
        <SldToolbarButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'arrow' }" :aria-pressed="(props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'arrow'" title="Arrow line" aria-label="Arrow line" @click="props.actions.setLineKind('arrow')">➞</SldToolbarButton>
        <SldToolbarButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (props.activeTool === 'line' ? props.lineWeight : props.selectedEdgeWeight) === 'normal' }" :aria-pressed="(props.activeTool === 'line' ? props.lineWeight : props.selectedEdgeWeight) === 'normal'" title="Normal line weight" aria-label="Normal line weight" @click="props.actions.setLineWeight('normal')">─</SldToolbarButton>
        <SldToolbarButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (props.activeTool === 'line' ? props.lineWeight : props.selectedEdgeWeight) === 'bold' }" :aria-pressed="(props.activeTool === 'line' ? props.lineWeight : props.selectedEdgeWeight) === 'bold'" title="Bold line weight" aria-label="Bold line weight" @click="props.actions.setLineWeight('bold')">━</SldToolbarButton>
        <SldToolbarButton v-if="props.selectedEdgeCount > 0" size="xs" variant="toolbar" title="Rotate selected lines 90 degrees" aria-label="Rotate selected lines" @click="props.actions.rotateEdges">↻</SldToolbarButton>
      </div>
      <span v-if="props.activeTool === 'line' || props.selectedEdgeCount > 0" class="switchgear-sld-package-canvas__toolbar-divider" role="separator" aria-orientation="vertical" />
      <div class="switchgear-sld-package-canvas__tool-tabs" role="group" aria-label="Add diagram objects">
      <SldToolbarButton size="xs" variant="toolbar" title="Add transformer symbol" aria-label="Add transformer symbol" @click="props.actions.addStatic('transformer')">＋</SldToolbarButton>
      <SldToolbarButton size="xs" variant="toolbar" title="Add ground symbol" aria-label="Add ground symbol" @click="props.actions.addStatic('ground')">⏚</SldToolbarButton>
      <SldToolbarButton size="xs" variant="toolbar" title="Add text label" aria-label="Add text label" @click="props.actions.addText">T</SldToolbarButton>
      <SldToolbarButton size="xs" variant="toolbar" title="Add line" aria-label="Add line" @click="props.actions.addLine">╱</SldToolbarButton>
      </div>
      <SldToolbarButton v-if="props.selectedTextCount === 1" size="xs" variant="toolbar" title="Edit selected text" aria-label="Edit selected text" @click="props.actions.editText">✎</SldToolbarButton>
      <div v-if="props.selectedStaticCount > 0" class="switchgear-sld-package-canvas__tool-tabs" role="group" aria-label="Symbol size">
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.selectedStaticSize === 'sm' }" :aria-pressed="props.selectedStaticSize === 'sm'" title="Small symbol" @click="props.actions.setStaticSize('sm')">S</button>
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.selectedStaticSize === 'md' }" :aria-pressed="props.selectedStaticSize === 'md'" title="Medium symbol" @click="props.actions.setStaticSize('md')">M</button>
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.selectedStaticSize === 'lg' }" :aria-pressed="props.selectedStaticSize === 'lg'" title="Large symbol" @click="props.actions.setStaticSize('lg')">L</button>
        <SldToolbarButton size="xs" variant="toolbar" title="Rotate selected symbols 90 degrees" aria-label="Rotate selected symbols" @click="props.actions.rotateStatic">↻</SldToolbarButton>
      </div>
      <span v-if="props.selectedStaticCount > 0 || props.selectedNodeCount > 1" class="switchgear-sld-package-canvas__toolbar-divider" role="separator" aria-orientation="vertical" />
      <div v-if="props.selectedNodeCount > 1" class="switchgear-sld-package-canvas__alignment-menu">
        <UiAffinoListbox
          :options="alignmentOptions"
          aria-label="Align selected switchgears"
          placeholder="↔"
          panel-width="9rem"
          compact-options
          @change="onAlignmentChange"
        />
      </div>
      <span class="switchgear-sld-package-canvas__toolbar-divider" role="separator" aria-orientation="vertical" />
      <div class="switchgear-sld-package-canvas__tool-tabs">
        <SldToolbarButton size="xs" variant="toolbar" :class="{ 'switchgear-sld-package-canvas__snap-toggle--active': props.snapEnabled }" :aria-pressed="props.snapEnabled" :title="props.snapEnabled ? 'Disable magnetic snap' : 'Enable magnetic snap'" :aria-label="props.snapEnabled ? 'Disable magnetic snap' : 'Enable magnetic snap'" @click="props.actions.toggleSnap">⌁</SldToolbarButton>
        <SldToolbarButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__icon-action" title="Zoom out" aria-label="Zoom out" @click="props.actions.zoom(-0.1)">−</SldToolbarButton>
        <span class="switchgear-sld-package-canvas__zoom-label">{{ props.zoomLabel }}</span>
        <SldToolbarButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__icon-action" title="Zoom in" aria-label="Zoom in" @click="props.actions.zoom(0.1)">+</SldToolbarButton>
      </div>
      <span class="switchgear-sld-package-canvas__toolbar-divider" role="separator" aria-orientation="vertical" />
      <SldToolbarButton size="xs" variant="toolbar" :disabled="!props.canUndo" title="Undo last change (Ctrl/Cmd+Z)" aria-label="Undo" @click="props.actions.undo">↶</SldToolbarButton>
      <SldToolbarButton size="xs" variant="toolbar" :disabled="!props.canRedo" title="Redo last change (Ctrl/Cmd+Shift+Z)" aria-label="Redo" @click="props.actions.redo">↷</SldToolbarButton>
      <SldToolbarButton size="xs" variant="toolbar" title="Fit all objects in view" aria-label="Fit all objects" @click="props.actions.fit">⌗</SldToolbarButton>
      <span class="switchgear-sld-package-canvas__toolbar-divider" role="separator" aria-orientation="vertical" />
      <SldToolbarButton size="xs" variant="toolbar" :class="{ 'switchgear-sld-package-canvas__snap-toggle--active': props.objectBrowserOpen }" title="Show or hide objects" aria-label="Show or hide objects" :aria-pressed="props.objectBrowserOpen" @click="props.actions.toggleObjectBrowser">☷</SldToolbarButton>
      <span class="switchgear-sld-package-canvas__toolbar-divider" role="separator" aria-orientation="vertical" />
      <SldToolbarButton size="xs" variant="toolbar" :disabled="props.selectionCount === 0" title="Clear selection (Esc)" aria-label="Clear selection" @click="props.actions.clear">×</SldToolbarButton>
      <SldToolbarButton size="xs" variant="toolbar" :disabled="!props.canDuplicate" title="Duplicate selected symbols, lines, or text (Ctrl/Cmd+D)" aria-label="Duplicate selection" @click="props.actions.duplicate">⧉</SldToolbarButton>
      <SldToolbarButton size="xs" variant="toolbar" :disabled="!props.canDelete" title="Delete selection (Delete/Backspace)" aria-label="Delete selection" @click="props.actions.delete">⌫</SldToolbarButton>
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
  min-height: calc(2 * 2.25rem + 0.35rem);
  align-content: flex-start;
}

.switchgear-sld-package-canvas__tool-tab {
  width: 2.25rem;
  height: 2.25rem;
  min-width: 2.25rem;
  padding: 0;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-lg);
  background: var(--color-white);
  color: var(--color-neutral-600);
  font: inherit;
  font-size: var(--text-toolbar);
  line-height: 1;
  font-weight: 600;
  cursor: pointer;
}

.switchgear-sld-package-canvas__actions :deep(.btn-toolbar) {
  width: 2.25rem;
  height: 2.25rem;
  min-width: 2.25rem;
  padding: 0;
  font-size: var(--text-toolbar);
  line-height: 1;
}

.switchgear-sld-package-canvas__actions :deep(.switchgear-sld-package-canvas__tool-tab) {
  width: 2.25rem;
  height: 2.25rem;
  min-width: 2.25rem;
  padding: 0;
  font-size: var(--text-toolbar);
  line-height: 1;
}

.switchgear-sld-package-canvas__actions :deep(.switchgear-sld-package-canvas__tool-tab.is-active) {
  border-color: var(--color-blue-300);
  background: var(--color-blue-50);
  color: var(--color-blue-800);
}

.switchgear-sld-package-canvas__tool-tab.is-active {
  border-color: var(--color-blue-300);
  background: var(--color-blue-50);
  color: var(--color-blue-800);
}

.switchgear-sld-package-canvas__actions :deep(.switchgear-sld-package-canvas__snap-toggle--active) {
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

.switchgear-sld-package-canvas__alignment-menu {
  width: 2.25rem;
  height: 2.25rem;
}

.switchgear-sld-package-canvas__alignment-menu :deep(.ui-affino-listbox__trigger) {
  width: 2.25rem;
  height: 2.25rem;
  min-width: 2.25rem;
  padding: 0;
  border-color: var(--color-neutral-200);
  border-radius: var(--radius-lg);
  background: var(--color-white);
  color: var(--color-neutral-600);
  font-size: var(--text-toolbar);
  line-height: 1;
  text-align: center;
  min-height: 2.25rem;
}

.switchgear-sld-package-canvas__toolbar-divider {
  width: 1px;
  height: 1.75rem;
  margin-inline: 0.15rem;
  background: var(--color-neutral-200);
}

:global(.dark) .switchgear-sld-package-canvas__alignment-menu :deep(.ui-affino-listbox__trigger) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-900);
  color: var(--color-neutral-300);
}

:global(.dark) .switchgear-sld-package-canvas__toolbar-divider {
  background: var(--color-neutral-700);
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
:global(.dark) .switchgear-sld-package-canvas__actions :deep(.switchgear-sld-package-canvas__snap-toggle--active) {
  border-color: var(--color-blue-500);
  background: color-mix(in srgb, var(--color-blue-900) 75%, transparent);
  color: var(--color-blue-100);
}
</style>
