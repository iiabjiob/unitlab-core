<script setup lang="ts">
import { computed } from "vue"
import {
  SequenceStatusEnum,
  type SequenceDef,
  type SequenceState,
} from "@/types/sequences"

const props = defineProps<{
  sequence: SequenceDef
  state: SequenceState
}>()

const emit = defineEmits(["start", "stop"])

// СТРОГИЕ, ЧИСТЫЕ ИКОНКИ (моноширинные)
const ICONS = {
  [SequenceStatusEnum.IDLE]:      "○",
  [SequenceStatusEnum.RUNNING]:   "●",
  [SequenceStatusEnum.ERROR]:     "▲",
  [SequenceStatusEnum.STOPPED]:   "■",
  [SequenceStatusEnum.COMPLETED]: "✓",
}

// Цвет статуса
const statusColor = computed(() => {
  switch (props.state.status) {
    case SequenceStatusEnum.RUNNING:   return "text-blue-400"
    case SequenceStatusEnum.ERROR:     return "text-red-400"
    case SequenceStatusEnum.STOPPED:   return "text-yellow-400"
    case SequenceStatusEnum.COMPLETED: return "text-green-400"
    default:                           return "text-neutral-500"
  }
})

// Лейбл статуса
const statusLabel = computed(() => {
  switch (props.state.status) {
    case SequenceStatusEnum.RUNNING:   return "Running"
    case SequenceStatusEnum.ERROR:     return "Error"
    case SequenceStatusEnum.STOPPED:   return "Stopped"
    case SequenceStatusEnum.COMPLETED: return "Completed"
    default:                           return "Idle"
  }
})

// Прогресс
const progressPercent = computed(() => {
  const st = props.state
  if (st.total_steps === 0) return 0
  return Math.round((st.completed_step_ids.length / st.total_steps) * 100)
})

const canStart = computed(() =>
  [
    SequenceStatusEnum.IDLE,
    SequenceStatusEnum.COMPLETED,
    SequenceStatusEnum.ERROR,
    SequenceStatusEnum.STOPPED,
  ].includes(props.state.status)
)

const canStop = computed(() => props.state.status === SequenceStatusEnum.RUNNING)
</script>

<template>
  <div class="py-5 flex flex-col gap-2">

    <!-- ROW 1: LEFT — Start, Status, Step -->
    

      <!-- LEFT -->
      <div class="flex items-center gap-4">

        <!-- Start / Stop always first -->
        <button
          v-if="canStart"
          @click="emit('start')"
          class="px-3 py-1 rounded bg-green-600 hover:bg-green-500
                 text-white text-sm font-medium"
        >
          ▶ Start
        </button>

        <button
          v-if="canStop"
          @click="emit('stop')"
          class="px-3 py-1 rounded bg-red-600 hover:bg-red-500
                 text-white text-sm font-medium"
        >
          ■ Stop
        </button>

        <!-- STATUS -->
        <div :class="['flex items-center gap-1 text-sm font-medium', statusColor]">
          <span class="font-mono">{{ ICONS[state.status] }}</span>
          <span>{{ statusLabel }}</span>
        </div>

        <!-- STEPS -->
        <div class="text-xs text-neutral-500 dark:text-neutral-400">
          Step {{ state.current_step_index }} / {{ state.total_steps }}
        </div>
        <!-- RIGHT → small progress text -->
        <div class="text-xs text-neutral-500 dark:text-neutral-400">
          ({{ progressPercent }}%)
        </div>
      </div>

    

    <!-- ROW 2: THIN PROGRESS BAR -->
    <!-- <div class="h-1 bg-neutral-800 dark:bg-neutral-700 rounded overflow-hidden">
      <div
        class="h-full bg-blue-500 transition-all duration-200"
        :style="{ width: progressPercent + '%' }"
      ></div>
    </div> -->

  </div>
</template>
