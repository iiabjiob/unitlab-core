<script setup lang="ts">
import UiButton from "@/components/ui/UiButton.vue"
import { SequenceStepType } from "@/types/sequences"
import type { SequenceStepCreate } from "@/types/sequences"

const emit = defineEmits<{
  (e: "add", payload: SequenceStepCreate): void
}>()

interface BtnDef {
  type: SequenceStepType
  label: string
}

const buttons: BtnDef[] = [
  { type: SequenceStepType.WAIT,       label: "Wait" },
  { type: SequenceStepType.DO_LATCH,   label: "Latch" },
  { type: SequenceStepType.DO_PULSE,   label: "Pulse" },
  { type: SequenceStepType.DO_PAIR,    label: "Pair" },
  { type: SequenceStepType.DO_BITMASK, label: "Mask" },
  { type: SequenceStepType.AO_SET,     label: "AO" },
]

function add(type: SequenceStepType) {
  emit("add", { sequence_step_type: type })
}
</script>

<template>
  <div class="flex items-center gap-3 px-1 py-2">

    <UiButton
      v-for="btn in buttons"
      :key="btn.type"
      size="xs"
      variant="secondary"
      @click="add(btn.type)"
    >
      {{ btn.label }}
    </UiButton>

  </div>
</template>
