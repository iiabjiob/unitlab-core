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
        <DeviceListSidebar @import-scd="requestScdImport" />
      </aside>
    </ResizablePanel>

    <section class="switchgears-page__content">
      <router-view />
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
        <DeviceListSidebar @import-scd="requestScdImport" />
      </div>
    </SlideOver>
  </div>
</template>

<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"

import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import SlideOver from "@/components/ui/SlideOver.vue"
import UiButton from "@/components/ui/UiButton.vue"
import { useViewport } from "@/composables/useViewport"
import { localSettingsKeys, readLocalSetting, writeLocalSetting } from "@/services/localSettingsStorage"
import { useRealtimeScopeStore } from "@/stores/realtimeScopeStore"

import DeviceListSidebar from "./components/SwitchgearListSidebar.vue"

const { isDesktop } = useViewport()
const sidebarOpen = ref(false)
const route = useRoute()
const router = useRouter()
const realtimeScopeStore = useRealtimeScopeStore()
const scopeId = "switchgears:page"
type SwitchgearView = "sld" | "settings"

function resolveSwitchgearView(routeName: unknown): SwitchgearView | null {
  if (routeName === "switchgears.sld") return "sld"
  if (routeName === "switchgears.detail" || routeName === "switchgears.list") return "settings"
  return null
}

function isSwitchgearsRoute(routeName: unknown) {
  return resolveSwitchgearView(routeName) !== null
}

function persistSwitchgearView(view: SwitchgearView) {
  writeLocalSetting(localSettingsKeys.switchgearsActiveView, view)
}

function restoreLastSwitchgearView() {
  const lastView = readLocalSetting<SwitchgearView>(
    localSettingsKeys.switchgearsActiveView,
    "settings",
    { validate: (value) => value === "sld" || value === "settings" ? value : null },
  )
  if (lastView === "sld") {
    void router.replace({ name: "switchgears.sld" })
    return
  }
  persistSwitchgearView("settings")
}

onMounted(() => {
  realtimeScopeStore.setGlobalRealtimeScope(scopeId, true)
  if (route.name === "switchgears.list") {
    restoreLastSwitchgearView()
  } else {
    const currentView = resolveSwitchgearView(route.name)
    if (currentView) persistSwitchgearView(currentView)
  }
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

watch(() => route.name, (nextName, previousName) => {
  const currentView = resolveSwitchgearView(nextName)
  if (currentView === "sld") {
    persistSwitchgearView("sld")
    return
  }
  if (currentView === "settings" && nextName === "switchgears.detail") {
    persistSwitchgearView("settings")
    return
  }
  if (nextName === "switchgears.list") {
    if (isSwitchgearsRoute(previousName)) {
      persistSwitchgearView("settings")
    } else {
      restoreLastSwitchgearView()
    }
  }
})

async function requestScdImport() {
  sidebarOpen.value = false
  await router.push({ name: "switchgears.sld", query: { import: "1" } })
}

</script>

<style scoped>
.switchgears-page {
  display: flex;
  height: 100%;
  min-height: 0;
  min-width: 0;
  flex-direction: column;
  overflow: hidden;
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
  flex: 0 0 auto;
  background: var(--color-white);
}

.switchgears-page__sidebar {
  display: flex;
  height: 100%;
  min-height: 0;
  flex-direction: column;
  padding: 1rem;
}

.switchgears-page__content {
  display: flex;
  min-height: 0;
  min-width: 0;
  flex: 1 1 auto;
  flex-direction: column;
  padding: 0.75rem;
}

.switchgears-page__drawer-content {
  padding: 1rem;
}

@media (min-width: 1024px) {
  .switchgears-page {
    flex-direction: row;
  }

  .switchgears-page__content {
    padding: 1rem;
  }

  .switchgears-page__manage-view {
    overflow: hidden;
  }

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
