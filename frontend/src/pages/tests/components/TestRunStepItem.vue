<script setup lang="ts">
import { computed } from "vue"
import type { TestRunState, TestRunStep } from "@/types/testRuns"
import { useTestRunStepStore } from "@/stores/testRunStepStore"
import UiButton from "@/components/ui/UiButton.vue"
import TrashIcon from "@/components/icons/TrashIcon.vue"

const props = defineProps<{ step: TestRunStep; active?: boolean; state?: TestRunState | null }>()
const emit = defineEmits<{ (e: "select", id: number): void; (e: "delete", id: number): void }>()

const stepStore = useTestRunStepStore()

const description = computed(() => stepStore.describeStep(props.step))

const statusClass = computed(() => {
  const st = props.state
  if (!st) return "bg-neutral-600"
  if (st.status === "failed" && st.current_step_index === props.step.order_index) {
    return "bg-red-500"
  }
  if (st.current_step_index > props.step.order_index) {
    return "bg-green-500"
  }
  if (st.current_step_index === props.step.order_index) {
    return "bg-blue-400"
  }
  return "bg-neutral-600"
})
</script>

<template>
  <div
    class="group flex items-center justify-between px-2 py-1.5 text-sm select-none transition-colors"
    :class="[
      active
        ? 'bg-neutral-50 text-blue-900 dark:bg-neutral-900/30 dark:text-blue-100'
        : 'hover:bg-neutral-100 dark:hover:bg-neutral-800'
    ]"
    @click="emit('select', step.id)"
  >
    <div class="flex items-center gap-3">
      <div class="w-2 h-2 rounded-full" :class="statusClass" />
      <div class="text-neutral-500 dark:text-neutral-400 cursor-grab" data-drag-handle role="button" tabindex="-1" aria-label="Drag step">
        ⋮⋮
      </div>
      <div class="w-6 text-neutral-500">{{ step.order_index + 1 }}</div>
      <div class="text-xs text-neutral-700 dark:text-neutral-300">
        {{ description }}
      </div>
    </div>
    <div class="flex items-center gap-2 transition-opacity" :class="active ? 'opacity-100' : 'opacity-0 group-hover:opacity-100'">
      <UiButton size="xs" variant="ghost" @click.stop="emit('delete', step.id)">
        <TrashIcon class="opacity-50 hover:opacity-100" size="12" />
      </UiButton>
    </div>
  </div>
</template>
