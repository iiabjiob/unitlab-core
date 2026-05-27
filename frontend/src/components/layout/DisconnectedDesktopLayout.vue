<template>
  <div class="disconnected-desktop-layout">
    <div class="disconnected-desktop-layout__body">
      <div class="disconnected-desktop-layout__aside">
        <AppAside />
      </div>

      <main class="disconnected-desktop-layout__main">
        <div class="disconnected-desktop-layout__content">
          <div class="disconnected-desktop-layout__message">
            <p v-if="status === 'initial'" class="disconnected-desktop-layout__title disconnected-desktop-layout__pulse">
              🔌 Connecting to server...
            </p>
            <div v-else>
              <p class="disconnected-desktop-layout__title">⚠️ Lost connection</p>
              <p class="disconnected-desktop-layout__detail disconnected-desktop-layout__pulse">Trying to reconnect…</p>
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

<style scoped>
.disconnected-desktop-layout {
  display: flex;
  height: 100dvh;
  flex-direction: column;
  background: var(--color-neutral-50);
  color: var(--color-neutral-800);
  font-family: var(--font-mono);
}

.disconnected-desktop-layout__body {
  display: flex;
  flex: 1 1 auto;
  overflow: hidden;
}

.disconnected-desktop-layout__aside {
  width: 15rem;
  flex-shrink: 0;
  border-right: 1px solid var(--color-neutral-200);
  background: var(--color-white);
}

.disconnected-desktop-layout__main {
  display: flex;
  flex: 1 1 auto;
  flex-direction: column;
  overflow: hidden;
}

.disconnected-desktop-layout__content {
  display: flex;
  flex: 1 1 auto;
  align-items: center;
  justify-content: center;
}

.disconnected-desktop-layout__message {
  text-align: center;
}

.disconnected-desktop-layout__title {
  margin: 0;
  font-size: var(--text-lg);
  font-weight: 600;
}

.disconnected-desktop-layout__detail {
  margin: 0.5rem 0 0;
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
}

.disconnected-desktop-layout__pulse {
  animation: disconnected-pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;
}

@keyframes disconnected-pulse {
  50% {
    opacity: 0.5;
  }
}

:global(.dark .disconnected-desktop-layout) {
  background: var(--color-neutral-900);
  color: var(--color-neutral-200);
}

:global(.dark .disconnected-desktop-layout__aside) {
  border-right-color: var(--color-neutral-700);
  background: var(--color-neutral-800);
}
</style>
