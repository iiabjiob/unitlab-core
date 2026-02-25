<template>
  <div class="flex h-full flex-col md:flex-row">
    <div class="border-b border-neutral-200 bg-white p-3 dark:border-neutral-800 dark:bg-neutral-900 md:hidden">
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
        Browse settings
      </UiButton>
    </div>

    <ResizablePanel
      v-if="isDesktop"
      class="bg-white dark:bg-neutral-900"
      placement="left"
      storageKey="settings-sidebar-width"
      :defaultSize="260"
      :minSize="220"
      :maxSize="420"
    >
      <aside class="flex h-full flex-col p-4">
        <SettingsListSidebar />
      </aside>
    </ResizablePanel>

    <section class="flex-1 overflow-y-auto p-3 md:p-4">
      <router-view />
    </section>

    <SlideOver
      v-if="!isDesktop"
      :open="sidebarOpen"
      title="Settings"
      placement="bottom"
      :max-height-vh="78"
      :close-on-item-click="true"
      @close="sidebarOpen = false"
    >
      <div class="p-4">
        <SettingsListSidebar />
      </div>
    </SlideOver>
  </div>
</template>

<script setup lang="ts">
import { ref, watch } from "vue"
import { useRoute } from "vue-router"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import SlideOver from "@/components/ui/SlideOver.vue"
import UiButton from "@/components/ui/UiButton.vue"
import { useViewport } from "@/composables/useViewport"
import SettingsListSidebar from "./components/SettingsListSidebar.vue"

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
