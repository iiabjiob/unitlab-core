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
        :minSize="72"
        :maxSize="400"
        @size-change="handleLeftAsideSizeChange"
      >
        <AppAside
          :compact="isAsideCompact"
          class="border-r border-neutral-200 dark:border-neutral-700"
        />
      </ResizablePanel>

      <!-- Center workspace (main + bottom log) -->
      <main class="flex-1 flex flex-col overflow-hidden">

        <header class="sticky top-0 z-20 border-b border-neutral-200 bg-white px-5 dark:border-neutral-800 dark:bg-neutral-900 h-20 flex items-center">
          <div class="flex flex-1 flex-wrap items-center justify-between gap-6">
            <div class="min-w-[220px] max-w-md flex-1">
              <WorkspaceSwitcher variant="mini" />
            </div>
            <div class="flex items-center gap-3">
              <GlobalRunStatusLink />
              <TimeComponent class="text-sm text-neutral-500 dark:text-neutral-400" />
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
import { computed, ref } from "vue"
import { useRoute } from "vue-router"

import AppAside from "./DesktopAside.vue"
import ResizablePanel from "../ui/ResizablePanel.vue"
import WorkspaceSwitcher from "@/components/workspaces/WorkspaceSwitcher.vue"
import TimeComponent from "../misc/TimeComponent.vue"
import GlobalRunStatusLink from "./GlobalRunStatusLink.vue"

const route = useRoute()
const meta = computed(() => ({
  leftAside: route.meta.leftAside ?? true,
}))

const leftAsideWidth = ref(240)
const ASIDE_COMPACT_THRESHOLD_PX = 228
const isAsideCompact = computed(() => leftAsideWidth.value <= ASIDE_COMPACT_THRESHOLD_PX)

function handleLeftAsideSizeChange(size: number) {
  if (!Number.isFinite(size)) {
    return
  }
  leftAsideWidth.value = Math.max(0, Math.round(size))
}

</script>
