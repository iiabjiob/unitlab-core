<template>
  <div class="rounded-2xl border border-neutral-200/70 bg-white/90 p-4 dark:border-neutral-800 dark:bg-neutral-950/40">
    <p class="text-[11px] uppercase tracking-[0.26em] text-neutral-500 dark:text-neutral-400">Settings · Updates</p>
    <h1 class="mt-1 text-sm font-semibold text-neutral-900 dark:text-neutral-100">Updates (Planned)</h1>
    <p v-if="serviceModeEnabled" class="mt-2 text-xs text-neutral-600 dark:text-neutral-300">
      This section will manage central module software updates (host services, containers, and firmware rollout tooling).
    </p>
    <p v-else class="mt-2 text-xs text-neutral-600 dark:text-neutral-300">
      Software update tooling is hidden in standard engineer mode.
    </p>
  </div>
</template>

<script setup lang="ts">
import { SETTINGS_SERVICE_MODE_ENABLED as serviceModeEnabled } from "@/config/settingsFeatures"
</script>
