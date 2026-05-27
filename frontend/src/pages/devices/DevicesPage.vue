<template>
  <div class="devices-page">
    <div class="devices-page__mobile-header">
      <UiButton
        variant="secondary"
        size="base"
        :full="true"
        type="button"
        @click="sidebarOpen = true"
      >
        <svg xmlns="http://www.w3.org/2000/svg" class="devices-page__browse-icon" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="1.5">
          <path stroke-linecap="round" stroke-linejoin="round" d="M4 6h16M4 12h12M4 18h8" />
        </svg>
        Browse devices
      </UiButton>
    </div>

    <ResizablePanel
      v-if="isDesktop"
      class="devices-page__sidebar-panel"
      placement="left"
      storageKey="page-sidebar-width"
      :defaultSize="240"
      :minSize="200"
      :maxSize="400"
    >
      <aside class="devices-page__sidebar">
        <DeviceListSidebar />
      </aside>
    </ResizablePanel>

    <section class="devices-page__content">
      <router-view />
    </section>

    <SlideOver
      v-if="!isDesktop"
      :open="sidebarOpen"
      title="Devices"
      placement="bottom"
      :max-height-vh="78"
      :close-on-item-click="true"
      @close="sidebarOpen = false"
    >
      <div class="devices-page__drawer-body">
        <DeviceListSidebar />
      </div>
    </SlideOver>
  </div>
</template>

<script setup lang="ts">
import { ref, watch } from "vue"
import { useRoute } from "vue-router"

import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import DeviceListSidebar from "./components/DeviceListSidebar.vue"
import SlideOver from "@/components/ui/SlideOver.vue"
import UiButton from "@/components/ui/UiButton.vue"
import { useViewport } from "@/composables/useViewport"

const { isDesktop } = useViewport()
const sidebarOpen = ref(false)
const route = useRoute()

watch(isDesktop, (next) => {
  if (next) sidebarOpen.value = false
})

watch(
  () => route.fullPath,
  () => {
    sidebarOpen.value = false
  },
)
</script>

<style scoped>
.devices-page {
  display: flex;
  height: 100%;
  flex-direction: column;
}

.devices-page__mobile-header {
  padding: 0.75rem;
  border-bottom: 1px solid var(--color-neutral-200);
  background: var(--color-white);
}

.devices-page__browse-icon {
  width: 1rem;
  height: 1rem;
}

.devices-page__sidebar-panel {
  background: var(--color-white);
}

.devices-page__sidebar {
  display: flex;
  height: 100%;
  flex-direction: column;
  padding: 1rem;
}

.devices-page__content {
  flex: 1 1 0%;
  padding: 0.75rem;
  overflow-y: auto;
}

.devices-page__drawer-body {
  padding: 1rem;
}

@media (min-width: 768px) {
  .devices-page {
    flex-direction: row;
  }

  .devices-page__content {
    padding: 1rem;
  }
}

@media (min-width: 1024px) {
  .devices-page__mobile-header {
    display: none;
  }
}

:global(.dark .devices-page__mobile-header) {
  border-bottom-color: var(--color-neutral-800);
  background: var(--color-neutral-900);
}

:global(.dark .devices-page__sidebar-panel) {
  background: var(--color-neutral-900);
}
</style>
