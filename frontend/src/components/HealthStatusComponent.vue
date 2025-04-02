<script setup>

import { useWebSocketStore } from "@/stores/websocket";
import AlertComponent from "./ui/AlertComponent.vue";

const wsStore = useWebSocketStore();

</script>

<template>
  <div class="text-xs">
    <AlertComponent
        :type="wsStore.isConnected ? 'success' : 'error'"
        :icon="wsStore.isConnected ? '🟢' : '🔴'"
        :message="wsStore.isConnected ? 'Online' : 'Offline'"
      />
  </div>
</template>
