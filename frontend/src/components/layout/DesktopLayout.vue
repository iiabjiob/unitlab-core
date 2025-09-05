<template>
  <div class="h-dvh flex flex-col bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200 font-mono">
    <div class="flex flex-1 overflow-hidden">
      <!-- Aside as complete block -->
      <AppAside class="w-72 border-r border-neutral-200 dark:border-neutral-800" />

      <main class="flex-1 overflow-auto">
        <RouterView />
      </main>
    </div>
  </div>
</template>

<script setup lang="ts">
import AppAside from "./AppAside.vue"
</script>
