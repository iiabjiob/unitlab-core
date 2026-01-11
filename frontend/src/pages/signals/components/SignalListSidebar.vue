<template>
  <div class="h-full flex flex-col">

    <div class="mb-3 space-y-2">
      <UiButton variant="primary" size="sm" full :disabled="workspaceMissing" @click="$emit('import')">
        + Import Signal List
      </UiButton>
      <UiButton variant="ghost" size="sm" full :disabled="workspaceMissing" @click="$emit('open-test-runs')">
        Test runs history
      </UiButton>
      <p
        v-if="workspaceMissing"
        class="mt-2 text-[11px] uppercase tracking-[0.3em] text-neutral-500 dark:text-neutral-400"
      >
        Choose a workspace to view snapshots
      </p>
    </div>

    <div class="mb-3">
      <input
        v-model="query"
        type="text"
        autocomplete="off"
        name="snapshot-search"
        :disabled="workspaceMissing"
        :placeholder="workspaceMissing ? 'Select a workspace to get started' : 'Search snapshots…'"
        class="w-full rounded-lg border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 placeholder-neutral-500 focus:outline-none disabled:opacity-60 dark:border-neutral-700 dark:bg-neutral-950 dark:text-neutral-100"
      />
    </div>

    <div class="flex-1 overflow-y-auto space-y-1">
      <div
        v-if="workspaceMissing"
        class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400"
      >
        Snapshots belong to a workspace. Pick one to manage signal imports.
      </div>

      <template v-else>
        <div
          v-if="loading"
          class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400"
        >
          Loading snapshots…
        </div>

        <template v-else>
          <div
            v-for="snapshot in filteredSnapshots"
            :key="snapshot.id"
          >
            <SignalListItem
              :snapshot="snapshot"
              :active="snapshot.id === selectedId"
              @select="$emit('select', snapshot.id)"
            />
          </div>

          <div
            v-if="filteredSnapshots.length === 0"
            class="text-gray-500 text-xs italic px-2 py-2"
          >
            No snapshots found
          </div>
        </template>
      </template>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from "vue"

import UiButton from "@/components/ui/UiButton.vue"
import SignalListItem from "./SignalListItem.vue"
import type { SignalSnapshotSummary } from "@/types/signal"
import { useWorkspaceStore } from "@/stores/workspaceStore"

const props = defineProps<{
  snapshots: SignalSnapshotSummary[]
  loading?: boolean
  selectedId: number | null
}>()

defineEmits<{ (e: "select", id: number): void; (e: "import"): void; (e: "open-test-runs"): void }>()

const workspaceStore = useWorkspaceStore()
const query = ref("")

const workspaceMissing = computed(() => !workspaceStore.activeWorkspaceId)

const filteredSnapshots = computed(() => {
  if (workspaceMissing.value) return []
  if (!query.value.trim()) return props.snapshots
  const q = query.value.toLowerCase()
  return props.snapshots.filter(snapshot => {
    const title = snapshot.source_filename ?? `snapshot-${snapshot.id}`
    return title.toLowerCase().includes(q) || (snapshot.source_hash?.toLowerCase().includes(q) ?? false)
  })
})

const loading = computed(() => props.loading ?? false)
</script>
