<template>
  <AppLayout/>
</template>

<script setup lang="ts">
import { onMounted } from "vue"
import AppLayout from './components/layout/AppLayout.vue';
import { useWebSocketStore } from './stores/websocketStore';
import { useEventLogStore } from "./stores/eventLogStore";
import { useSwitchgearStore } from "./stores/switchgearStore";
import { useSequenceStore } from "./stores/sequenceStore";

const wsStore = useWebSocketStore();
const eventsStore = useEventLogStore();
const switchgearStore = useSwitchgearStore()
const sequenceStore = useSequenceStore()

onMounted(async () => {
  // connect once when app is mounted
  wsStore.connect()

  eventsStore.fetchEvents(50)

  // параллельная загрузка
  await Promise.all([
    switchgearStore.fetchAll(),
    sequenceStore.fetchSequences(),
  ])

})

</script>
