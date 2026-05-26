<template>
  <UiMenu>
    <UiMenuTrigger asChild>
      <button
        type="button"
        class="theme-toggle__trigger"
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
        class="theme-toggle__item"
        @select="selectTheme(opt.value)"
      >
        <span class="theme-toggle__option">
          <span aria-hidden="true">{{ iconFor(opt.value) }}</span>
          <span>{{ opt.label }}</span>
          <span v-if="themeStore.mode === opt.value" class="theme-toggle__current">
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

<style scoped>
.theme-toggle__trigger {
  align-items: center;
  background: var(--color-white);
  border: 1px solid var(--color-neutral-300);
  border-radius: 0.5rem;
  color: var(--color-neutral-700);
  display: inline-flex;
  font-size: var(--text-base);
  height: 2.25rem;
  justify-content: center;
  line-height: 1.5rem;
  transition: background-color 150ms ease, border-color 150ms ease, color 150ms ease;
  width: 2.25rem;
}

.theme-toggle__trigger:hover {
  background: var(--color-neutral-100);
}

.theme-toggle__item {
  color: var(--color-neutral-900);
}

.theme-toggle__option {
  align-items: center;
  display: inline-flex;
  gap: 0.5rem;
  min-width: 0;
}

.theme-toggle__current {
  color: var(--color-neutral-500);
  font-size: 0.625rem;
  letter-spacing: 0.025em;
  line-height: 1rem;
  text-transform: uppercase;
}

.dark .theme-toggle__trigger {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-200);
}

.dark .theme-toggle__trigger:hover {
  background: var(--color-neutral-800);
}

.dark .theme-toggle__item {
  color: var(--color-neutral-100);
}

.dark .theme-toggle__current {
  color: var(--color-neutral-400);
}
</style>
