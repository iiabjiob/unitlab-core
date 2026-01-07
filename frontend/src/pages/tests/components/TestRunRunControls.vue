<script setup lang="ts">
import { computed } from "vue"
import type { TestRunState, TestRunSummary } from "@/types/testRuns"
import { TestRunStatusEnum } from "@/types/testRuns"

const props = defineProps<{
  run: TestRunSummary
  state?: TestRunState | null
  totalSteps: number
  starting?: boolean
  cancelling?: boolean
}>()
const emit = defineEmits<{ (e: "start"): void; (e: "cancel"): void }>()

const effectiveState = computed(() => props.state ?? null)
const status = computed(() => effectiveState.value?.status ?? props.run.status)
const statusLabel = computed(() => status.value.replace(/_/g, " "))

const statusColor = computed(() => {
  switch (status.value) {
    case TestRunStatusEnum.RUNNING:
      return "text-blue-400"
    case TestRunStatusEnum.COMPLETED:
      return "text-emerald-400"
    case TestRunStatusEnum.FAILED:
      return "text-red-400"
    case TestRunStatusEnum.CANCELLED:
      return "text-neutral-400"
    default:
      return "text-neutral-500"
  }
})

const progressPercent = computed(() => {
  const total = effectiveState.value?.total_steps ?? props.totalSteps
  if (!total) return 0
  const current = effectiveState.value?.current_step_index ?? props.run.current_step_index
  const clamped = Math.min(current, total)
  return Math.round((clamped / total) * 100)
})

const totalStepsDisplay = computed(() => effectiveState.value?.total_steps ?? props.totalSteps)
const currentIndexDisplay = computed(() => {
  const total = totalStepsDisplay.value
  const idx = effectiveState.value?.current_step_index ?? props.run.current_step_index
  if (!total) return 0
  return Math.min(idx + 1, total)
})

const canStart = computed(() =>
  [
    TestRunStatusEnum.PENDING,
    TestRunStatusEnum.COMPLETED,
    TestRunStatusEnum.FAILED,
    TestRunStatusEnum.CANCELLED,
  ].includes(status.value as TestRunStatusEnum),
)

const canCancel = computed(() => status.value === TestRunStatusEnum.RUNNING)
</script>

<template>
  <div class="py-4 flex flex-wrap items-center gap-4">
    <div class="flex items-center gap-2">
      <button
        v-if="canStart"
        type="button"
        class="px-3 py-1 rounded bg-green-600 hover:bg-green-500 text-white text-sm font-medium disabled:opacity-60"
        :disabled="props.starting"
        @click="emit('start')"
      >
        ▶ Start
      </button>
      <button
        v-if="canCancel"
        type="button"
        class="px-3 py-1 rounded bg-red-600 hover:bg-red-500 text-white text-sm font-medium disabled:opacity-60"
        :disabled="props.cancelling"
        @click="emit('cancel')"
      >
        ■ Cancel
      </button>
    </div>

    <div class="flex items-center gap-3 text-sm">
      <div :class="['font-medium flex items-center gap-1', statusColor]">
        <span class="font-mono">●</span>
        <span class="uppercase tracking-wider text-xs">{{ statusLabel }}</span>
      </div>
      <div class="text-xs text-neutral-500 dark:text-neutral-400">
        Step {{ currentIndexDisplay }} / {{ totalStepsDisplay }}
      </div>
      <div class="text-xs text-neutral-500 dark:text-neutral-400">
        ({{ progressPercent }}%)
      </div>
    </div>
  </div>
</template>
