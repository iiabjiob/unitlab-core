<script setup lang="ts">
import { computed } from "vue"
import type { SequenceStep } from "@/types/sequences"
import type { StepEditorChange } from "./editorTypes"

const props = defineProps<{
  step: SequenceStep
  disabled?: boolean
}>()

const emit = defineEmits<{
  (e: "update", payload: StepEditorChange): void
}>()

const delayMs = computed(() => Number(props.step.payload?.ms ?? 0))

function updateDelay(newVal: number | null) {
  const safe = Number.isFinite(newVal ?? NaN) ? Math.max(0, Number(newVal)) : 0
  emit("update", { payload: { ms: safe } })
}

function handleDelayInput(event: Event) {
  const target = event.target as HTMLInputElement | null
  updateDelay(target?.valueAsNumber ?? Number(target?.value ?? 0))
}
</script>

<template>
  <div class="sequence-step-form sequence-step-form--compact">
    <div>
      <label for="sequence-step-wait-ms" class="sequence-step-form__label">
        Delay, ms
      </label>
      <input
        type="number"
        autocomplete="off"
        id="sequence-step-wait-ms"
        name="sequence-step-wait-ms"
        min="0"
        class="sequence-step-form__input sequence-step-form__control--sm"
        :value="delayMs"
        :disabled="disabled"
        @change="handleDelayInput"
      />
    </div>
    <p class="sequence-step-form__hint">
      Instruction pauses for the specified duration before moving to the next step.
    </p>
  </div>
</template>
