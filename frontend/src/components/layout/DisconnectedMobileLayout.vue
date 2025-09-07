<template>
  <div class="h-dvh flex flex-col items-center justify-center bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200 font-mono">
    <div class="text-center p-6">
      <p v-if="status === 'initial'" class="text-lg font-semibold">🔌 Connecting to server...</p>
      <div v-else>
        <p class="text-lg font-semibold">⚠️ Lost connection</p>
        <p class="text-sm text-neutral-500 mt-2">Trying to reconnect…</p>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { useWebSocketStore } from "@/stores/websocketStore"
import { computed } from "vue"

const wsStore = useWebSocketStore()

const status = computed(() => {
  if (!wsStore.isConnected && !wsStore.everConnected) return "initial"
  if (wsStore.isConnected) return "connected"
  return "lost"
})
</script>
