<template>
  <div
    class="h-dvh flex flex-col bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200 font-mono"
  >
    <div class="flex flex-1 overflow-hidden">
      <!-- Left aside -->
      <div
        class="bg-white dark:bg-neutral-800 border-r border-neutral-200 dark:border-neutral-700 w-60 shrink-0"
      >
        <AppAside />
      </div>

      <!-- Center workspace -->
      <div class="flex-1 flex flex-col overflow-hidden">
        <!-- Main -->
        <main class="flex-1 flex items-center justify-center">
          <div class="text-center">
            <p v-if="status === 'initial'" class="text-lg font-semibold animate-pulse">
              🔌 Connecting to server...
            </p>
            <div v-else>
              <p class="text-lg font-semibold">⚠️ Lost connection</p>
              <p class="text-sm text-neutral-500 mt-2 animate-pulse">Trying to reconnect…</p>
            </div>
          </div>
        </main>

        <!-- Bottom aside placeholder -->
        <div
          class="h-40 border-t border-neutral-200 dark:border-neutral-700 bg-white dark:bg-neutral-800 flex flex-col"
        >
          <div class="px-3 py-2 border-b border-neutral-200 dark:border-neutral-700">
            <h4 class="font-bold text-sm">Event Log</h4>
          </div>
          <div class="flex-1 p-3 space-y-2 text-xs">
            <div class="h-3 rounded bg-neutral-200 dark:bg-neutral-700 animate-pulse" />
            <div class="h-3 rounded bg-neutral-200 dark:bg-neutral-700 animate-pulse" />
            <div class="h-3 rounded bg-neutral-200 dark:bg-neutral-700 animate-pulse" />
          </div>
        </div>
      </div>

      <!-- Right aside placeholder -->
      <div
        class="w-72 border-l border-neutral-200 dark:border-neutral-700 bg-white dark:bg-neutral-800 flex flex-col"
      >
        <div class="px-3 py-2 border-b border-neutral-200 dark:border-neutral-700">
          <h4 class="font-bold text-sm">Properties</h4>
        </div>
        <div class="flex-1 p-3 space-y-3 text-xs">
          <div class="h-4 rounded bg-neutral-200 dark:bg-neutral-700 animate-pulse" />
          <div class="h-4 rounded bg-neutral-200 dark:bg-neutral-700 animate-pulse" />
          <div class="h-4 rounded bg-neutral-200 dark:bg-neutral-700 animate-pulse" />
          <div class="h-4 rounded bg-neutral-200 dark:bg-neutral-700 animate-pulse" />
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import AppAside from "./DesktopAside.vue"
import { useWebSocketStore } from "@/stores/websocketStore"
import { computed } from "vue"

const wsStore = useWebSocketStore()

const status = computed(() => {
  if (!wsStore.isConnected && !wsStore.everConnected) return "initial"
  if (wsStore.isConnected) return "connected"
  return "lost"
})
</script>
