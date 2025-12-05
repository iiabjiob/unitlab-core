// src/stores/sequenceStore.ts
import { defineStore } from "pinia"
import { ref } from "vue"
import axios from "axios"
import {
  SequenceStatusEnum,
  type SequenceDef,
  type SequenceState,
} from "@/types/sequences"
import type { SequenceWsEvent } from "@/types/ws/events"
import { ApiBuilder } from "@/utils/api"
import { getLogger } from "@/utils/logger"
import { useSequenceStepStore } from "./sequenceStepStore"

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

export const useSequenceStore = defineStore("sequenceStore", () => {
  const sequences = ref<SequenceDef[]>([])
  const states = ref<Record<number, SequenceState>>({})
  const loading = ref(false)
  const stepStore = useSequenceStepStore()

  function stepsCount(seqId: number): number {
    return stepStore.stepsBySequence(seqId).value.length
  }

  function ensureState(seq: SequenceDef | number): SequenceState {
    const seqId = typeof seq === "number" ? seq : seq.id
    const totalSteps = stepsCount(seqId)
    if (!states.value[seqId]) {
      states.value[seqId] = createEmptyState(seqId, totalSteps)
    } else {
      states.value[seqId].total_steps = totalSteps
    }
    return states.value[seqId]
  }

  function applySnapshot(seqId: number, snapshot: SequenceState) {
    const totalSteps = snapshot.total_steps ?? stepsCount(seqId)
    states.value[seqId] = {
      ...createEmptyState(seqId, totalSteps),
      ...snapshot,
      total_steps: totalSteps,
      completed_step_ids: [...(snapshot.completed_step_ids ?? [])],
    }
    return states.value[seqId]
  }

  function upsertSequence(seq: SequenceDef) {
    const idx = sequences.value.findIndex((s) => s.id === seq.id)
    const normalized: SequenceDef = { ...seq, steps: undefined }
    if (idx === -1) {
      sequences.value.push(normalized)
    } else {
      sequences.value[idx] = normalized
    }

    if (seq.steps && seq.steps.length) {
      stepStore.hydrateSequence(seq.id, seq.steps)
    }
    ensureState(seq.id)
  }

  async function fetchSequences() {
    loading.value = true
    try {
      const { data } = await axios.get<SequenceDef[]>(ApiBuilder.sequences())
      sequences.value = []
      data.forEach((seq) => upsertSequence(seq))
      const sequencesWithoutSteps = data.filter((seq) => !seq.steps || seq.steps.length === 0)
      if (sequencesWithoutSteps.length) {
        await Promise.all(sequencesWithoutSteps.map((seq) => stepStore.fetchSteps(seq.id)))
      }
      const activeIds = new Set(data.map((seq) => seq.id))
      Object.keys(states.value).forEach((key) => {
        const seqId = Number(key)
        if (!activeIds.has(seqId)) {
          delete states.value[seqId]
          stepStore.dropSequence(seqId)
        }
      })
      await Promise.all(
        data.map(async (seq) => {
          try {
            await refreshState(seq.id)
          } catch (err) {
            logger.debug(`⚠️ No runtime state for sequence ${seq.id}`, err)
          }
        }),
      )
      logger.info(`📡 Loaded ${data.length} sequences`)
    } finally {
      loading.value = false
    }
  }

  async function createSequence(payload: { name: string; description?: string | null }) {
    const { data } = await axios.post<SequenceDef>(ApiBuilder.sequences(), {
      ...payload,
      steps: [],
    })
    upsertSequence(data)
    await refreshState(data.id)
    return data
  }

  async function updateSequence(id: number, payload: Partial<SequenceDef>) {
    const { data } = await axios.patch<SequenceDef>(ApiBuilder.sequence(id), payload)
    upsertSequence(data)
    return data
  }

  async function deleteSequence(id: number) {
    await axios.delete(ApiBuilder.sequence(id))
    sequences.value = sequences.value.filter((s) => s.id !== id)
    delete states.value[id]
    stepStore.dropSequence(id)
    logger.info(`🗑️ Sequence ${id} deleted`)
  }

  async function refreshState(seqId: number) {
    const { data } = await axios.get<SequenceState>(ApiBuilder.sequenceState(seqId))
    return applySnapshot(seqId, data)
  }

  async function startSequence(seqId: number) {
    const { data } = await axios.post<SequenceState>(ApiBuilder.sequenceStart(seqId), {})
    return applySnapshot(seqId, data)
  }

  async function stopSequence(seqId: number) {
    const { data } = await axios.post<SequenceState>(ApiBuilder.sequenceStop(seqId), {})
    return applySnapshot(seqId, data)
  }

  function updateCompletionFromIds(seqId: number, completedIds: number[]) {
    const state = ensureState(seqId)
    state.completed_step_ids = [...completedIds]
  }

  function handleSequenceEvent(event: SequenceWsEvent) {
    const state = ensureState(event.sequence_id)
    switch (event.event) {
      case "started": {
        state.status = SequenceStatusEnum.RUNNING
        state.run_id = event.run_id
        state.current_step_index = 0
        state.last_error = null
        updateCompletionFromIds(event.sequence_id, [])
        state.total_steps = event.total_steps ?? stepsCount(event.sequence_id)
        break
      }
      case "progress": {
        state.status = SequenceStatusEnum.RUNNING
        state.run_id = event.run_id
        updateCompletionFromIds(event.sequence_id, event.completed_steps)
        state.current_step_index = Math.min(event.step_index + 1, state.total_steps)
        state.last_error = null
        break
      }
      case "step_error": {
        state.run_id = event.run_id
        state.last_error = event.message
        state.current_step_index = event.step_index
        state.status = SequenceStatusEnum.ERROR
        break
      }
      case "error": {
        state.status = SequenceStatusEnum.ERROR
        state.run_id = event.run_id
        state.last_error = event.message
        break
      }
      case "stopped": {
        state.status = SequenceStatusEnum.STOPPED
        state.run_id = event.run_id
        state.last_error = null
        break
      }
      case "completed": {
        state.status = SequenceStatusEnum.COMPLETED
        state.run_id = event.run_id
        state.last_error = null
        state.current_step_index = state.total_steps
        const steps = stepStore.stepsBySequence(event.sequence_id).value
        updateCompletionFromIds(event.sequence_id, steps.map((s) => s.id))
        break
      }
    }
  }

  function getProgress(seq: SequenceDef | number): number {
    const st = ensureState(seq)
    const total = st.total_steps
    const done = st.completed_step_ids.length
    return total === 0 ? 0 : Math.round((done / total) * 100)
  }

  function hasError(seq: SequenceDef | number): boolean {
    return ensureState(seq).last_error != null
  }

  function isRunning(seq: SequenceDef | number): boolean {
    return ensureState(seq).status === SequenceStatusEnum.RUNNING
  }

  return {
    sequences,
    states,
    loading,
    ensureState,
    fetchSequences,
    createSequence,
    updateSequence,
    deleteSequence,
    refreshState,
    startSequence,
    stopSequence,
    handleSequenceEvent,
    getProgress,
    hasError,
    isRunning,
  }
})
