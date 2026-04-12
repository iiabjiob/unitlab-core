<template>
  <div class="flex h-full min-h-0 min-w-0 flex-col">
    <div class="mb-4 border-b border-neutral-200/70 pb-3 dark:border-neutral-800">
      <h1 class="text-sm font-semibold text-neutral-900 dark:text-neutral-100">Settings · Time / NTP Sync</h1>
      <p class="text-xs text-neutral-500 dark:text-neutral-400">
        Configure chrony NTP servers, monitor synchronization status, and manually set the central module time when the system is offline.
      </p>
    </div>
    <CoreNtpPanel />
  </div>
</template>

<script setup lang="ts">
import CoreNtpPanel from "@/components/settings/CoreNtpPanel.vue"
</script>

