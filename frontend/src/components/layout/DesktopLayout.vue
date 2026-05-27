<template>
  <div class="desktop-layout">
    <header class="desktop-layout__header">
      <div class="desktop-layout__brand">
        <AppLogo />
        <OnlineStatusComponent
          :status="status"
          :description="statusDescription"
          neutral-offline
        />
      </div>

      <div class="desktop-layout__runtime">
        <GlobalSignalTestStatus />
        <GlobalRunStatusLink :show-signal-chip="false" />
      </div>

      <div class="desktop-layout__utilities">
        <TimeComponent class="desktop-layout__clock" />
        <div class="desktop-layout__workspace">
          <WorkspaceSwitcher variant="toolbar" />
        </div>
        <ThemeToggle />
        <RouterLink
          to="/settings"
          class="btn btn-icon desktop-layout__settings-link"
          title="Settings"
          aria-label="Settings"
        >
          <svg
            aria-hidden="true"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            stroke-width="1.8"
            stroke-linecap="round"
            stroke-linejoin="round"
          >
            <path d="M12 15.5A3.5 3.5 0 1 0 12 8a3.5 3.5 0 0 0 0 7.5Z" />
            <path d="M19.4 15a1.7 1.7 0 0 0 .34 1.87l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06A1.7 1.7 0 0 0 15 19.36a1.7 1.7 0 0 0-1 .52V20a2 2 0 0 1-4 0v-.09a1.7 1.7 0 0 0-1-.52 1.7 1.7 0 0 0-1.87.34l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.7 1.7 0 0 0 4.64 15a1.7 1.7 0 0 0-.52-1H4a2 2 0 0 1 0-4h.09a1.7 1.7 0 0 0 .52-1 1.7 1.7 0 0 0-.34-1.87l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.7 1.7 0 0 0 9 4.64a1.7 1.7 0 0 0 1-.52V4a2 2 0 0 1 4 0v.09a1.7 1.7 0 0 0 1 .52 1.7 1.7 0 0 0 1.87-.34l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.7 1.7 0 0 0 19.36 9a1.7 1.7 0 0 0 .52 1H20a2 2 0 0 1 0 4h-.09a1.7 1.7 0 0 0-.51 1Z" />
          </svg>
        </RouterLink>
      </div>
    </header>

    <div class="desktop-layout__body">
      <ResizablePanel
        v-if="meta.leftAside"
        class="desktop-layout__aside-panel"
        placement="left"
        storageKey="left-aside-width"
        :defaultSize="240"
        :minSize="72"
        :maxSize="400"
        @size-change="handleLeftAsideSizeChange"
      >
        <AppAside
          :compact="isAsideCompact"
          :show-header="false"
          class="desktop-layout__aside"
        />
      </ResizablePanel>

      <main class="desktop-layout__main">
        <div class="desktop-layout__content">
          <RouterView />
        </div>
      </main>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from "vue"
import { RouterLink, useRoute } from "vue-router"

import AppAside from "./DesktopAside.vue"
import AppLogo from "./AppLogo.vue"
import ResizablePanel from "../ui/ResizablePanel.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"
import TimeComponent from "../misc/TimeComponent.vue"
import ThemeToggle from "../ui/ThemeToggle.vue"
import WorkspaceSwitcher from "@/components/workspaces/WorkspaceSwitcher.vue"
import GlobalRunStatusLink from "./GlobalRunStatusLink.vue"
import GlobalSignalTestStatus from "./GlobalSignalTestStatus.vue"
import { useSystemHealthStore } from "@/stores/systemHealthStore"
import { localSettingsKeys, readNumberLocalSetting } from "@/services/localSettingsStorage"

const route = useRoute()
const systemHealthStore = useSystemHealthStore()
const meta = computed(() => ({
  leftAside: route.meta.leftAside ?? true,
}))

