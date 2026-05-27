<script setup lang="ts">
import { computed } from "vue"
import type { Switchgear } from "@/types/switchgear"
import SwitchgearPositionIcon from "./SwitchgearPositionIcon.vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"

const props = defineProps<{
  switchgear: Switchgear
  x: number
  y: number
  labelOffsetX?: number
  labelOffsetY?: number
  selected?: boolean
  connectionMode?: boolean
  connectionSource?: boolean
}>()

const emit = defineEmits<{
  (event: "dragStart", pointer: PointerEvent): void
  (event: "labelDragStart", pointer: PointerEvent): void
  (event: "select", mouseEvent: MouseEvent): void
  (event: "openDetail"): void
}>()

const switchgearStore = useSwitchgearStore()

const nodeStyle = computed(() => ({
  left: `${props.x}px`,
  top: `${props.y}px`,
}))

const labelStyle = computed(() => ({
  transform: `translate(calc(-50% + ${props.labelOffsetX ?? 0}px), calc(-50% + ${props.labelOffsetY ?? 22}px))`,
}))

const positionState = computed(() => switchgearStore.resolveSwitchgearState(props.switchgear))
const positionStateLabel = computed(() => positionState.value)

function handleNodePointerDown(event: PointerEvent) {
  const target = event.target as HTMLElement | null
  if (target?.closest("[data-label-drag-handle]")) {
    return
  }
  emit("dragStart", event)
}

function handleLabelPointerDown(event: PointerEvent) {
  emit("labelDragStart", event)
}
</script>

<template>
  <article
    class="switchgear-sld-node"
    :style="nodeStyle"
    data-node-root
    @pointerdown.stop="handleNodePointerDown"
    @click.stop="emit('select', $event)"
  >
    <button
      type="button"
      class="switchgear-sld-node__button"
      :class="{
        'switchgear-sld-node__button--connection-source': connectionSource,
        'switchgear-sld-node__button--connection-target': connectionMode && !connectionSource,
      }"
      :title="positionStateLabel"
      @click.stop="emit('select', $event)"
      @dblclick.stop="emit('openDetail')"
    >
      <SwitchgearPositionIcon :state="positionState" size="md" />
    </button>

    <span
      class="switchgear-sld-node__label"
      :class="{ 'switchgear-sld-node__label--selected': selected }"
      :style="labelStyle"
      data-label-drag-handle
      :title="switchgear.name"
      @pointerdown.stop.prevent="handleLabelPointerDown"
      @click.stop="emit('select', $event)"
      @dblclick.stop="emit('openDetail')"
    >
      {{ switchgear.name }}
    </span>
  </article>
</template>

<style scoped>
.switchgear-sld-node {
  position: absolute;
  width: 2.5rem;
  height: 2.5rem;
  cursor: grab;
}

.switchgear-sld-node:active,
.switchgear-sld-node__button:active,
.switchgear-sld-node__label:active {
  cursor: grabbing;
}

.switchgear-sld-node__button {
  position: absolute;
  top: 50%;
  left: 50%;
  padding: 0.25rem;
  border: 0;
  border-radius: var(--radius-md);
  background: transparent;
  cursor: grab;
  outline: none;
  transform: translate(-50%, -50%);
  transition: box-shadow 0.15s ease;
}

.switchgear-sld-node__button:focus-visible {
  box-shadow: 0 0 0 2px color-mix(in srgb, var(--color-blue-500) 50%, transparent);
}

.switchgear-sld-node__button--connection-source {
  box-shadow:
    0 0 0 2px color-mix(in srgb, var(--color-amber-400) 70%, transparent),
    0 0 0 4px var(--color-white);
}

.switchgear-sld-node__button--connection-target {
  box-shadow:
    0 0 0 2px color-mix(in srgb, var(--color-blue-500) 50%, transparent),
    0 0 0 4px var(--color-white);
}

.switchgear-sld-node__label {
  position: absolute;
  top: 50%;
  left: 50%;
  padding: 0.125rem 0.375rem;
  border-radius: var(--radius-sm);
  background: color-mix(in srgb, var(--color-white) 85%, transparent);
  color: var(--color-neutral-700);
  cursor: grab;
  font-size: 0.625rem;
  font-weight: 500;
  letter-spacing: 0;
  text-transform: uppercase;
  white-space: nowrap;
  box-shadow: var(--shadow-sm);
}

.switchgear-sld-node__label--selected {
  background: color-mix(in srgb, var(--color-blue-100) 95%, transparent);
  color: var(--color-blue-800);
}

:global(.dark .switchgear-sld-node__button--connection-source) {
  box-shadow:
    0 0 0 2px color-mix(in srgb, var(--color-amber-400) 70%, transparent),
    0 0 0 4px var(--color-neutral-950);
}

:global(.dark .switchgear-sld-node__button--connection-target) {
  box-shadow:
    0 0 0 2px color-mix(in srgb, var(--color-blue-400) 50%, transparent),
    0 0 0 4px var(--color-neutral-950);
}

:global(.dark .switchgear-sld-node__label) {
  background: color-mix(in srgb, var(--color-neutral-900) 85%, transparent);
  color: var(--color-neutral-200);
}

:global(.dark .switchgear-sld-node__label--selected) {
  background: color-mix(in srgb, var(--color-blue-900) 65%, transparent);
  color: var(--color-blue-100);
}
</style>
