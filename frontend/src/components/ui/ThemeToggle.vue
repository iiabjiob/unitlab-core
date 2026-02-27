<template>
  <UiMenu>
    <UiMenuTrigger asChild>
      <button
        type="button"
        class="inline-flex h-9 w-9 items-center justify-center rounded-lg border border-neutral-300 bg-white text-base text-neutral-700 transition hover:bg-neutral-100 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200 dark:hover:bg-neutral-800"
        :aria-label="`Theme: ${activeLabel}`"
        :title="`Theme: ${activeLabel}`"
      >
        <span aria-hidden="true">{{ activeIcon }}</span>
      </button>
    </UiMenuTrigger>
    <UiMenuContent>
      <UiMenuLabel>Theme</UiMenuLabel>
      <UiMenuSeparator />
      <UiMenuItem
        v-for="opt in options"
        :key="opt.value"
        class="text-neutral-900 dark:text-neutral-100"
        @select="selectTheme(opt.value)"
      >
        <span class="inline-flex min-w-0 items-center gap-2">
          <span aria-hidden="true">{{ iconFor(opt.value) }}</span>
          <span>{{ opt.label }}</span>
          <span v-if="themeStore.mode === opt.value" class="text-[10px] uppercase tracking-wide text-neutral-500 dark:text-neutral-400">
            current
          </span>
        </span>
      </UiMenuItem>
    </UiMenuContent>
  </UiMenu>
</template>

<script setup lang="ts">
import { useThemeStore, type ThemeMode } from "@/stores/themeStore"
import {
  UiMenu,
  UiMenuContent,
  UiMenuItem,
  UiMenuLabel,
  UiMenuSeparator,
  UiMenuTrigger,
} from "@/components/ui/menu"
import { computed } from "vue"

const themeStore = useThemeStore()

const options: { label: string; value: ThemeMode }[] = [
  { label: "light", value: "light" },
  { label: "dark", value: "dark" },
  { label: "system", value: "auto" },
]

const activeLabel = computed(() => {
  if (themeStore.mode === "auto") {
    return "system"
  }
  return themeStore.currentTheme === "dark" ? "dark" : "light"
})

const activeIcon = computed(() => {
  if (themeStore.mode === "auto") {
    return "🖥"
  }
  return iconFor(themeStore.mode)
})

function iconFor(value: ThemeMode) {
  if (value === "light") return "☀"
  if (value === "dark") return "☾"
  return "🖥"
}

function selectTheme(value: ThemeMode) {
  themeStore.setMode(value)
}
</script>