const LEFT_ASIDE_DEFAULT_WIDTH_PX = 240
const LEFT_ASIDE_MIN_WIDTH_PX = 72
const LEFT_ASIDE_MAX_WIDTH_PX = 400
const ASIDE_COMPACT_THRESHOLD_PX = 130
const LEFT_ASIDE_STORAGE_KEY = "left-aside-width"

const status = computed(() => systemHealthStore.status)
const statusDescription = computed(() => (
  status.value === "degraded" ? systemHealthStore.tooltip : null
))
const leftAsideWidth = ref(resolveInitialLeftAsideWidth())
const isAsideCompact = computed(() => leftAsideWidth.value <= ASIDE_COMPACT_THRESHOLD_PX)

function handleLeftAsideSizeChange(size: number) {
  if (!Number.isFinite(size)) {
    return
  }
  leftAsideWidth.value = Math.max(0, Math.round(size))
}

function resolveInitialLeftAsideWidth() {
  const saved = readNumberLocalSetting(
    localSettingsKeys.resizablePanelSize(LEFT_ASIDE_STORAGE_KEY),
    null,
    { legacyKeys: [LEFT_ASIDE_STORAGE_KEY] },
  )
  const rawSize = saved ?? LEFT_ASIDE_DEFAULT_WIDTH_PX
  return Math.max(LEFT_ASIDE_MIN_WIDTH_PX, Math.min(LEFT_ASIDE_MAX_WIDTH_PX, Math.trunc(rawSize)))
}

</script>

<style scoped>
.desktop-layout {
  --desktop-layout-header-height: 5rem;
  display: flex;
  height: 100dvh;
  flex-direction: column;
  background: var(--color-neutral-50);
  color: var(--color-neutral-800);
  font-family: var(--font-mono);
}

.desktop-layout__body {
  display: flex;
  flex: 1 1 auto;
  min-height: 0;
  overflow: hidden;
}

.desktop-layout__aside-panel {
  flex: 0 0 auto;
  background: var(--color-white);
}

.desktop-layout__aside {
  min-height: 0;
}

.desktop-layout__main {
  display: flex;
  flex: 1 1 auto;
  min-width: 0;
  flex-direction: column;
  overflow: hidden;
}

.desktop-layout__header {
  z-index: 20;
  display: flex;
  flex: 0 0 var(--desktop-layout-header-height);
  height: var(--desktop-layout-header-height);
  min-height: var(--desktop-layout-header-height);
  max-height: var(--desktop-layout-header-height);
  align-items: center;
  gap: 1rem;
  overflow: hidden;
  padding: 0 1.25rem;
  border-bottom: 1px solid var(--color-neutral-200);
  background: var(--color-white);
}

.desktop-layout__brand {
  display: flex;
  flex: 0 0 auto;
  align-items: center;
  gap: 0.75rem;
  min-width: 0;
}

.desktop-layout__runtime {
  display: flex;
  flex: 1 1 auto;
  align-items: center;
  justify-content: flex-end;
  gap: 0.75rem;
  min-width: 0;
  overflow: hidden;
  white-space: nowrap;
}

.desktop-layout__utilities {
  display: flex;
  flex: 0 0 auto;
  align-items: center;
  gap: 0.5rem;
  min-width: 0;
}

.desktop-layout__clock {
  flex: 0 0 auto;
}

.desktop-layout__workspace {
  width: clamp(10rem, 17vw, 15rem);
  min-width: 0;
  overflow: hidden;
}

.desktop-layout__settings-link {
  flex: 0 0 auto;
  text-decoration: none;
}

.desktop-layout__content {
  flex: 1 1 auto;
  min-height: 0;
  min-width: 0;
  overflow: hidden;
}

:global(.dark .desktop-layout) {
  background: var(--color-neutral-900);
  color: var(--color-neutral-200);
}

:global(.dark .desktop-layout__aside-panel) {
  background: var(--color-neutral-800);
}

:global(.dark .desktop-layout__header) {
  border-bottom-color: var(--color-neutral-800);
  background: var(--color-neutral-900);
}
</style>
