<template>
  <div class="desktop-layout">
    <header class="desktop-layout__header">
      <div class="desktop-layout__brand">
        <AppBrandStatus />
      </div>

      <div class="desktop-layout__runtime">
        <GlobalSignalTestStatus />
        <GlobalRunStatusLink :show-signal-chip="false" />
      </div>

      <div class="desktop-layout__utilities">
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
        :minSize="LEFT_ASIDE_MIN_WIDTH_PX"
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
import AppBrandStatus from "./AppBrandStatus.vue"
import ResizablePanel from "../ui/ResizablePanel.vue"
import ThemeToggle from "../ui/ThemeToggle.vue"
import WorkspaceSwitcher from "@/components/workspaces/WorkspaceSwitcher.vue"
import GlobalRunStatusLink from "./GlobalRunStatusLink.vue"
import GlobalSignalTestStatus from "./GlobalSignalTestStatus.vue"
import { localSettingsKeys, readNumberLocalSetting } from "@/services/localSettingsStorage"

const route = useRoute()
const meta = computed(() => ({
  leftAside: route.meta.leftAside ?? true,
}))

const LEFT_ASIDE_DEFAULT_WIDTH_PX = 240
const LEFT_ASIDE_MIN_WIDTH_PX = 56
const LEFT_ASIDE_MAX_WIDTH_PX = 400
const ASIDE_COMPACT_THRESHOLD_PX = 130
const LEFT_ASIDE_STORAGE_KEY = "left-aside-width"

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
  background:
    linear-gradient(90deg, color-mix(in srgb, var(--runtime-accent) 5%, transparent) 1px, transparent 1px),
    linear-gradient(180deg, var(--color-neutral-50) 0%, color-mix(in srgb, var(--color-neutral-100) 70%, var(--color-white)) 100%);
  background-size: 4rem 4rem, auto;
  color: var(--color-neutral-800);
  font-family: var(--font-mono);
}

.desktop-layout__body {
  display: flex;
  flex: 1 1 auto;
  min-height: 0;
  background:
    linear-gradient(90deg, color-mix(in srgb, var(--runtime-accent) 4%, transparent) 1px, transparent 1px),
    linear-gradient(180deg, color-mix(in srgb, var(--color-white) 86%, var(--color-neutral-100)) 0%, var(--color-neutral-100) 100%);
  background-size: 4rem 4rem, auto;
  overflow: hidden;
}

.desktop-layout__aside-panel {
  flex: 0 0 auto;
  border-right: 1px solid color-mix(in srgb, var(--color-neutral-200) 80%, transparent);
  background: color-mix(in srgb, var(--runtime-panel-bg) 96%, var(--color-white));
  box-shadow:
    10px 0 24px rgb(15 23 42 / 0.06),
    inset -1px 0 0 rgb(255 255 255 / 0.66);
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
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-white) 96%, var(--runtime-accent-soft)), color-mix(in srgb, var(--color-white) 90%, var(--color-neutral-50)));
  box-shadow:
    0 1px 0 rgb(255 255 255 / 82%) inset,
    0 12px 24px rgb(15 23 42 / 0.06);
}

.desktop-layout__brand {
  display: flex;
  flex: 0 1 auto;
  align-items: center;
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
  display: flex;
  flex: 1 1 auto;
  min-height: 0;
  min-width: 0;
  overflow: auto;
}

:global(.dark .desktop-layout) {
  background:
    linear-gradient(90deg, color-mix(in srgb, var(--runtime-accent) 5%, transparent) 1px, transparent 1px),
    var(--color-neutral-950);
  background-size: 4rem 4rem, auto;
  color: var(--color-neutral-200);
}

:global(.dark .desktop-layout__body) {
  background:
    linear-gradient(90deg, color-mix(in srgb, var(--runtime-accent) 5%, transparent) 1px, transparent 1px),
    linear-gradient(180deg, color-mix(in srgb, var(--color-neutral-900) 86%, var(--color-neutral-950)) 0%, var(--color-neutral-950) 100%);
  background-size: 4rem 4rem, auto;
}

:global(.dark .desktop-layout__aside-panel) {
  border-right-color: color-mix(in srgb, var(--runtime-accent) 12%, var(--color-neutral-800));
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-neutral-900) 92%, var(--runtime-accent-soft)), color-mix(in srgb, var(--color-neutral-950) 88%, var(--color-neutral-900)));
  box-shadow:
    10px 0 28px rgb(0 0 0 / 0.24),
    inset -1px 0 0 rgb(255 255 255 / 0.03);
}

:global(.dark .desktop-layout__header) {
  border-bottom-color: color-mix(in srgb, var(--runtime-accent) 12%, var(--color-neutral-800));
  background:
    linear-gradient(180deg, color-mix(in srgb, var(--color-neutral-900) 92%, var(--runtime-accent-soft)), color-mix(in srgb, var(--color-neutral-950) 88%, var(--color-neutral-900)));
  box-shadow:
    0 1px 0 rgb(255 255 255 / 4%) inset,
    0 14px 26px rgb(0 0 0 / 0.26);
}
</style>
