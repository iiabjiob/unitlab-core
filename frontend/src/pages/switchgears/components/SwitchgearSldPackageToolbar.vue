<script setup lang="ts">
import SldToolbarButton from "./SwitchgearSldToolbarButton.vue"
import UiAffinoListbox from "../../../components/ui/UiAffinoListbox.vue"
import type { SwitchgearType } from "@/types/switchgear"

type PackageTool = "select" | "pan" | "line"
type EdgeStyle = "line" | "arrow"
type EdgeWeight = "normal" | "bold"
type DiagramStaticSize = "sm" | "md" | "lg"
type Alignment = "left" | "top" | "right" | "bottom"

const props = defineProps<{
  activeTool: PackageTool
  editMode: boolean
  lineKind: EdgeStyle
  lineWeight: EdgeWeight
  selectedEdgeCount: number
  selectedEdgeKind: EdgeStyle | "mixed" | null
  selectedEdgeWeight: EdgeWeight | "mixed" | null
  selectedStaticCount: number
  selectedStaticSize: DiagramStaticSize | "mixed" | null
  selectedNodeCount: number
  selectedTextCount: number
  selectedSwitchgearType: SwitchgearType | null
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
    toggleEditMode: () => void
    setLineKind: (kind: EdgeStyle) => void
    setLineWeight: (weight: EdgeWeight) => void
    rotateEdges: () => void
    addStatic: (kind: "transformer" | "ground") => void
    addLine: () => void
    addText: () => void
    editText: () => void
    setStaticSize: (size: DiagramStaticSize) => void
    rotateStatic: () => void
    setSwitchgearType: (type: SwitchgearType) => void
    rotateSwitchgear: () => void
    align: (edge: Alignment) => void
    bringToFront: () => void
    sendToBack: () => void
    toggleSnap: () => void
    zoom: (delta: number) => void
    undo: () => void
    redo: () => void
    fit: () => void
    clear: () => void
    duplicate: () => void
    delete: () => void
    toggleObjectBrowser: () => void
    exportSld: (scope: "full" | "selection") => void
    importSld: () => void
  }
}>()

const alignmentOptions = [
  { value: "left", label: "⇐  Align left" },
  { value: "top", label: "⇑  Align top" },
  { value: "right", label: "⇒  Align right" },
  { value: "bottom", label: "⇓  Align bottom" },
]

const switchgearTypeOptions = [
  { value: "switchgear", label: "Switchgear / breaker" },
  { value: "disconnector", label: "Disconnector" },
  { value: "earthing", label: "Earthing switch" },
]

function onAlignmentChange(value: string | number | null) {
  if (value === "left" || value === "top" || value === "right" || value === "bottom") {
    props.actions.align(value)
  }
}
</script>

