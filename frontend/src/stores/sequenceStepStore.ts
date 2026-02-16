// src/stores/sequenceStepStore.ts
import { defineStore } from "pinia"
import { ref, computed, watch } from "vue"
import type { SequenceStep, SequenceStepCreate } from "@/types/sequences"
import { SequenceStepType } from "@/types/sequences"
import { useChannelStore } from "./channelStore"
import { getLogger } from "@/utils/logger"
import { SequencesAPI } from "@/api/sequences.api"
import { useWorkspaceStore } from "./workspaceStore"

const logger = getLogger("SEQS")

export const useSequenceStepStore = defineStore("sequenceStepStore", () => {
  const steps = ref<SequenceStep[]>([])
  const loadedSequence = ref<Set<number>>(new Set())
  const activeStepId = ref<number | null>(null)
  const channelStore = useChannelStore()
  const workspaceStore = useWorkspaceStore()

  async function ensureSteps(seqId: number) {
    if (!loadedSequence.value.has(seqId)) {
      await fetchSteps(seqId)
      loadedSequence.value.add(seqId)
    }
  }

  function setActiveStep(stepId: number | null) {
    activeStepId.value = stepId
  }

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
    function resolve(id?: number | null, fallbackSignalKey?: string | null): string {
      if (!id) {
        const signalKey = String(fallbackSignalKey ?? "").trim()
        return signalKey ? `SIG:${signalKey}` : "n/a"
      }
      const ch = channelStore.channels.find((c) => c.id === id)
      return ch ? channelStore.resolveChannelFullLabel(ch) : `CH#${id}`
    }

    function resolvePairStateLabel(value: unknown): string {
      const state = Number(value ?? 0)
      switch (state) {
        case 0:
          return "Unknown"
        case 1:
          return "Open"
        case 2:
          return "Closed"
        case 3:
          return "Undefined"
        default:
          return "Custom"
      }
    }

    switch (step.sequence_step_type) {
      case SequenceStepType.WAIT:
        return `Wait ${step.payload?.ms ?? 0} ms`
      case SequenceStepType.DO_LATCH:
        return `DO: latch ${resolve(step.channel_id, step.payload?.signal_key)} = ${step.payload?.value ?? 0}`
      case SequenceStepType.DO_PULSE:
        return `DO: pulse ${resolve(step.channel_id, step.payload?.signal_key)} = ${step.payload?.value ?? 0} for ${step.payload?.pulse_ms ?? 0} ms`
      case SequenceStepType.DO_PAIR: {
        const ids = step.payload?.channel_ids ?? []
        const keys = step.payload?.signal_keys ?? []
        const [idA, idB] = ids
        return `Switch position ${resolvePairStateLabel(step.payload?.state2b)} · ${resolve(idA, keys[0])} + ${resolve(idB, keys[1])}`
      }
      case SequenceStepType.DO_BITMASK: {
        const deviceId = Number(step.payload?.device_id ?? NaN)
        const unitLabel = Number.isFinite(deviceId) ? channelStore.resolveUnitId(deviceId) : "unit n/a"
        return `Group control · ${unitLabel}`
      }
      case SequenceStepType.AO_SET:
        return `AO: set ${resolve(step.channel_id, step.payload?.signal_key)} = ${step.payload?.value ?? 0} mA`
      default:
        return `❓ Unknown step type: ${step.sequence_step_type}`
    }
  }

  function cloneStepPayload<T>(payload: T): T {
    if (payload === null || payload === undefined) {
      return payload
    }
    return JSON.parse(JSON.stringify(payload)) as T
  }

  async function fetchSteps(seqId: number) {
    const { data } = await SequencesAPI.getSteps(workspaceStore.requireWorkspaceId(), seqId)
    setStepsForSequence(seqId, data)
    logger.info(`📡 Loaded ${data.length} steps for sequence ${seqId}`)
  }

  async function addStep(seqId: number, step: SequenceStepCreate) {
    const payload = {
      ...step,
      sequence_step_type: step.sequence_step_type,
    }
    const { data } = await SequencesAPI.addStep(workspaceStore.requireWorkspaceId(), seqId, payload)
    steps.value.push(data)
    steps.value.sort((a, b) => a.order_index - b.order_index)
    return data
  }

  async function updateStep(seqId: number, stepId: number, changes: Partial<SequenceStep>) {
    const { data } = await SequencesAPI.updateStep(workspaceStore.requireWorkspaceId(), seqId, stepId, changes)
    const idx = steps.value.findIndex((s) => s.id === stepId)
    if (idx !== -1) {
      steps.value[idx] = { ...steps.value[idx], ...data }
      steps.value.sort((a, b) => a.order_index - b.order_index)
    }
    return data
  }

  async function deleteStep(seqId: number, stepId: number) {
    await SequencesAPI.deleteStep(workspaceStore.requireWorkspaceId(), seqId, stepId)
    steps.value = steps.value.filter((s) => s.id !== stepId)
    return true
  }

  async function reorderSteps(seqId: number, newOrder: number[]) {
    const { data } = await SequencesAPI.reorderSteps(workspaceStore.requireWorkspaceId(), seqId, { new_order: newOrder })
    setStepsForSequence(seqId, data)
    return data
  }

  async function replaceSteps(seqId: number, newSteps: SequenceStepCreate[]) {
    const { data } = await SequencesAPI.replaceSteps(workspaceStore.requireWorkspaceId(), seqId, newSteps)
    setStepsForSequence(seqId, data)
    return data
  }

  async function duplicateStep(seqId: number, stepId: number) {
    const ordered = [...stepsBySequence(seqId).value].sort((a, b) => a.order_index - b.order_index)
    const source = ordered.find((step) => step.id === stepId)
    if (!source) {
      throw new Error(`Step ${stepId} not found`)
    }

    const created = await addStep(seqId, {
      sequence_step_type: source.sequence_step_type,
      channel_id: source.channel_id ?? null,
      payload: cloneStepPayload(source.payload ?? null),
    })

    const newOrder = ordered.map((step) => step.id)
    const sourceIndex = newOrder.indexOf(stepId)
    newOrder.splice(sourceIndex + 1, 0, created.id)
    await reorderSteps(seqId, newOrder)
    return created
  }

  function resetAll() {
    steps.value = []
    loadedSequence.value = new Set()
    activeStepId.value = null
  }

  watch(
    () => workspaceStore.activeWorkspaceId,
    () => {
      resetAll()
    },
  )

  return {
    steps,
    stepsBySequence,
    activeStepId,
    ensureSteps,
    setActiveStep,
    enrichedStepsBySequence,
    getStepDescription,
    fetchSteps,
    addStep,
    updateStep,
    deleteStep,
    duplicateStep,
    reorderSteps,
    replaceSteps,
    hydrateSequence: setStepsForSequence,
    dropSequence: removeSequenceSteps,
    resetAll,
  }
})
