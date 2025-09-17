<!-- src/components/ui/ResizablePanel.vue -->
<template>
  <div
    ref="panelRef"
    class="relative flex"
    :class="panelClasses"
    :style="panelStyle"
  >
    <!-- Content -->
    <slot />

    <!-- Resize handle -->
    <div
      v-if="resizable"
      :class="handleClasses"
      @mousedown="startResize"
    />
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, type PropType } from "vue"

const props = defineProps({
  placement: { type: String as PropType<"left" | "right" | "top" | "bottom">, required: true },
  storageKey: String,
  defaultSize: { type: Number, default: 240 },
  minSize: { type: Number, default: 160 },
  maxSize: { type: Number, default: 400 },
  resizable: { type: Boolean, default: true },
})

const size = ref(props.defaultSize ?? 240)
const panelRef = ref<HTMLElement | null>(null)

// panel flex direction / border
const panelClasses = computed(() => {
  switch (props.placement) {
    case "left":
      return "h-full flex-col border-r border-neutral-200 dark:border-neutral-800"
    case "right":
      return "h-full flex-col border-l border-neutral-200 dark:border-neutral-800"
    case "top":
      return "w-full flex-col border-b border-neutral-200 dark:border-neutral-800"
    case "bottom":
      return "w-full flex-col border-t border-neutral-200 dark:border-neutral-800"
  }
})

// panel size
const panelStyle = computed(() => {
  if (props.placement === "left" || props.placement === "right") {
    return { width: size.value + "px" }
  } else {
    return { height: size.value + "px" }
  }
})

// handle classes
const handleClasses = computed(() => {
  if (props.placement === "left" || props.placement === "right") {
    return "absolute top-0 h-full w-1 cursor-col-resize hover:bg-neutral-300 dark:hover:bg-neutral-600 " +
      (props.placement === "left" ? "right-0" : "left-0")
  } else {
    return "absolute left-0 w-full h-1 cursor-row-resize hover:bg-neutral-300 dark:hover:bg-neutral-600 " +
      (props.placement === "top" ? "bottom-0" : "top-0")
  }
})

function startResize(e: MouseEvent) {
  const isHorizontal = props.placement === "left" || props.placement === "right"
  const start = isHorizontal ? e.clientX : e.clientY
  const startSize = size.value

  document.body.style.userSelect = "none"
  document.body.style.cursor = isHorizontal ? "col-resize" : "row-resize"

  function onMouseMove(ev: MouseEvent) {
    const current = isHorizontal ? ev.clientX : ev.clientY
    let delta = current - start

    // для right и bottom инвертируем delta
    if (props.placement === "right" || props.placement === "bottom") {
      delta = -delta
    }

    let newSize = startSize + delta
    newSize = Math.max(props.minSize ?? 160, Math.min(props.maxSize ?? 400, newSize))
    size.value = newSize
  }

  function onMouseUp() {
    if (props.storageKey) {
      localStorage.setItem(props.storageKey, String(size.value))
    }
    window.removeEventListener("mousemove", onMouseMove)
    window.removeEventListener("mouseup", onMouseUp)
    document.body.style.userSelect = ""
    document.body.style.cursor = ""
  }

  window.addEventListener("mousemove", onMouseMove)
  window.addEventListener("mouseup", onMouseUp)
}


onMounted(() => {
  if (props.storageKey) {
    const saved = localStorage.getItem(props.storageKey)
    if (saved) size.value = parseInt(saved)
  }
})
</script>