<template>
  <div class="switchgear-sld-package-canvas__toolbar" role="toolbar" aria-label="Single line diagram editor">
    <SldToolbarButton
      size="xs"
      variant="toolbar"
      class="switchgear-sld-package-canvas__mode-toggle"
      :class="{ 'is-active': props.editMode }"
      :aria-pressed="props.editMode"
      :title="props.editMode ? 'Switch to operator mode' : 'Enable diagram editing'"
      :aria-label="props.editMode ? 'Switch to operator mode' : 'Enable diagram editing'"
      @click="props.actions.toggleEditMode"
    >
      <span aria-hidden="true">{{ props.editMode ? '🔓' : '🔒' }}</span>
    </SldToolbarButton>
    <div v-if="props.editMode" class="switchgear-sld-package-canvas__actions">
      <div class="switchgear-sld-package-canvas__tool-tabs" role="group" aria-label="Canvas tool">
        <SldToolbarButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.activeTool === 'select' }" :aria-pressed="props.activeTool === 'select'" title="Select and move objects (V)" aria-label="Select tool" @click="props.actions.setTool('select')">↖</SldToolbarButton>
        <SldToolbarButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.activeTool === 'pan' }" :aria-pressed="props.activeTool === 'pan'" title="Pan the canvas (H)" aria-label="Pan tool" @click="props.actions.setTool('pan')"><svg class="switchgear-sld-package-canvas__toolbar-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 12h14M12 5v14M5 12l3-3M5 12l3 3M19 12l-3-3M19 12l-3 3M12 5 9 8M12 5l3 3M12 19l-3-3M12 19l3-3" /></svg></SldToolbarButton>
        <SldToolbarButton v-if="props.selectedTextCount === 1" size="xs" variant="toolbar" title="Edit selected text" aria-label="Edit selected text" @click="props.actions.editText">✎</SldToolbarButton>
      </div>
      <span class="switchgear-sld-package-canvas__toolbar-divider" role="separator" aria-orientation="vertical" />
      <div v-if="props.activeTool === 'line' || (props.selectedEdgeCount > 0 && !props.editMode)" class="switchgear-sld-package-canvas__tool-tabs" role="group" aria-label="Line style">
        <SldToolbarButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'line' }" :aria-pressed="(props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'line'" title="Plain line" aria-label="Plain line" @click="props.actions.setLineKind('line')">━</SldToolbarButton>
        <SldToolbarButton size="xs" variant="toolbar" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': (props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'arrow' }" :aria-pressed="(props.activeTool === 'line' ? props.lineKind : props.selectedEdgeKind) === 'arrow'" title="Arrow line" aria-label="Arrow line" @click="props.actions.setLineKind('arrow')">➞</SldToolbarButton>
        <SldToolbarButton v-if="props.selectedEdgeCount > 0" size="xs" variant="toolbar" title="Rotate selected lines 90 degrees" aria-label="Rotate selected lines" @click="props.actions.rotateEdges">↻</SldToolbarButton>
      </div>
      <span v-if="props.activeTool === 'line' || (props.selectedEdgeCount > 0 && !props.editMode)" class="switchgear-sld-package-canvas__toolbar-divider" role="separator" aria-orientation="vertical" />
      <div class="switchgear-sld-package-canvas__tool-tabs" role="group" aria-label="Add diagram objects">
      <SldToolbarButton size="xs" variant="toolbar" title="Add line" aria-label="Add line" @click="props.actions.addLine">╱</SldToolbarButton>
      <SldToolbarButton size="xs" variant="toolbar" title="Add text label" aria-label="Add text label" @click="props.actions.addText">T</SldToolbarButton>
      <SldToolbarButton size="xs" variant="toolbar" title="Add ground symbol" aria-label="Add ground symbol" @click="props.actions.addStatic('ground')">⏚</SldToolbarButton>
      <SldToolbarButton size="xs" variant="toolbar" title="Add transformer symbol" aria-label="Add transformer symbol" @click="props.actions.addStatic('transformer')"><svg class="switchgear-sld-package-canvas__toolbar-icon" viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="8" r="5" /><circle cx="12" cy="16" r="5" /></svg></SldToolbarButton>
      </div>
      <div v-if="props.selectedStaticCount > 0 && !props.editMode" class="switchgear-sld-package-canvas__tool-tabs" role="group" aria-label="Symbol size">
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.selectedStaticSize === 'sm' }" :aria-pressed="props.selectedStaticSize === 'sm'" title="Small symbol" @click="props.actions.setStaticSize('sm')">S</button>
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.selectedStaticSize === 'md' }" :aria-pressed="props.selectedStaticSize === 'md'" title="Medium symbol" @click="props.actions.setStaticSize('md')">M</button>
        <button type="button" class="switchgear-sld-package-canvas__tool-tab" :class="{ 'is-active': props.selectedStaticSize === 'lg' }" :aria-pressed="props.selectedStaticSize === 'lg'" title="Large symbol" @click="props.actions.setStaticSize('lg')">L</button>
        <SldToolbarButton size="xs" variant="toolbar" title="Rotate selected symbols 90 degrees" aria-label="Rotate selected symbols" @click="props.actions.rotateStatic">↻</SldToolbarButton>
      </div>
      <div v-if="props.selectedNodeCount === 1 && !props.editMode" class="switchgear-sld-package-canvas__switchgear-type">
        <UiAffinoListbox
          :model-value="props.selectedSwitchgearType"
          :options="switchgearTypeOptions"
          aria-label="Switchgear type"
          placeholder="Switchgear type"
          panel-width="12rem"
          compact-options
          @change="(value) => typeof value === 'string' && props.actions.setSwitchgearType(value as SwitchgearType)"
        ><template #trigger><span class="switchgear-sld-package-canvas__switchgear-type-icon"><svg v-if="props.selectedSwitchgearType === 'disconnector'" class="switchgear-sld-package-canvas__toolbar-icon" viewBox="0 0 24 24" aria-hidden="true"><circle cx="6" cy="12" r="2" /><circle cx="18" cy="12" r="2" /><path d="M8 12 16 6" /></svg><svg v-else-if="props.selectedSwitchgearType === 'earthing'" class="switchgear-sld-package-canvas__toolbar-icon" viewBox="0 0 24 24" aria-hidden="true"><path d="M12 4v10M6 15h12M8 19h8M10 22h4" /></svg><svg v-else class="switchgear-sld-package-canvas__toolbar-icon" viewBox="0 0 24 24" aria-hidden="true"><path d="M4 12h5M15 12h5" /><rect x="9" y="8" width="6" height="8" rx="1" /></svg></span></template></UiAffinoListbox>
        <SldToolbarButton size="xs" variant="toolbar" title="Rotate selected switchgear 90 degrees" aria-label="Rotate selected switchgear" @click="props.actions.rotateSwitchgear">↻</SldToolbarButton>
      </div>
      <span v-if="(props.selectedStaticCount > 0 && !props.editMode) || props.selectedNodeCount > 1" class="switchgear-sld-package-canvas__toolbar-divider" role="separator" aria-orientation="vertical" />
      <div v-if="props.selectedNodeCount > 1" class="switchgear-sld-package-canvas__alignment-menu">
        <UiAffinoListbox
          :options="alignmentOptions"
          aria-label="Align selected switchgears"
          placeholder="↔"
          panel-width="9rem"
          compact-options
          @change="onAlignmentChange"
        ><template #placeholder><svg class="switchgear-sld-package-canvas__toolbar-icon" viewBox="0 0 24 24" aria-hidden="true"><path d="M4 6h16M4 12h16M4 18h16" /><path d="m8 3-3 3 3 3M16 9l3 3-3 3" /></svg></template></UiAffinoListbox>
      </div>
      <span v-if="props.selectionCount > 0" class="switchgear-sld-package-canvas__toolbar-divider" role="separator" aria-orientation="vertical" />
      <div v-if="props.selectionCount > 0" class="switchgear-sld-package-canvas__tool-tabs" role="group" aria-label="Layer order">
        <SldToolbarButton size="xs" variant="toolbar" title="Bring to front" aria-label="Bring to front" @click="props.actions.bringToFront"><svg class="switchgear-sld-package-canvas__toolbar-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 9h9M5 14h14M5 19h14" /><path d="m16 7 3-3 3 3M19 4v9" /></svg></SldToolbarButton>
        <SldToolbarButton size="xs" variant="toolbar" title="Send to back" aria-label="Send to back" @click="props.actions.sendToBack"><svg class="switchgear-sld-package-canvas__toolbar-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 5h14M5 10h14M5 15h9" /><path d="m16 17 3 3 3-3M19 20V11" /></svg></SldToolbarButton>
      </div>
      <span class="switchgear-sld-package-canvas__toolbar-divider" role="separator" aria-orientation="vertical" />
      <SldToolbarButton size="xs" variant="toolbar" :disabled="!props.canUndo" title="Undo last change (Ctrl/Cmd+Z)" aria-label="Undo" @click="props.actions.undo">↶</SldToolbarButton>
      <SldToolbarButton size="xs" variant="toolbar" :disabled="!props.canRedo" title="Redo last change (Ctrl/Cmd+Shift+Z)" aria-label="Redo" @click="props.actions.redo">↷</SldToolbarButton>
      <span class="switchgear-sld-package-canvas__toolbar-divider" role="separator" aria-orientation="vertical" />
      <span class="switchgear-sld-package-canvas__toolbar-spacer" aria-hidden="true" />
      <SldToolbarButton size="xs" variant="toolbar" title="Export SLD" aria-label="Export SLD" @click="props.actions.exportSld('full')">⇩</SldToolbarButton>
      <SldToolbarButton size="xs" variant="toolbar" :disabled="props.selectionCount === 0" title="Export selected SLD objects" aria-label="Export selected SLD objects" @click="props.actions.exportSld('selection')">⇩*</SldToolbarButton>
      <SldToolbarButton size="xs" variant="toolbar" title="Import SLD" aria-label="Import SLD" @click="props.actions.importSld">⇧</SldToolbarButton>
      <SldToolbarButton size="xs" variant="toolbar" :class="{ 'switchgear-sld-package-canvas__snap-toggle--active': props.objectBrowserOpen }" title="Show or hide objects" aria-label="Show or hide objects" :aria-pressed="props.objectBrowserOpen" @click="props.actions.toggleObjectBrowser">☷</SldToolbarButton>
    </div>
    <span class="switchgear-sld-package-canvas__mode-label">
      {{ props.editMode ? 'Edit mode' : 'Operate mode' }}
    </span>
  </div>
