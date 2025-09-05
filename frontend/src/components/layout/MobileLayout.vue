<template>
  <div class="h-dvh flex flex-col bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200 font-mono">
    <!-- Header with burger + status -->
    <AppHeader @open-drawer="isDrawerOpen = true">
      <template #left>
        <div class="flex gap-3 items-center">

          <AppLogo/>
          <OnlineStatusComponent :status="status" />
        </div>
      </template>
      <template #right>
        <TimeComponent/>
      </template>
    </AppHeader>

    <!-- Main content -->
    <main class="flex-1 overflow-auto relative">
      <RouterView />

      <!-- Floating handle to open properties -->
      <button
        v-if="!isPropsOpen"
        class="absolute top-1/2 right-0 -translate-y-1/2 w-5 h-16 flex items-center justify-center bg-neutral-200 dark:bg-neutral-700 rounded-l cursor-pointer shadow"
        @click="isPropsOpen = true"
      >
        ‹
      </button>
    </main>

    <!-- Event log fixed at the bottom -->
    <div class="h-40 overflow-y-auto border-t border-neutral-200 dark:border-neutral-700">
      <EventLog />
    </div>
    <!-- Bottom validator (only inside workspace, like VS Code Problems) -->
        <BottomValidatorResizable :errors="3" :warnings="2">
          <BottomValidator
            :messages="[
              '❌ Device ID not found',
              '⚠️ Signal name too long',
              '⚠️ Unused template reference'
            ]"
          />
        </BottomValidatorResizable>

    <!-- Drawer with navigation only -->
    <AppDrawer :open="isDrawerOpen" @close="isDrawerOpen = false">
      <AppMenu/>
    </AppDrawer>

    <!-- Drawer: properties -->
    <AppDrawer :open="isPropsOpen" @close="isPropsOpen = false" side="right">
      <RightAside />
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
import AppLogo from "./AppLogo.vue"
import OnlineStatusComponent from "../OnlineStatusComponent.vue"
import BottomValidatorResizable from "./BottomValidatorResizable.vue"
import BottomValidator from "./BottomValidator.vue"
import TimeComponent from "../TimeComponent.vue"

// Drawer state
const isDrawerOpen = ref(false)
const isPropsOpen = ref(false)

const wsStore = useWebSocketStore()

const status = computed(() => {
  if (wsStore.isConnected) return "online"
  if (!wsStore.isConnected && wsStore.everConnected) return "offline"
  return "offline"
})

</script>
