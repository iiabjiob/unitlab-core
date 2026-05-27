<template>
  <aside class="desktop-aside">
    <div
      class="desktop-aside__header"
      :class="{ 'desktop-aside__header--compact': compact }"
    >
      <div class="desktop-aside__top" :class="{ 'desktop-aside__top--compact': compact }">
        <RouterLink
          v-if="compact"
          to="/"
          class="desktop-aside__home-link"
          title="Home"
          aria-label="Home"
        >
          UL
        </RouterLink>
        <span
          v-if="compact"
          class="desktop-aside__status-dot"
          :class="compactStatusClass"
          :title="`System status: ${status}`"
          aria-hidden="true"
        />
        <AppLogo v-else />
        <OnlineStatusComponent
          v-if="!compact"
          :status="status"
          :description="statusDescription"
          neutral-offline
        />
      </div>
      <TimeComponent
        v-if="!compact"
        class="desktop-aside__time"
      />
    </div>

    <AppMenu
      :compact="compact"
      :include-settings="false"
      class="desktop-aside__menu"
    />

    <div class="desktop-aside__footer" :class="{ 'desktop-aside__footer--compact': compact }">
      <div class="desktop-aside__footer-inner" :class="{ 'desktop-aside__footer-inner--compact': compact }">
        <RouterLink
          to="/settings"
          class="desktop-aside__settings-link"
          :class="compact ? 'desktop-aside__settings-link--compact' : 'desktop-aside__settings-link--full'"
          title="Settings"
          aria-label="Settings"
        >
          <span class="desktop-aside__settings-icon" aria-hidden="true">⚙️</span>
          <span v-if="!compact" class="desktop-aside__settings-label">Settings</span>
        </RouterLink>
        <ThemeToggle v-if="!compact" />
      </div>
    </div>

  </aside>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { RouterLink } from "vue-router"
import { useSystemHealthStore } from "@/stores/systemHealthStore"
import AppMenu from "./AppMenu.vue"
import AppLogo from "./AppLogo.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"
import TimeComponent from "../misc/TimeComponent.vue"
import ThemeToggle from "../ui/ThemeToggle.vue"

defineProps<{
  compact?: boolean
}>()

const systemHealthStore = useSystemHealthStore()

const status = computed(() => systemHealthStore.status)
const statusDescription = computed(() => (
  status.value === "degraded" ? systemHealthStore.tooltip : null
))
const compactStatusClass = computed(() => {
  if (status.value === "online") return "is-online"
  if (status.value === "degraded") return "is-degraded"
  return "is-offline"
})

</script>

<style scoped>
.desktop-aside {
  position: relative;
  display: flex;
  height: 100%;
  flex-direction: column;
}

.desktop-aside__header {
  display: flex;
  height: 5rem;
  flex-direction: column;
  justify-content: center;
  gap: 0.5rem;
  padding: 0 1.25rem;
  border-bottom: 1px solid var(--color-neutral-200);
}

.desktop-aside__header--compact {
  align-items: center;
  padding: 0 0.5rem;
}

.desktop-aside__top {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.75rem;
}

.desktop-aside__top--compact {
  width: 100%;
  justify-content: center;
}

.desktop-aside__home-link {
  display: inline-flex;
  width: 2.25rem;
  height: 2.25rem;
  align-items: center;
  justify-content: center;
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-md);
  color: var(--color-neutral-700);
  font-size: var(--text-xs);
  font-weight: 600;
  text-decoration: none;
}

.desktop-aside__home-link:hover {
  background: var(--color-neutral-100);
}

.desktop-aside__status-dot {
  width: 0.625rem;
  height: 0.625rem;
  border: 1px solid rgb(255 255 255 / 70%);
  border-radius: 999px;
  box-shadow: var(--shadow-sm);
}

.desktop-aside__status-dot.is-online {
  background: #4ade80;
}

.desktop-aside__status-dot.is-degraded {
  background: #fbbf24;
}

.desktop-aside__status-dot.is-offline {
  background: var(--color-neutral-400);
}

.desktop-aside__time {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.desktop-aside__menu {
  overflow-x: visible;
  overflow-y: auto;
  font-size: var(--text-base);
}

.desktop-aside__footer {
  padding: 1rem;
  border-top: 1px solid var(--color-neutral-200);
}

.desktop-aside__footer--compact {
  padding: 0.5rem;
}

.desktop-aside__footer-inner {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 0.5rem;
}

.desktop-aside__footer-inner--compact {
  flex-direction: column;
}

.desktop-aside__settings-link {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-md);
  background: var(--color-white);
  color: var(--color-neutral-700);
  cursor: default;
  text-decoration: none;
  transition: background-color 0.15s ease;
}

.desktop-aside__settings-link:hover {
  background: var(--color-neutral-100);
}

.desktop-aside__settings-link--compact {
  width: 2.5rem;
  height: 2.5rem;
}

.desktop-aside__settings-link--full {
  height: 2.25rem;
  gap: 0.5rem;
  padding: 0 0.75rem;
}

.desktop-aside__settings-icon {
  flex-shrink: 0;
  font-size: 1.5rem;
  line-height: 1;
}

.desktop-aside__settings-label {
  font-size: var(--text-xs);
  font-weight: 500;
}

:global(.dark .desktop-aside__header),
:global(.dark .desktop-aside__footer) {
  border-color: var(--color-neutral-700);
}

:global(.dark .desktop-aside__home-link) {
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-200);
}

:global(.dark .desktop-aside__home-link:hover) {
  background: var(--color-neutral-800);
}

:global(.dark .desktop-aside__time) {
  color: var(--color-neutral-400);
}

:global(.dark .desktop-aside__settings-link) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-900);
  color: var(--color-neutral-200);
}

:global(.dark .desktop-aside__settings-link:hover) {
  background: var(--color-neutral-800);
}
</style>
