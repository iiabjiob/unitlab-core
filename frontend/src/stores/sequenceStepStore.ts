// src/stores/sequenceStepStore.ts
import { defineStore } from "pinia"
import { ref, computed } from "vue"
import type { SequenceStep, SequenceStepCreate } from "@/types/sequences"
import { SequenceStepType } from "@/types/sequences"
import { useChannelStore } from "./channelStore"
import axios from "axios"
import { ApiBuilder } from "@/utils/api"
import { getLogger } from "@/utils/logger"

const logger = getLogger("SEQS")

export const useSequenceStepStore = defineStore("sequenceStepStore", () => {
  const steps = ref<SequenceStep[]>([])
  const channelStore = useChannelStore()

  const stepsBySequence = (seqId: number) =>
    computed(() => steps.value.filter((s) => s.sequence_id === seqId))

  const enrichedStepsBySequence = (seqId: number) =>
    computed(() =>
      stepsBySequence(seqId).value.map((step) => ({
        ...step,
        description: getStepDescription(step),
      })),
    )

  function setStepsForSequence(seqId: number, seqSteps: SequenceStep[]) {
    steps.value = steps.value
      .filter((s) => s.sequence_id !== seqId)
      .concat([...seqSteps].sort((a, b) => a.order_index - b.order_index))
  }

  function removeSequenceSteps(seqId: number) {
    steps.value = steps.value.filter((s) => s.sequence_id !== seqId)
  }

  function getStepDescription(step: SequenceStep): string {
    function resolve(id?: number | null): string {
      if (!id) return "n/a"
      const ch = channelStore.channels.find((c) => c.id === id)
      return ch ? channelStore.resolveChannelFullLabel(ch) : `CH#${id}`
    }

    switch (step.sequence_step_type) {
      case SequenceStepType.WAIT:
        return `Wait ${step.payload?.ms ?? 0} ms`
      case SequenceStepType.DO_LATCH:
        return `DO: latch ${resolve(step.channel_id)} = ${step.payload?.value ?? 0}`
      case SequenceStepType.DO_PULSE:
        return `DO: pulse ${resolve(step.channel_id)} = ${step.payload?.value ?? 0} for ${step.payload?.pulse_ms ?? 0} ms`
      case SequenceStepType.DO_PAIR: {
        const ids = step.payload?.channel_ids ?? []
        const [idA, idB] = ids
        return `DO: pair ${resolve(idA)} + ${resolve(idB)}, state=${step.payload?.state2b ?? 0}`
      }
      case SequenceStepType.DO_BITMASK:
        return `DO: bitmask 0x${(step.payload?.bitmask ?? 0).toString(16).toUpperCase()}`
      case SequenceStepType.AO_SET:
        return `AO: set ${resolve(step.channel_id)} = ${step.payload?.value ?? 0} mA`
      default:
        return `❓ Unknown step type: ${step.sequence_step_type}`
    }
  }

  async function fetchSteps(seqId: number) {
    const { data } = await axios.get<SequenceStep[]>(ApiBuilder.sequenceSteps(seqId))
    setStepsForSequence(seqId, data)
    logger.info(`📡 Loaded ${data.length} steps for sequence ${seqId}`)
  }

  async function addStep(seqId: number, step: SequenceStepCreate) {
    const payload = {
      ...step,
      sequence_step_type: step.sequence_step_type,
    }
    const { data } = await axios.post<SequenceStep>(ApiBuilder.sequenceSteps(seqId), payload)
    steps.value.push(data)
    steps.value.sort((a, b) => a.order_index - b.order_index)
    return data
  }

  async function updateStep(seqId: number, stepId: number, changes: Partial<SequenceStep>) {
    const { data } = await axios.patch<SequenceStep>(ApiBuilder.sequenceStep(seqId, stepId), changes)
    const idx = steps.value.findIndex((s) => s.id === stepId)
    if (idx !== -1) {
      steps.value[idx] = { ...steps.value[idx], ...data }
      steps.value.sort((a, b) => a.order_index - b.order_index)
    }
    return data
  }

  async function deleteStep(seqId: number, stepId: number) {
    await axios.delete(ApiBuilder.sequenceStep(seqId, stepId))
    steps.value = steps.value.filter((s) => s.id !== stepId)
    return true
  }

  async function reorderSteps(seqId: number, newOrder: number[]) {
    const { data } = await axios.post<SequenceStep[]>(ApiBuilder.sequenceStepsReorder(seqId), { new_order: newOrder })
    setStepsForSequence(seqId, data)
    return data
  }

  async function replaceSteps(seqId: number, newSteps: SequenceStepCreate[]) {
    const { data } = await axios.put<SequenceStep[]>(ApiBuilder.sequenceStepsReplace(seqId), newSteps)
    setStepsForSequence(seqId, data)
    return data
  }

  return {
    steps,
    stepsBySequence,
    enrichedStepsBySequence,
    getStepDescription,
    fetchSteps,
    addStep,
    updateStep,
    deleteStep,
    reorderSteps,
    replaceSteps,
    hydrateSequence: setStepsForSequence,
    dropSequence: removeSequenceSteps,
  }
})
