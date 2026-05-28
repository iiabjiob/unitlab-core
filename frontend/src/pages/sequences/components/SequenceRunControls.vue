<script setup lang="ts">
import { computed, ref } from "vue"
import UiButton from "@/components/ui/UiButton.vue"
import UiBadge from "@/components/ui/UiBadge.vue"
import { toUserFacingErrorMessage } from "@/api/errorMessages"
import type { SequenceDef, SequenceState } from "@/types/sequences"
import { SequenceStatusEnum } from "@/types/sequences"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useToastStore } from "@/stores/toastStore"

const props = defineProps<{
  sequence: SequenceDef
  state: SequenceState
}>()

const sequenceStore = useSequenceStore()
const toastStore = useToastStore()
const actionLoading = ref<"start" | "stop" | null>(null)

const state = computed(() => props.state)
const status = computed(() => state.value.status)

const statusVariant = computed(() => {
  switch (status.value) {
    case SequenceStatusEnum.RUNNING:
      return "success"
    case SequenceStatusEnum.ERROR:
      return "danger"
    case SequenceStatusEnum.CANCELLING:
      return "warning"
    case SequenceStatusEnum.COMPLETED:
      return "info"
    default:
      return "neutral"
  }
})

const progress = computed(() => sequenceStore.getProgress(props.sequence))
const executionProgress = computed(() => sequenceStore.getExecutionProgress(props.sequence))
const totalStepCount = computed(() => state.value.total_steps || 0)
const stepLabel = computed(() => {
  const total = state.value.total_steps ?? 0
  if (total === 0) return 0
  if (status.value === SequenceStatusEnum.COMPLETED) {
    return total
  }
  return Math.min(total, Math.max(1, Math.floor(executionProgress.value.done) + 1))
})

const canStart = computed(() => (
  status.value === SequenceStatusEnum.IDLE ||
  status.value === SequenceStatusEnum.STOPPED ||
  status.value === SequenceStatusEnum.COMPLETED ||
  status.value === SequenceStatusEnum.ERROR
))

const canStop = computed(() => (
  status.value === SequenceStatusEnum.RUNNING ||
  status.value === SequenceStatusEnum.PENDING ||
  status.value === SequenceStatusEnum.CANCELLING
))

const isReadOnly = computed(() => props.sequence.read_only)
const runDescription = computed(() => (
  isReadOnly.value
    ? "Direct run controls are disabled for read-only instructions. Duplicate it to run or edit."
    : "Quick execution that drives channels directly."
))

function formatElapsed(ms?: number | null) {
  if (!Number.isFinite(ms ?? NaN) || !ms || ms < 0) {
    return null
  }

  const totalSeconds = Math.floor(ms / 1000)
  const hours = Math.floor(totalSeconds / 3600)
  const minutes = Math.floor((totalSeconds % 3600) / 60)
  const seconds = totalSeconds % 60

  return [hours, minutes, seconds]
    .map((value) => String(value).padStart(2, "0"))
    .join(":")
}

const runtimeStepText = computed(() => {
  const runtime = state.value.runtime
  if (!runtime?.active_sequence_name) {
    return null
  }

  if (
    typeof runtime.active_step_index === "number"
    && typeof runtime.active_total_steps === "number"
  ) {
    return `${runtime.active_sequence_name} · step ${runtime.active_step_index + 1}/${runtime.active_total_steps}`
  }

  return runtime.active_sequence_name
})

const runtimePathText = computed(() => {
  const path = state.value.runtime?.execution_path?.filter(Boolean) ?? []
  if (path.length <= 1) {
    return null
  }
  return path.join(" -> ")
})

const runtimeIterationText = computed(() => {
  const runtime = state.value.runtime
  if (!runtime?.repeat_mode || typeof runtime.iteration_current !== "number") {
    return null
  }

  if (runtime.repeat_mode === "times") {
    if (typeof runtime.iteration_total === "number") {
      return `Iteration ${runtime.iteration_current} / ${runtime.iteration_total}`
    }
    return `Iteration ${runtime.iteration_current}`
  }

  if (runtime.repeat_mode === "duration") {
    return `Iteration ${runtime.iteration_current} · timed run`
  }

  return `Iteration ${runtime.iteration_current} · until stopped`
})

const runtimeElapsedText = computed(() => {
  const formatted = formatElapsed(state.value.runtime?.run_elapsed_ms)
  return formatted ? `Elapsed ${formatted}` : null
})

const runtimeItems = computed(() => [
  runtimeStepText.value,
  runtimePathText.value ? `Path: ${runtimePathText.value}` : null,
  runtimeIterationText.value,
  runtimeElapsedText.value,
].filter((item): item is string => Boolean(item)))

const isInFlight = computed(() => (
  status.value === SequenceStatusEnum.PENDING ||
  status.value === SequenceStatusEnum.RUNNING ||
  status.value === SequenceStatusEnum.CANCELLING
))

const showStatusBadge = computed(() => (
  isInFlight.value ||
  status.value === SequenceStatusEnum.ERROR ||
  status.value === SequenceStatusEnum.STOPPED
))

