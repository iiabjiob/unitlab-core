<template>
  <div
    class="flex flex-wrap text-xs gap-x-2 border border-gray-300 dark:border-gray-700 rounded px-2 py-0.5 text-gray-600 dark:text-gray-400">
    <span>{{ timeStore.sourceLabel }}</span>
    <span class="tabular-nums font-mono">{{ timeStore.formatted }}</span>
  </div>
</template>

<script setup lang="ts">
import { onMounted, onUnmounted } from 'vue'
import { useWebSocketStore } from '@/stores/useWebsocketStore'
import { useTimeStore } from '@/stores/useTimeStore'

const wsStore  = useWebSocketStore()
const timeStore = useTimeStore()

const WS_CHANNEL = "time_sync";

function handle(payload: { timestamp: string; source?: string }) {
  timeStore.updateFromSync(payload)
}

onMounted(() => {
  wsStore.subscribe([WS_CHANNEL])
  wsStore.subscribeToChannel(WS_CHANNEL, handle)
})

onUnmounted(() => {
  wsStore.unsubscribe([WS_CHANNEL])
  wsStore.unsubscribeFromChannel(WS_CHANNEL, handle)
})

</script>
