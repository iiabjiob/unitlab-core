<template>
  <nav class="flex-1 px-2 py-4 space-y-4">
    <div v-for="section in sections" :key="section.title" class="space-y-1">
      <p class="px-2 text-[11px] font-semibold uppercase tracking-wide text-neutral-400">
        {{ section.title }}
      </p>
      <RouterLink
        v-for="item in section.items"
        :key="item.to"
        :to="item.to"
        class="block rounded-xl pl-6 pr-3 py-2 text-sm font-medium text-neutral-700 transition-all hover:bg-neutral-100 hover:text-neutral-900 dark:text-neutral-200 dark:hover:bg-neutral-800/70 dark:hover:text-white"
        active-class="bg-neutral-100 text-neutral-900 dark:bg-neutral-800 dark:text-white"
      >
        {{ item.label }}
      </RouterLink>
    </div>
  </nav>
</template>

<script setup lang="ts">
const sections = [
  {
    title: "RUN",
    items: [{ to: "/test-runs", label: "Test Runs" }],
  },
  {
    title: "DESIGN",
    items: [
      { to: "/sequences", label: "Sequences" },
      { to: "/signals", label: "Signals" },
      { to: "/switchgears", label: "Switchgears" },
    ],
  },
  {
    title: "HARDWARE",
    items: [{ to: "/devices", label: "Devices" }],
  },
]
</script>