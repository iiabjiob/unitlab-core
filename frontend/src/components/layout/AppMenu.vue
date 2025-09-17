<template>
  <nav class="flex-1 py-2 space-y-1 px-2">
    <RouterLink
      v-for="item in items"
      :key="item.to"
      :to="item.to"
      class="flex items-center rounded h-8 px-2 text-neutral-600 dark:text-neutral-300 hover:bg-neutral-100 dark:hover:bg-neutral-700"
      active-class="bg-neutral-100 dark:bg-neutral-700 font-semibold text-neutral-900 dark:text-white"
    >
      <DraftIcon class="w-10" size="15" />
      <span>{{ item.label }}</span>
    </RouterLink>
  </nav>
</template>

<script setup lang="ts">
import DraftIcon from "../icons/DraftIcon.vue"

const items = [
  { to: "/devices", label: "Devices" },
  { to: "/switchgears", label: "Switchgears" },
  { to: "/signal-list", label: "Signal List" },
  { to: "/sequences", label: "Sequences" },
  { to: "/settings", label: "Settings" },
]
</script>
