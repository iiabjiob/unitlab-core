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

      <div class="desktop-layout__workspace">
        <WorkspaceSwitcher variant="mini" />
      </div>

      <div class="desktop-layout__status">
        <GlobalSignalTestStatus />
        <GlobalRunStatusLink :show-signal-chip="false" />
        <TimeComponent class="desktop-layout__clock" />
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
import { useRoute } from "vue-router"

import AppAside from "./DesktopAside.vue"
import AppLogo from "./AppLogo.vue"
import ResizablePanel from "../ui/ResizablePanel.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"
import TimeComponent from "../misc/TimeComponent.vue"
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
  gap: 1.25rem;
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

.desktop-layout__workspace {
  min-width: 110px;
  max-width: 20rem;
  flex: 1 1 18rem;
  overflow: hidden;
}

.desktop-layout__status {
  display: flex;
  flex: 0 0 auto;
  align-items: center;
  gap: 0.75rem;
  min-width: 0;
  white-space: nowrap;
}

.desktop-layout__clock {
  flex: 0 0 auto;
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
