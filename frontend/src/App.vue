<template>
  <div class="h-dvh flex flex-col text-base text-gray-800 dark:text-gray-200 bg-gray-100 dark:bg-gray-900 font-mono">
    <AppHeader class="border-b border-gray-200 dark:border-gray-800"/>
    <main class="flex-grow overflow-auto">
      <RouterView />
    </main>
    <AppFooter/>
  </div>
</template>

<script setup lang="ts">
import { onMounted } from "vue";
import AppFooter from "./components/AppFooter.vue";
import AppHeader from "./components/AppHeader.vue";
import { useWebSocketStore } from "./stores/websocketStore";
const wsStore = useWebSocketStore();

onMounted(() => {
  wsStore.connect()
});

</script>
