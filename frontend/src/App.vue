<template>
  <div class="h-dvh flex flex-col text-base text-neutral-800 dark:text-neutral-200 bg-neutral-50 dark:bg-neutral-900 font-mono">
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
          <p class="text-lg font-semibold">🔌 Connecting to server...</p>
        </div>
      </div>
    </template>

    <template v-else-if="wsStatus === 'lost'">

      <div class="flex flex-1 overflow-hidden">
        <AppAside />
        <main class="flex-1 flex flex-col overflow-hidden">
          <div class="flex-1 flex items-center justify-center">
            <div class="text-center">
              <p class="text-lg font-semibold">⚠️ Lost connection</p>
              <p class="text-sm text-neutral-500 mt-2">Trying to reconnect…</p>
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
