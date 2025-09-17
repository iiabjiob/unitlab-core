<template>
  <div class="h-dvh flex flex-col bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200 font-mono">
    <div class="flex flex-1 overflow-hidden">
      <!-- Left aside (меню всегда видно) -->
      <AppAside />

      <!-- Main area -->
      <main class="flex-1 flex items-center justify-center">
        <div class="text-center">
          <p v-if="status === 'initial'" class="text-lg font-semibold">🔌 Connecting to server...</p>
          <div v-else>
            <p class="text-lg font-semibold">⚠️ Lost connection</p>
            <p class="text-sm text-neutral-500 mt-2">Trying to reconnect…</p>
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

const wsStore = useWebSocketStore()

const status = computed(() => {
  if (!wsStore.isConnected && !wsStore.everConnected) return "initial"
  if (wsStore.isConnected) return "connected"
  return "lost"
})
</script>
