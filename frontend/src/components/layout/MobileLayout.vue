<template>
  <div class="h-dvh flex flex-col bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200 font-mono">
    <!-- Header with burger + status -->
    <MobileHeader @open-drawer="isDrawerOpen = true">
      <template #left>
        <div class="flex gap-3 items-center">
          <AppLogo />
          <OnlineStatusComponent :status="status" :description="statusDescription" neutral-offline />
        </div>
      </template>
      <template #right>
        <TimeComponent />
      </template>
    </MobileHeader>

    <div class="border-b border-neutral-200 bg-white px-5 py-3 dark:border-neutral-800 dark:bg-neutral-900">
      <WorkspaceSwitcher variant="compact" />
    </div>


    <!-- Main content -->
    <main class="flex-1 overflow-auto relative">
      <RouterView />
    </main>


    <!-- Navigation -->
    <SlideOver :open="isDrawerOpen" placement="right" title="Menu" :widthPx="360" @close="isDrawerOpen = false">
      <AppMenu />
    </SlideOver>

  </div>
</template>

<script setup lang="ts">
import { ref, computed } from "vue"

import AppMenu from "./AppMenu.vue"
import AppLogo from "./AppLogo.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"
import TimeComponent from "../misc/TimeComponent.vue"
import { useSystemHealthStore } from "@/stores/systemHealthStore"
import SlideOver from "../ui/SlideOver.vue"
import MobileHeader from "./MobileHeader.vue"
import WorkspaceSwitcher from "@/components/workspaces/WorkspaceSwitcher.vue"

// Drawer state
const isDrawerOpen = ref(false)

const systemHealthStore = useSystemHealthStore()

const status = computed(() => systemHealthStore.status)
const statusDescription = computed(() => systemHealthStore.tooltip)

</script>
