<template>
  <div
    class="relative h-full border-l border-neutral-200 dark:border-neutral-800 transition-[width] duration-100 overflow-hidden"
    :style="{ width: collapsed ? '0px' : width + 'px' }"
  >
    <RightAside v-if="!collapsed" @collapse="collapsed = true" />

    <!-- Resize handle -->
    <div
      v-if="!collapsed"
      class="absolute top-0 left-0 h-full w-1 cursor-col-resize hover:w-1 hover:bg-neutral-300 dark:hover:bg-neutral-600 transition-all"
      @mousedown="startResize"
    ></div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, watch } from "vue"
import RightAside from "./RightAside.vue"
import { useSelectionStore } from "@/stores/selectionStore"

const width = ref(280)
const collapsed = ref(false)
const selection = useSelectionStore()

function startResize(e: MouseEvent) {
  const startX = e.clientX
  const startWidth = width.value

  document.body.style.userSelect = "none"
  document.body.style.cursor = "col-resize"

  function onMouseMove(ev: MouseEvent) {
    const delta = startX - ev.clientX
    width.value = Math.max(100, startWidth + delta)
  }

  function onMouseUp() {
    localStorage.setItem("right-aside-width", String(width.value))
    localStorage.setItem("right-aside-collapsed", String(collapsed.value))

    document.body.style.userSelect = ""
    document.body.style.cursor = ""

    window.removeEventListener("mousemove", onMouseMove)
    window.removeEventListener("mouseup", onMouseUp)
  }

  window.addEventListener("mousemove", onMouseMove)
  window.addEventListener("mouseup", onMouseUp)
}

onMounted(() => {
  const savedCollapsed = localStorage.getItem("right-aside-collapsed")
  if (savedCollapsed !== null) collapsed.value = savedCollapsed === "true"

  const savedWidth = localStorage.getItem("right-aside-width")
  if (savedWidth) width.value = parseInt(savedWidth, 10)
})

// сохраняем настройки
watch([collapsed, width], ([c, w]) => {
  localStorage.setItem("right-aside-collapsed", String(c))
  if (!c) localStorage.setItem("right-aside-width", String(w))
})

// 👉 автооткрытие панели при выборе нода
watch(() => selection.selected, (val) => {
  if (val) collapsed.value = false
})
</script>

