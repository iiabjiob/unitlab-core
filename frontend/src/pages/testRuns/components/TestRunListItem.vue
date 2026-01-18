<template>
  <div
    :class="[
      'cursor-pointer rounded-xl border px-3 py-2 transition-colors',
      active
        ? 'border-blue-500 bg-blue-50 dark:border-blue-400/80 dark:bg-blue-400/10'
        : 'border-transparent hover:border-neutral-300 hover:bg-neutral-50 dark:hover:border-neutral-700 dark:hover:bg-neutral-800'
    ]"
  >
    <div class="flex items-center justify-between text-sm font-semibold text-neutral-900 dark:text-neutral-50">
      <span>Run #{{ run.id }}</span>
      <UiBadge :variant="statusVariant(run.status)">{{ run.status }}</UiBadge>
    </div>
    <div class="mt-1 text-xs text-neutral-500 dark:text-neutral-400">
      <span class="uppercase tracking-[0.3em] text-[10px] text-neutral-400 dark:text-neutral-500">{{ snapshotLabel }}</span>
      <span class="mx-1">·</span>
      <span>{{ created }}</span>
    </div>
    <div class="mt-2 text-xs text-neutral-600 dark:text-neutral-300" v-if="sequenceSummary">
      {{ sequenceSummary }}
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import UiBadge from "@/components/ui/UiBadge.vue"
import type { TestRunRecord } from "@/types/signal"

const props = defineProps<{
  run: TestRunRecord
  active?: boolean
  sequenceNameMap: Map<number, string>
}>()

const created = computed(() => new Date(props.run.created_at).toLocaleString())
const snapshotLabel = computed(() => (props.run.snapshot ? "SNAPSHOT READY" : "SNAPSHOT PENDING"))

const sequenceSummary = computed(() => {
  if (!props.run.sequence_ids.length) return ""
  const names = props.run.sequence_ids
    .map(id => props.sequenceNameMap.get(id) ?? `Instruction #${id}`)
  if (!names.length) return ""
  return names.slice(0, 2).join(", ") + (names.length > 2 ? ` +${names.length - 2} more` : "")
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
</script>
