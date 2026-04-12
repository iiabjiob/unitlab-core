<script setup lang="ts">
import { computed } from "vue"
import SequencePickerCombobox from "@/components/ui/SequencePickerCombobox.vue"
import type { SequenceStep } from "@/types/sequences"
import type { StepEditorChange } from "./editorTypes"

const props = defineProps<{
  step: SequenceStep
  disabled?: boolean
}>()

const emit = defineEmits<{
  (e: "update", payload: StepEditorChange): void
}>()

function normalizeTargetSequenceId(value: string | number | null) {
  if (value === null || value === undefined || String(value).trim() === "") {
    return null
  }
  const numeric = Number(value)
  return Number.isFinite(numeric) && numeric > 0 ? numeric : null
}

const targetSequenceId = computed(() => normalizeTargetSequenceId(props.step.payload?.target_sequence_id ?? null))

function updateTargetSequence(value: string | number | null) {
  const nextId = normalizeTargetSequenceId(value)
  emit("update", {
    payload: {
      target_sequence_id: nextId,
    },
  })
}
</script>

<template>
  <div class="space-y-4">
    <div>
      <label class="text-xs font-semibold text-neutral-500 dark:text-neutral-400" for="sequence-step-call-sequence">
        Target instruction
      </label>
      <SequencePickerCombobox
        id="sequence-step-call-sequence"
        name="sequence-step-call-sequence"
        class="mt-1"
        :model-value="targetSequenceId"
        :excluded-ids="[props.step.sequence_id]"
        :disabled="disabled"
        @update:modelValue="updateTargetSequence"
      />
    </div>

    <p class="text-xs text-neutral-500 dark:text-neutral-400">
      Reuses another instruction as a child block and executes it once in place.
    </p>
  </div>
</template>
