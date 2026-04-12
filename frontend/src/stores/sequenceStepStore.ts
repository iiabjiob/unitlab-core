// src/stores/sequenceStepStore.ts
import { defineStore } from "pinia"
import { ref, computed, watch } from "vue"
import type { SequenceStep, SequenceStepCreate } from "@/types/sequences"
import { SequenceStepType } from "@/types/sequences"
import { useChannelStore } from "./channelStore"
import { getLogger } from "@/utils/logger"
import { SequencesAPI } from "@/api/sequences.api"
import { useWorkspaceStore } from "./workspaceStore"
import { useSequenceStore } from "./sequenceStore"

const logger = getLogger("SEQS")

export const useSequenceStepStore = defineStore("sequenceStepStore", () => {
  const steps = ref<SequenceStep[]>([])
  const loadedSequence = ref<Set<number>>(new Set())
  const activeStepId = ref<number | null>(null)
  let tempStepId = -1
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
      case SequenceStepType.CALL_SEQUENCE: {
        const targetSequenceId = Number(step.payload?.target_sequence_id ?? NaN)
        const targetSequence = Number.isFinite(targetSequenceId)
          ? useSequenceStore().sequences.find((sequence) => sequence.id === targetSequenceId)
          : null
        return `Call instruction ${targetSequence?.name ?? (Number.isFinite(targetSequenceId) ? `#${targetSequenceId}` : "n/a")}`
      }
      case SequenceStepType.REPEAT_SEQUENCE: {
        const targetSequenceId = Number(step.payload?.target_sequence_id ?? NaN)
        const targetSequence = Number.isFinite(targetSequenceId)
          ? useSequenceStore().sequences.find((sequence) => sequence.id === targetSequenceId)
          : null
        const targetLabel = targetSequence?.name ?? (Number.isFinite(targetSequenceId) ? `#${targetSequenceId}` : "n/a")
        const mode = String(step.payload?.repeat_mode ?? "times")
        if (mode === "duration") {
          return `Repeat ${targetLabel} for ${step.payload?.duration_ms ?? 0} ms`
        }
        if (mode === "until_stopped") {
          return `Repeat ${targetLabel} until stopped`
        }
        return `Repeat ${targetLabel} × ${step.payload?.iterations ?? 1}`
      }
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

  function buildStepCreateFromStep(step: SequenceStep): SequenceStepCreate {
    return {
      sequence_step_type: step.sequence_step_type,
      channel_id: step.channel_id ?? null,
      payload: cloneStepPayload(step.payload ?? null),
    }
  }

  async function fetchSteps(seqId: number) {
    const { data } = await SequencesAPI.getSteps(workspaceStore.requireWorkspaceId(), seqId)
    setStepsForSequence(seqId, data)
    logger.info(`📡 Loaded ${data.length} steps for sequence ${seqId}`)
  }

  async function addStep(seqId: number, step: SequenceStepCreate) {
    const now = new Date().toISOString()
    const maxOrderIndex = stepsBySequence(seqId).value.reduce(
      (max, item) => Math.max(max, item.order_index),
      -1,
    )

    const optimisticStep: SequenceStep = {
      id: tempStepId--,
      sequence_id: seqId,
      order_index: maxOrderIndex + 1,
      sequence_step_type: step.sequence_step_type,
      channel_id: step.channel_id ?? null,
      payload: cloneStepPayload(step.payload ?? null),
      created_at: now,
      updated_at: now,
    }

    steps.value.push(optimisticStep)
    steps.value.sort((a, b) => a.order_index - b.order_index)

    const payload = {
      ...step,
      sequence_step_type: step.sequence_step_type,
    }

    try {
      const { data } = await SequencesAPI.addStep(workspaceStore.requireWorkspaceId(), seqId, payload)
      const idx = steps.value.findIndex((item) => item.id === optimisticStep.id)
      if (idx !== -1) {
        steps.value[idx] = data
      } else {
        steps.value.push(data)
      }
      steps.value.sort((a, b) => a.order_index - b.order_index)
      return data
    } catch (error) {
      steps.value = steps.value.filter((item) => item.id !== optimisticStep.id)
      throw error
    }
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
    const removed = steps.value.find((step) => step.id === stepId && step.sequence_id === seqId)
    steps.value = steps.value.filter((step) => step.id !== stepId)

    try {
      await SequencesAPI.deleteStep(workspaceStore.requireWorkspaceId(), seqId, stepId)
      return true
    } catch (error) {
      if (removed) {
        steps.value.push(removed)
        steps.value.sort((a, b) => a.order_index - b.order_index)
      }
      throw error
    }
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

  async function insertSteps(seqId: number, stepDrafts: SequenceStepCreate[], insertAfterStepId: number | null = null) {
    if (!stepDrafts.length) {
      return [] as SequenceStep[]
    }

    const ordered = [...stepsBySequence(seqId).value].sort((a, b) => a.order_index - b.order_index)
    const baseOrder = ordered.map((step) => step.id)

    let insertIndex = baseOrder.length
    if (insertAfterStepId !== null) {
      const currentIndex = baseOrder.indexOf(insertAfterStepId)
      if (currentIndex >= 0) {
        insertIndex = currentIndex + 1
      }
    }

    const createdIds: number[] = []
    for (const draft of stepDrafts) {
      const created = await addStep(seqId, {
        sequence_step_type: draft.sequence_step_type,
        channel_id: draft.channel_id ?? null,
        payload: cloneStepPayload(draft.payload ?? null),
      })
      createdIds.push(created.id)
    }

    const newOrder = [...baseOrder]
    newOrder.splice(insertIndex, 0, ...createdIds)
    await reorderSteps(seqId, newOrder)

    const byId = new Map(stepsBySequence(seqId).value.map((step) => [step.id, step] as const))
    return createdIds
      .map((id) => byId.get(id) ?? null)
      .filter((step): step is SequenceStep => step !== null)
  }

  async function duplicateStep(seqId: number, stepId: number) {
    const source = stepsBySequence(seqId).value.find((step) => step.id === stepId)
    if (!source) {
      throw new Error(`Step ${stepId} not found`)
    }

    const inserted = await insertSteps(seqId, [buildStepCreateFromStep(source)], stepId)
    const created = inserted[0]
    if (!created) {
      throw new Error("Failed to duplicate step")
    }
    return created
  }

  async function duplicateSteps(seqId: number, stepIds: number[], insertAfterStepId: number | null = null) {
    const uniqueStepIds = Array.from(new Set(stepIds))
    if (!uniqueStepIds.length) {
      return [] as SequenceStep[]
    }

    const ordered = [...stepsBySequence(seqId).value].sort((a, b) => a.order_index - b.order_index)

    const orderedSources = ordered
      .filter((step) => uniqueStepIds.includes(step.id))
      .map((step) => buildStepCreateFromStep(step))

    if (!orderedSources.length) {
      return [] as SequenceStep[]
    }

    const fallbackInsertAfterStepId = insertAfterStepId
      ?? ordered.find((step) => uniqueStepIds.includes(step.id))?.id
      ?? null

    return insertSteps(seqId, orderedSources, fallbackInsertAfterStepId)
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
    duplicateSteps,
    reorderSteps,
    replaceSteps,
    insertSteps,
    buildStepCreateFromStep,
    hydrateSequence: setStepsForSequence,
    dropSequence: removeSequenceSteps,
    resetAll,
  }
})
