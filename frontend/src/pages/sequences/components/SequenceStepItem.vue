<template>
  <UiMenu>
    <UiMenuTrigger as-child trigger="contextmenu">
    <div
      class="group flex items-center justify-between px-2 py-1.5 text-sm select-none transition-colors"
      :class="[
        selected
          ? 'bg-blue-50 text-blue-900 dark:bg-blue-500/15 dark:text-blue-100'
          : active
            ? 'bg-neutral-50 text-blue-900 dark:bg-neutral-900/30 dark:text-blue-100'
            : 'hover:bg-neutral-100 dark:hover:bg-neutral-800'
      ]"
      @click="emitSelect"
    >
      <!-- LEFT -->
      <div class="flex items-center gap-3">

        <!-- Status dot -->
        <div class="w-2 h-2 rounded-full"
            :class="statusClass" />

        <!-- Drag handle -->
        <div
          class="text-neutral-500 dark:text-neutral-400 cursor-grab"
          data-drag-handle
          role="button"
          tabindex="-1"
          aria-label="Drag step"
        >
          ⋮⋮
        </div>

        <!-- Index -->
        <div class="w-6 text-neutral-500 ">
          {{ step.order_index+1 }}
        </div>

        <!-- Description -->
        <div class="text-xs text-neutral-700 dark:text-neutral-300">
          {{ description }}
        </div>
      </div>

    </div>
    </UiMenuTrigger>
    <UiMenuContent>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="emit('copy', step.id)">
        Copy
      </UiMenuItem>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="emit('pasteAfter', step.id)">
        Paste below
      </UiMenuItem>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="emit('duplicate', step.id)">
        Duplicate
      </UiMenuItem>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="emit('selectAll')">
        Select all
      </UiMenuItem>
      <UiMenuItem danger @select="emit('delete', step.id)">
        Delete
      </UiMenuItem>
    </UiMenuContent>
  </UiMenu>
</template>

<script setup lang="ts">
import { computed } from "vue"
import type { SequenceStep } from "@/types/sequences"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useSequenceStepStore } from "@/stores/sequenceStepStore"
import { UiMenu, UiMenuTrigger, UiMenuContent, UiMenuItem } from "@/components/ui/menu"

const props = defineProps<{
  step: SequenceStep
  active?: boolean
  selected?: boolean
}>()

const emit = defineEmits<{
  (e: "delete", stepId: number): void
  (e: "duplicate", stepId: number): void
  (e: "copy", stepId: number): void
  (e: "pasteAfter", stepId: number): void
  (e: "selectAll"): void
  (e: "select", payload: { stepId: number; shiftKey: boolean; ctrlKey: boolean; metaKey: boolean }): void
}>()

const seqStore = useSequenceStore()
const stepStore = useSequenceStepStore()

const description = computed(() =>
  stepStore.getStepDescription(props.step)
)

function emitSelect(event: MouseEvent) {
  emit("select", {
    stepId: props.step.id,
    shiftKey: event.shiftKey,
    ctrlKey: event.ctrlKey,
    metaKey: event.metaKey,
  })
}

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
