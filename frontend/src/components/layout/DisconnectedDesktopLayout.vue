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
      <main class="flex-1 flex flex-col overflow-hidden">
        <!-- Main -->
        <div class="flex-1 flex items-center justify-center">
          <div class="text-center">
            <p v-if="status === 'initial'" class="text-lg font-semibold animate-pulse">
              🔌 Connecting to server...
            </p>
            <div v-else>
              <p class="text-lg font-semibold">⚠️ Lost connection</p>
              <p class="text-sm text-neutral-500 mt-2 animate-pulse">Trying to reconnect…</p>
            </div>
          </div>
        </div>

      </main>

    </div>
  </div>
</template>

<script setup lang="ts">
import AppAside from "./DesktopAside.vue"
import { useWebSocketStore } from "@/stores/websocketStore"
import { computed } from "vue"
import { storeToRefs } from "pinia"

const wsStore = useWebSocketStore()
const {
  isConnected: wsIsConnected,
  everConnected: wsEverConnected,
  hasStarted: wsHasStarted,
  isConnecting: wsIsConnecting,
  reconnectAttempts: wsReconnectAttempts,
} = storeToRefs(wsStore)

const status = computed(() => {
  if (wsIsConnected.value) return "connected"
  if (!wsHasStarted.value) return "initial"
  if (wsIsConnecting.value && !wsEverConnected.value && wsReconnectAttempts.value === 0) {
    return "initial"
  }
  return "lost"
})
</script>
