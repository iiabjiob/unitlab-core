// src/stores/sequenceStepStore.ts
import { defineStore } from "pinia"
import { ref, computed } from "vue"
import type { SequenceStep, SequenceStepCreate } from "@/types/sequences"
import type { WSMessage } from "@/types/ws/messages"
import { WSAction, CmdMode } from "@/types/ws/messages"
import { StepKind } from "@/types/sequences"
import { useChannelStore } from "./channelStore"
import axios from "axios"
import { ApiBuilder } from "@/utils/api"
import { getLogger } from "@/utils/logger"
import { validateOne, clearOne } from "@/validators/syncValidation"
import { validateSequenceStep } from "@/validators/sequenceStep"
import { SCHEMA_NAMES } from "@/property-schemas/types"

const logger = getLogger("SEQS")

export const useSequenceStepStore = defineStore("sequenceStepStore", () => {
  const steps = ref<SequenceStep[]>([])
  const channelStore = useChannelStore()

  // Selectors
  const stepsBySequence = (seqId: number) =>
    computed(() => steps.value.filter((s) => s.sequence_id === seqId))

  const enrichedStepsBySequence = (seqId: number) =>
    computed(() =>
      stepsBySequence(seqId).value.map(step => ({
        ...step,
        description: getStepDescription(step),
        wsMessage: toWSMessage(step),
      }))
    )

  // -------------------------------------------------------------------
  // Description builder
  // -------------------------------------------------------------------
  function getStepDescription(step: SequenceStep): string {
    function resolve(id?: number | null): string {
      if (!id) return "n/a"
      const ch = channelStore.channels.find(c => c.id === id)
      return ch ? channelStore.resolveChannelFullLabel(ch) : `CH#${id}`
    }

    switch (step.kind) {
      case StepKind.WAIT:
        return `Wait ${step.payload?.ms ?? 0} ms`
      case StepKind.DO_LATCH:
        return `DO: latch ${resolve(step.channel_id)} = ${step.payload?.value ?? 0}`
      case StepKind.DO_PULSE:
        return `DO: pulse ${resolve(step.channel_id)} = ${step.payload?.value ?? 0} for ${step.payload?.pulse_ms ?? 0} ms`
      case StepKind.DO_PAIR: {
        const ids = step.payload?.channel_ids ?? []
        const [idA, idB] = ids
        return `DO: pair ${resolve(idA)} + ${resolve(idB)}, state=${step.payload?.state2b ?? 0}`
      }
      case StepKind.DO_BITMASK:
        return `DO: bitmask 0x${(step.payload?.bitmask ?? 0).toString(16).toUpperCase()}`
      case StepKind.AO_SET:
        return `AO: set ${resolve(step.channel_id)} = ${step.payload?.value ?? 0} mA`
      default:
        return `❓ Unknown step kind: ${step.kind}`
    }
  }

  function toWSMessage(step: SequenceStep): WSMessage | null {
    switch (step.kind) {
      case StepKind.WAIT:
        return null

      case StepKind.DO_LATCH: {
        if (!step.channel_id) return null
        const ch = channelStore.channels.find(c => c.id === step.channel_id)
        if (!ch) return null

        const value = Number(step.payload?.value ?? 0)

        return {
          action: WSAction.SET_DO_COMMAND,
          unit_id: channelStore.resolveUnitId(ch.device_id),
          mode: CmdMode.SET_SINGLE_BIT,
          ch: ch.index,
          value,
        }
      }

      case StepKind.DO_PULSE: {
        if (!step.channel_id) return null
        const ch = channelStore.channels.find(c => c.id === step.channel_id)
        if (!ch) return null

        const value = Number(step.payload?.value ?? 0)

        return {
          action: WSAction.SET_DO_COMMAND,
          unit_id: channelStore.resolveUnitId(ch.device_id),
          mode: CmdMode.SET_PULSE_BIT,
          ch: ch.index,
          value,
          pulse_ms: step.payload?.pulse_ms ?? 0,
        }
      }

      case StepKind.DO_PAIR: {
        const ids = step.payload?.channel_ids ?? []
        if (ids.length !== 2) return null
        const chA = channelStore.channels.find(c => c.id === ids[0])
        const chB = channelStore.channels.find(c => c.id === ids[1])
        if (!chA || !chB) return null

        return {
          action: WSAction.SET_DO_COMMAND,
          unit_id: channelStore.resolveUnitId(chA.device_id),
          mode: CmdMode.SET_PAIR_BIT,
          chA: chA.index,
          chB: chB.index,
          state2b: Number(step.payload?.state2b ?? 0),
        }
      }

      case StepKind.DO_BITMASK: {
        const deviceId = step.payload?.device_id
        if (!deviceId) return null

        const ch = channelStore.channels.find(c => c.device_id === deviceId)
        if (!ch) return null

        return {
          action: WSAction.SET_DO_COMMAND,
          unit_id: channelStore.resolveUnitId(ch.device_id),
          mode: CmdMode.SET_ALL_BIT,
          bitmask: Number(step.payload?.bitmask ?? 0),
        }
      }

      case StepKind.AO_SET: {
        if (!step.channel_id) return null
        const ch = channelStore.channels.find(c => c.id === step.channel_id)
        if (!ch) return null

        // AO всегда float
        const value = Number(step.payload?.value ?? 0)

        return {
          action: WSAction.SET_AO_COMMAND,
          unit_id: channelStore.resolveUnitId(ch.device_id),
          ch: ch.index,
          value,
        }
      }

      default:
        return null
    }
  }


  // -------------------------------------------------------------------
  // CRUD
  // -------------------------------------------------------------------
  async function fetchSteps(seqId: number) {
    const { data } = await axios.get(ApiBuilder.sequenceSteps(seqId))
    steps.value = steps.value
      .filter(s => s.sequence_id !== seqId)
      .concat(data)
      .sort((a, b) => a.order_index - b.order_index)

    data.forEach((st: SequenceStep) =>
      validateOne(SCHEMA_NAMES.SEQUENCE_STEP, st, validateSequenceStep),
    )
  }

  async function addStep(seqId: number, step: SequenceStepCreate) {
    const { data } = await axios.post(ApiBuilder.sequenceSteps(seqId), step)
    steps.value.push(data)
    steps.value.sort((a, b) => a.order_index - b.order_index)
    validateOne(SCHEMA_NAMES.SEQUENCE_STEP, data, validateSequenceStep)
    return data
  }

  async function updateStep(seqId: number, stepId: number, changes: Partial<SequenceStep>) {
    const { data } = await axios.patch(ApiBuilder.sequenceStep(seqId, stepId), changes)
    const idx = steps.value.findIndex(s => s.id === stepId)
    if (idx !== -1) {
      steps.value[idx] = { ...steps.value[idx], ...data }
      steps.value.sort((a, b) => a.order_index - b.order_index)
    }
    validateOne(SCHEMA_NAMES.SEQUENCE_STEP, data, validateSequenceStep)
    return data
  }

  async function deleteStep(seqId: number, stepId: number) {
    await axios.delete(ApiBuilder.sequenceStep(seqId, stepId))
    steps.value = steps.value.filter(s => s.id !== stepId)
    clearOne(SCHEMA_NAMES.SEQUENCE_STEP, stepId)
    return true
  }

  async function reorderSteps(seqId: number, newOrder: number[]) {
    const { data } = await axios.post(ApiBuilder.sequenceStepsReorder(seqId), { new_order: newOrder })
    steps.value = steps.value
      .filter(s => s.sequence_id !== seqId)
      .concat(data)
      .sort((a, b) => a.order_index - b.order_index)
    return data
  }

  async function replaceSteps(seqId: number, newSteps: SequenceStep[]) {
    const { data } = await axios.put(ApiBuilder.sequenceSteps(seqId), newSteps)
    steps.value = steps.value
      .filter(s => s.sequence_id !== seqId)
      .concat(data)
      .sort((a, b) => a.order_index - b.order_index)
    return data
  }

  return {
    steps,
    stepsBySequence,
    enrichedStepsBySequence,
    getStepDescription,
    toWSMessage,
    fetchSteps,
    addStep,
    updateStep,
    deleteStep,
    reorderSteps,
    replaceSteps,
  }
})
