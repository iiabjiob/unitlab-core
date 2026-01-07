<script setup lang="ts">
import { computed, ref, watch } from "vue"
import type { TestRunSummary, TestRunStep } from "@/types/testRuns"
import { useTestRunStepStore } from "@/stores/testRunStepStore"
import { useTestRunStore } from "@/stores/testRunStore"
import DraggableList from "@/components/ui/DraggableList.vue"
import UiButton from "@/components/ui/UiButton.vue"
import TestRunStepItem from "./TestRunStepItem.vue"

const props = defineProps<{ run: TestRunSummary }>()
const stepStore = useTestRunStepStore()
const runStore = useTestRunStore()

const steps = computed(() => stepStore.stepsByRun(props.run.id).value)
const state = computed(() => runStore.states[props.run.id])
const draggableSteps = ref<TestRunStep[]>([])

watch(
  steps,
  (next) => {
    draggableSteps.value = [...next]
    if (!next.length) {
      stepStore.setActiveStep(null)
      return
    }
    if (!stepStore.activeStepId || !next.some(step => step.id === stepStore.activeStepId)) {
      stepStore.setActiveStep(next[0].id)
    }
  },
  { immediate: true },
)

function itemKey(step: TestRunStep) {
  return String(step.id)
}

function selectStep(stepId: number) {
  stepStore.setActiveStep(stepId)
}

async function handleReorder(nextItems: TestRunStep[]) {
  draggableSteps.value = nextItems
  const newOrder = nextItems.map(item => item.id)
  try {
    await stepStore.reorderSteps(props.run.id, newOrder)
    runStore.resetState(props.run.id)
  } catch (error) {
    console.error("Failed to reorder test steps", error)
    draggableSteps.value = [...steps.value]
  }
}

async function handleDelete(stepId: number) {
  try {
    await stepStore.deleteStep(props.run.id, stepId)
    runStore.resetState(props.run.id)
    if (stepStore.activeStepId === stepId) {
      const remaining = steps.value.filter(step => step.id !== stepId)
      stepStore.setActiveStep(remaining[0]?.id ?? null)
    }
  } catch (error) {
    console.error("Failed to delete test step", error)
  }
}
</script>

<template>
  <div class="flex flex-col h-full overflow-hidden p-4">
    <div class="text-xs uppercase tracking-wide text-neutral-500 dark:text-neutral-400">
      Steps ({{ steps.length }})
    </div>

    <div class="py-2">
      <UiButton size="xs" variant="secondary" full disabled>
        Channel picker coming soon
      </UiButton>
    </div>

    <div class="flex-1 overflow-y-auto mt-4">
      <DraggableList
        :items="draggableSteps"
        :item-key="itemKey"
        wrapper-tag="div"
        item-tag="div"
        handle-only
        class="divide-y divide-neutral-300 dark:divide-neutral-700"
        :style="{ gap: '0' }"
        @update:items="handleReorder"
      >
        <template #default="{ item }">
          <TestRunStepItem
            :step="item"
            :state="state"
            :active="stepStore.activeStepId === item.id"
            @select="selectStep"
            @delete="handleDelete"
          />
        </template>
      </DraggableList>
      <div v-if="steps.length === 0" class="mt-6 rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400">
        Use the upcoming channel picker to add steps here.
      </div>
    </div>
  </div>
</template>
