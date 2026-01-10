<template>
  <div class="h-full overflow-hidden">
    <table class="min-w-full divide-y divide-neutral-200 dark:divide-neutral-800">
      <thead class="bg-neutral-50 dark:bg-neutral-900/50">
        <tr>
          <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Source</th>
          <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Rows</th>
          <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Status</th>
          <th class="px-3 py-2 text-left text-xs font-semibold uppercase tracking-wide text-neutral-500">Updated</th>
          <th class="px-3 py-2"></th>
        </tr>
      </thead>
      <tbody class="divide-y divide-neutral-100 dark:divide-neutral-800">
        <tr v-for="snapshot in snapshots" :key="snapshot.id">
          <td class="px-3 py-2">
            <p class="font-medium text-neutral-900 dark:text-neutral-50">
              {{ snapshot.source_filename ?? `Snapshot #${snapshot.id}` }}
            </p>
            <p class="text-xs text-neutral-500">{{ snapshot.source_hash }}</p>
          </td>
          <td class="px-3 py-2 text-sm text-neutral-700 dark:text-neutral-200">{{ snapshot.rows_count }}</td>
          <td class="px-3 py-2">
            <UiBadge :variant="snapshot.status === 'locked' ? 'success' : 'warning'">
              {{ snapshot.status }}
            </UiBadge>
          </td>
          <td class="px-3 py-2 text-sm text-neutral-500">{{ formatTime(snapshot.updated_at) }}</td>
          <td class="px-3 py-2">
            <div class="flex items-center justify-end gap-2">
              <button class="btn-secondary" @click="$emit('configure', snapshot.id)">Configure</button>
              <button
                class="btn-secondary"
                :disabled="snapshot.status === 'locked'"
                @click="lock(snapshot.id)"
              >
                Lock
              </button>
              <button class="btn-destructive" @click="$emit('delete', snapshot.id)">Delete</button>
            </div>
          </td>
        </tr>
      </tbody>
    </table>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted } from "vue"

import { useSignalSnapshotStore } from "@/stores/signalSnapshotStore"
import { useToastStore } from "@/stores/toastStore"
import UiBadge from "@/components/ui/UiBadge.vue"
import { formatDate } from "@/utils/datetime"

const snapshotStore = useSignalSnapshotStore()
const toastStore = useToastStore()
const snapshots = computed(() => snapshotStore.snapshots)

onMounted(() => {
  snapshotStore.refreshSnapshots()
})

function formatTime(value: string) {
  return formatDate(value)
}

async function lock(id: number) {
  try {
    await snapshotStore.lockSnapshot(id)
    toastStore.success("Snapshot locked")
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}
</script>
