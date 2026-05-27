<template>
  <div class="disconnected-mobile-layout">
    <div class="disconnected-mobile-layout__message">
      <p v-if="status === 'initial'" class="disconnected-mobile-layout__title">🔌 Connecting to server...</p>
      <div v-else>
        <p class="disconnected-mobile-layout__title">⚠️ Lost connection</p>
        <p class="disconnected-mobile-layout__detail">Trying to reconnect…</p>
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

<style scoped>
.disconnected-mobile-layout {
  display: flex;
  height: 100dvh;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  background: var(--color-neutral-50);
  color: var(--color-neutral-800);
  font-family: var(--font-mono);
}

.disconnected-mobile-layout__message {
  padding: 1.5rem;
  text-align: center;
}

.disconnected-mobile-layout__title {
  margin: 0;
  font-size: var(--text-lg);
  font-weight: 600;
}

.disconnected-mobile-layout__detail {
  margin: 0.5rem 0 0;
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
}

:global(.dark .disconnected-mobile-layout) {
  background: var(--color-neutral-900);
  color: var(--color-neutral-200);
}
</style>
