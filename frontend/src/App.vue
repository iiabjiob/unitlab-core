<script setup>

  import AppFooter from "@/components/AppFooter.vue";
  import AppHeader from "@/components/AppHeader.vue";
  import { onMounted } from 'vue';
  import { useWebSocketStore } from '@/stores/websocket';

  const websocketStore = useWebSocketStore();

  onMounted(() => {
    websocketStore.connect();
  });
</script>

<template>
  <div class="h-screen flex flex-col text-base text-gray-800 dark:text-gray-200 bg-gray-100 dark:bg-gray-900 font-mono">
    <AppHeader class="border-b border-gray-200 dark:border-gray-800"/>

    <main class="flex-grow overflow-auto p-5">
      <RouterView />
    </main>

    <AppFooter/>
  </div>
</template>