const showProgressPercent = computed(() => (
  isInFlight.value ||
  status.value === SequenceStatusEnum.ERROR ||
  status.value === SequenceStatusEnum.STOPPED
))

const runHeadline = computed(() => {
  if (isReadOnly.value) return "Read-only instruction"
  switch (status.value) {
    case SequenceStatusEnum.PENDING:
      return "Preparing run"
    case SequenceStatusEnum.RUNNING:
      return "Running instruction"
    case SequenceStatusEnum.CANCELLING:
      return "Stopping safely"
    case SequenceStatusEnum.ERROR:
      return "Run needs attention"
    case SequenceStatusEnum.STOPPED:
      return "Run stopped"
    default:
      return "Ready"
  }
})

const runSummaryText = computed(() => {
  if (isReadOnly.value) {
    return runDescription.value
  }

  const total = totalStepCount.value
  if (status.value === SequenceStatusEnum.RUNNING) {
    return total > 0
      ? `Step ${stepLabel.value}/${total} · ${progress.value}%`
      : "Running without configured steps"
  }

  if (status.value === SequenceStatusEnum.PENDING) {
    return "Queued for execution"
  }

  if (status.value === SequenceStatusEnum.CANCELLING) {
    return total > 0
      ? `Stop requested · step ${stepLabel.value}/${total}`
      : "Stop requested"
  }

  if (status.value === SequenceStatusEnum.ERROR) {
    return state.value.last_error ? "Check execution log for failure details" : "Last run failed"
  }

  if (status.value === SequenceStatusEnum.STOPPED) {
    return total > 0
      ? `Stopped at step ${stepLabel.value}/${total}`
      : "Stopped by operator"
  }

  if (status.value === SequenceStatusEnum.COMPLETED) {
    return "Last run finished. Ready for next run."
  }

  return "No active run"
})

const runtimeFallbackText = computed(() => {
  if (isInFlight.value) {
    return "Waiting for live runtime status"
  }
  if (status.value === SequenceStatusEnum.ERROR) {
    return "Failure details are kept in the execution log"
  }
  if (status.value === SequenceStatusEnum.STOPPED) {
    return "Run state is preserved for review"
  }
  return ""
})

async function startInstruction() {
  actionLoading.value = "start"
  try {
    await sequenceStore.startSequence(props.sequence.id)
    toastStore.success("Instruction started")
  } catch (err) {
    toastStore.error(toUserFacingErrorMessage(err, "Failed to start instruction"))
  } finally {
    actionLoading.value = null
  }
}

async function stopInstruction() {
  actionLoading.value = "stop"
  try {
    await sequenceStore.stopSequence(props.sequence.id)
    toastStore.success("Stop requested")
  } catch (err) {
    toastStore.error(toUserFacingErrorMessage(err, "Failed to stop instruction"))
  } finally {
    actionLoading.value = null
  }
}

const isRunning = computed(() => (
  status.value === SequenceStatusEnum.RUNNING ||
  status.value === SequenceStatusEnum.PENDING
))

const runButtonVariant = computed(() => (
  isRunning.value ? "danger" : "success"
))

const runButtonDisabled = computed(() => (
  isRunning.value
    ? (!canStop.value || !!actionLoading.value)
    : (!canStart.value || !!actionLoading.value)
))

async function toggleRun() {
  if (isRunning.value) {
    await stopInstruction()
  } else {
    await startInstruction()
  }
}
</script>

<template>
  <section class="sequence-run-controls">
    <div class="sequence-run-controls__top">
      <div class="sequence-run-controls__action">
        <UiButton
          v-if="!isReadOnly"
          size="sm"
          :variant="runButtonVariant"
          :disabled="runButtonDisabled"
          class="sequence-run-controls__run-button"
          @click="toggleRun"
        >
          <template v-if="isRunning">
            <span class="run-icon run-icon--stop" aria-hidden="true"></span>
            <span v-if="actionLoading === 'stop'">Stopping…</span>
            <span v-else>Stop</span>
          </template>
          <template v-else>
            <span class="run-icon run-icon--play" aria-hidden="true"></span>
            <span v-if="actionLoading === 'start'">Starting…</span>
            <span v-else>Run Instruction</span>
          </template>
        </UiButton>
        <div
          v-else
          class="sequence-run-controls__read-only"
        >
          Locked
        </div>
      </div>

      <div class="sequence-run-controls__summary">
        <div class="sequence-run-controls__summary-head">
          <span class="sequence-run-controls__headline">{{ runHeadline }}</span>
          <UiBadge
            v-if="showStatusBadge"
            :variant="statusVariant"
            class="sequence-run-controls__status"
          >
            {{ status }}
          </UiBadge>
        </div>
        <div class="sequence-run-controls__summary-text">
          {{ runSummaryText }}
        </div>
      </div>

      <div
        class="sequence-run-controls__percent"
        :class="{ 'sequence-run-controls__percent--hidden': !showProgressPercent }"
      >
        {{ progress }}%
      </div>
    </div>

    <div
      class="sequence-run-controls__runtime"
      :class="{ 'sequence-run-controls__runtime--muted': runtimeItems.length === 0 }"
    >
      <template v-if="runtimeItems.length > 0">
        <span
          v-for="item in runtimeItems"
          :key="item"
          class="sequence-run-controls__runtime-chip"
        >
          {{ item }}
        </span>
      </template>
      <span v-else-if="runtimeFallbackText" class="sequence-run-controls__runtime-placeholder">
        {{ runtimeFallbackText }}
      </span>
    </div>

    <p v-if="state.last_error" class="sequence-run-controls__error">
      Error: {{ state.last_error }}
    </p>
  </section>
