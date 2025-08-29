<template>
  <div class="h-dvh flex flex-col text-base text-gray-800 dark:text-gray-200 bg-gray-100 dark:bg-gray-900 font-mono">

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

    <!-- Footer (опционально) -->
    <!-- <AppFooter /> -->
  </div>
</template>

<script setup lang="ts">
import AppHeader from "./components/AppHeader.vue";
import AppAside from "./components/AppAside.vue";
// import AppFooter from "./components/AppFooter.vue";

</script>
