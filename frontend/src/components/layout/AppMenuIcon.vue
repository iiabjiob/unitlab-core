<script lang="ts">
export type AppMenuIconName = "devices" | "signals" | "switchgears" | "sequences" | "settings"
</script>

<script setup lang="ts">
defineProps<{
  name: AppMenuIconName
}>()
</script>

<template>
  <svg
    aria-hidden="true"
    viewBox="0 0 24 24"
    fill="none"
    stroke="currentColor"
    stroke-width="1.8"
    stroke-linecap="round"
    stroke-linejoin="round"
  >
    <template v-if="name === 'devices'">
      <rect x="6.5" y="6.5" width="11" height="11" rx="2" />
      <rect x="10" y="10" width="4" height="4" rx="1" />
      <path d="M8 3.5V6.5" />
      <path d="M12 3.5V6.5" />
      <path d="M16 3.5V6.5" />
      <path d="M8 17.5V20.5" />
      <path d="M12 17.5V20.5" />
      <path d="M16 17.5V20.5" />
      <path d="M3.5 8H6.5" />
      <path d="M3.5 12H6.5" />
      <path d="M3.5 16H6.5" />
      <path d="M17.5 8H20.5" />
      <path d="M17.5 12H20.5" />
      <path d="M17.5 16H20.5" />
    </template>

    <template v-else-if="name === 'signals'">
      <path d="M5 6.5H19" />
      <path d="M5 12H19" />
      <path d="M5 17.5H19" />
      <circle cx="8" cy="6.5" r="1.4" />
      <circle cx="15" cy="12" r="1.4" />
      <circle cx="10.5" cy="17.5" r="1.4" />
    </template>

    <template v-else-if="name === 'switchgears'">
      <circle cx="4.8" cy="12" r="1.7" />
      <circle cx="19.2" cy="12" r="1.7" />
      <path d="M6.5 12H9.4" />
      <path d="M14.6 12H17.5" />
      <path d="M9.5 12L15.4 8.6" />
      <circle cx="9.5" cy="12" r="0.9" />
    </template>

    <template v-else-if="name === 'sequences'">
      <rect x="3.5" y="4" width="12" height="4.5" rx="1.5" />
      <rect x="6" y="10" width="12" height="4.5" rx="1.5" />
      <rect x="8.5" y="16" width="12" height="4.5" rx="1.5" />
      <path d="M8.5 8.5V10" />
      <path d="M11 14.5V16" />
    </template>

    <template v-else>
      <circle cx="12" cy="12" r="3" />
      <path d="M12 3.8V6" />
      <path d="M12 18V20.2" />
      <path d="M3.8 12H6" />
      <path d="M18 12H20.2" />
      <path d="M6.2 6.2L7.8 7.8" />
      <path d="M16.2 16.2L17.8 17.8" />
      <path d="M17.8 6.2L16.2 7.8" />
      <path d="M7.8 16.2L6.2 17.8" />
    </template>
  </svg>
</template>
