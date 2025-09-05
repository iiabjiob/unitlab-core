<template>
  <div class="h-dvh flex flex-col bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200 font-mono">
    <!-- Header with burger + status -->
    <AppHeader @open-drawer="isDrawerOpen = true">
      <template #left>
        <AppLogo/>
      </template>
      <template #right>
        <OnlineStatusComponent :status="status" />
      </template>
    </AppHeader>

    <!-- Main content -->
    <main class="flex-1 overflow-auto">
      <RouterView />
    </main>

    <!-- Event log fixed at the bottom -->
    <div class="h-40 overflow-y-auto border-t border-neutral-200 dark:border-neutral-700">
      <EventLog />
    </div>

    <!-- Drawer with navigation only -->
    <AppDrawer :open="isDrawerOpen" @close="isDrawerOpen = false">
      <AppMenu/>
    </AppDrawer>
  </div>
</template>

<script setup lang="ts">
import { ref, computed } from "vue"
import AppHeader from "./AppHeader.vue"
import AppDrawer from "./AppDrawer.vue"
import EventLog from "./EventLog.vue"
import AppMenu from "./AppMenu.vue"
import { useWebSocketStore } from "@/stores/websocketStore"
import AppLogo from "../AppLogo.vue"
import OnlineStatusComponent from "../OnlineStatusComponent.vue"

// Drawer state
const isDrawerOpen = ref(false)

const wsStore = useWebSocketStore()

const status = computed(() => {
  if (wsStore.isConnected) return "online"
  if (!wsStore.isConnected && wsStore.everConnected) return "offline"
  return "connecting"
})

</script>
