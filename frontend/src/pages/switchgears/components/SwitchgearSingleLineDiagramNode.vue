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
        'switchgear-sld-node__button--selected': selected,
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
  border: 1px solid color-mix(in srgb, var(--color-blue-300) 34%, transparent);
  border-radius: var(--radius-md);
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-white) 88%, transparent), color-mix(in srgb, var(--color-blue-50) 64%, transparent));
  cursor: grab;
  outline: none;
  box-shadow:
    0 10px 22px rgb(15 23 42 / 0.12),
    inset 0 1px 0 rgb(255 255 255 / 0.7);
  transform: translate(-50%, -50%);
  transition: border-color 0.15s ease, box-shadow 0.15s ease, background-color 0.15s ease;
}

.switchgear-sld-node__button:hover {
  border-color: color-mix(in srgb, var(--color-blue-400) 52%, transparent);
}

.switchgear-sld-node__button:focus-visible {
  box-shadow: 0 0 0 2px color-mix(in srgb, var(--color-blue-500) 50%, transparent);
}

.switchgear-sld-node__button--selected {
  border-color: color-mix(in srgb, var(--color-blue-500) 78%, transparent);
  box-shadow:
    0 0 0 2px color-mix(in srgb, var(--color-blue-400) 32%, transparent),
    0 14px 26px rgb(37 99 235 / 0.18),
    inset 0 1px 0 rgb(255 255 255 / 0.76);
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
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 64%, transparent);
  background: color-mix(in srgb, var(--color-white) 88%, transparent);
  color: var(--color-neutral-700);
  cursor: grab;
  font-size: 0.625rem;
  font-weight: 500;
  letter-spacing: 0;
  text-transform: uppercase;
  white-space: nowrap;
  box-shadow:
    0 8px 18px rgb(15 23 42 / 0.1),
    inset 0 1px 0 rgb(255 255 255 / 0.72);
}

.switchgear-sld-node__label--selected {
  border-color: color-mix(in srgb, var(--color-blue-300) 70%, transparent);
  background: color-mix(in srgb, var(--color-blue-100) 92%, transparent);
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
  border-color: color-mix(in srgb, var(--color-neutral-700) 70%, transparent);
  background: color-mix(in srgb, var(--color-neutral-900) 86%, transparent);
  color: var(--color-neutral-200);
  box-shadow:
    0 10px 20px rgb(0 0 0 / 0.28),
    inset 0 1px 0 rgb(255 255 255 / 0.05);
}

:global(.dark .switchgear-sld-node__label--selected) {
  border-color: color-mix(in srgb, var(--color-blue-400) 54%, transparent);
  background: color-mix(in srgb, var(--color-blue-900) 65%, transparent);
  color: var(--color-blue-100);
}

:global(.dark .switchgear-sld-node__button) {
  border-color: color-mix(in srgb, var(--color-blue-500) 28%, transparent);
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-neutral-800) 80%, transparent), color-mix(in srgb, var(--color-neutral-950) 88%, transparent));
  box-shadow:
    0 12px 26px rgb(0 0 0 / 0.34),
    inset 0 1px 0 rgb(255 255 255 / 0.06);
}

:global(.dark .switchgear-sld-node__button:hover) {
  border-color: color-mix(in srgb, var(--color-blue-400) 48%, transparent);
}

:global(.dark .switchgear-sld-node__button--selected) {
  border-color: color-mix(in srgb, var(--color-blue-300) 76%, transparent);
  box-shadow:
    0 0 0 2px color-mix(in srgb, var(--color-blue-400) 28%, transparent),
    0 16px 28px rgb(14 165 233 / 0.16),
    inset 0 1px 0 rgb(255 255 255 / 0.08);
}
</style>
