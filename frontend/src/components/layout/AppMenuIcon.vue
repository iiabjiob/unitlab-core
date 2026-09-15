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
      <rect x="6" y="6" width="12" height="12" rx="3" />
      <rect x="10" y="10" width="4" height="4" rx="0.75" class="app-menu-icon__accent-fill" />
      <path d="M8 3.5V6M12 3.5V6M16 3.5V6M8 18V20.5M12 18V20.5M16 18V20.5" />
      <path d="M3.5 8H6M3.5 12H6M3.5 16H6M18 8H20.5M18 12H20.5M18 16H20.5" />
      <path d="M2.5 12h3m-1.5-1.5L5.5 12 4 13.5M21.5 12h-3m1.5-1.5L20.5 12 19 13.5" class="app-menu-icon__accent" />
    </template>

    <template v-else-if="name === 'signals'">
      <path d="M7 3.5h8l3 3v14H7a2 2 0 0 1-2-2v-13a2 2 0 0 1 2-2Z" />
      <path d="M15 3.5v3h3" />
      <path d="M11 8v10M13.5 8H16.5M13.5 12H16.5M13.5 16H16.5" />
      <circle cx="8.5" cy="9" r="1.1" class="app-menu-icon__accent-fill" />
      <circle cx="8.5" cy="13" r="1.1" />
      <circle cx="8.5" cy="17" r="1.1" class="app-menu-icon__accent-fill" />
    </template>

    <template v-else-if="name === 'switchgears'">
      <circle cx="5" cy="11" r="1.5" />
      <circle cx="19" cy="11" r="1.5" />
      <path d="M6.5 11h3M15 11h2.5M9.5 11l5.5-4" />
      <path d="M12 14v3M8.5 17h7M10 19h5M11 21h3" class="app-menu-icon__accent" />
    </template>

    <template v-else-if="name === 'sequences'">
      <rect x="6" y="2.5" width="12" height="4" rx="1.4" />
      <rect x="6" y="10" width="12" height="4" rx="1.4" class="app-menu-icon__accent" />
      <rect x="6" y="17.5" width="12" height="4" rx="1.4" />
      <path d="M12 6.5v3.5M12 14v3.5" />
      <circle cx="9" cy="4.5" r="0.8" />
      <circle cx="9" cy="12" r="0.8" class="app-menu-icon__accent-fill" />
      <circle cx="9" cy="19.5" r="0.8" />
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

<style>
.app-menu-icon__accent {
  stroke: var(--color-blue-500);
}

.app-menu-icon__accent-fill {
  fill: var(--color-blue-500);
  stroke: var(--color-blue-500);
}
</style>
