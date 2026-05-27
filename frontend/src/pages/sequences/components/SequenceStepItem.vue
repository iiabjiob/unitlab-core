<template>
  <UiMenu>
    <UiMenuTrigger as-child trigger="contextmenu">
    <div
      class="sequence-step-item"
      :class="[
        selected
          ? 'sequence-step-item--selected'
          : active
            ? 'sequence-step-item--active'
            : 'sequence-step-item--idle'
      ]"
      @click="emitSelect"
    >
      <div class="sequence-step-item__main">

        <div class="sequence-step-item__status"
            :class="statusClass" />

        <div
          class="sequence-step-item__drag-handle"
          data-drag-handle
          role="button"
          tabindex="-1"
          aria-label="Drag step"
        >
          ⋮⋮
        </div>

        <div class="sequence-step-item__index">
          {{ step.order_index+1 }}
        </div>

        <div class="sequence-step-item__description">
          {{ description }}
        </div>
      </div>

    </div>
    </UiMenuTrigger>
    <UiMenuContent>
      <UiMenuItem class="sequence-step-item__menu-item" @select="emit('copy', step.id)">
        Copy
      </UiMenuItem>
      <UiMenuItem class="sequence-step-item__menu-item" @select="emit('pasteAfter', step.id)">
        Paste below
      </UiMenuItem>
      <UiMenuItem class="sequence-step-item__menu-item" @select="emit('duplicate', step.id)">
        Duplicate
      </UiMenuItem>
      <UiMenuItem class="sequence-step-item__menu-item" @select="emit('selectAll')">
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
  if (!st) return "sequence-step-item__status--idle"

  if (st.last_error && st.current_step_index === props.step.order_index)
    return "sequence-step-item__status--error"

  if (st.completed_step_ids.includes(props.step.id))
    return "sequence-step-item__status--completed"

  if (st.current_step_index === props.step.order_index)
    return "sequence-step-item__status--running"

  return "sequence-step-item__status--idle"
})
</script>

<style scoped>
.sequence-step-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0.375rem 0.5rem;
  color: var(--color-neutral-700);
  font-size: var(--text-sm);
  outline: none;
  user-select: none;
  transition: background 150ms ease, color 150ms ease;
}

.sequence-step-item--idle:hover {
  background: var(--color-neutral-100);
}

.sequence-step-item--active {
  background: var(--color-neutral-50);
  color: var(--color-blue-900);
}

.sequence-step-item--selected {
  background: var(--color-blue-100);
  color: var(--color-blue-900);
}

.sequence-step-item__main {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  min-width: 0;
}

.sequence-step-item__status {
  width: 0.5rem;
  height: 0.5rem;
  flex-shrink: 0;
  border-radius: 999px;
}

.sequence-step-item__status--idle {
  background: var(--color-neutral-600);
}

.sequence-step-item__status--error {
  background: var(--color-red-500);
}

.sequence-step-item__status--completed {
  background: var(--color-green-500);
}

.sequence-step-item__status--running {
  background: var(--color-blue-400);
}

.sequence-step-item__drag-handle {
  color: var(--color-neutral-500);
  cursor: grab;
}

.sequence-step-item__index {
  width: 1.5rem;
  color: var(--color-neutral-500);
}

.sequence-step-item__description {
  min-width: 0;
  color: var(--color-neutral-700);
  font-size: var(--text-xs);
}

:global(.sequence-step-item__menu-item) {
  color: var(--color-neutral-900);
}

:global(.dark .sequence-step-item--idle:hover) {
  background: var(--color-neutral-800);
}

:global(.dark .sequence-step-item--active) {
  background: color-mix(in srgb, var(--color-neutral-900) 30%, transparent);
  color: var(--color-blue-100);
}

:global(.dark .sequence-step-item--selected) {
  background: color-mix(in srgb, var(--color-blue-500) 15%, transparent);
  color: var(--color-blue-100);
}

:global(.dark .sequence-step-item__drag-handle),
:global(.dark .sequence-step-item__index) {
  color: var(--color-neutral-400);
}

:global(.dark .sequence-step-item__description) {
  color: var(--color-neutral-300);
}

:global(.dark .sequence-step-item__menu-item) {
  color: var(--color-neutral-200);
}
</style>
