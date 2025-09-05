<template>
  <div
    ref="asideRef"
    class="relative h-full flex flex-col border-r border-neutral-200 dark:border-neutral-800 bg-white dark:bg-neutral-900"
    :style="{ width: width + 'px' }"
  >
    <!-- Content -->
    <slot />

    <!-- Resize handle -->
    <div
      class="absolute top-0 right-0 h-full w-1 cursor-col-resize hover:w-1 hover:bg-neutral-300 dark:hover:bg-neutral-600 transition-all"
      @mousedown="startResize"
    ></div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from "vue"

const minWidth = 220
const maxWidth = 400
const width = ref(240) // default width
const asideRef = ref<HTMLElement | null>(null)

function startResize(e: MouseEvent) {
  const startX = e.clientX
  const startWidth = width.value

  // disable text selection & set resize cursor
  document.body.style.userSelect = "none"
  document.body.style.cursor = "col-resize"

  function onMouseMove(ev: MouseEvent) {
    const delta = ev.clientX - startX
    let newWidth = startWidth + delta
    newWidth = Math.max(minWidth, Math.min(maxWidth, newWidth))
    width.value = newWidth
  }

  function onMouseUp() {
    localStorage.setItem("left-aside-width", String(width.value))
    window.removeEventListener("mousemove", onMouseMove)
    window.removeEventListener("mouseup", onMouseUp)

    // restore styles
    document.body.style.userSelect = ""
    document.body.style.cursor = ""
  }

  window.addEventListener("mousemove", onMouseMove)
  window.addEventListener("mouseup", onMouseUp)
}

onMounted(() => {
  const saved = localStorage.getItem("left-aside-width")
  if (saved) width.value = parseInt(saved)
})
</script>
