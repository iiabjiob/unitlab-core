<template>
  <aside class="desktop-aside">
    <div
      v-if="showHeader"
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
    </div>

    <AppMenu
      :compact="compact"
      :include-settings="false"
      class="desktop-aside__menu"
    />
  </aside>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { RouterLink } from "vue-router"
import { useSystemHealthStore } from "@/stores/systemHealthStore"
import AppMenu from "./AppMenu.vue"
import AppLogo from "./AppLogo.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"

withDefaults(defineProps<{
  compact?: boolean
  showHeader?: boolean
}>(), {
  compact: false,
  showHeader: true,
})

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

.desktop-aside__menu {
  flex: 1 1 auto;
  min-height: 0;
  overflow-x: visible;
  overflow-y: auto;
  font-size: var(--text-base);
}

:global(.dark .desktop-aside__header) {
  border-color: var(--color-neutral-700);
}

:global(.dark .desktop-aside__home-link) {
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-200);
}

:global(.dark .desktop-aside__home-link:hover) {
  background: var(--color-neutral-800);
}

</style>
