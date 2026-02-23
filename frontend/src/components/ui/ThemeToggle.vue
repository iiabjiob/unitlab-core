<template>
  <UiMenu>
    <UiMenuTrigger asChild>
      <button
        type="button"
        class="inline-flex items-center gap-2 rounded-lg border border-neutral-300 bg-white px-3 py-1.5 text-xs font-medium text-neutral-700 transition hover:bg-neutral-100 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200 dark:hover:bg-neutral-800"
        :aria-label="`Theme: ${activeLabel}`"
        :title="`Theme: ${activeLabel}`"
      >
        <span class="inline-flex h-2 w-2 rounded-full" :class="activeDotClass" aria-hidden="true" />
        <span>{{ activeLabel }}</span>
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
          <span class="inline-flex h-2 w-2 rounded-full" :class="dotClass(opt.value)" aria-hidden="true" />
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
  { label: "Light", value: "light" },
  { label: "Dark", value: "dark" },
  { label: "Auto", value: "auto" },
]

const activeLabel = computed(() => {
  return options.find(opt => opt.value === themeStore.mode)?.label ?? "Theme"
})

const activeDotClass = computed(() => dotClass(themeStore.mode))

function dotClass(value: ThemeMode) {
  if (value === "light") return "bg-amber-400"
  if (value === "dark") return "bg-indigo-400"
  return "bg-emerald-400"
}

function selectTheme(value: ThemeMode) {
  themeStore.setMode(value)
}
</script>
