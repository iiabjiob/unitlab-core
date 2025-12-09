<script setup lang="ts">
import { computed, ref, watch } from "vue"
import { useSequenceStepStore } from "@/stores/sequenceStepStore"
import SequenceStepAddToolbar from "./SequenceStepAddToolbar.vue"
import SequenceStepItem from "./SequenceStepItem.vue"
import DraggableList from "@/components/ui/DraggableList.vue"
import type { SequenceDef, SequenceStep } from "@/types/sequences"

const props = defineProps<{ sequence: SequenceDef }>()

const stepStore = useSequenceStepStore()

type EnrichedStep = SequenceStep & { description: string }

const steps = computed<EnrichedStep[]>(() =>
  stepStore.enrichedStepsBySequence(props.sequence.id).value
)

const draggableSteps = ref<EnrichedStep[]>([])

watch(
  steps,
  (next) => {
    draggableSteps.value = [...next]
  },
  { immediate: true }
)

function selectStep(stepId: number) {
  stepStore.activeStepId = stepId
}

function itemKey(step: EnrichedStep) {
  return String(step.id)
}

async function handleReorder(nextItems: EnrichedStep[]) {
  draggableSteps.value = nextItems
  const newOrder = nextItems.map((item) => item.id)

  try {
    await stepStore.reorderSteps(props.sequence.id, newOrder)
  } catch (error) {
    console.error("Failed to reorder steps", error)
    draggableSteps.value = [...steps.value]
  }
}
</script>

<template>
  <div class="flex flex-col h-full overflow-hidden p-4">

    <!-- HEADER -->
    <div class="text-xs uppercase tracking-wide text-neutral-500 dark:text-neutral-400">
      Steps ({{ steps.length }})
    </div>

    <!-- TOOLBAR -->
    <div class="py-2">
      <SequenceStepAddToolbar
        @add="stepStore.addStep(sequence.id, $event)"
      />
    </div>

    <!-- LIST -->
    <div class="flex-1 overflow-y-auto mt-5">
      <DraggableList
        :items="draggableSteps"
        :item-key="itemKey"
        wrapper-tag="div"
        item-tag="div"
        class="divide-y divide-neutral-300 dark:divide-neutral-700"
        :style="{ gap: '0' }"
        @update:items="handleReorder"
      >
        <template #default="{ item }">
          <SequenceStepItem
            :step="item"
            @click="selectStep(item.id)"
          />
        </template>
      </DraggableList>
    </div>

  </div>
</template>
