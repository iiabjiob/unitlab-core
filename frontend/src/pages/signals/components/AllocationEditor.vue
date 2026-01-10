<template>
  <div class="flex h-full flex-col gap-4 p-4">
    <header class="flex items-center justify-between">
      <div>
        <h2 class="text-lg font-semibold text-neutral-900 dark:text-neutral-50">Allocation</h2>
        <p class="text-sm text-neutral-500">Map snapshot rows to physical channels.</p>
      </div>
      <div class="flex items-center gap-2">
        <select v-model="selectedSnapshotId" class="input">
          <option :value="null">Select snapshot</option>
          <option v-for="snapshot in snapshots" :key="snapshot.id" :value="snapshot.id">
            {{ snapshot.source_filename ?? `Snapshot #${snapshot.id}` }}
          </option>
        </select>
        <button class="btn-primary" :disabled="!selectedSnapshotId" @click="save">Save mapping</button>
      </div>
    </header>

    <div v-if="!selectedSnapshotId" class="flex flex-1 items-center justify-center rounded-lg border border-dashed border-neutral-300 p-8 text-neutral-500">
      Select a snapshot to configure allocation
    </div>

    <div v-else class="flex flex-1 flex-col gap-3 overflow-hidden">
      <div class="grid flex-1 grid-cols-1 gap-4 lg:grid-cols-2">
        <section class="rounded-lg border border-neutral-200 bg-white p-4 shadow-sm dark:border-neutral-800 dark:bg-neutral-900">
          <h3 class="mb-3 text-sm font-semibold uppercase tracking-wide text-neutral-500">Snapshot rows</h3>
          <div class="max-h-80 overflow-y-auto text-sm">
            <ul>
              <li
                v-for="(row, index) in snapshotRows"
                :key="index"
                class="flex items-center justify-between border-b border-neutral-100 py-2 text-neutral-700 dark:border-neutral-800 dark:text-neutral-100"
              >
                <span class="truncate text-xs font-medium">Row {{ index + 1 }} — {{ row.signal ?? row.name ?? 'unnamed' }}</span>
                <span class="text-xs text-neutral-400">{{ row.key ?? row.id ?? 'N/A' }}</span>
              </li>
            </ul>
          </div>
        </section>
        <section class="rounded-lg border border-neutral-200 bg-white p-4 shadow-sm dark:border-neutral-800 dark:bg-neutral-900">
          <h3 class="mb-3 text-sm font-semibold uppercase tracking-wide text-neutral-500">Channel mapping</h3>
          <div class="space-y-2">
            <div
              v-for="(item, index) in mapping"
              :key="index"
              class="flex items-center gap-2"
            >
              <input
                v-model="item.channel_id"
                class="input flex-1"
                placeholder="Channel ID"
              />
              <input
                v-model="item.signal_key"
                class="input flex-1"
                placeholder="Signal key"
              />
              <input
                v-model.number="item.signal_row_index"
                type="number"
                class="input w-24"
                min="0"
              />
            </div>
            <button class="btn-secondary w-full" type="button" @click="addMapping">Add row</button>
          </div>
        </section>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, watch } from "vue"

import { useSignalSnapshotStore } from "@/stores/signalSnapshotStore"
import { useToastStore } from "@/stores/toastStore"
import type { AllocationMappingItem } from "@/types/signal"

const props = defineProps<{ snapshotId: number | null }>()

const snapshotStore = useSignalSnapshotStore()
const toastStore = useToastStore()

const selectedSnapshotId = ref<number | null>(props.snapshotId ?? null)
const mapping = ref<AllocationMappingItem[]>([])
const snapshotRows = ref<Array<Record<string, unknown>>>([])

const snapshots = computed(() => snapshotStore.snapshots)

watch(
  () => props.snapshotId,
  (next) => {
    selectedSnapshotId.value = next ?? null
  },
)

watch(
  selectedSnapshotId,
  (next) => {
    if (!next) {
      mapping.value = createEmptyMapping()
      snapshotRows.value = []
      return
    }
    initialize(next)
  },
  { immediate: true },
)

async function initialize(snapshotId: number) {
  try {
    const allocation = await snapshotStore.getAllocation(snapshotId)
    mapping.value = allocation.mapping.length ? allocation.mapping : createEmptyMapping()
    const snapshot = await snapshotStore.getSnapshot(snapshotId)
    snapshotRows.value = snapshot.data ?? []
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

function createEmptyMapping(): AllocationMappingItem[] {
  return [
    {
      channel_id: "",
      signal_key: "",
      signal_row_index: 0,
      meta: null,
    },
  ]
}

function addMapping() {
  mapping.value.push({ channel_id: "", signal_key: "", signal_row_index: mapping.value.length, meta: null })
}

async function save() {
  if (!selectedSnapshotId.value) return
  try {
    await snapshotStore.saveAllocation(selectedSnapshotId.value, mapping.value)
    toastStore.success("Allocation saved")
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}
</script>
