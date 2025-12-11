<template>
  <div
    class="inline-flex items-center rounded-md overflow-hidden bg-neutral-100 dark:bg-neutral-800 p-0.5"
    role="radiogroup"
  >
    <button
      v-for="opt in options"
      :key="opt.value"
      role="radio"
      :aria-checked="themeStore.mode === opt.value"
      @click.stop="themeStore.setMode(opt.value)"
      class="px-3 py-1 text-xs rounded-md transition-all select-none"
      :class="buttonClass(opt.value)"
    >
      {{ opt.label }}
    </button>
  </div>
</template>

<script setup lang="ts">
import { useThemeStore, type ThemeMode } from "@/stores/themeStore"

const themeStore = useThemeStore()

const options: { label: string; value: ThemeMode }[] = [
  { label: "Light", value: "light" },
  { label: "Dark", value: "dark" },
  { label: "Auto", value: "auto" },
]

function buttonClass(value: ThemeMode) {
  const active = themeStore.mode === value
  return active
    ? "bg-neutral-200 dark:bg-neutral-700 text-neutral-900 dark:text-neutral-50 ring-1 ring-neutral-300 dark:ring-neutral-600"
    : "text-neutral-600 dark:text-neutral-300 hover:bg-neutral-200/50 dark:hover:bg-neutral-700/50"
}
</script>
