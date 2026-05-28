<template>
  <div v-if="isVisible" class="global-run-status-link">
    <GlobalProgressStatusCard
      v-if="sequenceChipVisible"
      :compact="compact"
      :interactive="!sequenceControlsVisible"
      label="Sequence"
      :percent="sequenceProgressPercent"
      :detail="sequenceDetailText"
      :dot-class="sequenceIndicatorClass"
      :bar-class="sequenceBarClass"
      @click="navigateToSequence"
    >
      <template #actions>
        <UiButton
          v-if="!compact && canPauseSequence"
          size="xs"
          variant="ghost"
          :disabled="sequenceControlBusy"
          title="Pause instruction"
          @click.stop="controlSequence('pause')"
        >
          ⏸
        </UiButton>
        <UiButton
          v-if="!compact && canStopSequence"
          size="xs"
          variant="ghost"
          :disabled="sequenceControlBusy"
          title="Stop instruction"
          @click.stop="controlSequence('stop')"
        >
          ■
        </UiButton>
      </template>
    </GlobalProgressStatusCard>

    <GlobalProgressStatusCard
      v-if="signalChipVisible"
      :compact="compact"
      interactive
      label="Test run"
      :percent="signalProgressPercent"
      :detail="signalDetailText"
      :dot-class="signalIndicatorClass"
      :bar-class="signalBarClass"
      @click="navigateToSignals"
    />
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from "vue"
import { storeToRefs } from "pinia"
import { useRouter } from "vue-router"

import UiButton from "@/components/ui/UiButton.vue"
import { useSignalJobStore } from "@/stores/signalJobStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useToastStore } from "@/stores/toastStore"
import { SequenceStatusEnum, type SequenceState } from "@/types/sequences"
import GlobalProgressStatusCard from "./GlobalProgressStatusCard.vue"

const props = withDefaults(defineProps<{ compact?: boolean; showSignalChip?: boolean }>(), {
  compact: false,
  showSignalChip: true,
})

const router = useRouter()
const signalJobStore = useSignalJobStore()
const sequenceStore = useSequenceStore()
const toastStore = useToastStore()

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
const signalChipVisible = computed(() => Boolean(props.showSignalChip && activeSignalTestRun.value))
const isVisible = computed(() => sequenceChipVisible.value || signalChipVisible.value)
const sequenceControlBusy = ref(false)

const canPauseSequence = computed(() => activeSequenceState.value?.status === SequenceStatusEnum.RUNNING)
const canStopSequence = computed(() => {
  const status = activeSequenceState.value?.status
  return status === SequenceStatusEnum.PENDING
    || status === SequenceStatusEnum.RUNNING
    || status === SequenceStatusEnum.CANCELLING
})

const sequenceControlsVisible = computed(() => !compact.value && (canPauseSequence.value || canStopSequence.value))

const sequenceProgress = computed(() => {
  const sequenceState = activeSequenceState.value
  if (!sequenceState) {
    return { done: 0, total: 0, percent: 0, completedTopSteps: 0 }
  }
  return sequenceStore.getExecutionProgress(sequenceState.sequence_id)
})

const sequenceProgressPercent = computed(() => {
  return sequenceProgress.value.percent
})

const sequenceDetailText = computed(() => {
  const sequenceState = activeSequenceState.value
  if (!sequenceState) {
    return ""
  }
  const { done, total } = sequenceProgress.value
  const name = activeSequence.value?.name || `#${sequenceState.sequence_id}`
  const stateLabel = sequenceState.status === SequenceStatusEnum.PENDING
    ? "queued"
    : sequenceState.status === SequenceStatusEnum.CANCELLING
      ? "cancelling"
      : "running"

  const currentStep = total > 0
    ? Math.min(total, Math.max(1, Math.floor(done) + 1))
    : 0
  const chunks = [
    name,
    stateLabel,
    total > 0 ? `step ${currentStep}/${total}` : "",
    sequenceRuntimeText.value,
    sequenceEstText.value,
  ].filter(Boolean)

  return chunks.join(" · ")
})

const sequenceEstText = computed(() => {
  const state = activeSequenceState.value
  if (!state) {
    return ""
  }
  const { done, total } = sequenceProgress.value
  if (total <= 0 || done <= 0 || done >= total) {
    return ""
  }

  const startedAtMs = toMillis(String(state.started_at ?? ""))
  if (!startedAtMs) {
    return ""
  }

  const elapsedSeconds = Math.max(1, Math.round((Date.now() - startedAtMs) / 1000))
  const rate = done / elapsedSeconds
  if (!Number.isFinite(rate) || rate <= 0) {
    return ""
  }

  const remaining = Math.max(0, total - done)
  if (remaining <= 0) {
    return ""
  }

  const estimatedSeconds = Math.max(1, Math.round(remaining / rate))
  return `EST ${formatDurationShort(estimatedSeconds)}`
})

