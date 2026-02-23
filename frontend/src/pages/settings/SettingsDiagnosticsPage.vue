<template>
  <div class="flex h-full min-h-0 min-w-0 flex-col">
    <div class="mb-4 border-b border-neutral-200/70 pb-3 dark:border-neutral-800">
      <h1 class="text-sm font-semibold text-neutral-900 dark:text-neutral-100">Settings · Core Diagnostics</h1>
      <p class="text-xs text-neutral-500 dark:text-neutral-400">
        Diagnostics of the central UnitLab Core host (Raspberry Pi): thermals, load, memory, disk, and host services.
      </p>
    </div>
    <CoreDiagnosticsPanel />
  </div>
</template>

<script setup lang="ts">
import CoreDiagnosticsPanel from "@/components/settings/CoreDiagnosticsPanel.vue"
</script>

