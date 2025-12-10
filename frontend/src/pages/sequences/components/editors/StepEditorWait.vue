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
  <div class="space-y-3">
    <div>
      <label class="text-xs font-semibold text-neutral-500 dark:text-neutral-400">
        Delay, ms
      </label>
      <input
        type="number"
        min="0"
        class="mt-1 w-32 rounded border border-neutral-300 px-2 py-1 text-sm
               dark:border-neutral-700 dark:bg-neutral-800"
        :value="delayMs"
        :disabled="disabled"
        @change="handleDelayInput"
      />
    </div>
    <p class="text-xs text-neutral-500 dark:text-neutral-400">
      Sequence pauses for the specified duration before moving to the next step.
    </p>
  </div>
</template>
