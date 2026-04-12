<script setup lang="ts">
import { computed } from "vue"
import SequencePickerCombobox from "@/components/ui/SequencePickerCombobox.vue"
import UiAffinoListbox from "@/components/ui/UiAffinoListbox.vue"
import type { SequenceRepeatMode, SequenceStep } from "@/types/sequences"
import type { StepEditorChange } from "./editorTypes"

const props = defineProps<{
  step: SequenceStep
  disabled?: boolean
}>()

const emit = defineEmits<{
  (e: "update", payload: StepEditorChange): void
}>()

const repeatMode = computed<SequenceRepeatMode>(() => {
  const raw = String(props.step.payload?.repeat_mode ?? "times").trim()
  if (raw === "duration" || raw === "until_stopped") {
    return raw
  }
  return "times"
})

const iterations = computed(() => {
  const raw = props.step.payload?.iterations
  if (raw === null || raw === undefined || String(raw).trim() === "") {
    return 1
  }
  return Number.isFinite(Number(raw)) ? Math.max(1, Number(raw)) : 1
})

const durationMs = computed(() => {
  const raw = props.step.payload?.duration_ms
  if (raw === null || raw === undefined || String(raw).trim() === "") {
    return 1000
  }
  return Number.isFinite(Number(raw)) ? Math.max(1, Number(raw)) : 1000
})

const repeatModeOptions = computed(() => [
  { value: "times", label: "Fixed count" },
  { value: "duration", label: "For duration" },
  { value: "until_stopped", label: "Until stopped" },
])

function normalizeTargetSequenceId(value: string | number | null) {
  if (value === null || value === undefined || String(value).trim() === "") {
    return null
  }
  const numeric = Number(value)
  return Number.isFinite(numeric) && numeric > 0 ? numeric : null
}

const targetSequenceId = computed(() => normalizeTargetSequenceId(props.step.payload?.target_sequence_id ?? null))

function buildPayload(patch: Partial<Record<string, number | string | null>>) {
  const mode = (patch.repeat_mode as SequenceRepeatMode | undefined) ?? repeatMode.value
  const nextTargetId = Object.prototype.hasOwnProperty.call(patch, "target_sequence_id")
    ? patch.target_sequence_id
    : targetSequenceId.value
  const normalizedTargetId = normalizeTargetSequenceId(nextTargetId ?? null)

  if (mode === "duration") {
    return {
      target_sequence_id: normalizedTargetId,
      repeat_mode: "duration",
      duration_ms: Object.prototype.hasOwnProperty.call(patch, "duration_ms")
        ? Math.max(1, Number(patch.duration_ms ?? 1))
        : durationMs.value,
    }
  }

  if (mode === "until_stopped") {
    return {
      target_sequence_id: normalizedTargetId,
      repeat_mode: "until_stopped",
    }
  }

  return {
    target_sequence_id: normalizedTargetId,
    repeat_mode: "times",
    iterations: Object.prototype.hasOwnProperty.call(patch, "iterations")
      ? Math.max(1, Number(patch.iterations ?? 1))
      : iterations.value,
  }
}

function updateTargetSequence(value: string | number | null) {
  emit("update", {
    payload: buildPayload({ target_sequence_id: value }),
    replacePayload: true,
  })
}

function updateMode(value: string | number | null) {
  const nextMode: SequenceRepeatMode = value === "duration" || value === "until_stopped" ? value : "times"
  emit("update", {
    payload: buildPayload({ repeat_mode: nextMode }),
    replacePayload: true,
  })
}

function updateIterations(event: Event) {
  const target = event.target as HTMLInputElement | null
  emit("update", {
    payload: buildPayload({ iterations: target?.valueAsNumber ?? Number(target?.value ?? 1) }),
    replacePayload: true,
  })
}

function updateDuration(event: Event) {
  const target = event.target as HTMLInputElement | null
  emit("update", {
    payload: buildPayload({ duration_ms: target?.valueAsNumber ?? Number(target?.value ?? 1000) }),
    replacePayload: true,
  })
}
</script>

<template>
  <div class="space-y-4">
    <div>
      <label class="text-xs font-semibold text-neutral-500 dark:text-neutral-400" for="sequence-step-repeat-sequence">
        Target instruction
      </label>
      <SequencePickerCombobox
        id="sequence-step-repeat-sequence"
        name="sequence-step-repeat-sequence"
        class="mt-1"
        :model-value="targetSequenceId"
        :excluded-ids="[props.step.sequence_id]"
        :disabled="disabled"
        @update:modelValue="updateTargetSequence"
      />
    </div>

    <div>
      <label class="text-xs font-semibold text-neutral-500 dark:text-neutral-400" for="sequence-step-repeat-mode">
        Repeat mode
      </label>
      <UiAffinoListbox
        id="sequence-step-repeat-mode"
        name="sequence-step-repeat-mode"
        class="mt-1"
        :model-value="repeatMode"
        :options="repeatModeOptions"
        :disabled="disabled"
        aria-label="Repeat mode"
        @update:modelValue="updateMode"
      />
    </div>

    <div v-if="repeatMode === 'times'">
      <label class="text-xs font-semibold text-neutral-500 dark:text-neutral-400" for="sequence-step-repeat-iterations">
        Iterations
      </label>
      <input
        id="sequence-step-repeat-iterations"
        name="sequence-step-repeat-iterations"
        type="number"
        min="1"
        class="mt-1 w-32 rounded border border-neutral-300 px-2 py-1 text-sm dark:border-neutral-700 dark:bg-neutral-800"
        :value="iterations"
        :disabled="disabled"
        @change="updateIterations"
      />
    </div>

    <div v-else-if="repeatMode === 'duration'">
      <label class="text-xs font-semibold text-neutral-500 dark:text-neutral-400" for="sequence-step-repeat-duration">
        Duration, ms
      </label>
      <input
        id="sequence-step-repeat-duration"
        name="sequence-step-repeat-duration"
        type="number"
        min="1"
        class="mt-1 w-40 rounded border border-neutral-300 px-2 py-1 text-sm dark:border-neutral-700 dark:bg-neutral-800"
        :value="durationMs"
        :disabled="disabled"
        @change="updateDuration"
      />
    </div>

    <p class="text-xs text-neutral-500 dark:text-neutral-400">
      Runs the target instruction in a loop for a fixed count, a time window, or until the operator stops the run.
    </p>
  </div>
</template>
