<template>
  <div class="flex h-full flex-col">
    <div class="mb-4 space-y-2">
      <UiButton variant="primary" size="sm" full @click="$emit('import')">
        + Import Signal List
      </UiButton>
      <UiButton variant="ghost" size="sm" full @click="$emit('open-test-runs')">
        Test runs history
      </UiButton>
    </div>

    <div class="mb-4">
      <input
        v-model="query"
        type="text"
        autocomplete="off"
        name="snapshot-search"
        placeholder="Search snapshots…"
        class="w-full rounded-lg border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 placeholder-neutral-500 focus:outline-none disabled:opacity-60 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100"
      />
    </div>

    <div class="flex-1 space-y-1 overflow-y-auto">
      <div
        v-if="loading"
        class="rounded-xl border border-dashed border-neutral-200 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400"
      >
        Loading snapshots…
      </div>

      <div v-else-if="filteredSnapshots.length === 0" class="rounded-xl border border-dashed border-neutral-200 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400">
        No snapshots yet.
      </div>

      <button
        v-for="snapshot in filteredSnapshots"
        :key="snapshot.id"
        type="button"
        class="flex w-full items-center justify-between rounded-xl border px-3 py-2 text-left transition"
        :class="snapshot.id === selectedId
          ? 'border-neutral-900 bg-neutral-900/5 text-neutral-900 dark:border-white/70 dark:bg-white/5 dark:text-white'
          : 'border-transparent bg-transparent text-neutral-600 hover:border-neutral-300 hover:bg-white dark:text-neutral-300 dark:hover:border-neutral-700 dark:hover:bg-neutral-900'"
        @click="$emit('select', snapshot.id)"
      >
        <div class="min-w-0">
          <p class="truncate text-sm font-semibold">
            {{ snapshot.source_filename ?? `Snapshot #${snapshot.id}` }}
          </p>
          <p class="truncate text-[11px] uppercase tracking-wide text-neutral-400">
            {{ snapshot.rows_count }} rows · schema v{{ snapshot.schema_version }}
          </p>
        </div>
        <UiBadge :variant="snapshot.status === 'locked' ? 'success' : 'warning'">
          {{ snapshot.status }}
        </UiBadge>
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from "vue"

import UiBadge from "@/components/ui/UiBadge.vue"
import UiButton from "@/components/ui/UiButton.vue"
import type { SignalSnapshotSummary } from "@/types/signal"

const props = defineProps<{
  snapshots: SignalSnapshotSummary[]
  loading?: boolean
  selectedId: number | null
}>()

defineEmits<{ (e: "select", id: number): void; (e: "import"): void; (e: "open-test-runs"): void }>()

const query = ref("")

const filteredSnapshots = computed(() => {
  if (!query.value.trim()) return props.snapshots
  const q = query.value.toLowerCase()
  return props.snapshots.filter(snapshot => {
    const title = snapshot.source_filename ?? `snapshot-${snapshot.id}`
    return title.toLowerCase().includes(q) || (snapshot.source_hash?.toLowerCase().includes(q) ?? false)
  })
})

const loading = computed(() => props.loading ?? false)
</script>
