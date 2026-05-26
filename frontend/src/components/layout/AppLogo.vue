<template>
  <div class="app-logo">
    <RouterLink
      to="/"
      class="app-logo__link"
    >
      UnitLab
    </RouterLink>
  </div>
</template>

<script setup lang="ts">
import { RouterLink } from "vue-router"
</script>

<style scoped>
.app-logo {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  column-gap: 0.75rem;
}

.app-logo__link {
  color: var(--color-neutral-800);
  font-size: var(--text-xl);
  font-weight: 700;
  text-decoration: none;
}

:global(.dark) .app-logo__link {
  color: var(--color-neutral-200);
}
</style>
