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
    class="absolute flex items-center justify-center rounded-md transition focus-visible:outline-none"
    :class="selected ? 'ring-2 ring-sky-400/60 ring-offset-2 ring-offset-white dark:ring-offset-neutral-950' : ''"
    :style="rootStyle"
    style="color: #2563eb"
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
      class="overflow-visible"
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
      class="overflow-visible"
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