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
    class="switchgear-sld-node absolute h-10 w-10 cursor-grab active:cursor-grabbing"
    :style="nodeStyle"
    data-node-root
    @pointerdown.stop="handleNodePointerDown"
    @click.stop="emit('select', $event)"
  >
    <button
      type="button"
      class="absolute left-1/2 top-1/2 -translate-x-1/2 -translate-y-1/2 cursor-grab rounded-md p-1 transition focus-visible:outline-none active:cursor-grabbing"
      :class="{
        'ring-2 ring-amber-400/70 ring-offset-2 ring-offset-white dark:ring-offset-neutral-950': connectionSource,
        'ring-2 ring-sky-400/50 ring-offset-2 ring-offset-white dark:ring-offset-neutral-950': connectionMode && !connectionSource,
      }"
      :title="positionStateLabel"
      @click.stop="emit('select', $event)"
      @dblclick.stop="emit('openDetail')"
    >
      <SwitchgearPositionIcon :state="positionState" size="md" />
    </button>

    <span
      class="absolute left-1/2 top-1/2 cursor-grab whitespace-nowrap rounded px-1.5 py-0.5 text-[10px] font-medium uppercase tracking-[0.12em] shadow-sm active:cursor-grabbing"
      :class="selected
        ? 'bg-sky-100/95 text-sky-800 dark:bg-sky-900/65 dark:text-sky-100'
        : 'bg-white/85 text-neutral-700 dark:bg-neutral-900/85 dark:text-neutral-200'"
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
