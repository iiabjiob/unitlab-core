<script setup lang="ts">
import { computed, ref, watch } from "vue"
import UiButton from "@/components/ui/UiButton.vue"
import UiAlert from "@/components/ui/UiAlert.vue"
import { useSequenceStepStore } from "@/stores/sequenceStepStore"
import { useSequenceStore } from "@/stores/sequenceStore"
import type { SequenceDef, SequenceStep } from "@/types/sequences"
import { SequenceStepType } from "@/types/sequences"
import type { Component } from "vue"

import StepEditorWait from "./editors/StepEditorWait.vue"
import StepEditorLatch from "./editors/StepEditorLatch.vue"
import StepEditorPulse from "./editors/StepEditorPulse.vue"
import StepEditorPair from "./editors/StepEditorPair.vue"
import StepEditorMask from "./editors/StepEditorMask.vue"
import StepEditorAO from "./editors/StepEditorAO.vue"
import StepEditorCallSequence from "./editors/StepEditorCallSequence.vue"
import StepEditorRepeatSequence from "./editors/StepEditorRepeatSequence.vue"
import type { StepEditorChange } from "./editors/editorTypes"

const props = defineProps<{
  sequence: SequenceDef | null
  step: SequenceStep | null
}>()

const emit = defineEmits<{
  (e: "close"): void
}>()

const stepStore = useSequenceStepStore()
const sequenceStore = useSequenceStore()

const saving = ref(false)
const error = ref<string | null>(null)

const componentMap: Record<SequenceStepType, Component> = {
  [SequenceStepType.WAIT]: StepEditorWait,
  [SequenceStepType.DO_LATCH]: StepEditorLatch,
  [SequenceStepType.DO_PULSE]: StepEditorPulse,
  [SequenceStepType.DO_PAIR]: StepEditorPair,
  [SequenceStepType.DO_BITMASK]: StepEditorMask,
  [SequenceStepType.AO_SET]: StepEditorAO,
  [SequenceStepType.CALL_SEQUENCE]: StepEditorCallSequence,
  [SequenceStepType.REPEAT_SEQUENCE]: StepEditorRepeatSequence,
}

const typeLabels: Record<SequenceStepType, string> = {
  [SequenceStepType.WAIT]: "Wait",
  [SequenceStepType.DO_LATCH]: "DO · Latch",
  [SequenceStepType.DO_PULSE]: "DO · Pulse",
  [SequenceStepType.DO_PAIR]: "Switch position",
  [SequenceStepType.DO_BITMASK]: "Group control",
  [SequenceStepType.AO_SET]: "AO · Set",
  [SequenceStepType.CALL_SEQUENCE]: "Call instruction",
  [SequenceStepType.REPEAT_SEQUENCE]: "Repeat instruction",
}

const editorComponent = computed(() => {
  if (!props.step) return null
  return componentMap[props.step.sequence_step_type]
})

const headerLabel = computed(() => {
  if (!props.step) return "Step editor"
  return `${typeLabels[props.step.sequence_step_type]} · #${props.step.order_index + 1}`
})

const description = computed(() => (props.step ? stepStore.getStepDescription(props.step) : ""))

watch(
  () => props.step?.id,
  () => {
    error.value = null
  },
)

async function handleUpdate(patch: StepEditorChange) {
  if (!props.sequence || !props.step) {
    return
  }

  const body: Record<string, unknown> = {}

  if (Object.prototype.hasOwnProperty.call(patch, "channel_id")) {
    body.channel_id = patch.channel_id ?? null
  }

  if (patchHasPayload(patch)) {
    const base = props.step.payload ?? {}
    const nextPayload = patch.replacePayload
      ? patch.payload ?? null
      : { ...base, ...(patch.payload ?? {}) }
    body.payload = nextPayload
  }

  if (Object.keys(body).length === 0) {
    return
  }

  saving.value = true
  error.value = null
  try {
    await stepStore.updateStep(props.sequence.id, props.step.id, body)
    sequenceStore.resetState(props.sequence.id)
  } catch (err) {
    error.value = err instanceof Error ? err.message : "Failed to update step"
  } finally {
    saving.value = false
  }
}

function patchHasPayload(patch: StepEditorChange): boolean {
  return Object.prototype.hasOwnProperty.call(patch, "payload")
}

function exitEditMode() {
  emit("close")
}
</script>

<template>
    <div v-if="step && editorComponent" class="sequence-step-editor">
      <div class="sequence-step-editor__header">
        <div>
          <div class="sequence-step-editor__eyebrow">
            Editing step
          </div>
          <div class="sequence-step-editor__title">
            {{ headerLabel }}
          </div>
          <div class="sequence-step-editor__description">
            {{ description }}
          </div>
        </div>
        <div class="sequence-step-editor__actions">
          <UiButton
            size="xs"
            variant="ghost"
            @click="exitEditMode"
          >
            x
          </UiButton>
        </div>
      </div>

      <component
        :is="editorComponent"
        :key="step.id"
        :step="step"
        @update="handleUpdate"
      />

      <UiAlert v-if="error" type="error" :message="error" />
    </div>
</template>

<style scoped>
.sequence-step-editor {
  display: flex;
  flex-direction: column;
  gap: 1rem;
  overflow-y: auto;
  padding: 1.25rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-md);
  background: var(--color-neutral-50);
}

.sequence-step-editor__header {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 1rem;
}

.sequence-step-editor__eyebrow {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  letter-spacing: 0.025em;
  text-transform: uppercase;
}

.sequence-step-editor__title {
  color: var(--color-neutral-900);
  font-size: var(--text-lg);
  font-weight: 600;
}

.sequence-step-editor__description {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
}

.sequence-step-editor__actions {
  display: flex;
  gap: 0.5rem;
}

:global(.dark .sequence-step-editor) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-900);
}

:global(.dark .sequence-step-editor__eyebrow),
:global(.dark .sequence-step-editor__description) {
  color: var(--color-neutral-400);
}

:global(.dark .sequence-step-editor__title) {
  color: var(--color-white);
}
</style>