</template>

<style scoped>
.switchgear-sld-package-canvas__toolbar {
  display: flex;
  align-items: flex-start !important;
  justify-content: space-between;
  gap: 0.75rem;
  flex-wrap: nowrap;
  min-width: 0;
}

.switchgear-sld-package-canvas__mode-toggle {
  width: 2.25rem;
  height: 2.25rem;
  min-width: 2.25rem;
  padding: 0;
  font-size: var(--text-toolbar);
  line-height: 1;
  flex: 0 0 auto;
}

.switchgear-sld-package-canvas__mode-toggle.is-active {
  border-color: var(--color-blue-300);
  background: var(--color-blue-50);
  color: var(--color-blue-800);
}

.switchgear-sld-package-canvas__toolbar :deep(.switchgear-sld-package-canvas__mode-toggle.is-active) {
  border-color: var(--color-blue-300) !important;
  background: var(--color-blue-50) !important;
  color: var(--color-blue-800) !important;
  box-shadow: inset 0 1px 2px rgb(var(--color-blue-900-rgb) / 0.12);
}

.switchgear-sld-package-canvas__toolbar :deep(.switchgear-sld-package-canvas__mode-toggle:not(.is-active)) {
  border-color: var(--color-neutral-200);
  background: var(--color-white);
  color: var(--color-neutral-600);
  box-shadow: none;
}

