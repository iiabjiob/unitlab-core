<template>
  <div
    class="group flex items-center justify-between px-2 py-1.5 text-sm select-none
           transition-colors
           hover:bg-neutral-100 dark:hover:bg-neutral-800"
  >
    <!-- LEFT -->
    <div class="flex items-center gap-3">

      <!-- Status dot -->
      <div class="w-2 h-2 rounded-full"
           :class="statusClass" />

      <!-- Drag handle -->
      <div class="text-neutral-500 dark:text-neutral-400 cursor-grab">⋮⋮</div>

      <!-- Index -->
      <div class="w-6 text-neutral-400 dark:text-neutral-500">
        {{ step.order_index }}
      </div>

      <!-- Description -->
      <div class="text-xs text-neutral-700 dark:text-neutral-300">
        {{ description }}
      </div>
    </div>

    <!-- RIGHT ACTIONS (appear only on hover) -->
    <div
      class="flex items-center gap-2 opacity-0 group-hover:opacity-100 transition-opacity"
    >
      <button
        @click.stop="emit('edit', step)"
        class="text-neutral-400 dark:text-neutral-500 hover:text-blue-500 dark:hover:text-blue-400"
      >✎</button>

      <button
        @click.stop="emit('delete', step.id)"
        class="text-neutral-400 dark:text-neutral-500 hover:text-red-500 dark:hover:text-red-400"
      >🗑</button>
    </div>
  </div>
</template>



<script setup lang="ts">
import { computed } from "vue"
import type { SequenceStep } from "@/types/sequences"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useSequenceStepStore } from "@/stores/sequenceStepStore"

const props = defineProps<{
  step: SequenceStep
}>()

const emit = defineEmits(["edit", "delete"])

const seqStore = useSequenceStore()
const stepStore = useSequenceStepStore()

const description = computed(() =>
  stepStore.getStepDescription(props.step)
)

const statusClass = computed(() => {
  const st = seqStore.states[props.step.sequence_id]
  if (!st) return "bg-neutral-600"

  if (st.last_error && st.current_step_index === props.step.order_index)
    return "bg-red-500"

  if (st.completed_step_ids.includes(props.step.id))
    return "bg-green-500"

  if (st.current_step_index === props.step.order_index)
    return "bg-blue-400"

  return "bg-neutral-600"
})
</script>
