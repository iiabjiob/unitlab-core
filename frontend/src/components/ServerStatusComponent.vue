<template>
  <AlertComponent
    v-if="!serverAvailable"
    message="Connection to server lost. Trying to reconnect..."
    type="error"
    icon="⚠️"
  />
</template>

<script setup>
import { onMounted, onUnmounted } from "vue";
import AlertComponent from "@/components/AlertComponent.vue";
import { useServerStatus } from "@/composables/useServerStatus"; // Глобальный composable

const { serverAvailable, startChecking, stopChecking } = useServerStatus();

onMounted(startChecking);
onUnmounted(stopChecking);
</script>