.switchgear-sld-package-canvas__mode-label {
  flex: 0 0 auto;
  margin-left: 0.35rem;
  padding: 0.35rem 0.6rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-pill);
  background: var(--color-neutral-50);
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
  font-weight: 600;
  white-space: nowrap;
}

.switchgear-sld-package-canvas__actions,
.switchgear-sld-package-canvas__tool-tabs {
  display: flex;
  align-items: center;
  gap: 0.35rem;
  flex-wrap: wrap;
}

.switchgear-sld-package-canvas__actions {
  flex-wrap: nowrap !important;
  overflow-x: auto;
  overflow-y: hidden;
  scrollbar-width: thin;
}

.switchgear-sld-package-canvas__actions > .switchgear-sld-package-canvas__tool-tabs {
  flex-wrap: nowrap;
}

.switchgear-sld-package-canvas__actions {
  flex: 1 1 auto;
  min-width: 0;
  min-height: 0;
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

.switchgear-sld-package-canvas__toolbar-icon {
  display: block;
  width: 1.15rem;
  height: 1.15rem;
  fill: none;
  stroke: currentColor;
  stroke-linecap: round;
  stroke-linejoin: round;
  stroke-width: 1.8;
  pointer-events: none;
}

.switchgear-sld-package-canvas__actions :deep(.btn-toolbar) {
  width: 2.25rem;
  height: 2.25rem;
  min-width: 2.25rem;
  padding: 0;
  font-size: var(--text-toolbar);
  line-height: 1;
}

.switchgear-sld-package-canvas__toolbar :deep(.switchgear-sld-package-canvas__mode-toggle) {
  width: 2.25rem !important;
  height: 2.25rem !important;
  min-width: 2.25rem !important;
  padding: 0 !important;
  font-size: var(--text-toolbar) !important;
  line-height: 1 !important;
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

.switchgear-sld-package-canvas__switchgear-type {
  display: flex;
  align-items: center;
  gap: 0.35rem;
  width: 4.85rem;
  min-width: 4.85rem;
  height: 2.25rem;
}

.switchgear-sld-package-canvas__switchgear-type-icon {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 100%;
}

.switchgear-sld-package-canvas__switchgear-type :deep(.ui-affino-listbox__trigger) {
  min-height: 2.25rem;
  width: 100%;
  padding-inline: 0.65rem;
  border-color: var(--color-neutral-200);
  border-radius: var(--radius-lg);
  background: var(--color-white);
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
  text-align: left;
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

:global(.dark) .switchgear-sld-package-canvas__switchgear-type :deep(.ui-affino-listbox__trigger) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-900);
  color: var(--color-neutral-300);
}

:global(.dark) .switchgear-sld-package-canvas__toolbar-divider {
  background: var(--color-neutral-700);
}

@media (max-width: 900px) {
  .switchgear-sld-package-canvas__actions {
    flex-basis: auto;
  }
}

:global(.dark) .switchgear-sld-package-canvas__tool-tab {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-900);
  color: var(--color-neutral-300);
}

