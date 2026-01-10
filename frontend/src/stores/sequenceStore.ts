import { defineStore } from "pinia"
import { ref, watch } from "vue"
import { SequencesAPI } from "@/api/sequences.api"
import {
  SequenceStatusEnum,
  type SequenceDef,
  type SequenceState,
  type SequenceStep,
} from "@/types/sequences"

import type { SequenceWsEvent } from "@/types/ws/events"
import { getLogger } from "@/utils/logger"
import { useSequenceStepStore } from "./sequenceStepStore"
import { useSequenceLogStore } from "@/stores/sequenceLogStore"
import { useWorkspaceStore } from "./workspaceStore"

const logger = getLogger("SEQ")

function createEmptyState(seqId: number, totalSteps: number): SequenceState {
  return {
    sequence_id: seqId,
    status: SequenceStatusEnum.IDLE,
    run_id: null,
    current_step_index: 0,
    total_steps: totalSteps,
    completed_step_ids: [],
    last_error: null,
    started_at: null,
    finished_at: null,
  }
}

const mapStatus = (raw: string): SequenceStatusEnum => {
  switch (raw.toLowerCase()) {
    case "idle": return SequenceStatusEnum.IDLE
    case "pending": return SequenceStatusEnum.PENDING
    case "running": return SequenceStatusEnum.RUNNING
    case "cancelling": return SequenceStatusEnum.CANCELLING
    case "stopped": return SequenceStatusEnum.STOPPED
    case "completed": return SequenceStatusEnum.COMPLETED
    case "error": return SequenceStatusEnum.ERROR
    default:
      console.warn("Unknown sequence status:", raw)
      return SequenceStatusEnum.IDLE
  }
}


