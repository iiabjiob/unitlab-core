<template>
  <div class="h-dvh flex flex-col text-base text-gray-800 dark:text-gray-200 bg-gray-100 dark:bg-gray-900 font-mono">
    <AppHeader class="border-b border-gray-200 dark:border-gray-800"/>

    <main class="flex-grow overflow-auto">

      <template v-if="!wsStore.isInitialized">
        <div class="h-full flex items-center justify-center text-gray-500"></div>
      </template>
      <template v-else-if="wsStore.isConnected">
        <RouterView />
      </template>
      <template v-else>
        <div class="h-full flex flex-col justify-center items-center text-center space-y-4">
          <AlertComponent
            type="error"
            icon="🔌"
            message="Connection lost. Check your network or try again later."
          />
          <ButtonComponent @click="reconnect">RECONNECT</ButtonComponent>
        </div>
      </template>
    </main>

    <AppFooter/>
  </div>
</template>

<script setup lang="ts">

import { onMounted } from 'vue';
import { useWebSocketStore } from '@/stores/useWebsocketStore';

import AppFooter from "@/components/AppFooter.vue";
import AppHeader from "@/components/AppHeader.vue";
import AlertComponent from "@/components/ui/AlertComponent.vue";
import ButtonComponent from "@/components/ui/ButtonComponent.vue";

const wsStore = useWebSocketStore();

onMounted(() => {
  wsStore.connect()
});

const reconnect = () => {
  wsStore.connect()
};

</script>
