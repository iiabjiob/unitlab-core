<template>
  <div class="h-dvh flex flex-col bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200 font-mono">
    <!-- Header with burger + status -->
    <MobileHeader @open-drawer="isDrawerOpen = true">
      <template #left>
        <div class="flex gap-3 items-center">
          <AppLogo />
          <OnlineStatusComponent :status="status" />
        </div>
      </template>
      <template #right>
        <TimeComponent />
      </template>
    </MobileHeader>

    <div class="border-b border-neutral-200 bg-white px-5 py-3 dark:border-neutral-800 dark:bg-neutral-900">
      <ProjectSwitcher variant="compact" />
      <div class="mt-3 flex flex-wrap gap-4 text-[11px] text-neutral-500 dark:text-neutral-400">
        <div class="min-w-[140px] flex-1">
          <p class="text-[10px] uppercase tracking-[0.3em]">Created</p>
          <p class="font-mono text-xs text-neutral-900 dark:text-neutral-100">{{ createdLabel }}</p>
        </div>
        <div class="min-w-[140px] flex-1">
          <p class="text-[10px] uppercase tracking-[0.3em]">Modified</p>
          <p class="font-mono text-xs text-neutral-900 dark:text-neutral-100">{{ updatedLabel }}</p>
        </div>
      </div>
    </div>


    <!-- Main content -->
    <main class="flex-1 overflow-auto relative">
      <RouterView />
    </main>


    <!-- Navigation -->
    <SlideOver :open="isDrawerOpen" placement="right" title="Menu" :widthPx="360" @close="isDrawerOpen = false">
      <AppMenu />
    </SlideOver>

  </div>
</template>

<script setup lang="ts">
import { ref, computed } from "vue"

import AppMenu from "./AppMenu.vue"
import AppLogo from "./AppLogo.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"
import TimeComponent from "../misc/TimeComponent.vue"
import { useWebSocketStore } from "@/stores/websocketStore"
import SlideOver from "../ui/SlideOver.vue"
import MobileHeader from "./MobileHeader.vue"
import ProjectSwitcher from "@/components/projects/ProjectSwitcher.vue"
import { useProjectStore } from "@/stores/projectStore"

// Drawer state
const isDrawerOpen = ref(false)

// WebSocket connection status
const wsStore = useWebSocketStore()
const status = computed(() => {
  if (wsStore.isConnected) return "online"
  if (!wsStore.isConnected && wsStore.everConnected) return "offline"
  return "offline"
})

const projectStore = useProjectStore()

function formatProjectDate(value?: string | null) {
  if (!value) return "--"
  const date = new Date(value)
  if (Number.isNaN(date.getTime())) return "--"
  return date.toISOString().slice(0, 10)
}

const createdLabel = computed(() => formatProjectDate(projectStore.activeProject?.created_at))
const updatedLabel = computed(() => formatProjectDate(projectStore.activeProject?.updated_at))

</script>