export const useSequenceStore = defineStore("sequenceStore", () => {

  const sequences = ref<SequenceDef[]>([])
  const states = ref<Record<number, SequenceState>>({})
  const loading = ref(false)
  const loadedOnce = ref(false)
  const stepStore = useSequenceStepStore()
  const logStore = useSequenceLogStore()
  const workspaceStore = useWorkspaceStore()
    
  async function ensureLoaded() {
    if (!workspaceStore.activeWorkspaceId) {
      logger.debug("⏸️ No active workspace selected, skipping sequence load")
      return
    }

    if (!loadedOnce.value) {
      await fetchSequences()
    }
  }

  function stepsCount(seqId: number): number {
    return stepStore.stepsBySequence(seqId).value.length
  }

  function ensureState(seq: SequenceDef | number): SequenceState {
    const id = typeof seq === "number" ? seq : seq.id
    const total = stepsCount(id)

    if (!states.value[id]) {
      states.value[id] = createEmptyState(id, total)
    } else {
      states.value[id].total_steps = total
    }

    return states.value[id]
  }

  function resetState(seqId: number) {
    states.value[seqId] = createEmptyState(seqId, stepsCount(seqId))
  }

  function applySnapshot(seqId: number, snapshot: SequenceState) {
    const total = snapshot.total_steps ?? stepsCount(seqId)

    states.value[seqId] = {
      ...createEmptyState(seqId, total),
      ...snapshot,
      status: mapStatus(snapshot.status as any),
      total_steps: total,
      completed_step_ids: [...(snapshot.completed_step_ids ?? [])],
    }

    return states.value[seqId]
  }

  function upsertSequence(seq: SequenceDef) {
    const idx = sequences.value.findIndex(s => s.id === seq.id)
    const normalized = { ...seq }
    delete normalized.steps

    if (idx === -1) sequences.value.push(normalized)
    else sequences.value[idx] = normalized

    if (seq.steps?.length) {
      stepStore.hydrateSequence(seq.id, seq.steps)
    }

    ensureState(seq.id)
  }

  async function fetchSequences() {
    if (!workspaceStore.activeWorkspaceId) return
    const workspaceId = workspaceStore.requireWorkspaceId()
    loading.value = true
    try {
      const { data } = await SequencesAPI.list(workspaceId)

      sequences.value = []
      data.forEach(upsertSequence)
      loadedOnce.value = true
      logger.info(`📡 Loaded ${data.length} sequences for workspace ${workspaceId}`)
    } finally {
      loading.value = false
    }
  }

  function nextDefaultName(): string {
    const base = "New Sequence"
    const names = sequences.value.map(s => s.name)

    let n = 1
    while (names.includes(`${base} ${n}`)) {
      n++
    }

    return `${base} ${n}`
  }

  function nextDuplicateName(sourceName: string): string {
    const base = sourceName.trim() || "Sequence"
    const names = new Set(sequences.value.map(s => s.name))

    let suffix = " copy"
    let counter = 2
    let candidate = `${base}${suffix}`

    while (names.has(candidate)) {
      candidate = `${base}${suffix} ${counter}`
      counter += 1
    }

    return candidate
  }

  async function createSequenceAuto() {
    const name = nextDefaultName()
    return await createSequence({ name, description: "" })
  }

  async function createSequence(payload: { name: string; description?: string }) {
    const { data } = await SequencesAPI.create(workspaceStore.requireWorkspaceId(), { ...payload, steps: [] })
    upsertSequence(data)
    await refreshState(data.id)
    return data
  }

  async function updateSequence(id: number, payload: Partial<SequenceDef>) {
    const { data } = await SequencesAPI.update(workspaceStore.requireWorkspaceId(), id, payload)
    upsertSequence(data)
    return data
  }

  async function deleteSequence(id: number) {
    await SequencesAPI.delete(workspaceStore.requireWorkspaceId(), id)
    sequences.value = sequences.value.filter(s => s.id !== id)
    delete states.value[id]
    stepStore.dropSequence(id)
  }

  async function duplicateSequence(id: number) {
    const original = sequences.value.find(seq => seq.id === id)
    if (!original) {
      throw new Error(`Sequence ${id} not found`)
    }

    await stepStore.ensureSteps(id)
    const stepsPayload = stepStore.stepsBySequence(id).value
      .map(step => ({
        sequence_step_type: step.sequence_step_type,
        channel_id: step.channel_id,
        payload: step.payload ?? null,
      }))

    const { data } = await SequencesAPI.create(workspaceStore.requireWorkspaceId(), {
      name: nextDuplicateName(original.name),
      description: original.description ?? "",
      steps: stepsPayload,
    })

    upsertSequence(data)
    await refreshState(data.id)
    return data
  }

  async function refreshState(id: number) {
    const { data } = await SequencesAPI.getState(workspaceStore.requireWorkspaceId(), id)
    return applySnapshot(id, data)
  }

  async function startSequence(id: number) {
    const { data } = await SequencesAPI.start(workspaceStore.requireWorkspaceId(), id)
    return applySnapshot(id, data)
  }

  async function stopSequence(id: number) {
    const { data } = await SequencesAPI.stop(workspaceStore.requireWorkspaceId(), id)
    return applySnapshot(id, data)
  }

  function handleSequenceEvent(event: SequenceWsEvent) {
    if (!sequences.value.some(seq => seq.id === event.sequence_id)) {
      return
    }
    const prev = ensureState(event.sequence_id)

    switch (event.event) {
      case "started":
        states.value[event.sequence_id] = {
          ...prev,
          status: SequenceStatusEnum.RUNNING,
          current_step_index: 0,
          completed_step_ids: [],
          last_error: null,
        }

        logStore.push(event.sequence_id, {
          type: "info",
          message: "Sequence started",
          run_id: event.run_id,
        })
        break

      case "progress":
        states.value[event.sequence_id] = {
          ...prev,
          status: SequenceStatusEnum.RUNNING,
          current_step_index: event.step_index,
          completed_step_ids: [...event.completed_steps],
          last_error: null,
        }

        logStore.push(event.sequence_id, {
          type: "step",
          message: `Step ${event.step_index + 1}/${prev.total_steps} completed`,
          run_id: event.run_id,
          ...buildStepLogMeta(event.sequence_id, {
            stepId: event.step_id,
            stepIndex: event.step_index,
            stepType: event.step_type,
          }),
        })
        break

      case "stopping":
        states.value[event.sequence_id] = {
          ...prev,
          status: SequenceStatusEnum.CANCELLING,
          current_step_index: event.current_step_index,
          total_steps: event.total_steps ?? prev.total_steps,
        }

        logStore.push(event.sequence_id, {
          type: "info",
          message: "Stop requested, waiting for current step to finish",
          run_id: event.run_id,
        })
        break

      case "step_error":
        states.value[event.sequence_id] = {
          ...prev,
          status: SequenceStatusEnum.ERROR,
          current_step_index: event.step_index,
          last_error: event.message,
        }

        logStore.push(event.sequence_id, {
          type: "error",
          message: `Step ${event.step_index + 1} error: ${event.message}`,
          run_id: event.run_id,
          ...buildStepLogMeta(event.sequence_id, {
            stepIndex: event.step_index,
          }),
        })
        break

      case "error":
        states.value[event.sequence_id] = {
          ...prev,
          status: SequenceStatusEnum.ERROR,
          last_error: event.message,
        }

        logStore.push(event.sequence_id, {
          type: "error",
          message: `Sequence error: ${event.message}`,
          run_id: event.run_id,
        })
        break

      case "stopped":
        states.value[event.sequence_id] = {
          ...prev,
          status: SequenceStatusEnum.STOPPED,
        }

        logStore.push(event.sequence_id, {
          type: "info",
          message: "Sequence stopped by user",
          run_id: event.run_id,
        })
        break

      case "completed":
        states.value[event.sequence_id] = {
          ...prev,
          status: SequenceStatusEnum.COMPLETED,
          current_step_index: prev.total_steps,
          completed_step_ids: [...prev.completed_step_ids],
        }

        logStore.push(event.sequence_id, {
          type: "info",
          message: "Sequence completed successfully",
          run_id: event.run_id,
        })
        break
    }
  }

  function resetForWorkspaceChange() {
    sequences.value = []
    states.value = {}
    loadedOnce.value = false
    stepStore.resetAll()
    logStore.resetAll()
  }

  function findStepById(seqId: number, stepId?: number | null): SequenceStep | undefined {
    if (!stepId) return undefined
    return stepStore.steps.find((step) => step.sequence_id === seqId && step.id === stepId)
  }

  function findStepByIndex(seqId: number, index?: number | null): SequenceStep | undefined {
    if (index === undefined || index === null || index < 0) return undefined
    return stepStore.steps.find((step) => step.sequence_id === seqId && step.order_index === index)
  }

  function buildStepLogMeta(
    seqId: number,
    opts: { stepId?: number | null; stepIndex?: number | null; stepType?: string },
  ): Record<string, any> {
    const meta: Record<string, any> = {}

    if (opts.stepId != null) meta.step_id = opts.stepId
    if (opts.stepIndex != null) meta.step_index = opts.stepIndex
    if (opts.stepType) meta.step_type = opts.stepType

    const step = findStepById(seqId, opts.stepId) ?? findStepByIndex(seqId, opts.stepIndex)
    if (step) {
      meta.step_id = step.id
      meta.step_index = step.order_index
      meta.step_type = step.sequence_step_type
      meta.step_label = stepStore.getStepDescription(step)
      meta.step_payload = step.payload ?? null
      meta.channel_id = step.channel_id ?? null
      meta.details = meta.step_label
    }

    return meta
  }

  watch(
    () => workspaceStore.activeWorkspaceId,
    (workspaceId) => {
      resetForWorkspaceChange()
      if (workspaceId) {
        void fetchSequences()
      }
    },
  )



  function getProgress(seq: SequenceDef | number) {
    const st = ensureState(seq)
    if (st.total_steps === 0) return 0
    return Math.round((st.completed_step_ids.length / st.total_steps) * 100)
  }

  return {
    sequences,
    states,
    loading,
    ensureLoaded,
    fetchSequences,
    updateSequence,
    createSequence,
    createSequenceAuto,
    deleteSequence,
    duplicateSequence,
    resetState,

    refreshState,
    startSequence,
    stopSequence,
    handleSequenceEvent,

    getProgress,
    isRunning: (s: SequenceDef | number) => ensureState(s).status === SequenceStatusEnum.RUNNING,

  }
})
