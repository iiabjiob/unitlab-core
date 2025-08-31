<template>
  <div class="h-dvh flex flex-col text-base text-gray-800 dark:text-gray-200 bg-gray-100 dark:bg-gray-900 font-mono">
    <template v-if="wsStore.isConnected">
      <div class="flex flex-1 overflow-hidden">
        <!-- Aside -->
        <AppAside />

        <!-- Main Content -->
        <main class="flex-1 overflow-auto">
          <!-- Header -->
          <AppHeader class="border-b border-gray-200 dark:border-gray-800" />
          <RouterView />
        </main>
      </div>
    </template>

    <template v-else>
      <div class="flex-1 flex items-center justify-center text-center">
        <div>
          <p class="text-lg font-semibold">⚠️ Server unavailable</p>
          <p class="text-sm text-gray-500 mt-2">Trying to reconnect…</p>
        </div>
      </div>
    </template>
  </div>
</template>

<script setup lang="ts">
import AppHeader from "./components/AppHeader.vue"
import AppAside from "./components/AppAside.vue"
import { useWebSocketStore } from "@/stores/websocketStore"

const wsStore = useWebSocketStore()
</script>
