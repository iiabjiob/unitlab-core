<template>
  <AppLayout/>
</template>

<script setup lang="ts">
import { onMounted } from "vue"
import AppLayout from './components/layout/AppLayout.vue';
import { useWebSocketStore } from './stores/websocketStore';
import { useEventLogStore } from "./stores/eventLogStore";

const wsStore = useWebSocketStore();
const eventsStore = useEventLogStore();

onMounted(() => {
  // connect once when app is mounted
  wsStore.connect()
  eventsStore.fetchEvents(50);

})

</script>
