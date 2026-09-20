<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, watch } from "vue"
import type { ComponentPublicInstance } from "vue"
import { useFloatingPopover, usePopoverController } from "@affino/popover-vue"
import { closeAllTooltipControllers } from "@/components/ui/tooltipSingletonRegistry"
import SldToolbarButton from "./SwitchgearSldToolbarButton.vue"
import UiAffinoListbox from "../../../components/ui/UiAffinoListbox.vue"
import type { SwitchgearType } from "@/types/switchgear"

type SelectionKind = "node" | "edge" | "static" | "text" | "common"
type EdgeStyle = "line" | "arrow"
type EdgeWeight = "normal" | "bold"
type LineStyle = "solid" | "dashed" | "dotted"
type StaticSize = "sm" | "md" | "lg"

const props = defineProps<{
  kind: SelectionKind
  switchgearType?: SwitchgearType | null
  edgeKind?: EdgeStyle | "mixed" | null
  edgeWeight?: EdgeWeight | "mixed" | null
  edgeWidth?: number | "mixed" | null
  edgeStyle?: LineStyle | "mixed" | null
  textFontSize?: number | "mixed" | null
  textBold?: boolean | "mixed" | null
  staticSize?: StaticSize | "mixed" | null
  expanded: boolean
  actions: {
    setSwitchgearType: (type: SwitchgearType) => void
    rotateSwitchgear: () => void
    setLineKind: (kind: EdgeStyle) => void
    setLineWeight: (weight: EdgeWeight) => void
    setLineWidth: (width: number) => void
    setLineStyle: (style: LineStyle) => void
    rotateEdges: () => void
    setStaticSize: (size: StaticSize) => void
    rotateStatic: () => void
    rotateSelection: () => void
    editText: () => void
    setTextFontSize: (size: number) => void
    toggleTextBold: () => void
    toggleExpanded: () => void
  }
}>()

const switchgearTypeOptions = [
  { value: "switchgear", label: "Switchgear / breaker" },
  { value: "disconnector", label: "Disconnector" },
  { value: "earthing", label: "Earthing switch" },
]
const staticSizes: StaticSize[] = ["sm", "md", "lg"]
const lineWidths = [1, 2, 3, 4, 6, 8].map(value => ({ value, label: `${value} pt` }))
const lineStyles = [
  { value: "solid", label: "Solid" },
  { value: "dashed", label: "Dashed" },
  { value: "dotted", label: "Dotted" },
]
const textSizes = [10, 12, 14, 18, 24, 36, 48, 64, 72, 96, 144].map(value => ({ value, label: `${value} px` }))

const popover = usePopoverController(
  { closeOnInteractOutside: false, closeOnEscape: true, role: "dialog" },
  {
    onOpen: () => {
      closeAllTooltipControllers("programmatic")
      if (!props.expanded) props.actions.toggleExpanded()
    },
    onClose: () => {
      if (props.expanded) props.actions.toggleExpanded()
    },
  },
)
const floating = useFloatingPopover(popover, {
  placement: "auto",
  align: "center",
  gutter: 8,
  viewportPadding: 12,
  strategy: "fixed",
  teleportTo: "body",
  zIndex: 1100,
})
const triggerProps = computed(() => popover.getTriggerProps({ role: "dialog" }))
const triggerAriaProps = computed(() => {
  const { onClick: _onClick, onKeydown: _onKeydown, ...ariaProps } = triggerProps.value
  return ariaProps
})
const contentProps = computed(() => popover.getContentProps({ role: "dialog", tabIndex: -1 }))
const contentStyle = computed(() => floating.contentStyle.value)
let positionFrame = 0

function setPopoverTriggerRef(target: Element | ComponentPublicInstance | null): void {
  const element = target instanceof HTMLElement
    ? target
    : target && "$el" in target && target.$el instanceof HTMLElement
      ? target.$el
      : null
  floating.triggerRef.value = element
}

function handlePropertiesTriggerKeydown(event: KeyboardEvent): void {
  triggerProps.value.onKeydown?.(event)
}

function handlePropertiesPointerdown(): void {
  closeAllTooltipControllers("pointer")
}

watch(() => props.expanded, (expanded) => {
  if (expanded && !popover.state.value.open) popover.open("programmatic")
  if (!expanded && popover.state.value.open) popover.close("programmatic")
}, { immediate: true })

watch(() => popover.state.value.open, async (open) => {
  if (!open) {
    if (positionFrame) cancelAnimationFrame(positionFrame)
    positionFrame = 0
    return
  }
  await nextTick()
  await nextTick()
  await floating.updatePosition()
  const updateWhileOpen = () => {
    if (!popover.state.value.open) {
      positionFrame = 0
      return
    }
    void floating.updatePosition()
    positionFrame = requestAnimationFrame(updateWhileOpen)
  }
  positionFrame = requestAnimationFrame(updateWhileOpen)
})

