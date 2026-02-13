<template>
  <div class="h-dvh flex flex-col bg-neutral-50 dark:bg-neutral-900 text-neutral-800 dark:text-neutral-200 font-mono">
    <div class="flex flex-1 overflow-hidden">
      <!-- Left aside -->
      <ResizablePanel
        v-if="meta.leftAside"
        class="bg-white dark:bg-neutral-800"
        placement="left"
        storageKey="left-aside-width"
        :defaultSize="240"
        :minSize="200"
        :maxSize="400"
      >
        <AppAside class="border-r border-neutral-200 dark:border-neutral-700"/>
      </ResizablePanel>

      <!-- Center workspace (main + bottom log) -->
      <main class="flex-1 flex flex-col overflow-hidden">

        <header class="sticky top-0 z-20 border-b border-neutral-200 bg-white px-5 dark:border-neutral-800 dark:bg-neutral-900 h-20 flex items-center">
          <div class="flex flex-1 flex-wrap items-center justify-between gap-6">
            <div class="min-w-[220px] max-w-md flex-1">
              <WorkspaceSwitcher variant="mini" />
            </div>
            <TimeComponent class="text-sm text-neutral-500 dark:text-neutral-400" />
          </div>
        </header>

        <!-- Main content -->
        <div class="flex-1 overflow-auto">
          <RouterView />
        </div>
      </main>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { useRoute } from "vue-router"

import AppAside from "./DesktopAside.vue"
import ResizablePanel from "../ui/ResizablePanel.vue"
import WorkspaceSwitcher from "@/components/workspaces/WorkspaceSwitcher.vue"
import TimeComponent from "../misc/TimeComponent.vue"

const route = useRoute()
const meta = computed(() => ({
  leftAside: route.meta.leftAside ?? true,
}))

</script>
