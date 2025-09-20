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
        @export="$emit('export')"
        @delete="onDelete"
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
        :disabled="!sequence.steps.length || store.hasBlockingErrors(sequence)"
      >
        {{ st.status === "running" ? "Stop" : "Start" }}
      </UiButton>

      <UiButton
        type="secondary"
        size="sm"
        @click.stop="onReset"
        :disabled="!sequence.steps.length || store.hasBlockingErrors(sequence) || st.status === 'running' || st.status === 'idle'"
        >
        Reset
      </UiButton>

      <BadgeComponent :variant="statusVariant" class="text-xs">{{ statusLabel }}</BadgeComponent>
    </div>

    <!-- Progress bar -->
    <ProgressBar
      class="my-2"
      :value="store.getProgress(sequence)"
      :disabled="!sequence.steps.length"
    />

    <draggable
      v-model="props.sequence.steps"
      item-key="id"
      handle=".drag-handle"
      @end="onReorder"
      ghost-class="dragging"
    >
      <template #item="{ element, index }">
        <SequenceStep
          :index="index"
          @delete="store.deleteStep(sequence.id, element.id!)"
          :description="store.getStepDescription(props.sequence, index)"
          :completed="store.ensureState(props.sequence).completed[index]"
          :error="store.ensureState(props.sequence).lastError &&
                  store.ensureState(props.sequence).index === index"
        >
          <!-- Добавляем иконку для drag -->
          <template #prefix>
            <span class="drag-handle cursor-grab text-neutral-500">⋮⋮</span>
          </template>
        </SequenceStep>
      </template>
    </draggable>

    <!-- Global error -->
    <p v-if="st.lastError" class="mt-3 text-xs text-red-600 font-mono">
      ⚠️ Error at step {{ st.index }}: {{ st.lastError }}
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
import { type SequenceDef, type SequenceStepCreate, StepKind } from "@/types/sequences"
import BadgeComponent from "@/components/ui/BadgeComponent.vue"
import UiButton from "../ui/UiButton.vue"
import ProgressBar from "../ui/ProgressBar.vue"
import SequenceMenu from "./SequenceMenu.vue"
import SequenceStep from "./SequenceStep.vue"
import draggable from "vuedraggable"
import AddStepButton from "./AddStepButton.vue"

const props = defineProps<{ sequence: SequenceDef }>()
const store = useSequenceStore()

const st = computed(() => store.ensureState(props.sequence))

const statusLabel = computed(() => {
  switch (st.value.status) {
    case "idle": return "Idle"
    case "running": return "Running"
    case "completed": return "Completed"
    case "stopped": return "Stopped"
  }
})

async function onStartStop() {
  if (st.value.status === "running") {
    store.stop(props.sequence)
  } else {
    // сбрасываем перед запуском
    store.resetState(props.sequence)
    await store.start(props.sequence)
  }
}

async function onReset() {

  store.resetState(props.sequence)
}

function onReorder() {
  const newOrder = props.sequence.steps
  .map(s => s.id)
  .filter((id): id is number => id !== undefined)

  store.reorderSteps(props.sequence.id, newOrder)

  store.resetState(props.sequence)
}

function onDelete() {
  store.deleteSequence(props.sequence.id)
}

// доступные типы шагов (потом можно вынести в конфиг)
const availableKinds: StepKind[] = Object.values(StepKind)

async function addStep(kind: StepKind) {
  const newStep: SequenceStepCreate = {
    kind,
    unit_id: null,
    payload: {}
  }
  await store.addStep(props.sequence.id, newStep)
}

async function addDefault(kind: StepKind) {

  const newStep: SequenceStepCreate = {
    kind: StepKind.WAIT,
    unit_id: null,
    payload: { ms: 500 },
  }
  await store.addStep(props.sequence.id, newStep)
}

const statusVariant = computed(() => {
  switch (st.value.status) {
    case "idle": return "neutral"
    case "running": return "info"
    case "completed": return "success"
    case "stopped": return "danger"
    default: return "neutral"
  }
})

</script>