onBeforeUnmount(() => {
  if (positionFrame) cancelAnimationFrame(positionFrame)
})
</script>

<template>
  <div class="switchgear-sld-selection-panel" @pointerdown.stop>
    <button
      :ref="setPopoverTriggerRef"
      v-bind="triggerAriaProps"
      class="btn btn-toolbar btn-xs switchgear-sld-selection-panel__toggle"
      :class="{ 'is-hidden': props.expanded }"
      aria-label="Open selected object properties"
      @pointerdown="handlePropertiesPointerdown"
      @click="props.actions.toggleExpanded"
      @keydown="handlePropertiesTriggerKeydown"
    >
      <span aria-hidden="true">•••</span>
    </button>

    <Teleport to="body">
      <div
        v-if="props.expanded"
        :ref="floating.contentRef"
        v-bind="contentProps"
        class="switchgear-sld-selection-panel switchgear-sld-selection-panel--popover is-expanded"
        :style="contentStyle"
        @pointerdown.stop
      >
      <div class="switchgear-sld-selection-panel__body">
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
          <UiAffinoListbox :model-value="props.edgeWidth === 'mixed' ? null : props.edgeWidth" :options="lineWidths" aria-label="Line thickness" placeholder="Width" compact-options @change="(value) => typeof value === 'number' && props.actions.setLineWidth(value)" />
          <UiAffinoListbox :model-value="props.edgeStyle === 'mixed' ? null : props.edgeStyle" :options="lineStyles" aria-label="Line style" placeholder="Style" compact-options @change="(value) => typeof value === 'string' && props.actions.setLineStyle(value as LineStyle)" />
        </div>
        <SldToolbarButton size="xs" variant="toolbar" title="Rotate selected lines 90 degrees" aria-label="Rotate selected lines" @click="props.actions.rotateEdges">↻</SldToolbarButton>
      </template>

      <template v-else-if="props.kind === 'static'">
        <div class="switchgear-sld-selection-panel__tabs" role="group" aria-label="Symbol size">
          <SldToolbarButton v-for="size in staticSizes" :key="size" size="xs" variant="toolbar" :class="{ 'is-active': props.staticSize === size }" :title="`${size} symbol`" @click="props.actions.setStaticSize(size)">{{ size.toUpperCase() }}</SldToolbarButton>
        </div>
        <SldToolbarButton size="xs" variant="toolbar" title="Rotate selected symbol 90 degrees" aria-label="Rotate selected symbol" @click="props.actions.rotateStatic">↻</SldToolbarButton>
      </template>

      <template v-else-if="props.kind === 'text'">
        <UiAffinoListbox :model-value="props.textFontSize === 'mixed' ? null : props.textFontSize" :options="textSizes" aria-label="Text size" placeholder="Size" compact-options @change="(value) => typeof value === 'number' && props.actions.setTextFontSize(value)" />
        <SldToolbarButton size="xs" variant="toolbar" :class="{ 'is-active': props.textBold === true }" title="Toggle bold text" aria-label="Toggle bold text" @click="props.actions.toggleTextBold">B</SldToolbarButton>
        <SldToolbarButton size="xs" variant="toolbar" title="Edit selected text" aria-label="Edit selected text" @click="props.actions.editText">✎</SldToolbarButton>
      </template>
      <SldToolbarButton v-else-if="props.kind === 'common'" size="xs" variant="toolbar" title="Rotate selected objects 90 degrees" aria-label="Rotate selected objects" @click="props.actions.rotateSelection">↻</SldToolbarButton>

      <SldToolbarButton v-else size="xs" variant="toolbar" title="Edit selected text" aria-label="Edit selected text" @click="props.actions.editText">✎</SldToolbarButton>
      </div>
      </div>
    </Teleport>
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

.switchgear-sld-selection-panel--popover {
  position: fixed;
  z-index: 1100;
}

.switchgear-sld-selection-panel__toggle.is-hidden {
  opacity: 0 !important;
  border-color: transparent !important;
  background: transparent !important;
  box-shadow: none !important;
  color: transparent !important;
  pointer-events: none;
}

.switchgear-sld-selection-panel__body,
.switchgear-sld-selection-panel__tabs {
  display: flex;
  align-items: center;
  gap: 0.3rem;
}

.switchgear-sld-selection-panel__body :deep(.ui-affino-listbox__trigger) {
  min-height: 2.25rem;
  height: 2.25rem;
}

.switchgear-sld-selection-panel__body :deep(.btn-toolbar) {
  width: 2.25rem;
  min-width: 2.25rem;
  min-height: 2.25rem;
  height: 2.25rem;
  padding: 0;
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
