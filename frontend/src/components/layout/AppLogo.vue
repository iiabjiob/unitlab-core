<template>
  <div class="app-logo">
    <RouterLink
      to="/"
      class="app-logo__link"
      aria-label="UnitLab home"
    >
      <span class="app-logo__mark" aria-hidden="true">
        <span>U</span>
        <span>L</span>
      </span>
      <span class="app-logo__wordmark" aria-hidden="true">
        <span class="app-logo__unit">Unit</span><span class="app-logo__lab">Lab</span>
      </span>
    </RouterLink>
  </div>
</template>

<script setup lang="ts">
import { RouterLink } from "vue-router"
</script>

<style scoped>
.app-logo {
  display: inline-flex;
  align-items: center;
  min-width: 0;
}

.app-logo__link {
  display: inline-flex;
  align-items: center;
  gap: 0.625rem;
  color: var(--color-neutral-800);
  min-width: 0;
  text-decoration: none;
}

.app-logo__mark {
  display: inline-flex;
  width: 2.25rem;
  height: 2.25rem;
  flex: 0 0 auto;
  align-items: center;
  justify-content: center;
  gap: 0.0625rem;
  border: 1px solid color-mix(in srgb, var(--color-blue-500) 45%, var(--color-neutral-950));
  border-radius: var(--radius-lg);
  background:
    linear-gradient(145deg, var(--color-neutral-950), var(--color-neutral-800)),
    var(--color-neutral-950);
  box-shadow:
    0 10px 22px rgb(15 23 42 / 18%),
    0 0 0 3px color-mix(in srgb, var(--color-blue-500) 10%, transparent),
    inset 0 1px 0 rgb(255 255 255 / 14%);
  color: var(--color-white);
  font-size: 0.8125rem;
  font-weight: 800;
  line-height: 1;
  text-align: center;
}

.app-logo__mark span {
  min-width: 0.5rem;
}

.app-logo__wordmark {
  display: inline-flex;
  align-items: baseline;
  min-width: 0;
  font-size: var(--text-xl);
  font-weight: 800;
  letter-spacing: 0;
  line-height: 1;
}

.app-logo__unit {
  color: var(--color-neutral-950);
}

.app-logo__lab {
  color: var(--color-blue-600);
}

:global(.dark .app-logo__link) {
  color: var(--color-neutral-200);
}

:global(.dark .app-logo__mark) {
  border-color: color-mix(in srgb, var(--color-blue-400) 50%, var(--color-neutral-50));
  background:
    linear-gradient(145deg, var(--color-white), var(--color-neutral-200)),
    var(--color-white);
  color: var(--color-neutral-950);
  box-shadow:
    0 10px 22px rgb(0 0 0 / 34%),
    0 0 0 3px color-mix(in srgb, var(--color-blue-400) 10%, transparent),
    inset 0 1px 0 rgb(255 255 255 / 72%);
}

:global(.dark .app-logo__unit) {
  color: var(--color-neutral-50);
}

:global(.dark .app-logo__lab) {
  color: var(--color-blue-400);
}
</style>
