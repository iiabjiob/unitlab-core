<template>
  <div v-if="isVisible" class="flex items-center gap-1.5">
    <button
      v-if="sequenceChipVisible"
      type="button"
      class="inline-flex items-center rounded-md border border-neutral-200 bg-neutral-50/85 text-neutral-700 transition hover:bg-neutral-100 dark:border-neutral-700 dark:bg-neutral-800/70 dark:text-neutral-200 dark:hover:bg-neutral-800"
      :class="compact ? 'gap-1 px-2 py-1 text-[10px]' : 'gap-2 px-2.5 py-1 text-[11px]'"
      :title="sequenceTooltipText"
      :aria-label="sequenceTooltipText"
      @click="navigateToSequence"
    >
      <span
        class="h-1.5 w-1.5 rounded-full"
        :class="sequenceIndicatorClass"
        aria-hidden="true"
      ></span>
      <span class="font-semibold uppercase tracking-[0.08em]">Sequence</span>
      <span v-if="!compact" class="max-w-[220px] truncate">{{ sequenceDetailText }}</span>
    </button>

    <button
      v-if="signalChipVisible"
      type="button"
      class="inline-flex items-center rounded-md border border-neutral-200 bg-neutral-50/85 text-neutral-700 transition hover:bg-neutral-100 dark:border-neutral-700 dark:bg-neutral-800/70 dark:text-neutral-200 dark:hover:bg-neutral-800"
      :class="compact ? 'gap-1 px-2 py-1 text-[10px]' : 'gap-2 px-2.5 py-1 text-[11px]'"
      :title="signalTooltipText"
      :aria-label="signalTooltipText"
      @click="navigateToSignals"
    >
      <span
        class="h-1.5 w-1.5 rounded-full"
        :class="signalIndicatorClass"
        aria-hidden="true"
      ></span>
      <span class="font-semibold uppercase tracking-[0.08em]">Test Run</span>
      <span v-if="!compact" class="max-w-[220px] truncate">{{ signalDetailText }}</span>
    </button>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { storeToRefs } from "pinia"
import { useRouter } from "vue-router"

import { useSignalJobStore } from "@/stores/signalJobStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import { SequenceStatusEnum, type SequenceState } from "@/types/sequences"

const props = withDefaults(defineProps<{ compact?: boolean }>(), {
  compact: false,
})

const router = useRouter()
const signalJobStore = useSignalJobStore()
const sequenceStore = useSequenceStore()

const { activeJobs } = storeToRefs(signalJobStore)
const { states, sequences } = storeToRefs(sequenceStore)

const compact = computed(() => Boolean(props.compact))

const activeSignalTestRun = computed(() => (
  activeJobs.value.find(job => String(job.operation) === "test_run") ?? null
))

const activeSequenceState = computed<SequenceState | null>(() => {
  const activeStatuses = new Set<SequenceStatusEnum>([
    SequenceStatusEnum.PENDING,
    SequenceStatusEnum.RUNNING,
    SequenceStatusEnum.CANCELLING,
  ])

  const candidates = Object.values(states.value)
    .filter(state => activeStatuses.has(state.status))

  if (!candidates.length) {
    return null
  }

  const toMillis = (value: string | null | undefined): number => {
    const parsed = Date.parse(String(value ?? ""))
    return Number.isFinite(parsed) ? parsed : 0
  }

  return candidates
    .slice()
    .sort((left, right) => {
      const leftStartedAt = toMillis(left.started_at)
      const rightStartedAt = toMillis(right.started_at)
      if (leftStartedAt !== rightStartedAt) {
        return rightStartedAt - leftStartedAt
      }
      return right.sequence_id - left.sequence_id
    })[0] ?? null
})

const activeSequence = computed(() => {
  const state = activeSequenceState.value
  if (!state) {
    return null
  }
  return sequences.value.find(sequence => sequence.id === state.sequence_id) ?? null
})

const sequenceChipVisible = computed(() => Boolean(activeSequenceState.value))
const signalChipVisible = computed(() => Boolean(activeSignalTestRun.value))
const isVisible = computed(() => sequenceChipVisible.value || signalChipVisible.value)

const sequenceDetailText = computed(() => {
  const sequenceState = activeSequenceState.value
  if (!sequenceState) {
    return ""
  }
  const total = Math.max(0, Number(sequenceState.total_steps ?? 0))
  const current = Math.max(0, Number(sequenceState.current_step_index ?? 0))
  const step = total > 0 ? Math.min(total, current + 1) : 0
  const name = activeSequence.value?.name || `#${sequenceState.sequence_id}`
  const stateLabel = sequenceState.status === SequenceStatusEnum.PENDING
    ? "queued"
    : sequenceState.status === SequenceStatusEnum.CANCELLING
      ? "cancelling"
      : "running"
  return `${name} · ${stateLabel}${total > 0 ? ` · ${step}/${total}` : ""}`
})

const signalDetailText = computed(() => {
  const job = activeSignalTestRun.value
  if (!job) {
    return ""
  }
  const total = Math.max(0, Number(job.progress_total ?? 0))
  const done = Math.max(0, Number(job.progress_done ?? 0))
  const message = String(job.message ?? "").trim()
  if (message) {
    return message
  }
  return total > 0 ? `${done}/${total}` : String(job.status)
})

const sequenceIndicatorClass = computed(() => {
  if (activeSequenceState.value?.status === SequenceStatusEnum.CANCELLING) {
    return "bg-amber-500"
  }
  return "bg-emerald-500"
})

const signalIndicatorClass = computed(() => {
  if (activeSignalTestRun.value?.status === "paused") {
    return "bg-amber-500"
  }
  return "bg-emerald-500"
})

const sequenceTooltipText = computed(() => {
  const suffix = sequenceDetailText.value ? `: ${sequenceDetailText.value}` : ""
  return `Open running sequence${suffix}`
})

const signalTooltipText = computed(() => {
  const suffix = signalDetailText.value ? `: ${signalDetailText.value}` : ""
  return `Open active test run${suffix}`
})

function navigateToSequence() {
  const state = activeSequenceState.value
  if (!state) {
    return
  }
  void router.push({
    name: "instructions.detail",
    params: { id: state.sequence_id },
  }).catch(() => {
    return
  })
}

function navigateToSignals() {
  if (!activeSignalTestRun.value) {
    return
  }
  void router.push({ name: "signals.home" }).catch(() => {
    return
  })
}
</script>
