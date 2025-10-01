<template>
  <div class="flex items-center gap-0.5 text-xs overflow-hidden">

    <button
      v-for="opt in options"
      :key="opt.value"
      class="px-2 py-1 cursor-pointer"
      :class="[
        'transition-colors',
        themeStore.mode === opt.value
          ? 'bg-neutral-200 dark:bg-neutral-700 font-bold'
          : 'hover:bg-neutral-100 dark:hover:bg-neutral-900'
      ]"
      @click.stop="themeStore.setMode(opt.value)"
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
</script>
