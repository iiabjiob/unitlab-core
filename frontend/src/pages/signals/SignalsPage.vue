<template>
  <div class="flex h-full min-h-0 min-w-0 flex-col md:flex-row">
    <div class="border-b border-neutral-200 bg-white p-3 dark:border-neutral-800 dark:bg-neutral-900 md:hidden">
      <button
        class="flex w-full items-center justify-center gap-2 rounded-lg border border-neutral-200 bg-white px-3 py-2 text-sm font-semibold text-neutral-700 shadow-sm dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100"
        type="button"
        @click="sidebarOpen = true"
      >
        <svg xmlns="http://www.w3.org/2000/svg" class="h-4 w-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" stroke-width="1.5">
          <path stroke-linecap="round" stroke-linejoin="round" d="M4 6h16M4 12h12M4 18h8" />
        </svg>
        Browse snapshots
      </button>
    </div>

    <ResizablePanel
      v-if="isDesktop"
      class="bg-white dark:bg-neutral-900"
      placement="left"
      storageKey="signals-sidebar-width"
      :defaultSize="260"
      :minSize="220"
      :maxSize="400"
    >
      <aside class="flex h-full flex-col p-4">
        <SignalListSidebar
          :snapshots="snapshots"
          :loading="snapshotStore.loading"
          :selected-id="selectedSnapshotId"
          @select="selectSnapshot"
          @import="openImport"
        />
      </aside>
    </ResizablePanel>

    <section class="flex-1 overflow-y-auto p-3 md:p-0">
      <RouterView />
    </section>

    <SlideOver
      v-if="!isDesktop"
      :open="sidebarOpen"
      title="Signals"
      placement="left"
      :widthPx="360"
      @close="sidebarOpen = false"
    >
      <div class="p-4">
        <SignalListSidebar
          :snapshots="snapshots"
          :loading="snapshotStore.loading"
          :selected-id="selectedSnapshotId"
          @select="selectSnapshot"
          @import="openImport"
        />
      </div>
    </SlideOver>

    <SignalImportModal :open="importModalOpen" @close="closeImport" @imported="handleImported" />
  </div>
</template>

<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useRoute, useRouter } from "vue-router"

import SignalListSidebar from "./components/SignalListSidebar.vue"
import SignalImportModal from "./components/SignalImportModal.vue"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import SlideOver from "@/components/ui/SlideOver.vue"
import { useSignalSnapshotStore } from "@/stores/signalSnapshotStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useViewport } from "@/composables/useViewport"
import { getLogger } from "@/utils/logger"

const snapshotStore = useSignalSnapshotStore()
const workspaceStore = useWorkspaceStore()
const { isDesktop } = useViewport()
const route = useRoute()
const router = useRouter()
const logger = getLogger("signals-page")
const signalsDiagMode = typeof window !== "undefined"
  ? (new URLSearchParams(window.location.search).get("signalsdiag") ?? "").trim().toLowerCase()
  : ""
const signalsDiagNoBootstrap = signalsDiagMode === "no-bootstrap"

const snapshots = computed(() => snapshotStore.snapshots)
const sidebarOpen = ref(false)
const importModalOpen = ref(false)
const lastBootstrappedWorkspaceId = ref<number | null>(null)
const bootstrapping = ref(false)

const selectedSnapshotId = computed<number | null>(() => {
  if (route.name !== "signals.detail") return null
  const raw = Array.isArray(route.params.snapshotId) ? route.params.snapshotId[0] : route.params.snapshotId
  const parsed = Number(raw)
  return Number.isFinite(parsed) ? parsed : null
})

watch(isDesktop, (next) => {
  if (next) sidebarOpen.value = false
})

watch(
  () => workspaceStore.activeWorkspaceId,
  (workspaceId) => {
    if (!workspaceId) {
      lastBootstrappedWorkspaceId.value = null
      return
    }
    if (lastBootstrappedWorkspaceId.value === workspaceId && snapshots.value.length) {
      return
    }
    lastBootstrappedWorkspaceId.value = workspaceId
    bootstrapping.value = true
    void bootstrapWorkspace(workspaceId).finally(() => {
      bootstrapping.value = false
    })
  },
  { immediate: true },
)

async function bootstrapWorkspace(workspaceId: number) {
  if (signalsDiagNoBootstrap) {
    logger.warn(`Signals bootstrap skipped via signalsdiag=${signalsDiagMode || "unknown"} for #${workspaceId}`)
    return
  }
  try {
    await snapshotStore.refreshSnapshots()
    logger.info(`Signals workspace bootstrapped for #${workspaceId}`)
  } catch (err) {
    logger.error("Failed to bootstrap Signals workspace", err)
  }
}

async function selectSnapshot(id: number) {
  await router.push({ name: "signals.detail", params: { snapshotId: id } })
  sidebarOpen.value = false
}

function openImport() {
  importModalOpen.value = true
}

function closeImport() {
  importModalOpen.value = false
}

async function handleImported(snapshotId: number) {
  await selectSnapshot(snapshotId)
  importModalOpen.value = false
}
</script>
