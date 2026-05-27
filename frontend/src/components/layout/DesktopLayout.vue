<template>
  <div class="desktop-layout">
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
          class="desktop-layout__aside"
        />
      </ResizablePanel>

      <main class="desktop-layout__main">
        <header class="desktop-layout__header">
          <div class="desktop-layout__header-inner">
            <div class="desktop-layout__workspace">
              <WorkspaceSwitcher variant="mini" />
            </div>
            <div class="desktop-layout__status">
              <GlobalSignalTestStatus />
              <GlobalRunStatusLink :show-signal-chip="false" />
            </div>
          </div>
        </header>

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
import ResizablePanel from "../ui/ResizablePanel.vue"
import WorkspaceSwitcher from "@/components/workspaces/WorkspaceSwitcher.vue"
import GlobalRunStatusLink from "./GlobalRunStatusLink.vue"
import GlobalSignalTestStatus from "./GlobalSignalTestStatus.vue"

const route = useRoute()
const meta = computed(() => ({
  leftAside: route.meta.leftAside ?? true,
}))

const leftAsideWidth = ref(240)
const ASIDE_COMPACT_THRESHOLD_PX = 130
const isAsideCompact = computed(() => leftAsideWidth.value <= ASIDE_COMPACT_THRESHOLD_PX)

function handleLeftAsideSizeChange(size: number) {
  if (!Number.isFinite(size)) {
    return
  }
  leftAsideWidth.value = Math.max(0, Math.round(size))
}

</script>

<style scoped>
.desktop-layout {
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
  overflow: hidden;
}

.desktop-layout__aside-panel {
  background: var(--color-white);
}

.desktop-layout__aside {
  border-right: 1px solid var(--color-neutral-200);
}

.desktop-layout__main {
  display: flex;
  flex: 1 1 auto;
  flex-direction: column;
  overflow: hidden;
}

.desktop-layout__header {
  position: sticky;
  top: 0;
  z-index: 20;
  display: flex;
  height: 5rem;
  align-items: center;
  padding: 0 1.25rem;
  border-bottom: 1px solid var(--color-neutral-200);
  background: var(--color-white);
}

.desktop-layout__header-inner {
  display: flex;
  flex: 1 1 auto;
  flex-wrap: wrap;
  align-items: center;
  justify-content: space-between;
  gap: 1.5rem;
}

.desktop-layout__workspace {
  min-width: 110px;
  max-width: 20rem;
  flex: 1 1 auto;
}

.desktop-layout__status {
  display: flex;
  align-items: center;
  gap: 0.75rem;
}

.desktop-layout__content {
  flex: 1 1 auto;
  overflow: auto;
}

:global(.dark .desktop-layout) {
  background: var(--color-neutral-900);
  color: var(--color-neutral-200);
}

:global(.dark .desktop-layout__aside-panel) {
  background: var(--color-neutral-800);
}

:global(.dark .desktop-layout__aside) {
  border-right-color: var(--color-neutral-700);
}

:global(.dark .desktop-layout__header) {
  border-bottom-color: var(--color-neutral-800);
  background: var(--color-neutral-900);
}
</style>
