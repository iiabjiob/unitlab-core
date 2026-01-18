<template>
  <nav class="flex-1 px-2 py-4 space-y-4">
    <div v-for="section in sections" :key="section.title" class="space-y-1">
      <p class="px-2 text-[11px] font-semibold uppercase tracking-wide text-neutral-400">
        {{ section.title }}
      </p>
      <div v-for="item in section.items" :key="item.to">
        <RouterLink
          :to="item.to"
          class="block rounded-xl pl-6 pr-3 py-2 text-sm font-medium text-neutral-700 transition-all hover:bg-neutral-100 hover:text-neutral-900 dark:text-neutral-200 dark:hover:bg-neutral-800/70 dark:hover:text-white"
          active-class="bg-neutral-100 text-neutral-900 dark:bg-neutral-800 dark:text-white"
        >
          {{ item.label }}
        </RouterLink>
        <RouterLink
          v-for="child in item.children ?? []"
          :key="child.to"
          :to="child.to"
          class="mt-1 block rounded-xl pl-10 pr-3 py-2 text-sm font-medium text-neutral-600 transition-all hover:bg-neutral-100 hover:text-neutral-900 dark:text-neutral-300 dark:hover:bg-neutral-800/70 dark:hover:text-white"
          active-class="bg-neutral-100 text-neutral-900 dark:bg-neutral-800 dark:text-white"
        >
          {{ child.label }}
        </RouterLink>
      </div>
    </div>
  </nav>
</template>

<script setup lang="ts">
type MenuItem = {
  to: string
  label: string
  children?: MenuItem[]
}

type MenuSection = {
  title: string
  items: MenuItem[]
}

const sections: MenuSection[] = [
  {
    title: "HARDWARE",
    items: [{ to: "/devices", label: "Devices" }],
  },
  {
    title: "DESIGN",
    items: [
      { to: "/signals", label: "Signals" },
      { to: "/switchgears", label: "Switchgears" },
    ],
  },
  {
    title: "RUN",
    items: [
      {
        to: "/test-runs",
        label: "Test Runs",
        children: [{ to: "/test-runs/instructions", label: "Instructions" }],
      },
    ],
  },
]
</script>
