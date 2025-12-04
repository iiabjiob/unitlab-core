<template>
  <li
    class="flex flex-col h-full"
  >
    <!-- Header -->
    <div class="flex items-center justify-between">
      <div class="flex items-center gap-2 flex-wrap">
        <span class="font-mono font-semibold">
          {{ sequence.name }}
        </span>
      </div>

      <!-- меню действий -->
      <SequenceMenu
        @export="$emit('export', props.sequence)"
        @delete="$emit('delete', props.sequence)"
      />
    </div>

    <!-- Описание -->
    <p class="mt-1 text-xs text-neutral-500">
      {{ sequence.description }}
    </p>

    <div class="flex gap-3 items-center py-3">
      <UiButton
        size="sm"
        type="secondary"
        @click.stop="onStartStop"
        :disabled="!steps.length"
      >
        {{ st.status === SequenceStatusEnum.RUNNING ? "Stop" : "Start" }}
      </UiButton>

      <UiButton
        type="secondary"
        size="sm"
        @click.stop="onReset"
        :disabled="!steps.length || st.status === 'running' || st.status === 'idle'"
      >
        Reset
      </UiButton>

      <UiBadge :variant="statusVariant" class="text-xs">{{ statusLabel }}</UiBadge>
    </div>

    <!-- Progress bar -->
    <ProgressBar
      class="my-2"
      :value="seqStore.getProgress(sequence)"
      :disabled="!steps.length"
    />

    <draggable
      :list="enrichedSteps"
      item-key="id"
      handle=".drag-handle"
      @end="onReorder"
      ghost-class="dragging"
    >
      <template #item="{ element, index }">
        <SequenceStep
          :index="index"
          @delete="seqStepStore.deleteStep(sequence.id, element.id!)"
          :description="element.description"
          :completed="seqStore.ensureState(sequence).completed[index]"
          :error="seqStore.ensureState(sequence).lastError &&
                  seqStore.ensureState(sequence).index === index"
        >
          <template #prefix>
            <span class="drag-handle cursor-grab text-neutral-500">⋮⋮</span>
          </template>
        </SequenceStep>
      </template>
    </draggable>

    <!-- Global error -->
    <p v-if="st.lastError" class="mt-3 text-xs text-red-600 font-mono">
      ⚠️ Error at step {{ st.index+1 }}: {{ st.lastError }}
    </p>
  </li>

  <!-- Add step control -->
  <div class="mt-2 relative overflow-visible">
    <AddStepButton
      :kinds="availableKinds"
      @add="addDefault"
    />
  </div>

</template>

<script setup lang="ts">
import { computed } from "vue"
import { useSequenceStore } from "@/stores/sequenceStore"
import { type SequenceDef, SequenceStatusEnum, type SequenceStepCreate, StepKind } from "@/types/sequences"
import UiBadge from "@/components/ui/UiBadge.vue"
import UiButton from "../ui/UiButton.vue"
import ProgressBar from "../ui/ProgressBar.vue"
import SequenceMenu from "./SequenceMenu.vue"
import SequenceStep from "./SequenceStep.vue"
import draggable from "vuedraggable"
import AddStepButton from "./AddStepButton.vue"
import { useSequenceStepStore } from "@/stores/sequenceStepStore"

const props = defineProps<{ sequence: SequenceDef }>()

const seqStore = useSequenceStore()
const seqStepStore = useSequenceStepStore()

const emit = defineEmits<{
  (e: "export", seq: SequenceDef): void
  (e: "delete", seq: SequenceDef): void
}>()

const st = computed(() => seqStore.ensureState(props.sequence))

const steps = computed(() => seqStepStore.stepsBySequence(props.sequence.id).value)
const enrichedSteps = computed(() => seqStepStore.enrichedStepsBySequence(props.sequence.id).value)


const statusLabel = computed(() => {
  switch (st.value.status) {
    case SequenceStatusEnum.IDLE: return "Idle"
    case SequenceStatusEnum.RUNNING: return "Running"
    case SequenceStatusEnum.COMPLETED: return "Completed"
    case SequenceStatusEnum.STOPPED: return "Stopped"
  }
})

async function onStartStop() {
  if (st.value.status === SequenceStatusEnum.RUNNING) {
    seqStore.stop(props.sequence)
  } else {
    // сбрасываем перед запуском
    seqStore.resetState(props.sequence)
    await seqStore.start(props.sequence)
  }
}

async function onReset() {

  seqStore.resetState(props.sequence)
}

async function onReorder() {
  const newOrder: number[] = enrichedSteps.value
    .map((s) => s.id)
    .filter((id): id is number => id !== undefined)

  await seqStepStore.reorderSteps(props.sequence.id, newOrder)
  seqStore.resetState(props.sequence)
}


// доступные типы шагов (потом можно вынести в конфиг)
const availableKinds: StepKind[] = Object.values(StepKind)

async function addDefault(kind: StepKind) {
  const newStep: SequenceStepCreate = {
    kind: StepKind.WAIT,
    channel_id: null,
    payload: { ms: 500 },
  }
  await seqStepStore.addStep(props.sequence.id, newStep)
}

const statusVariant = computed(() => {
  switch (st.value.status) {
    case SequenceStatusEnum.IDLE: return "neutral"
    case SequenceStatusEnum.RUNNING: return "info"
    case SequenceStatusEnum.COMPLETED: return "success"
    case SequenceStatusEnum.STOPPED: return "danger"
    default: return "neutral"
  }
})

</script>
