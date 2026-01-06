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

        <header class="sticky top-0 z-20 border-b border-neutral-200/70 bg-white/90 px-6 py-4 backdrop-blur dark:border-neutral-800/70 dark:bg-neutral-900/90">
          <div class="flex flex-wrap items-center justify-between gap-4">
            <ProjectSwitcher />
            <div
              v-if="activeProject"
              class="text-right text-xs text-neutral-500 dark:text-neutral-400"
            >
              <p class="uppercase tracking-[0.4em] text-[10px]">UUID</p>
              <p class="font-mono text-sm text-neutral-800 dark:text-neutral-100">
                {{ activeProject.uuid }}
              </p>
            </div>
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
import ProjectSwitcher from "@/components/projects/ProjectSwitcher.vue"
import { useProjectStore } from "@/stores/projectStore"

const route = useRoute()
const meta = computed(() => ({
  leftAside: route.meta.leftAside ?? true,
}))

const projectStore = useProjectStore()
const activeProject = computed(() => projectStore.activeProject)
</script>
