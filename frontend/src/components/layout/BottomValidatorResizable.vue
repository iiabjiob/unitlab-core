<template>
  <div
    class="relative border-t border-neutral-200 dark:border-neutral-800 bg-white dark:bg-neutral-900 transition-[height] duration-200 ease-in-out select-none"
    :style="{ height: expanded ? height + 'px' : COMPACT_H + 'px' }"
  >
    <!-- Compact bar (always visible) -->
    <div
      class="flex items-center justify-between px-3 h-6 text-xs cursor-pointer hover:bg-neutral-50 dark:hover:bg-neutral-950"
      @click="toggle()"
    >
      <div class="flex items-center gap-3 font-bold">
        <div class="flex items-center gap-1">
          <span>🚫</span>
          <span class="text-xs">{{ errors }}</span>
        </div>
        <div class="flex items-center gap-1">
          <span>⚠️</span>
          <span class="text-xs">{{ warnings }}</span>
        </div>
      </div>
    </div>

    <!-- Resizable content (only when expanded) -->
    <div v-if="expanded" class="h-[calc(100%-24px)]">
      <slot />
    </div>

    <!-- Resize handle at the top edge -->
    <div
      v-if="expanded"
      class="absolute top-0 left-0 w-full h-1 cursor-row-resize hover:h-1 hover:bg-neutral-300 dark:hover:bg-neutral-600 transition-all"
      @mousedown="startResize"
      title="Drag to resize"
    ></div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, watch } from "vue"

// Public API (errors/warnings can come from a store later)
const props = withDefaults(defineProps<{
  errors?: number
  warnings?: number
}>(), {
  errors: 0,
  warnings: 0,
})

const errors   = ref(props.errors)
const warnings = ref(props.warnings)

// Persistent UI state
const expanded = ref(true)
const height   = ref(200)

const MIN_H      = 120          // minimal expanded height
const COMPACT_H  = 24           // compact strip height

function toggle() {
  // Toggle between compact and expanded
  expanded.value = !expanded.value
}

// Drag-resize logic
function startResize(e: MouseEvent) {
  const startY = e.clientY
  const startH = height.value

  // Disable selection + set cursor
  document.body.style.userSelect = "none"
  document.body.style.cursor     = "row-resize"

  function onMouseMove(ev: MouseEvent) {
    const delta = startY - ev.clientY
    const newH  = Math.max(MIN_H, startH + delta)
    height.value = newH
  }

  function onMouseUp() {
    // Persist
    localStorage.setItem("bottom-validator-height", String(height.value))
    localStorage.setItem("bottom-validator-expanded", String(expanded.value))

    // Restore styles
    document.body.style.userSelect = ""
    document.body.style.cursor     = ""

    window.removeEventListener("mousemove", onMouseMove)
    window.removeEventListener("mouseup", onMouseUp)
  }

  window.addEventListener("mousemove", onMouseMove)
  window.addEventListener("mouseup", onMouseUp)
}

// Restore persisted state
onMounted(() => {
  const savedExpanded = localStorage.getItem("bottom-validator-expanded")
  if (savedExpanded !== null) expanded.value = savedExpanded === "true"

  const savedH = localStorage.getItem("bottom-validator-height")
  if (savedH) height.value = Math.max(MIN_H, parseInt(savedH, 10))
})

// Persist on change
watch([expanded, height], ([ex, h]) => {
  localStorage.setItem("bottom-validator-expanded", String(ex))
  if (ex) localStorage.setItem("bottom-validator-height", String(h))
})
</script>

<style scoped>
/* Optional: prevent accidental text selection inside during drag */
</style>
