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
const stepLabel = computed(() => {
  const total = state.value.total_steps ?? 0
  if (total === 0) return 0
  if (status.value === SequenceStatusEnum.COMPLETED) {
    return total
  }
  return Math.min(state.value.current_step_index + 1, total)
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

const progressText = computed(() => {
  const total = state.value.total_steps || 0
  return `Progress: ${progress.value}% · step ${stepLabel.value} of ${total}`
})

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
    <div class="sequence-run-controls__main">
      <template v-if="!isReadOnly">
        <UiButton
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
            <span v-else>Run</span>
          </template>
        </UiButton>
      </template>
      <div
        v-else
        class="sequence-run-controls__read-only"
      >
        {{ runDescription }}
      </div>
      <UiBadge :variant="statusVariant" class="sequence-run-controls__status">
        {{ status }}
      </UiBadge>
      <span class="sequence-run-controls__progress">
        {{ progressText }}
      </span>
    </div>
    <div
      v-if="runtimeStepText || runtimePathText || runtimeIterationText || runtimeElapsedText"
      class="sequence-run-controls__runtime"
    >
      <span v-if="runtimeStepText">{{ runtimeStepText }}</span>
      <span v-if="runtimePathText">Path: {{ runtimePathText }}</span>
      <span v-if="runtimeIterationText">{{ runtimeIterationText }}</span>
      <span v-if="runtimeElapsedText">{{ runtimeElapsedText }}</span>
    </div>
    <p v-if="state.last_error" class="sequence-run-controls__error">
      Error: {{ state.last_error }}
    </p>
  </section>
</template>

<style scoped>
.sequence-run-controls {
  margin-top: 1rem;
  padding: 1.25rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: 1rem;
  background: color-mix(in srgb, var(--color-white) 80%, transparent);
  box-shadow: var(--shadow-sm);
}

.sequence-run-controls__main {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.75rem;
}

.sequence-run-controls__run-button {
  width: 100%;
  justify-content: center;
  gap: 0.5rem;
}

.sequence-run-controls__read-only {
  display: flex;
  align-items: center;
  padding: 0.5rem 0.75rem;
  border: 1px dashed var(--color-neutral-300);
  border-radius: var(--radius-sm);
  color: var(--color-neutral-600);
  font-size: var(--text-sm);
}

.sequence-run-controls__status {
  display: inline-flex;
  justify-content: center;
}

.sequence-run-controls__progress,
.sequence-run-controls__runtime {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.sequence-run-controls__runtime {
  display: flex;
  flex-wrap: wrap;
  column-gap: 1rem;
  row-gap: 0.25rem;
  margin-top: 0.75rem;
}

.sequence-run-controls__error {
  margin-top: 0.75rem;
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
  .sequence-run-controls__run-button {
    width: auto;
    min-width: 150px;
  }

  .sequence-run-controls__status {
    min-width: 110px;
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

:global(.dark .sequence-run-controls__progress),
:global(.dark .sequence-run-controls__runtime) {
  color: var(--color-neutral-400);
}

:global(.dark .sequence-run-controls__error) {
  color: var(--color-red-300);
}
</style>
