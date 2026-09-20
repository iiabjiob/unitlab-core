<script setup lang="ts">
import SldToolbarButton from "./SwitchgearSldToolbarButton.vue"
import UiAffinoListbox from "../../../components/ui/UiAffinoListbox.vue"
import type { SwitchgearType } from "@/types/switchgear"

type SelectionKind = "node" | "edge" | "static" | "text" | "common"
type EdgeStyle = "line" | "arrow"
type EdgeWeight = "normal" | "bold"
type StaticSize = "sm" | "md" | "lg"

const props = defineProps<{
  kind: SelectionKind
  switchgearType?: SwitchgearType | null
  edgeKind?: EdgeStyle | "mixed" | null
  edgeWeight?: EdgeWeight | "mixed" | null
  staticSize?: StaticSize | "mixed" | null
  expanded: boolean
  actions: {
    setSwitchgearType: (type: SwitchgearType) => void
    rotateSwitchgear: () => void
    setLineKind: (kind: EdgeStyle) => void
    setLineWeight: (weight: EdgeWeight) => void
    rotateEdges: () => void
    setStaticSize: (size: StaticSize) => void
    rotateStatic: () => void
    rotateSelection: () => void
    editText: () => void
    toggleExpanded: () => void
  }
}>()

const switchgearTypeOptions = [
  { value: "switchgear", label: "Switchgear / breaker" },
  { value: "disconnector", label: "Disconnector" },
  { value: "earthing", label: "Earthing switch" },
]
const staticSizes: StaticSize[] = ["sm", "md", "lg"]
</script>

<template>
  <div class="switchgear-sld-selection-panel" :class="{ 'is-expanded': props.expanded }" @pointerdown.stop>
    <SldToolbarButton
      size="xs"
      variant="toolbar"
      class="switchgear-sld-selection-panel__toggle"
      :aria-expanded="props.expanded"
      aria-label="Open selected object properties"
      title="Object properties"
      @click="props.actions.toggleExpanded"
    >
      <span aria-hidden="true">•••</span>
    </SldToolbarButton>

    <div v-if="props.expanded" class="switchgear-sld-selection-panel__body">
      <template v-if="props.kind === 'node'">
        <UiAffinoListbox
          :model-value="props.switchgearType"
          :options="switchgearTypeOptions"
          aria-label="Switchgear type"
          placeholder="Type"
          panel-width="12rem"
          compact-options
          @change="(value) => typeof value === 'string' && props.actions.setSwitchgearType(value as SwitchgearType)"
        />
        <SldToolbarButton size="xs" variant="toolbar" title="Rotate switchgear 90 degrees" aria-label="Rotate switchgear" @click="props.actions.rotateSwitchgear">↻</SldToolbarButton>
      </template>

      <template v-else-if="props.kind === 'edge'">
        <div class="switchgear-sld-selection-panel__tabs" role="group" aria-label="Line style">
          <SldToolbarButton size="xs" variant="toolbar" :class="{ 'is-active': props.edgeKind === 'line' }" title="Plain line" @click="props.actions.setLineKind('line')">━</SldToolbarButton>
          <SldToolbarButton size="xs" variant="toolbar" :class="{ 'is-active': props.edgeKind === 'arrow' }" title="Arrow line" @click="props.actions.setLineKind('arrow')">➞</SldToolbarButton>
          <SldToolbarButton size="xs" variant="toolbar" :class="{ 'is-active': props.edgeWeight === 'normal' }" title="Normal line weight" @click="props.actions.setLineWeight('normal')">─</SldToolbarButton>
          <SldToolbarButton size="xs" variant="toolbar" :class="{ 'is-active': props.edgeWeight === 'bold' }" title="Bold line weight" @click="props.actions.setLineWeight('bold')">━</SldToolbarButton>
        </div>
        <SldToolbarButton size="xs" variant="toolbar" title="Rotate selected lines 90 degrees" aria-label="Rotate selected lines" @click="props.actions.rotateEdges">↻</SldToolbarButton>
      </template>

      <template v-else-if="props.kind === 'static'">
        <div class="switchgear-sld-selection-panel__tabs" role="group" aria-label="Symbol size">
          <SldToolbarButton v-for="size in staticSizes" :key="size" size="xs" variant="toolbar" :class="{ 'is-active': props.staticSize === size }" :title="`${size} symbol`" @click="props.actions.setStaticSize(size)">{{ size.toUpperCase() }}</SldToolbarButton>
        </div>
        <SldToolbarButton size="xs" variant="toolbar" title="Rotate selected symbol 90 degrees" aria-label="Rotate selected symbol" @click="props.actions.rotateStatic">↻</SldToolbarButton>
      </template>

      <SldToolbarButton v-else-if="props.kind === 'common'" size="xs" variant="toolbar" title="Rotate selected objects 90 degrees" aria-label="Rotate selected objects" @click="props.actions.rotateSelection">↻</SldToolbarButton>

      <SldToolbarButton v-else size="xs" variant="toolbar" title="Edit selected text" aria-label="Edit selected text" @click="props.actions.editText">✎</SldToolbarButton>
    </div>
  </div>
</template>

<style scoped>
.switchgear-sld-selection-panel {
  display: inline-flex;
  align-items: center;
  gap: 0.3rem;
}

.switchgear-sld-selection-panel.is-expanded {
  padding: 0.3rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 76%, transparent);
  border-radius: var(--radius-xl);
  background: color-mix(in srgb, var(--color-white) 94%, transparent);
  box-shadow: 0 12px 28px rgb(var(--color-slate-900-rgb) / 0.2);
  backdrop-filter: blur(10px);
}

.switchgear-sld-selection-panel__body,
.switchgear-sld-selection-panel__tabs {
  display: flex;
  align-items: center;
  gap: 0.3rem;
}

.switchgear-sld-selection-panel__tabs :deep(.btn-toolbar.is-active) {
  border-color: var(--color-blue-300);
  background: var(--color-blue-50);
  color: var(--color-blue-800);
}

:global(.dark .switchgear-sld-selection-panel) {
  color: var(--color-neutral-100);
}

:global(.dark .switchgear-sld-selection-panel.is-expanded) {
  border-color: color-mix(in srgb, var(--color-neutral-700) 86%, transparent);
  background: color-mix(in srgb, var(--color-neutral-950) 92%, transparent);
  box-shadow: 0 12px 28px rgb(0 0 0 / 0.42);
}

:global(.dark .switchgear-sld-selection-panel .btn-toolbar) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-900);
  color: var(--color-neutral-200);
}

:global(.dark .switchgear-sld-selection-panel .btn-toolbar:hover:not(:disabled)) {
  border-color: var(--color-neutral-600);
  background: var(--color-neutral-800);
  color: var(--color-neutral-50);
}

:global(.dark .switchgear-sld-selection-panel .ui-affino-listbox__trigger) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-900);
  color: var(--color-neutral-200);
}

:global(.dark .switchgear-sld-selection-panel .ui-affino-listbox__panel) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-900);
  color: var(--color-neutral-100);
}

:global(.dark .switchgear-sld-selection-panel__tabs .btn-toolbar.is-active) {
  border-color: var(--color-blue-500);
  background: color-mix(in srgb, var(--color-blue-900) 75%, transparent);
  color: var(--color-blue-100);
}
</style>
