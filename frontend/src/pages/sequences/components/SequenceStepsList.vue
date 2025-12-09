<script setup lang="ts">
import { computed } from "vue"
import { useSequenceStepStore } from "@/stores/sequenceStepStore"
import SequenceStepAddToolbar from "./SequenceStepAddToolbar.vue"
import SequenceStepItem from "./SequenceStepItem.vue"
import type { SequenceDef } from "@/types/sequences"

const props = defineProps<{ sequence: SequenceDef }>()

const stepStore = useSequenceStepStore()

const steps = computed(() =>
  stepStore.enrichedStepsBySequence(props.sequence.id).value
)

function selectStep(stepId: number) {
  stepStore.activeStepId = stepId
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
    <div class="flex-1 overflow-y-auto divide-y divide-neutral-300 dark:divide-neutral-700 mt-5">
      <SequenceStepItem
        v-for="step in steps"
        :key="step.id"
        :step="step"
        :active="stepStore.activeStepId === step.id"
        @select="selectStep"
      />
    </div>

  </div>
</template>
