<template>
  <div class="mobile-layout">
    <MobileHeader @open-drawer="isDrawerOpen = true">
      <template #left>
        <div class="mobile-layout__header-left">
          <AppLogo />
          <OnlineStatusComponent :status="status" :description="statusDescription" neutral-offline />
        </div>
      </template>
      <template #right>
        <div class="mobile-layout__header-right">
          <GlobalSignalTestStatus compact />
          <GlobalRunStatusLink compact :show-signal-chip="false" />
          <TimeComponent class="mobile-layout__clock" />
        </div>
      </template>
    </MobileHeader>

    <div class="mobile-layout__workspace">
      <WorkspaceSwitcher variant="compact" />
    </div>

    <main class="mobile-layout__main">
      <RouterView />
    </main>

    <SlideOver
      :open="isDrawerOpen"
      placement="bottom"
      title="Menu"
      :max-height-vh="78"
      :close-on-item-click="true"
      @close="isDrawerOpen = false"
    >
      <AppMenu />
    </SlideOver>

  </div>
</template>

<script setup lang="ts">
import { ref, computed, watch } from "vue"
import { useRoute } from "vue-router"

import AppMenu from "./AppMenu.vue"
import AppLogo from "./AppLogo.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"
import TimeComponent from "../misc/TimeComponent.vue"
import { useSystemHealthStore } from "@/stores/systemHealthStore"
import SlideOver from "../ui/SlideOver.vue"
import MobileHeader from "./MobileHeader.vue"
import WorkspaceSwitcher from "@/components/workspaces/WorkspaceSwitcher.vue"
import GlobalRunStatusLink from "./GlobalRunStatusLink.vue"
import GlobalSignalTestStatus from "./GlobalSignalTestStatus.vue"

// Drawer state
const isDrawerOpen = ref(false)
const route = useRoute()

const systemHealthStore = useSystemHealthStore()

const status = computed(() => systemHealthStore.status)
const statusDescription = computed(() => (
  status.value === "degraded" ? systemHealthStore.tooltip : null
))

watch(
  () => route.fullPath,
  () => {
    isDrawerOpen.value = false
  },
)

</script>

<style scoped>
.mobile-layout {
  display: flex;
  height: 100dvh;
  flex-direction: column;
  background: var(--color-neutral-50);
  color: var(--color-neutral-800);
  font-family: var(--font-mono);
}

.mobile-layout__header-left,
.mobile-layout__header-right {
  display: flex;
  align-items: center;
}

.mobile-layout__header-left {
  gap: 0.75rem;
}

.mobile-layout__header-right {
  gap: 0.5rem;
}

.mobile-layout__clock {
  display: none;
}

.mobile-layout__workspace {
  padding: 0.75rem 1.25rem;
  border-bottom: 1px solid var(--color-neutral-200);
  background: var(--color-white);
}

.mobile-layout__main {
  position: relative;
  flex: 1 1 auto;
  overflow: auto;
}

@media (min-width: 640px) {
  .mobile-layout__clock {
    display: inline-flex;
  }
}

:global(.dark .mobile-layout) {
  background: var(--color-neutral-900);
  color: var(--color-neutral-200);
}

:global(.dark .mobile-layout__workspace) {
  border-bottom-color: var(--color-neutral-800);
  background: var(--color-neutral-900);
}
</style>
