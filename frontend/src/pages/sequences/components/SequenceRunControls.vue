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
  <section class="mt-4 rounded-2xl border border-neutral-200 bg-white/80 p-5 shadow-sm dark:border-neutral-800 dark:bg-neutral-900/80">
    <div class="flex flex-wrap items-center gap-3">
      <template v-if="!isReadOnly">
        <UiButton
          size="sm"
          :variant="runButtonVariant"
          :disabled="runButtonDisabled"
          class="w-full justify-center gap-2 sm:w-auto sm:min-w-[150px]"
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
        class="flex items-center rounded border border-dashed border-neutral-300 px-3 py-2 text-sm text-neutral-600 dark:border-neutral-700 dark:text-neutral-300"
      >
        {{ runDescription }}
      </div>
      <UiBadge :variant="statusVariant" class="inline-flex justify-center sm:min-w-[110px]">
        {{ status }}
      </UiBadge>
      <span class="text-xs text-neutral-500 dark:text-neutral-400">
        {{ progressText }}
      </span>
    </div>
    <p v-if="state.last_error" class="mt-3 text-xs text-red-600 dark:text-red-300">
      Error: {{ state.last_error }}
    </p>
  </section>
</template>

<style scoped>
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
</style>
