<template>
  <div class="h-dvh flex flex-col bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200 font-mono">
    <!-- Header with burger + status -->
    <AppHeader @open-drawer="isDrawerOpen = true">
      <template #left>
        <div class="flex gap-3 items-center">
          <AppLogo />
          <OnlineStatusComponent :status="status" />
        </div>
      </template>
      <template #right>
        <TimeComponent />
      </template>
    </AppHeader>

    <!-- Header bulk actions -->
    <div v-if="meta.headerBulkActions" class="shrink-0">
      <slot name="header-bulk-actions" />
    </div>

    <!-- Main content -->
    <main class="flex-1 overflow-auto relative">
      <RouterView />

      <!-- Floating handle to open properties -->
      <button
        v-if="meta.rightAside && !isPropsOpen"
        class="absolute top-1/2 right-0 -translate-y-1/2 w-5 h-16 flex items-center justify-center bg-neutral-200 dark:bg-neutral-700 rounded-l cursor-pointer shadow"
        @click="isPropsOpen = true"
      >
        ‹
      </button>
    </main>

    <!-- Event log fixed at the bottom -->
    <div
      v-if="meta.globalEventLog"
      class="h-40 overflow-y-auto border-t border-neutral-200 dark:border-neutral-700"
    >
      <EventLog />
    </div>

    <!-- Bottom validator -->
    <BottomValidatorResizable v-if="meta.bottomValidator" :errors="3" :warnings="2">
      <BottomValidator
        :messages="[
          '💥 Device ID not found',
          '⚠️ Signal name too long',
          '⚠️ Unused template reference'
        ]"
      />
    </BottomValidatorResizable>

    <!-- Drawer with navigation -->
    <AppDrawer :open="isDrawerOpen" @close="isDrawerOpen = false">
      <AppMenu />
    </AppDrawer>

    <!-- Drawer: properties -->
    <AppDrawer v-if="meta.rightAside" :open="isPropsOpen" @close="isPropsOpen = false" side="right">
      <RightAside />
    </AppDrawer>
  </div>
</template>

<script setup lang="ts">
import { ref, computed } from "vue"
import { useRoute } from "vue-router"

import AppHeader from "./AppHeader.vue"
import AppDrawer from "./AppDrawer.vue"
import EventLog from "./EventLog.vue"
import AppMenu from "./AppMenu.vue"
import AppLogo from "./AppLogo.vue"
import OnlineStatusComponent from "../OnlineStatusComponent.vue"
import BottomValidatorResizable from "./BottomValidatorResizable.vue"
import BottomValidator from "./BottomValidator.vue"
import TimeComponent from "../TimeComponent.vue"
import RightAside from "./RightAside.vue"
import { useWebSocketStore } from "@/stores/websocketStore"

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
  rightAside: route.meta.rightAside ?? true,
  bottomValidator: route.meta.bottomValidator ?? true,
  globalEventLog: route.meta.globalEventLog ?? true,
  headerBulkActions: route.meta.headerBulkActions ?? false,
}))
</script>
