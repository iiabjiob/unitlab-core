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
        Browse signals
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

    <section class="flex-1 overflow-y-auto bg-neutral-50 p-4 dark:bg-neutral-950">
      <div class="rounded-2xl border border-neutral-200 bg-white shadow-sm dark:border-neutral-800 dark:bg-neutral-900">
        <UTabs v-model:activeIndex="activeTab" :tabs="tabs">
          <template #tab-0>
            <div class="h-full overflow-hidden">
              <SnapshotTable @configure="selectSnapshot" @delete="handleDelete" />
            </div>
          </template>
          <template #tab-1>
            <AllocationEditor :snapshot-id="selectedSnapshotId" />
          </template>
          <template #tab-2>
            <TestRunsTable />
          </template>
        </UTabs>
      </div>
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
  </div>
</template>

<script setup lang="ts">
import { computed, ref, watch } from "vue"

import AllocationEditor from "./components/AllocationEditor.vue"
import SignalListSidebar from "./components/SignalListSidebar.vue"
import SnapshotTable from "./components/SnapshotTable.vue"
import TestRunsTable from "./components/TestRunsTable.vue"
import ResizablePanel from "@/components/ui/ResizablePanel.vue"
import SlideOver from "@/components/ui/SlideOver.vue"
import UTabs from "@/components/ui/UTabs.vue"
import { useSignalSnapshotStore } from "@/stores/signalSnapshotStore"
import { useViewport } from "@/composables/useViewport"

const snapshotStore = useSignalSnapshotStore()
const { isDesktop } = useViewport()

const activeTab = ref(0)
const selectedSnapshotId = ref<number | null>(null)
const snapshots = computed(() => snapshotStore.snapshots)
const sidebarOpen = ref(false)
const tabs = [
  { label: "Snapshots" },
  { label: "Allocation" },
  { label: "Test Runs" },
]

watch(isDesktop, (next) => {
  if (next) sidebarOpen.value = false
})

defineSlots<{
  "tab-0": () => void
  "tab-1": () => void
  "tab-2": () => void
}>()

function selectSnapshot(id: number) {
  selectedSnapshotId.value = id
  activeTab.value = 1
  sidebarOpen.value = false
}

function openImport() {
  // TODO: open modal
}

async function handleDelete(id: number) {
  await snapshotStore.deleteSnapshot(id)
  if (selectedSnapshotId.value === id) {
    selectedSnapshotId.value = null
  }
}
</script>
