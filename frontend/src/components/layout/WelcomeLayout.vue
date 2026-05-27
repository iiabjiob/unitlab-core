<template>
  <div class="welcome-layout">
    <RouterView />
  </div>
</template>

<script setup lang="ts">
import { RouterView } from "vue-router"
</script>

<style scoped>
.welcome-layout {
  min-height: 100vh;
  min-height: 100svh;
  background: var(--color-neutral-50);
  color: var(--color-neutral-900);
}

:global(.dark .welcome-layout) {
  background: var(--color-neutral-950);
  color: var(--color-neutral-50);
}
</style>
