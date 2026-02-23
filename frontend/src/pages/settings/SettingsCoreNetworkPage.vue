<template>
  <div class="flex h-full min-h-0 min-w-0 flex-col">
    <div class="mb-4 border-b border-neutral-200/70 pb-3 dark:border-neutral-800">
      <h1 class="text-sm font-semibold text-neutral-900 dark:text-neutral-100">Settings · Core Network</h1>
      <p class="text-xs text-neutral-500 dark:text-neutral-400">
        Manage access point (AP) and optional STA uplink using the host NetworkManager agent.
      </p>
    </div>
    <CoreNetworkPanel />
  </div>
</template>

<script setup lang="ts">
import CoreNetworkPanel from "@/components/settings/CoreNetworkPanel.vue"
</script>

