<template>
  <aside
    class="bg-white dark:bg-neutral-800 flex flex-col h-full relative"
    :class="status === 'offline' ? 'opacity-60 pointer-events-none select-none' : ''"
    :aria-disabled="status === 'offline'"
  >
    <!-- Header -->
    <div class="px-5 py-3 border-b border-neutral-200 dark:border-neutral-700">
      <div class="flex justify-between items-center gap-3">
        <AppLogo />
        <OnlineStatusComponent :status="status" />
      </div>
      <TimeComponent class="text-sm ml-auto" />
    </div>

    <!-- Menu (растягивается на всё доступное место, но учитывает высоту лога) -->
    <AppMenu class="text-base overflow-auto" :style="{ flex: 1, marginBottom: eventLogHeight + 'px' }" />

    <!-- Event log (resizable) -->
    <div
      v-if="meta.globalEventLog"
      class="absolute bottom-0 left-0 right-0 border-t border-neutral-200 dark:border-neutral-700 bg-white dark:bg-neutral-800"
      :style="{ height: eventLogHeight + 'px' }"
    >
      <EventLog />
      <!-- Resize handle -->
      <div
        class="absolute top-0 left-0 right-0 h-1 cursor-row-resize hover:bg-neutral-300 dark:hover:bg-neutral-600 transition-all"
        @mousedown="startResize"
      ></div>
    </div>
  </aside>
</template>

<script setup lang="ts">
import { ref, computed, onMounted } from "vue"
import { useRoute } from "vue-router"
import { useWebSocketStore } from "@/stores/websocketStore"
import { useEventLogStore } from "@/stores/eventLogStore"
import AppMenu from "./AppMenu.vue"
import EventLog from "./EventLog.vue"
import AppLogo from "./AppLogo.vue"
import TimeComponent from "../misc/TimeComponent.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"

const wsStore = useWebSocketStore()
const eventLogStore = useEventLogStore()
const route = useRoute()

const status = computed(() => {
  if (wsStore.isConnected) return "online"
  if (!wsStore.isConnected && wsStore.everConnected) return "offline"
  return "offline"
})

// Meta flags
const meta = computed(() => ({
  globalEventLog: route.meta.globalEventLog ?? true,
}))

// --- Resize logic ---
const minHeight = 100
const maxHeight = 400
const defaultHeight = 200
const eventLogHeight = ref(defaultHeight)

function startResize(e: MouseEvent) {
  const startY = e.clientY
  const startHeight = eventLogHeight.value

  document.body.style.userSelect = "none"
  document.body.style.cursor = "row-resize"

  function onMouseMove(ev: MouseEvent) {
    const delta = startY - ev.clientY
    let newHeight = startHeight + delta
    newHeight = Math.max(minHeight, Math.min(maxHeight, newHeight))
    eventLogHeight.value = newHeight
  }

  function onMouseUp() {
    localStorage.setItem("aside-eventlog-height", String(eventLogHeight.value))
    window.removeEventListener("mousemove", onMouseMove)
    window.removeEventListener("mouseup", onMouseUp)
    document.body.style.userSelect = ""
    document.body.style.cursor = ""
  }

  window.addEventListener("mousemove", onMouseMove)
  window.addEventListener("mouseup", onMouseUp)
}

onMounted(() => {
  const saved = localStorage.getItem("aside-eventlog-height")
  if (saved) eventLogHeight.value = parseInt(saved, 10)
})
</script>
