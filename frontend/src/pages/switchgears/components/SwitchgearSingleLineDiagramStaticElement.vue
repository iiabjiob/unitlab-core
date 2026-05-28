<script setup lang="ts">
import { computed } from "vue"

type DiagramStaticKind = "transformer" | "ground"

const props = defineProps<{
  id: string
  kind: DiagramStaticKind
  x: number
  y: number
  rotation: number
  selected?: boolean
}>()

const emit = defineEmits<{
  (event: "dragStart", pointer: PointerEvent): void
  (event: "select", mouseEvent: MouseEvent): void
  (event: "contextMenu", mouseEvent: MouseEvent): void
}>()

const dimensions = computed(() => {
  if (props.kind === "transformer") {
    return { width: 80, height: 80 }
  }

  return { width: 40, height: 40 }
})

const rootStyle = computed(() => ({
  left: `${props.x}px`,
  top: `${props.y}px`,
  width: `${dimensions.value.width}px`,
  height: `${dimensions.value.height}px`,
  transform: `translate(-50%, -50%) rotate(${props.rotation}deg)`,
  transformOrigin: "50% 50%",
}))

function handlePointerDown(event: PointerEvent) {
  emit("dragStart", event)
}
</script>

<template>
  <button
    type="button"
    class="switchgear-sld-static-element"
    :class="{ 'switchgear-sld-static-element--selected': selected }"
    :style="rootStyle"
    data-static-root
    @pointerdown.stop="handlePointerDown"
    @click.stop="emit('select', $event)"
    @contextmenu.stop.prevent="emit('contextMenu', $event)"
  >
    <svg
      v-if="kind === 'transformer'"
      :width="dimensions.width"
      :height="dimensions.height"
      :viewBox="`0 0 ${dimensions.width} ${dimensions.height}`"
      class="switchgear-sld-static-element__graphic"
      fill="none"
      stroke="currentColor"
      stroke-width="4"
      stroke-linecap="round"
      stroke-linejoin="round"
    >
      <circle cx="40" cy="28" r="20" />
      <circle cx="40" cy="52" r="20" />
    </svg>

    <svg
      v-else
      :width="dimensions.width"
      :height="dimensions.height"
      :viewBox="`0 0 ${dimensions.width} ${dimensions.height}`"
      class="switchgear-sld-static-element__graphic"
      fill="none"
      stroke="currentColor"
      stroke-width="4"
      stroke-linecap="square"
      stroke-linejoin="round"
    >
      <path d="M4 20h20" />
      <path d="M24 6v28" />
      <path d="M32 10v20" />
      <path d="M38 14v12" />
    </svg>
  </button>
</template>

<style scoped>
.switchgear-sld-static-element {
  position: absolute;
  display: flex;
  align-items: center;
  justify-content: center;
  border: 1px solid transparent;
  border-radius: var(--radius-md);
  background: transparent;
  color: var(--color-blue-600);
  cursor: grab;
  outline: none;
  transition: border-color 0.15s ease, box-shadow 0.15s ease, background-color 0.15s ease;
}

.switchgear-sld-static-element:hover {
  background: color-mix(in srgb, var(--color-white) 56%, transparent);
  border-color: color-mix(in srgb, var(--color-blue-300) 36%, transparent);
}

.switchgear-sld-static-element:active {
  cursor: grabbing;
}

.switchgear-sld-static-element:focus-visible {
  box-shadow: 0 0 0 2px color-mix(in srgb, var(--color-blue-500) 50%, transparent);
}

.switchgear-sld-static-element--selected {
  background: color-mix(in srgb, var(--color-white) 62%, transparent);
  border-color: color-mix(in srgb, var(--color-blue-400) 56%, transparent);
  box-shadow:
    0 0 0 2px color-mix(in srgb, var(--color-blue-400) 32%, transparent),
    0 12px 24px rgb(37 99 235 / 0.14);
}

.switchgear-sld-static-element__graphic {
  overflow: visible;
}

:global(.dark .switchgear-sld-static-element--selected) {
  background: color-mix(in srgb, var(--color-neutral-900) 62%, transparent);
  border-color: color-mix(in srgb, var(--color-blue-400) 52%, transparent);
  box-shadow:
    0 0 0 2px color-mix(in srgb, var(--color-blue-400) 28%, transparent),
    0 14px 26px rgb(14 165 233 / 0.14);
}

:global(.dark .switchgear-sld-static-element:hover) {
  background: color-mix(in srgb, var(--color-neutral-900) 58%, transparent);
  border-color: color-mix(in srgb, var(--color-blue-400) 36%, transparent);
}
</style>
