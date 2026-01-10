<template>
  <div class="flex h-full flex-col">
    <header class="flex items-center justify-between border-b border-neutral-200 px-4 py-3 dark:border-neutral-800">
      <div>
        <h2 class="text-lg font-semibold text-neutral-900 dark:text-neutral-50">Test Runs</h2>
        <p class="text-sm text-neutral-500">History of test executions for this workspace.</p>
      </div>
      <div class="flex items-center gap-2">
        <select v-model.number="selectedSnapshot" class="input">
          <option :value="null">Select locked snapshot</option>
          <option
            v-for="snapshot in lockedSnapshots"
            :key="snapshot.id"
            :value="snapshot.id"
          >
            {{ snapshot.source_filename ?? `Snapshot #${snapshot.id}` }}
          </option>
        </select>
        <input v-model.number="sequenceId" type="number" class="input w-32" placeholder="Sequence ID" />
        <button class="btn-primary" :disabled="!canRun" @click="run">Run test</button>
      </div>
    </header>

    <section class="flex-1 overflow-y-auto">
      <table class="min-w-full divide-y divide-neutral-200 dark:divide-neutral-800">
        <thead>
          <tr>
            <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Run</th>
            <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Snapshot</th>
            <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Sequence</th>
            <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Status</th>
            <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Created</th>
            <th class="px-3 py-2"></th>
          </tr>
        </thead>
        <tbody class="divide-y divide-neutral-100 dark:divide-neutral-800">
          <tr v-for="run in runs" :key="run.id">
            <td class="px-3 py-2 font-medium text-neutral-900 dark:text-neutral-50">#{{ run.id }}</td>
            <td class="px-3 py-2 text-sm">Snapshot {{ run.signal_snapshot_id }}</td>
            <td class="px-3 py-2 text-sm">Sequence {{ run.sequence_id }}</td>
            <td class="px-3 py-2">
              <UiBadge :variant="statusVariant(run.status)">{{ run.status }}</UiBadge>
            </td>
            <td class="px-3 py-2 text-sm text-neutral-500">{{ formatDate(run.created_at) }}</td>
            <td class="px-3 py-2 text-right">
              <button class="btn-secondary" @click="repeat(run.id)">Repeat</button>
            </td>
          </tr>
        </tbody>
      </table>
    </section>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from "vue"

import { useSignalSnapshotStore } from "@/stores/signalSnapshotStore"
import { useToastStore } from "@/stores/toastStore"
import UiBadge from "@/components/ui/UiBadge.vue"
import { formatDate } from "@/utils/datetime"

const snapshotStore = useSignalSnapshotStore()
const toastStore = useToastStore()
const runs = computed(() => snapshotStore.runs)
const lockedSnapshots = computed(() => snapshotStore.lockedSnapshots)
const selectedSnapshot = ref<number | null>(null)
const sequenceId = ref<number | null>(null)
const canRun = computed(() => Boolean(selectedSnapshot.value && sequenceId.value))

onMounted(() => {
  snapshotStore.refreshRuns()
})

function statusVariant(status: string) {
  switch (status) {
    case "completed":
      return "success"
    case "running":
      return "info"
    case "failed":
      return "danger"
    default:
      return "warning"
  }
}

async function run() {
  if (!selectedSnapshot.value || !sequenceId.value) return
  try {
    await snapshotStore.createTestRun({ signal_snapshot_id: selectedSnapshot.value, sequence_id: sequenceId.value })
    toastStore.success("Test run started")
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}

async function repeat(runId: number) {
  try {
    await snapshotStore.repeatRun(runId)
    toastStore.success("Test run cloned")
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}
</script>
