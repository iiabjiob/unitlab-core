<template>
  <UiMenu ref="menuRef">
    <div
      class="group flex items-center justify-between px-2 py-1.5 text-sm select-none transition-colors"
      :class="[
        active
          ? 'bg-neutral-50 text-blue-900 dark:bg-neutral-900/30 dark:text-blue-100'
          : 'hover:bg-neutral-100 dark:hover:bg-neutral-800'
      ]"
      @click="emit('select', step.id)"
      @contextmenu="openContextMenu"
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

      <!-- RIGHT ACTIONS (appear only on hover) -->
      <div
        class="flex items-center gap-2 transition-opacity"
        :class="active ? 'opacity-100' : 'opacity-0 group-hover:opacity-100'"
      >
        <UiButton
          size="xs"
          variant="ghost"
          @click.stop="emit('delete', step.id)"
        >
          <TrashIcon class="opacity-50 hover:opacity-100" size="12" />
        </UiButton>
      </div>
    </div>
    <UiMenuContent>
      <UiMenuItem class="text-neutral-900 dark:text-neutral-200" @select="emit('duplicate', step.id)">
        Duplicate
      </UiMenuItem>
      <UiMenuItem danger @select="emit('delete', step.id)">
        Delete
      </UiMenuItem>
    </UiMenuContent>
  </UiMenu>
</template>

<script setup lang="ts">
import { computed, ref } from "vue"
import type { SequenceStep } from "@/types/sequences"
import { useSequenceStore } from "@/stores/sequenceStore"
import { useSequenceStepStore } from "@/stores/sequenceStepStore"
import TrashIcon from "@/components/icons/TrashIcon.vue";
import UiButton from "@/components/ui/UiButton.vue";
import { UiMenu, UiMenuContent, UiMenuItem, type MenuController } from "@affino/menu-vue"

const props = defineProps<{
  step: SequenceStep
  active?: boolean
}>()

const emit = defineEmits<{
  (e: "delete", stepId: number): void
  (e: "duplicate", stepId: number): void
  (e: "select", stepId: number): void
}>()

const seqStore = useSequenceStore()
const stepStore = useSequenceStepStore()
const menuRef = ref<{ controller?: MenuController } | null>(null)

const description = computed(() =>
  stepStore.getStepDescription(props.step)
)

function openContextMenu(event: MouseEvent) {
  event.preventDefault()
  event.stopPropagation()
  const controller = menuRef.value?.controller
  if (!controller) return
  controller.setAnchor({ x: event.clientX, y: event.clientY, width: 0, height: 0 })
  controller.open("pointer")
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
