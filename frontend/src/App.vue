<template>
  <div class="h-dvh flex flex-col text-base text-gray-800 dark:text-gray-200 bg-gray-50 dark:bg-gray-900 font-mono">
    <template v-if="wsStatus === 'connected'">
      <!-- Нормальный UI -->
      <div class="flex flex-1 overflow-hidden">
        <AppAside />
        <main class="flex-1 overflow-auto">
          <RouterView />
        </main>
      </div>
    </template>

    <template v-else-if="wsStatus === 'initial'">
      <!-- Заглушка при первом запуске -->
      <div class="flex-1 flex items-center justify-center">
        <div class="text-center">
          <!-- <img src="/logo.svg" class="h-16 w-16 mx-auto mb-4" /> -->
          <p class="text-lg font-semibold">🔌 Connecting to server...</p>
        </div>
      </div>
    </template>

    <template v-else-if="wsStatus === 'lost'">

      <div class="flex flex-1 overflow-hidden">
        <AppAside />
        <main class="flex-1 flex flex-col overflow-hidden">
          <!-- <AppHeader class="border-b border-gray-200 dark:border-gray-800" /> -->
          <div class="flex-1 flex items-center justify-center">
            <div class="text-center">
              <p class="text-lg font-semibold">⚠️ Lost connection</p>
              <p class="text-sm text-gray-500 mt-2">Trying to reconnect…</p>
            </div>
          </div>
        </main>
      </div>
    </template>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import AppAside from "./components/AppAside.vue"
import { useWebSocketStore } from "@/stores/websocketStore"

const wsStore = useWebSocketStore()

const wsStatus = computed(() => {
  if (!wsStore.isConnected && !wsStore.everConnected) return "initial"
  if (wsStore.isConnected) return "connected"
  return "lost"
})
</script>