const sequenceRuntimeText = computed(() => {
  const runtime = activeSequenceState.value?.runtime
  if (!runtime) {
    return ""
  }

  const chunks: string[] = []
  const path = runtime.execution_path?.filter(Boolean) ?? []
  if (path.length > 1 && runtime.active_sequence_name) {
    chunks.push(runtime.active_sequence_name)
  }

  if (
    typeof runtime.active_step_index === "number"
    && typeof runtime.active_total_steps === "number"
    && path.length > 1
  ) {
    chunks.push(`nested ${runtime.active_step_index + 1}/${runtime.active_total_steps}`)
  }

  if (runtime.repeat_mode === "times" && typeof runtime.iteration_current === "number") {
    chunks.push(typeof runtime.iteration_total === "number"
      ? `iter ${runtime.iteration_current}/${runtime.iteration_total}`
      : `iter ${runtime.iteration_current}`)
  } else if (runtime.repeat_mode === "duration" && typeof runtime.iteration_current === "number") {
    chunks.push(`iter ${runtime.iteration_current}`)
  } else if (runtime.repeat_mode === "until_stopped" && typeof runtime.iteration_current === "number") {
    chunks.push(`iter ${runtime.iteration_current}`)
  }

  return chunks.join(" · ")
})

const signalProgress = computed(() => {
  const job = activeSignalTestRun.value
  if (!job) {
    return { done: 0, total: 0 }
  }

  let total = Math.max(0, Number(job.progress_total ?? 0))
  let done = Math.max(0, Number(job.progress_done ?? 0))

  const message = String(job.message ?? "")
  const ratioMatch = message.match(/(\d+)\s*\/\s*(\d+)/)
  if (ratioMatch) {
    const parsedDone = Number(ratioMatch[1] ?? 0)
    const parsedTotal = Number(ratioMatch[2] ?? 0)
    if (Number.isFinite(parsedDone) && parsedDone >= 0) {
      done = Math.max(done, parsedDone)
    }
    if (Number.isFinite(parsedTotal) && parsedTotal > 0) {
      total = Math.max(total, parsedTotal)
    }
  }

  if (total > 0 && done > total) {
    done = total
  }

  return { done, total }
})

const signalProgressPercent = computed(() => {
  const { done, total } = signalProgress.value
  if (!total) return 0
  return Math.max(0, Math.min(100, Math.round((done / total) * 100)))
})

const signalDetailText = computed(() => {
  const job = activeSignalTestRun.value
  if (!job) {
    return ""
  }
  const { done, total } = signalProgress.value
  const message = String(job.message ?? "").trim()
  if (message) {
    return message
  }
  return total > 0 ? `${done}/${total}` : String(job.status)
})

const sequenceIndicatorClass = computed(() => {
  if (activeSequenceState.value?.status === SequenceStatusEnum.CANCELLING) {
    return "global-progress-card__tone--warning"
  }
  return "global-progress-card__tone--success"
})

const sequenceBarClass = computed(() => {
  if (activeSequenceState.value?.status === SequenceStatusEnum.CANCELLING) {
    return "global-progress-card__tone--warning"
  }
  return "global-progress-card__tone--success"
})

const signalIndicatorClass = computed(() => {
  if (activeSignalTestRun.value?.status === "paused") {
    return "global-progress-card__tone--warning"
  }
  return "global-progress-card__tone--success"
})

const signalBarClass = computed(() => {
  if (activeSignalTestRun.value?.status === "paused") {
    return "global-progress-card__tone--warning"
  }
  return "global-progress-card__tone--success"
})

function formatDurationShort(seconds: number): string {
  const normalized = Math.max(0, Math.round(seconds))
  const minutes = Math.floor(normalized / 60)
  const remSeconds = normalized % 60
  if (minutes <= 0) {
    return `${remSeconds}s`
  }
  return `${minutes}m ${remSeconds}s`
}

function toMillis(value: string): number {
  const parsed = Date.parse(String(value ?? ""))
  return Number.isFinite(parsed) ? parsed : 0
}

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

async function controlSequence(action: "pause" | "stop") {
  const state = activeSequenceState.value
  if (!state || sequenceControlBusy.value) {
    return
  }

  sequenceControlBusy.value = true
  try {
    await sequenceStore.stopSequence(state.sequence_id)
  } catch (err) {
    const fallback = action === "pause" ? "Failed to pause instruction" : "Failed to stop instruction"
    toastStore.error(err instanceof Error ? err.message : fallback)
  } finally {
    sequenceControlBusy.value = false
  }
}
</script>

<style scoped>
.global-run-status-link {
  display: flex;
  align-items: center;
  gap: 0.375rem;
}
</style>