:global(.dark) .switchgear-sld-package-canvas__mode-label {
  border-color: var(--color-neutral-800);
  background: var(--color-neutral-950);
  color: var(--color-neutral-400);
}

:global(.dark) .switchgear-sld-package-canvas__mode-toggle.is-active {
  border-color: var(--color-neutral-600);
  background: var(--color-neutral-800);
  color: var(--color-neutral-100);
}

:global(.dark .switchgear-sld-package-canvas__mode-toggle.is-active) {
  border-color: var(--color-neutral-700) !important;
  background: var(--color-neutral-950) !important;
  color: var(--color-neutral-200) !important;
  box-shadow: inset 0 1px 2px rgb(0 0 0 / 0.5);
}

:global(html.dark) .switchgear-sld-package-canvas__tool-tab,
:global(html.dark) .switchgear-sld-package-canvas__actions :deep(.btn-toolbar),
:global(html.dark) .switchgear-sld-package-canvas__mode-toggle {
  border-color: var(--color-neutral-700) !important;
  background: var(--color-neutral-900) !important;
  color: var(--color-neutral-200) !important;
}

:global(html.dark) .switchgear-sld-package-canvas__mode-toggle.is-active {
  border-color: var(--color-neutral-700) !important;
  background: var(--color-neutral-950) !important;
  color: var(--color-neutral-200) !important;
}

:global(html.dark) .switchgear-sld-package-canvas__mode-label {
  border-color: var(--color-neutral-800) !important;
  background: var(--color-neutral-950) !important;
  color: var(--color-neutral-300) !important;
}

:global(html.dark) .switchgear-sld-package-canvas__toolbar .switchgear-sld-package-canvas__mode-label {
  border-color: rgb(63 63 70) !important;
  background: rgb(24 24 27) !important;
  color: rgb(212 212 216) !important;
}

:global(.dark .switchgear-sld-package-canvas__mode-toggle:not(.is-active)) {
  border-color: var(--color-neutral-800);
  background: var(--color-neutral-950);
  color: var(--color-neutral-400);
  box-shadow: none;
}

:global(.dark) .switchgear-sld-package-canvas__tool-tab:hover:not(:disabled),
:global(.dark) .switchgear-sld-package-canvas__actions :deep(.btn-toolbar:hover:not(:disabled)) {
  border-color: var(--color-neutral-600);
  background: var(--color-neutral-800);
  color: var(--color-neutral-100);
}

:global(.dark) .switchgear-sld-package-canvas__tool-tab.is-active,
:global(.dark) .switchgear-sld-package-canvas__actions :deep(.switchgear-sld-package-canvas__snap-toggle--active) {
  border-color: var(--color-blue-500);
  background: color-mix(in srgb, var(--color-blue-900) 75%, transparent);
  color: var(--color-blue-100);
}

:global(.dark .switchgear-sld-package-canvas__actions .btn-toolbar.is-active) {
  border-color: var(--color-blue-500) !important;
  background: color-mix(in srgb, var(--color-blue-900) 75%, transparent) !important;
  color: var(--color-blue-100) !important;
}

:global(.dark) .switchgear-sld-package-canvas__tool-tab.is-active:hover:not(:disabled),
:global(.dark) .switchgear-sld-package-canvas__actions :deep(.switchgear-sld-package-canvas__snap-toggle--active:hover:not(:disabled)) {
  border-color: var(--color-blue-400);
  background: var(--color-blue-800);
  color: var(--color-blue-50);
}

:global(.dark .switchgear-sld-package-canvas__mode-toggle.is-active:hover:not(:disabled)) {
  border-color: var(--color-neutral-600);
  background: var(--color-neutral-800) !important;
  color: var(--color-neutral-50) !important;
}
</style>
