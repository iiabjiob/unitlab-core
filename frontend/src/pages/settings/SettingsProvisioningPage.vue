<template>
  <div class="settings-page-shell">
    <div class="settings-page-shell__header">
      <h1 class="settings-page-shell__title">Settings · Provisioning</h1>
      <p class="settings-page-shell__description">
        Host-side provisioning checks and repair actions for central module services.
      </p>
    </div>
    <CoreProvisionPanel v-if="serviceModeEnabled" />
    <div
      v-else
      class="settings-page-shell__notice"
    >
      Provisioning tools are hidden in standard engineer mode. Enable service mode to access install/repair actions.
    </div>
  </div>
</template>

<script setup lang="ts">
import CoreProvisionPanel from "@/components/settings/CoreProvisionPanel.vue"
import { SETTINGS_SERVICE_MODE_ENABLED as serviceModeEnabled } from "@/config/settingsFeatures"
</script>

<style scoped>
.settings-page-shell {
  display: flex;
  min-width: 0;
  min-height: 0;
  height: 100%;
  flex-direction: column;
}

.settings-page-shell__header {
  margin-bottom: 1rem;
  padding-bottom: 0.75rem;
  border-bottom: 1px solid color-mix(in srgb, var(--color-neutral-200) 70%, transparent);
}

.settings-page-shell__title {
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  font-weight: 600;
}

.settings-page-shell__description {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.settings-page-shell__notice {
  padding: 1rem;
  border: 1px solid color-mix(in srgb, var(--color-neutral-200) 70%, transparent);
  border-radius: 1rem;
  background: color-mix(in srgb, var(--color-white) 90%, transparent);
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
}

:global(.dark .settings-page-shell__header),
:global(.dark .settings-page-shell__notice){
  border-color: var(--color-neutral-800);
}

:global(.dark .settings-page-shell__title){
  color: var(--color-neutral-100);
}

:global(.dark .settings-page-shell__description){
  color: var(--color-neutral-400);
}

:global(.dark .settings-page-shell__notice){
  background: color-mix(in srgb, var(--color-neutral-950) 40%, transparent);
  color: var(--color-neutral-300);
}
</style>
