<template>
  <UiMenu>
    <UiMenuTrigger asChild>
      <button
        type="button"
        class="btn btn-icon theme-toggle__trigger"
        :aria-label="`Theme: ${activeLabel}`"
        :title="`Theme: ${activeLabel}`"
      >
        <svg
          v-if="activeIconType === 'system'"
          aria-hidden="true"
          class="theme-toggle__icon"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          stroke-width="1.8"
          stroke-linecap="round"
          stroke-linejoin="round"
        >
          <rect x="3" y="4" width="18" height="12" rx="2" />
          <path d="M8 20h8" />
          <path d="M12 16v4" />
        </svg>
        <svg
          v-else-if="activeIconType === 'light'"
          aria-hidden="true"
          class="theme-toggle__icon"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          stroke-width="1.8"
          stroke-linecap="round"
          stroke-linejoin="round"
        >
          <circle cx="12" cy="12" r="4" />
          <path d="M12 2v2" />
          <path d="M12 20v2" />
          <path d="m4.93 4.93 1.41 1.41" />
          <path d="m17.66 17.66 1.41 1.41" />
          <path d="M2 12h2" />
          <path d="M20 12h2" />
          <path d="m6.34 17.66-1.41 1.41" />
          <path d="m19.07 4.93-1.41 1.41" />
        </svg>
        <svg
          v-else
          aria-hidden="true"
          class="theme-toggle__icon"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          stroke-width="1.8"
          stroke-linecap="round"
          stroke-linejoin="round"
        >
          <path d="M20.5 14.8A8.5 8.5 0 0 1 9.2 3.5 7 7 0 1 0 20.5 14.8Z" />
        </svg>
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
          <span class="theme-toggle__option-icon" aria-hidden="true">
            <svg
              v-if="opt.value === 'auto'"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="1.8"
              stroke-linecap="round"
              stroke-linejoin="round"
            >
              <rect x="3" y="4" width="18" height="12" rx="2" />
              <path d="M8 20h8" />
              <path d="M12 16v4" />
            </svg>
            <svg
              v-else-if="opt.value === 'light'"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="1.8"
              stroke-linecap="round"
              stroke-linejoin="round"
            >
              <circle cx="12" cy="12" r="4" />
              <path d="M12 2v2" />
              <path d="M12 20v2" />
              <path d="m4.93 4.93 1.41 1.41" />
              <path d="m17.66 17.66 1.41 1.41" />
              <path d="M2 12h2" />
              <path d="M20 12h2" />
              <path d="m6.34 17.66-1.41 1.41" />
              <path d="m19.07 4.93-1.41 1.41" />
            </svg>
            <svg
              v-else
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="1.8"
              stroke-linecap="round"
              stroke-linejoin="round"
            >
              <path d="M20.5 14.8A8.5 8.5 0 0 1 9.2 3.5 7 7 0 1 0 20.5 14.8Z" />
            </svg>
          </span>
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

const activeIconType = computed<"light" | "dark" | "system">(() => {
  if (themeStore.mode === "auto") {
    return "system"
  }
  return themeStore.mode
})

function selectTheme(value: ThemeMode) {
  themeStore.setMode(value)
}
</script>

<style scoped>
.theme-toggle__trigger {
  flex: 0 0 auto;
}

.theme-toggle__icon {
  height: 1.125rem;
  width: 1.125rem;
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

.theme-toggle__option-icon {
  display: inline-flex;
  height: 1rem;
  width: 1rem;
}

.theme-toggle__option-icon svg {
  height: 1rem;
  width: 1rem;
}

.theme-toggle__current {
  color: var(--color-neutral-500);
  font-size: 0.625rem;
  letter-spacing: 0.025em;
  line-height: 1rem;
  text-transform: uppercase;
}

:global(.dark .theme-toggle__item) {
  color: var(--color-neutral-100);
}

:global(.dark .theme-toggle__current) {
  color: var(--color-neutral-400);
}
</style>
