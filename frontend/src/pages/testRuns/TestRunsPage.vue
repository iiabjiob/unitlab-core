<template>
  <div class="flex h-full flex-col md:flex-row">
    <div class="border-b border-neutral-200 bg-white p-3 dark:border-neutral-800 dark:bg-neutral-900 md:hidden">
      <button
        class="flex w-full items-center justify-center gap-2 rounded-lg border border-neutral-200 bg-white px-3 py-2 text-sm font-semibold text-neutral-700 shadow-sm dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100"
        type="button"
        @click="sidebarOpen = true"
      >
        <svg xmlns="http://www.w3.org/2000/svg" class="h-4 w-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="1.5">
          <path stroke-linecap="round" stroke-linejoin="round" d="M4 6h16M4 12h12M4 18h8" />
        </svg>
        Browse test runs
      </button>
    </div>

    <ResizablePanel
      v-if="isDesktop"
      class="bg-white dark:bg-neutral-900"
      placement="left"
      storageKey="test-runs-sidebar-width"
      :defaultSize="280"
      :minSize="220"
      :maxSize="380"
    >
      <aside class="flex h-full flex-col p-4">
        <TestRunListSidebar
          :runs="runs"
          :loading="runsLoading"
          :selected-id="selectedRunId"
          :sequence-name-map="sequenceNameMap"
          @select="handleSelect"
          @create="handleCreate"
        />
      </aside>
    </ResizablePanel>

    <section class="flex-1 overflow-y-auto p-3 md:p-4">
      <RouterView />
    </section>

    <SlideOver
      v-if="!isDesktop"
      :open="sidebarOpen"
      title="Test runs"
      placement="left"
      :widthPx="360"
      @close="sidebarOpen = false"
    >
      <div class="p-4">
        <TestRunListSidebar
          :runs="runs"
          :loading="runsLoading"
          :selected-id="selectedRunId"
          :sequence-name-map="sequenceNameMap"
          @select="handleSelect"
          @create="handleCreate"
        />
      </div>
    </SlideOver>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"

import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import SlideOver from "@/components/ui/SlideOver.vue"
import TestRunListSidebar from "./components/TestRunListSidebar.vue"
import { useTestRunStore } from "@/stores/testRunStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useViewport } from "@/composables/useViewport"

const testRunStore = useTestRunStore()
const sequenceStore = useSequenceStore()
const workspaceStore = useWorkspaceStore()
const { isDesktop } = useViewport()
const route = useRoute()
const router = useRouter()

const sidebarOpen = ref(false)

const runs = computed(() => testRunStore.testRuns)
const runsLoading = computed(() => testRunStore.loading)

const sequenceNameMap = computed(() => {
  const map = new Map<number, string>()
  sequenceStore.sequences.forEach(seq => map.set(seq.id, seq.name))
  return map
})

const selectedRunId = computed<number | null>(() => {
  if (route.name !== "testRuns.detail") return null
  const raw = Array.isArray(route.params.runId) ? route.params.runId[0] : route.params.runId
  const parsed = Number(raw)
  return Number.isFinite(parsed) ? parsed : null
})

async function hydrate() {
  if (!workspaceStore.activeWorkspaceId) return
  await Promise.allSettled([
    testRunStore.refreshRuns(),
    sequenceStore.ensureLoaded(),
  ])
}

onMounted(() => {
  void hydrate()
})

watch(
  () => workspaceStore.activeWorkspaceId,
  () => {
    void hydrate()
  },
)

watch(isDesktop, (next) => {
  if (next) sidebarOpen.value = false
})

function handleSelect(id: number) {
  router.push({ name: "testRuns.detail", params: { runId: id } })
  sidebarOpen.value = false
}

function handleCreate() {
  router.push({ name: "testRuns.new" })
  sidebarOpen.value = false
}
</script>