</template>

<style scoped>
.sequence-run-controls {
  margin-top: 1rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: 1rem;
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
  box-shadow: var(--shadow-sm);
  display: flex;
  flex: 0 0 auto;
  flex-direction: column;
  gap: 0.75rem;
  min-height: 6.75rem;
  padding: 1rem;
}

.sequence-run-controls__top {
  display: grid;
  grid-template-columns: 1fr;
  align-items: center;
  gap: 0.75rem;
}

.sequence-run-controls__action {
  min-width: 0;
}

.sequence-run-controls__run-button {
  width: 100%;
  justify-content: center;
  gap: 0.5rem;
}

.sequence-run-controls__read-only {
  display: flex;
  align-items: center;
  justify-content: center;
  min-height: 2rem;
  padding: 0.5rem 0.75rem;
  border: 1px dashed var(--color-neutral-300);
  border-radius: var(--radius-sm);
  color: var(--color-neutral-600);
  font-size: var(--text-sm);
  font-weight: 600;
}

.sequence-run-controls__summary {
  min-width: 0;
}

.sequence-run-controls__summary-head {
  align-items: center;
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
}

.sequence-run-controls__headline {
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  font-weight: 700;
}

.sequence-run-controls__summary-text {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  margin-top: 0.125rem;
}

.sequence-run-controls__status {
  display: inline-flex;
  justify-content: center;
}

.sequence-run-controls__percent {
  color: var(--color-neutral-900);
  font-family: var(--font-mono);
  font-size: var(--text-lg);
  font-weight: 700;
  justify-self: start;
  line-height: 1;
}

.sequence-run-controls__percent--hidden {
  opacity: 0;
}

.sequence-run-controls__runtime {
  align-items: center;
  color: var(--color-neutral-500);
  display: flex;
  flex-wrap: wrap;
  font-size: var(--text-xs);
  gap: 0.375rem;
  min-height: 1.5rem;
}

.sequence-run-controls__runtime--muted {
  color: color-mix(in srgb, var(--color-neutral-500) 68%, transparent);
}

.sequence-run-controls__runtime-chip {
  border: 1px solid color-mix(in srgb, var(--color-neutral-300) 70%, transparent);
  border-radius: 999px;
  background: color-mix(in srgb, var(--color-white) 72%, transparent);
  color: var(--color-neutral-600);
  font-size: 11px;
  line-height: 1;
  padding: 0.25rem 0.5rem;
}

.sequence-run-controls__runtime-placeholder {
  line-height: 1.25rem;
}

.sequence-run-controls__error {
  color: var(--color-red-500);
  font-size: var(--text-xs);
}

.run-icon {
  display: inline-block;
  width: 0;
  height: 0;
  margin-right: 0.35rem;
}

.run-icon--play {
  border-top: 6px solid transparent;
  border-bottom: 6px solid transparent;
  border-left: 10px solid currentColor;
}

.run-icon--stop {
  width: 10px;
  height: 10px;
  background-color: currentColor;
  border-radius: 1px;
}

@media (min-width: 640px) {
  .sequence-run-controls__top {
    grid-template-columns: auto minmax(0, 1fr) auto;
  }

  .sequence-run-controls__run-button {
    width: auto;
    min-width: 150px;
  }

  .sequence-run-controls__status {
    min-width: 110px;
  }

  .sequence-run-controls__percent {
    justify-self: end;
  }
}

:global(.dark .sequence-run-controls) {
  border-color: var(--color-neutral-800);
  background: color-mix(in srgb, var(--color-neutral-900) 80%, transparent);
}

:global(.dark .sequence-run-controls__read-only) {
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-300);
}

:global(.dark .sequence-run-controls__headline),
:global(.dark .sequence-run-controls__percent) {
  color: var(--color-neutral-100);
}

:global(.dark .sequence-run-controls__summary-text),
:global(.dark .sequence-run-controls__runtime) {
  color: var(--color-neutral-400);
}

:global(.dark .sequence-run-controls__runtime-chip) {
  border-color: color-mix(in srgb, var(--color-neutral-700) 72%, transparent);
  background: color-mix(in srgb, var(--color-neutral-900) 76%, transparent);
  color: var(--color-neutral-300);
}

:global(.dark .sequence-run-controls__error) {
  color: var(--color-red-300);
}
</style>
