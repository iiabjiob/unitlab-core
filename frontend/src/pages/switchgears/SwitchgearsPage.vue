<template>
  <div class="switchgears-page">
    <div class="switchgears-page__mobile-bar">
      <UiButton
        variant="secondary"
        size="base"
        :full="true"
        type="button"
        @click="sidebarOpen = true"
      >
        <svg
          xmlns="http://www.w3.org/2000/svg"
          class="switchgears-page__browse-icon"
          fill="none"
          viewBox="0 0 24 24"
          stroke="currentColor"
          stroke-width="1.5"
        >
          <path stroke-linecap="round" stroke-linejoin="round" d="M4 6h16M4 12h12M4 18h8" />
        </svg>
        Browse switchgears
      </UiButton>
    </div>

    <ResizablePanel
      v-if="isDesktop"
      class="switchgears-page__sidebar-panel"
      placement="left"
      storageKey="page-sidebar-width"
      :defaultSize="240"
      :minSize="200"
      :maxSize="400"
    >
      <aside class="switchgears-page__sidebar">
        <DeviceListSidebar />
      </aside>
    </ResizablePanel>

    <section class="switchgears-page__content">
      <div class="switchgears-page__view-tabs">
        <button
          type="button"
          class="switchgears-page__view-tab"
          :class="{ 'is-active': activeView === 'manage' }"
          @click="setActiveView('manage')"
        >
          Manage
        </button>
        <button
          type="button"
          class="switchgears-page__view-tab"
          :class="{ 'is-active': activeView === 'sld' }"
          @click="setActiveView('sld')"
        >
          Single Line Diagram
        </button>
      </div>

      <div v-if="activeView === 'manage'" class="switchgears-page__manage-view">
        <router-view />
      </div>

      <div v-else class="switchgears-page__sld-view">
        <SwitchgearSingleLineDiagram />
      </div>
    </section>

    <SlideOver
      v-if="!isDesktop"
      :open="sidebarOpen"
      title="Switchgears"
      placement="bottom"
      :max-height-vh="78"
      :close-on-item-click="true"
      @close="sidebarOpen = false"
    >
      <div class="switchgears-page__drawer-content">
        <DeviceListSidebar />
      </div>
    </SlideOver>
  </div>
</template>

<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref, watch } from "vue"
import { useRoute } from "vue-router"

import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import DeviceListSidebar from "./components/SwitchgearListSidebar.vue"
import SlideOver from "@/components/ui/SlideOver.vue"
import UiButton from "@/components/ui/UiButton.vue"
import SwitchgearSingleLineDiagram from "./components/SwitchgearSingleLineDiagram.vue"
import { useViewport } from "@/composables/useViewport"
import { useRealtimeScopeStore } from "@/stores/realtimeScopeStore"
import { localSettingsKeys, readLocalSetting, writeLocalSetting } from "@/services/localSettingsStorage"

const { isDesktop } = useViewport()
const sidebarOpen = ref(false)
const activeView = ref<"manage" | "sld">("manage")
const route = useRoute()
const realtimeScopeStore = useRealtimeScopeStore()
const scopeId = "switchgears:page"
const LEGACY_ACTIVE_VIEW_STORAGE_KEY = "unitlab.switchgears.active-view"

onMounted(() => {
  realtimeScopeStore.setGlobalRealtimeScope(scopeId, true)
  activeView.value = readLocalSetting<"manage" | "sld">(
    localSettingsKeys.switchgearsActiveView,
    "manage",
    {
      legacyKeys: [LEGACY_ACTIVE_VIEW_STORAGE_KEY],
      parseLegacy: raw => raw,
      validate: normalizeSwitchgearsActiveView,
    },
  )
})

onBeforeUnmount(() => {
  realtimeScopeStore.setGlobalRealtimeScope(scopeId, false)
})

watch(isDesktop, (next) => {
  if (next) sidebarOpen.value = false
})

watch(
  () => route.fullPath,
  () => {
    sidebarOpen.value = false
  },
)

function setActiveView(view: "manage" | "sld") {
  activeView.value = view
  writeLocalSetting(localSettingsKeys.switchgearsActiveView, view, {
    legacyKeys: [LEGACY_ACTIVE_VIEW_STORAGE_KEY],
  })
}

function normalizeSwitchgearsActiveView(value: unknown): "manage" | "sld" | null {
  return value === "manage" || value === "sld" ? value : null
}
</script>

<style scoped>
.switchgears-page {
  display: flex;
  height: 100%;
  flex-direction: column;
}

.switchgears-page__mobile-bar {
  padding: 0.75rem;
  border-bottom: 1px solid var(--color-neutral-200);
  background: var(--color-white);
}

.switchgears-page__browse-icon {
  width: 1rem;
  height: 1rem;
}

.switchgears-page__sidebar-panel {
  background: var(--color-white);
}

.switchgears-page__sidebar {
  display: flex;
  height: 100%;
  flex-direction: column;
  padding: 1rem;
}

.switchgears-page__content {
  display: flex;
  min-height: 0;
  flex: 1 1 auto;
  flex-direction: column;
  padding: 0.75rem;
}

.switchgears-page__view-tabs {
  display: inline-flex;
  width: fit-content;
  margin-bottom: 0.75rem;
  padding: 0.25rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: 0.75rem;
  background: color-mix(in srgb, var(--color-white) 85%, transparent);
  box-shadow: var(--shadow-sm);
}

.switchgears-page__view-tab {
  padding: 0.5rem 0.75rem;
  border: 0;
  border-radius: 0.5rem;
  background: transparent;
  color: var(--color-neutral-500);
  font: inherit;
  font-size: var(--text-sm);
  font-weight: 500;
  transition: background-color 0.15s ease, color 0.15s ease, box-shadow 0.15s ease;
}

.switchgears-page__view-tab:hover {
  color: var(--color-neutral-900);
}

.switchgears-page__view-tab.is-active {
  background: var(--color-neutral-100);
  color: var(--color-neutral-900);
  box-shadow: var(--shadow-sm);
}

.switchgears-page__manage-view,
.switchgears-page__sld-view {
  min-height: 0;
  flex: 1 1 auto;
}

.switchgears-page__manage-view {
  overflow-y: auto;
}

.switchgears-page__sld-view {
  overflow: hidden;
}

.switchgears-page__drawer-content {
  padding: 1rem;
}

@media (min-width: 768px) {
  .switchgears-page {
    flex-direction: row;
  }

  .switchgears-page__content {
    padding: 1rem;
  }
}

@media (min-width: 1024px) {
  .switchgears-page__mobile-bar {
    display: none;
  }
}

:global(.dark .switchgears-page__mobile-bar) {
  border-bottom-color: var(--color-neutral-800);
  background: var(--color-neutral-900);
}

:global(.dark .switchgears-page__sidebar-panel) {
  background: var(--color-neutral-900);
}

:global(.dark .switchgears-page__view-tabs) {
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-900) 80%, transparent);
}

:global(.dark .switchgears-page__view-tab) {
  color: var(--color-neutral-400);
}

:global(.dark .switchgears-page__view-tab:hover),
:global(.dark .switchgears-page__view-tab.is-active) {
  color: var(--color-neutral-100);
}

:global(.dark .switchgears-page__view-tab.is-active) {
  background: var(--color-neutral-800);
}
</style>
