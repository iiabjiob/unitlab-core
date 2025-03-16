<script setup>

import AppFooter from "@/components/AppFooter.vue";
import AppHeader from "@/components/AppHeader.vue";

import { useHealthCheck } from "@/composables/useHealthCheck";
import AlertComponent from "@/components/AlertComponent.vue";

// Подключаем Health Check (WebSocket + Polling fallback)
const { serverAvailable } = useHealthCheck();

</script>

<template>
  <div class="h-screen flex flex-col text-base text-gray-800 dark:text-gray-200 bg-gray-100 dark:bg-gray-900">
    <AppHeader class="border-b border-gray-200 dark:border-gray-800"/>

    <div v-if="!serverAvailable" class="flex-grow overflow-auto p-5">
      <AlertComponent
        message="Connection to server lost. Trying to reconnect..."
        type="error"
        icon="⚠️"
      />
    </div>
    <!-- Основной контент скрывается, если сервер недоступен -->
    <main v-if="serverAvailable" class="flex-grow overflow-auto p-5">
      <RouterView />
    </main>

    <AppFooter/>
  </div>
</template>
