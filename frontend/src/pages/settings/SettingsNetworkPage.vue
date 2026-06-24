<template>
  <div class="settings-page-shell">
    <div class="settings-page-shell__header">
      <h1 class="settings-page-shell__title">Settings · Network / RJ45</h1>
      <p class="settings-page-shell__description">
        Configure the Raspberry Pi Ethernet path for MMS / SCADA verification. The host agent owns the RJ45 settings, while the backend only proxies the command.
      </p>
    </div>
    <CoreNetworkPanel />
  </div>
</template>

<script setup lang="ts">
import CoreNetworkPanel from "@/components/settings/CoreNetworkPanel.vue"
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

:global(.dark .settings-page-shell__header){
  border-bottom-color: var(--color-neutral-800);
}

:global(.dark .settings-page-shell__title){
  color: var(--color-neutral-100);
}

:global(.dark .settings-page-shell__description){
  color: var(--color-neutral-400);
}
</style>
