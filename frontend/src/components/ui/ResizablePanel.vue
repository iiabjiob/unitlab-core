<!-- src/components/ui/ResizablePanel.vue -->
<template>
  <div
    ref="panelRef"
    class="resizable-panel"
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

import { localSettingsKeys, readNumberLocalSetting, writeLocalSetting } from "@/services/localSettingsStorage"

const emit = defineEmits<{
  (e: "size-change", size: number): void
}>()

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
      return "resizable-panel--left"
    case "right":
      return "resizable-panel--right"
    case "top":
      return "resizable-panel--top"
    case "bottom":
      return "resizable-panel--bottom"
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
    return [
      "resizable-panel__handle",
      "resizable-panel__handle--vertical",
      props.placement === "left" ? "resizable-panel__handle--right" : "resizable-panel__handle--left",
    ]
  } else {
    return [
      "resizable-panel__handle",
      "resizable-panel__handle--horizontal",
      props.placement === "top" ? "resizable-panel__handle--bottom" : "resizable-panel__handle--top",
    ]
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

    // Flip delta for right and bottom handles
    if (props.placement === "right" || props.placement === "bottom") {
      delta = -delta
    }

    let newSize = startSize + delta
    newSize = Math.max(props.minSize ?? 160, Math.min(props.maxSize ?? 400, newSize))
    size.value = newSize
    emit("size-change", newSize)
  }

  function onMouseUp() {
    if (props.storageKey) {
      writeLocalSetting(
        localSettingsKeys.resizablePanelSize(props.storageKey),
        Math.trunc(size.value),
        { legacyKeys: [props.storageKey] },
      )
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
    const saved = readNumberLocalSetting(
      localSettingsKeys.resizablePanelSize(props.storageKey),
      null,
      { legacyKeys: [props.storageKey] },
    )
    if (saved !== null) {
      size.value = Math.max(props.minSize ?? 160, Math.min(props.maxSize ?? 400, Math.trunc(saved)))
    }
  }
  emit("size-change", size.value)
})
</script>

<style scoped>
.resizable-panel {
  display: flex;
  position: relative;
}

.resizable-panel--left,
.resizable-panel--right {
  flex-direction: column;
  height: 100%;
}

.resizable-panel--top,
.resizable-panel--bottom {
  flex-direction: column;
  width: 100%;
}

.resizable-panel--left {
  border-right: 1px solid var(--color-neutral-200);
}

.resizable-panel--right {
  border-left: 1px solid var(--color-neutral-200);
}

.resizable-panel--top {
  border-bottom: 1px solid var(--color-neutral-200);
}

.resizable-panel--bottom {
  border-top: 1px solid var(--color-neutral-200);
}

.resizable-panel__handle {
  position: absolute;
}

.resizable-panel__handle:hover {
  background: var(--color-neutral-300);
}

.resizable-panel__handle--vertical {
  cursor: col-resize;
  height: 100%;
  top: 0;
  width: 0.25rem;
}

.resizable-panel__handle--horizontal {
  cursor: row-resize;
  height: 0.25rem;
  left: 0;
  width: 100%;
}

.resizable-panel__handle--left {
  left: 0;
}

.resizable-panel__handle--right {
  right: 0;
}

.resizable-panel__handle--top {
  top: 0;
}

.resizable-panel__handle--bottom {
  bottom: 0;
}

.dark .resizable-panel--left {
  border-right-color: var(--color-neutral-700);
}

.dark .resizable-panel--right {
  border-left-color: var(--color-neutral-700);
}

.dark .resizable-panel--top {
  border-bottom-color: var(--color-neutral-700);
}

.dark .resizable-panel--bottom {
  border-top-color: var(--color-neutral-700);
}

.dark .resizable-panel__handle:hover {
  background: var(--color-neutral-600);
}
</style>
