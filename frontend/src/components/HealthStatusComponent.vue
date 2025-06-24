<template>
  <template v-if="!wsStore.isInitialized">
    <div></div>
  </template>
  <div v-else class="text-xs">
    <AlertComponent
        :type="alertType"
        :icon="alertIcon"
        :message="alertMessage"
      />
  </div>

</template>

<script setup lang="ts">

import { computed } from 'vue'
import { useWebSocketStore } from "@/stores/websocketStore";
import AlertComponent from "./ui/AlertComponent.vue";

const wsStore = useWebSocketStore();

const alertType = computed(() => wsStore.isConnected ? 'success' : 'error')
const alertIcon = computed(() => wsStore.isConnected ? '🟢' : '🔴')
const alertMessage = computed(() => wsStore.isConnected ? 'Online' : 'Offline')

</script>
