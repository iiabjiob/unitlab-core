<template>
  <AppLayout/>
</template>

<script setup lang="ts">
import { onMounted } from "vue"
import AppLayout from './components/layout/AppLayout.vue';
import { useWebSocketStore } from './stores/websocketStore';

const wsStore = useWebSocketStore();

onMounted(() => {
  // connect once when app is mounted
  wsStore.connect()
})

</script>
