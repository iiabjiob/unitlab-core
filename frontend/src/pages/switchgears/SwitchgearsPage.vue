<template>
  <div class="flex h-full flex-col md:flex-row">
    <div class="border-b border-neutral-200 bg-white p-3 dark:border-neutral-800 dark:bg-neutral-900 lg:hidden">
      <UiButton
        variant="secondary"
        size="base"
        :full="true"
        type="button"
        @click="sidebarOpen = true"
      >
        <svg xmlns="http://www.w3.org/2000/svg" class="h-4 w-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="1.5">
          <path stroke-linecap="round" stroke-linejoin="round" d="M4 6h16M4 12h12M4 18h8" />
        </svg>
        Browse switchgears
      </UiButton>
    </div>

    <ResizablePanel
      v-if="isDesktop"
      class="bg-white dark:bg-neutral-900"
      placement="left"
      storageKey="page-sidebar-width"
      :defaultSize="240"
      :minSize="200"
      :maxSize="400"
    >
      <aside class="flex h-full flex-col p-4">
        <DeviceListSidebar />
      </aside>
    </ResizablePanel>

    <section class="flex min-h-0 flex-1 flex-col p-3 md:p-4">
      <div class="mb-3 inline-flex w-fit rounded-xl border border-neutral-200 bg-white/85 p-1 shadow-sm dark:border-neutral-800 dark:bg-neutral-900/80">
        <button
          type="button"
          class="rounded-lg px-3 py-2 text-sm font-medium transition"
          :class="activeView === 'manage'
            ? 'bg-neutral-100 text-neutral-900 shadow-sm dark:bg-neutral-800 dark:text-neutral-100'
            : 'text-neutral-500 hover:text-neutral-900 dark:text-neutral-400 dark:hover:text-neutral-100'"
          @click="setActiveView('manage')"
        >
          Manage
        </button>
        <button
          type="button"
          class="rounded-lg px-3 py-2 text-sm font-medium transition"
          :class="activeView === 'sld'
            ? 'bg-neutral-100 text-neutral-900 shadow-sm dark:bg-neutral-800 dark:text-neutral-100'
            : 'text-neutral-500 hover:text-neutral-900 dark:text-neutral-400 dark:hover:text-neutral-100'"
          @click="setActiveView('sld')"
        >
          Single Line Diagram
        </button>
      </div>

      <div v-if="activeView === 'manage'" class="min-h-0 flex-1 overflow-y-auto">
        <router-view />
      </div>

      <div v-else class="min-h-0 flex-1 overflow-hidden">
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
      <div class="p-4">
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
