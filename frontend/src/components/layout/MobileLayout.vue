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


    <!-- Main content -->
    <main class="flex-1 overflow-auto relative">
      <RouterView />
    </main>


    <!-- Navigation -->
    <SlideOver :open="isDrawerOpen" placement="right" title="Menu" :widthPx="360" @close="isDrawerOpen = false">
      <AppMenu />
    </SlideOver>

    <!-- Properties -->
    <SlideOver
      v-if="meta.rightAside"
      ref="propsPanel"
      :open="isPropsOpen"
      placement="bottom"
      :maxHeightVh="75"
      @close="selection.clear()"
    >
    <div class="slide-over-content">

      <PropertiesPanel
          v-if="selection.selectedItem"
          :schema="resolveSchema(selection.selected!.type)"
          :item="selection.selectedItem!"
          @update="onUpdate"
        />
        <div
          v-else
          class="flex-1 flex items-center justify-center text-xs text-neutral-500"
        >
          No item selected
        </div>

    </div>
    </SlideOver>
  </div>
</template>

<script setup lang="ts">
import { ref, computed } from "vue"
import { useRoute } from "vue-router"

import AppMenu from "./AppMenu.vue"
import AppLogo from "./AppLogo.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"
import TimeComponent from "../misc/TimeComponent.vue"
import { useWebSocketStore } from "@/stores/websocketStore"
import SlideOver from "../ui/SlideOver.vue"
import { useSelectionStore } from "@/stores/selectionStore"
import { resolveSchema } from "@/property-schemas/propertySchemas"
import MobileHeader from "./MobileHeader.vue"
import { updateEntity } from "@/property-schemas/updateEntity"
import PropertiesPanel from "../properties/PropertiesPanel.vue"

const selection = useSelectionStore()

async function onUpdate(key: string, value: any) {
  if (!selection.selected || !selection.selectedItem) return
  const { type } = selection.selected
  await updateEntity(type as any, selection.selectedItem as any, key, value)
}

// Drawer state
const isDrawerOpen = ref(false)
const isPropsOpen = ref(false)

// WebSocket connection status
const wsStore = useWebSocketStore()
const status = computed(() => {
  if (wsStore.isConnected) return "online"
  if (!wsStore.isConnected && wsStore.everConnected) return "offline"
  return "offline"
})

// Read route meta
const route = useRoute()
const meta = computed(() => ({
  toolbar: route.meta.toolbar ?? true,
  leftAside: route.meta.leftAside ?? true,
  rightAside: route.meta.rightAside ?? true,
  bottomAside: route.meta.bottomAside ?? true,
  toolbarComponent: route.meta.toolbarComponent ?? null,
}))
</script>
