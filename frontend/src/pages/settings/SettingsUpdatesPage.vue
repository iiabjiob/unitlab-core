<template>
  <div class="settings-updates-card">
    <p class="settings-updates-card__eyebrow">Settings · Updates</p>
    <h1 class="settings-updates-card__title">Updates (Planned)</h1>
    <p v-if="serviceModeEnabled" class="settings-updates-card__body">
      This section will manage central module software updates (host services, containers, and firmware rollout tooling).
    </p>
    <p v-else class="settings-updates-card__body">
      Software update tooling is hidden in standard engineer mode.
    </p>
  </div>
</template>

<script setup lang="ts">
import { SETTINGS_SERVICE_MODE_ENABLED as serviceModeEnabled } from "@/config/settingsFeatures"
</script>

<style scoped>
.settings-updates-card {
  padding: 1rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 70%, transparent);
  border-radius: 1rem;
  background: color-mix(in srgb, var(--color-white) 90%, transparent);
}

.settings-updates-card__eyebrow {
  color: var(--color-neutral-500);
  font-size: 11px;
  letter-spacing: 0;
  text-transform: uppercase;
}

.settings-updates-card__title {
  margin-top: 0.25rem;
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  font-weight: 600;
}

.settings-updates-card__body {
  margin-top: 0.5rem;
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
}

:global(.dark) .settings-updates-card {
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-950) 40%, transparent);
}

:global(.dark) .settings-updates-card__eyebrow {
  color: var(--color-neutral-400);
}

:global(.dark) .settings-updates-card__title {
  color: var(--color-neutral-100);
}

:global(.dark) .settings-updates-card__body {
  color: var(--color-neutral-300);
}
</style>
