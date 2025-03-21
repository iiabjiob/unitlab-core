<script setup>

import { useWebSocketStore } from "@/stores/websocket";

const wsStore = useWebSocketStore();

</script>

<template>
  <div class="text-xs">
    <span v-if="wsStore.isConnected" class="text-green-500">🟢 Online</span>
    <span v-else class="text-red-500">🔴 Offline</span>
  </div>
</template>
