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
