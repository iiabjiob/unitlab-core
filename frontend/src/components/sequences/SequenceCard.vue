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
        @delete="$emit('delete')"
      />
    </div>

    <!-- Описание -->
    <p class="mt-1 text-xs text-neutral-500">
      {{ sequence.description }}
    </p>

    <div class="flex gap-3 items-center py-3">
      <UiButton size="sm" type="primary" @click.stop="onStart" :disabled="store.isRunning(sequence)">Start</UiButton>
      <UiButton size="sm" type="secondary" @click.stop="onStop" :disabled="!store.isRunning(sequence)">Stop</UiButton>
      <UiButton size="sm" type="secondary" @click.stop="onReset" :disabled="store.isRunning(sequence)">Reset</UiButton>

      <BadgeComponent class="text-xs">{{ statusLabel }}</BadgeComponent>
    </div>

    <!-- Progress bar -->
    <ProgressBar
      class="mt-2"
      :value="store.getProgress(sequence)"
    />

    <!-- Steps checklist -->
    <ol class="mt-3 space-y-1 text-sm">
      <SequenceStep
        v-for="(s, i) in sequence.steps"
        :key="s.id ?? i"
        :index="i"
        :description="store.getStepDescription(sequence, i)"
        :completed="st.completed[i]"
        :error="st.lastError"
      />
    </ol>

    <!-- Error -->
    <p v-if="st.lastError" class="text-xs text-red-600 mt-2">
      Error: {{ st.lastError }}
    </p>
  </li>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { useSequenceStore } from "@/stores/sequenceStore"
import type { SequenceDef } from "@/types/sequences"
import BadgeComponent from "@/components/ui/BadgeComponent.vue"
import UiButton from "../ui/UiButton.vue"
import ProgressBar from "../ui/ProgressBar.vue"
import SequenceMenu from "./SequenceMenu.vue"
import SequenceStep from "./SequenceStep.vue"

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

async function onStart() {
  await store.start(props.sequence)
}
function onStop() {
  store.stop(props.sequence)
}
function onReset() {
  store.resetState(props.sequence)
}
function deleteSequence() {
  store.deleteSequence(props.sequence.id)
}
</script>
