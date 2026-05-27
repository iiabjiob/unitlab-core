<template>
  <div class="sequences-page">
    <div class="sequences-page__mobile-header">
      <UiButton
        variant="secondary"
        size="base"
        :full="true"
        type="button"
        @click="sidebarOpen = true"
      >
        <svg xmlns="http://www.w3.org/2000/svg" class="sequences-page__browse-icon" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="1.5">
          <path stroke-linecap="round" stroke-linejoin="round" d="M4 6h16M4 12h12M4 18h8" />
        </svg>
        Browse instructions
      </UiButton>
    </div>

    <ResizablePanel
      v-if="isDesktop"
      class="sequences-page__sidebar-panel"
      placement="left"
      storageKey="page-sidebar-width"
      :defaultSize="240"
      :minSize="200"
      :maxSize="400"
    >
      <aside class="sequences-page__sidebar">
        <SequenceListSidebar />
      </aside>
    </ResizablePanel>

    <section class="sequences-page__content">
      <router-view />
    </section>

    <SlideOver
      v-if="!isDesktop"
      :open="sidebarOpen"
      title="Instructions"
      placement="bottom"
      :max-height-vh="78"
      :close-on-item-click="true"
      @close="sidebarOpen = false"
    >
      <div class="sequences-page__drawer-body">
        <SequenceListSidebar />
      </div>
    </SlideOver>
  </div>
</template>

<script setup lang="ts">
import { ref, watch } from "vue"
import { useRoute } from "vue-router"

import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import SequenceListSidebar from "./components/SequenceListSidebar.vue"
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
.sequences-page {
  display: flex;
  height: 100%;
  min-height: 0;
  min-width: 0;
  flex-direction: column;
  overflow: hidden;
}

.sequences-page__mobile-header {
  padding: 0.75rem;
  border-bottom: 1px solid var(--color-neutral-200);
  background: var(--color-white);
}

.sequences-page__browse-icon {
  width: 1rem;
  height: 1rem;
}

.sequences-page__sidebar-panel {
  flex: 0 0 auto;
  background: var(--color-white);
}

.sequences-page__sidebar {
  display: flex;
  height: 100%;
  min-height: 0;
  flex-direction: column;
  padding: 1rem;
}

.sequences-page__content {
  flex: 1 1 0%;
  min-height: 0;
  min-width: 0;
  padding: 0.75rem;
  overflow-y: auto;
}

.sequences-page__drawer-body {
  padding: 1rem;
}

@media (min-width: 1024px) {
  .sequences-page {
    flex-direction: row;
  }

  .sequences-page__content {
    padding: 1rem;
  }

  .sequences-page__mobile-header {
    display: none;
  }
}

:global(.dark .sequences-page__mobile-header) {
  border-bottom-color: var(--color-neutral-800);
  background: var(--color-neutral-900);
}

:global(.dark .sequences-page__sidebar-panel) {
  background: var(--color-neutral-900);
}
</style>
