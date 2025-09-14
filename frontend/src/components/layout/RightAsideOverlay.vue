<template>
  <div
    class="absolute top-0 right-0 h-full shadow-lg z-20 overflow-hidden
           transition-transform duration-100 ease-out"
    :style="{ width: width + 'px' }"
    :class="collapsed ? 'translate-x-full' : 'translate-x-0'"
  >
    <!-- Content -->
    <div class="h-full w-full flex flex-col bg-neutral-100 dark:bg-neutral-800">
      <!-- Header -->
      <div class="flex items-center justify-end px-3 pt-3 font-semibold">
        <button
          class="w-6 h-6 flex items-center justify-center rounded hover:bg-neutral-200 dark:hover:bg-neutral-700 cursor-pointer"
          @click="collapsed = true"
        >
          ✖
        </button>
      </div>

      <!-- Properties -->
      <PropertiesPanel
        v-if="selection.selectedItem"
        :schema="resolveSchema(selection.selected!.type)"
        :item="selection.selectedItem!"
        @update="onUpdate"
      />
      <div v-else class="flex-1 flex items-center justify-center text-xs text-neutral-500">
        No item selected
      </div>
    </div>

    <!-- Resize handle -->
    <div
      v-if="!collapsed"
      class="absolute top-0 left-0 h-full w-1 cursor-col-resize hover:bg-neutral-300 dark:hover:bg-neutral-600 transition-all"
      @mousedown="startResize"
    ></div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, watch } from "vue"
import { useSelectionStore } from "@/stores/selectionStore"
import { resolveSchema } from "@/property-schemas/propertySchemas"
import PropertiesPanel from "../PropertiesPanel.vue"
import { updateEntity } from "@/utils/updateEntity"

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

watch([collapsed, width], ([c, w]) => {
  localStorage.setItem("right-aside-collapsed", String(c))
  if (!c) localStorage.setItem("right-aside-width", String(w))
})

watch(() => selection.selected, (val) => {
  if (val) collapsed.value = false
})

async function onUpdate(key: any, value: any) {
  if (!selection.selected || !selection.selectedItem) return
  const { type } = selection.selected
  await updateEntity(type as any, selection.selectedItem as any, key as any, value)
}
</script>
