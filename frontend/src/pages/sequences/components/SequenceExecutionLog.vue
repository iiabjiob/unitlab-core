<script setup lang="ts">
import { computed } from "vue"
import ExecutionLogPanel from "@/components/ui/ExecutionLogPanel.vue"
import { useSequenceLogStore } from "@/stores/sequenceLogStore"
import { useSequenceStepStore } from "@/stores/sequenceStepStore"
import type { SequenceDef, SequenceState } from "@/types/sequences"
import type { SequenceLogEntry } from "@/stores/sequenceLogStore"

const props = defineProps<{
  sequence: SequenceDef
  state: SequenceState
}>()

const logStore = useSequenceLogStore()
const stepStore = useSequenceStepStore()

const logs = computed<SequenceLogEntry[]>(() => logStore.logs[props.sequence.id] ?? [])
const selectedIndex = computed<number | null>(() => {
  const activeId = stepStore.activeStepId
  if (!activeId) return null
  const activeStep = stepStore.steps.find((step) => step.id === activeId)

  const idx = logs.value.findIndex((log) => {
    if (log.step_id === activeId) return true
    if (activeStep && typeof log.step_index === "number") {
      return log.step_index === activeStep.order_index
    }
    return false
  })

  return idx === -1 ? null : idx
})

function resolveStepIdFromLog(log: SequenceLogEntry): number | null {
  if (log.step_id) return log.step_id
  if (typeof log.step_index === "number") {
    const step = stepStore.steps.find(
      (item) => item.sequence_id === props.sequence.id && item.order_index === log.step_index,
    )
    if (step) return step.id
  }
  return null
}

function handleSelect(payload: { index: number; log: SequenceLogEntry | Record<string, any> }) {
  const stepId = resolveStepIdFromLog(payload.log as SequenceLogEntry)
  if (!stepId) return
  if (stepStore.activeStepId === stepId) return
  stepStore.setActiveStep(stepId)
}
</script>

<template>
  <ExecutionLogPanel
    :logs="logs"
    selectable
    :selected-index="selectedIndex"
    @select="handleSelect"
  />
</template>
