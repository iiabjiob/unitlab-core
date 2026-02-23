<template>
  <div class="flex h-full min-h-0 min-w-0 flex-col">
    <div class="mb-4 border-b border-neutral-200/70 pb-3 dark:border-neutral-800">
      <h1 class="text-sm font-semibold text-neutral-900 dark:text-neutral-100">Settings · Provisioning</h1>
      <p class="text-xs text-neutral-500 dark:text-neutral-400">
        Host-side provisioning checks and repair actions for central module services.
      </p>
    </div>
    <CoreProvisionPanel v-if="serviceModeEnabled" />
    <div
      v-else
      class="rounded-2xl border border-neutral-200/70 bg-white/90 p-4 text-xs text-neutral-600 dark:border-neutral-800 dark:bg-neutral-950/40 dark:text-neutral-300"
    >
      Provisioning tools are hidden in standard engineer mode. Enable service mode to access install/repair actions.
    </div>
  </div>
</template>

<script setup lang="ts">
import CoreProvisionPanel from "@/components/settings/CoreProvisionPanel.vue"
import { SETTINGS_SERVICE_MODE_ENABLED as serviceModeEnabled } from "@/config/settingsFeatures"
</script>
