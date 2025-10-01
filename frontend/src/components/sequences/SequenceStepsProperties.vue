<!-- src/components/sequences/SequenceStepsProperties.vue -->
<template>
  <div class="mt-2">
    <h4 class="font-bold text-sm">Steps</h4>

    <PropertiesPanel
      v-for="st in steps"
      :key="st.id"
      :schema="sequenceStepPropertySchema"
      :item="st"
      :item-id="st.id"
      schema-name="sequence_step"
      @update="(key, value) => sequenceStepPropertySchema.update(st, key as keyof SequenceStep, value)"
    />
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { sequenceStepPropertySchema } from "@/property-schemas/sequenceStep.schema"
import PropertiesPanel from "../properties/PropertiesPanel.vue"
import type { SequenceDef, SequenceStep } from "@/types/sequences"
import { useSequenceStepStore } from "@/stores/sequenceStepStore"

const props = defineProps<{
  sequence: SequenceDef
}>()

const seqStepStore = useSequenceStepStore()

// получаем реактивный список шагов из стора
const steps = computed<SequenceStep[]>(() =>
  seqStepStore.stepsBySequence(props.sequence.id).value
)
</script>
