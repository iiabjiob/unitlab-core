// src/stores/sequenceStore.ts
import { defineStore } from "pinia"
import { ref } from "vue"
import { useWebSocketStore } from "./websocketStore"
import {
  StepKind,
  type SequenceDef,
  type SequenceStatus,
  type SequenceStep,
  type SequenceStepCreate,
} from "@/types/sequences"
import { getLogger } from "@/utils/logger"
import { describeStep, toWSMessage } from "@/utils/sequenceUtils"
import axios from "axios"
import { ApiBuilder } from "@/utils/api"
import { useValidationStore } from "./validationStore"
import { VALIDATION_LEVELS } from "@/validators/types"
import { validateSequence } from "@/validators/sequence"
import { validateSequenceStep } from "@/validators/sequenceStep"
import { SCHEMA_NAMES } from "@/property-schemas/types"
import { validateOne, clearOne } from "@/validators/syncValidation"
import { useChannelStore } from "./channelStore"
import { useDeviceStore } from "./deviceStore"

const logger = getLogger("SEQ")

function sleep(ms: number) {
  return new Promise<void>((resolve) => setTimeout(resolve, ms))
}

type SeqState = {
  status: SequenceStatus
  index: number
  completed: boolean[]
  lastError: string | null
  cancelToken: { cancelled: boolean }
}

export const useSequenceStore = defineStore("sequenceStore", () => {
  const sequences = ref<SequenceDef[]>([])
  const states = ref<Record<string, SeqState>>({})

  // -------- Helpers --------
  function ensureState(seq: SequenceDef): SeqState {
    if (!states.value[seq.id]) {
      states.value[seq.id] = {
        status: "idle",
        index: 0,
        completed: new Array(seq.steps.length).fill(false),
        lastError: null,
        cancelToken: { cancelled: false },
      }
    }
    return states.value[seq.id]
  }

  // -------- API: sequences --------
  async function fetchSequences() {
    const { data } = await axios.get(ApiBuilder.sequences())
    sequences.value = data
    sequences.value.forEach((seq) => ensureState(seq))
  }

  async function createSequence(payload: { name: string; description?: string; steps: any[] }) {
    const { data } = await axios.post(ApiBuilder.sequences(), payload)
    sequences.value.push(data)
    ensureState(data)

    validateOne(SCHEMA_NAMES.SEQUENCE, data, validateSequence)
    return data
  }

  async function updateSequence(id: number, payload: Partial<SequenceDef>) {
    const { data } = await axios.patch(ApiBuilder.sequence(id), payload)
    const idx = sequences.value.findIndex((s) => s.id === id)
    if (idx !== -1) sequences.value[idx] = data
    return data
  }

  async function updateSequenceField(id: number, changes: Partial<SequenceDef>) {
    try {
      const { data } = await axios.patch(ApiBuilder.sequence(id), changes)
      const idx = sequences.value.findIndex((s) => s.id === id)
      if (idx !== -1) sequences.value[idx] = data

      validateOne(SCHEMA_NAMES.SEQUENCE, data, validateSequence)

      logger.debug(`✅ Sequence ${id} updated with`, changes)
    } catch (error) {
      logger.error(`💥 Failed to update sequence ${id}:`, error)
    }
  }

  async function deleteSequence(id: number) {
    await axios.delete(ApiBuilder.sequence(id))
    sequences.value = sequences.value.filter((s) => s.id !== id)
    delete states.value[id]
    clearOne(SCHEMA_NAMES.SEQUENCE, id)
    logger.info(`Sequence ${id} deleted`)
  }

  // -------- API: steps --------
  async function addStep(seqId: number, step: SequenceStepCreate) {
    const { data } = await axios.post(ApiBuilder.sequenceSteps(seqId), step)
    const seq = sequences.value.find((s) => s.id === seqId)
    if (seq) {
      seq.steps.push(data)
      ensureState(seq)
      validateOne(SCHEMA_NAMES.SEQUENCE_STEP, data, validateSequenceStep)
    }
    return data
  }

  async function updateStep(seqId: number, stepId: number, changes: Partial<SequenceStep>) {
    const { data } = await axios.patch(ApiBuilder.sequenceStep(seqId, stepId), changes)
    const seq = sequences.value.find((s) => s.id === seqId)

    if (seq) {

      // Обновляем шаг в последовательности
      seq.steps = seq.steps.map((st) =>
        st.id === stepId
          ? {
              ...st,
              ...data,
              payload: { ...st.payload, ...data.payload, ...changes.payload },
            }
          : st,
      )

      validateOne(SCHEMA_NAMES.SEQUENCE_STEP, data, validateSequenceStep)
    }

    return data
  }

  async function deleteStep(seqId: number, stepId: number) {
    await axios.delete(ApiBuilder.sequenceStep(seqId, stepId))
    const seq = sequences.value.find((s) => s.id === seqId)
    if (seq) {
      seq.steps = seq.steps.filter((s) => s.id !== stepId)
      ensureState(seq)
      clearOne(SCHEMA_NAMES.SEQUENCE_STEP, stepId)
    }
    return true
  }

  async function reorderSteps(seqId: number, newOrder: number[]) {
    const { data } = await axios.post(ApiBuilder.sequenceStepsReorder(seqId), { new_order: newOrder })
    const seq = sequences.value.find((s) => s.id === seqId)
    if (seq) {
      seq.steps = data
      ensureState(seq)
    }
  }

  async function replaceSteps(seqId: number, steps: SequenceStep[]) {
    const { data } = await axios.put(ApiBuilder.sequenceSteps(seqId), steps)
    const seq = sequences.value.find((s) => s.id === seqId)
    if (seq) {
      seq.steps = data
      ensureState(seq)
    }
    return data
  }

  // -------- Execution --------
  async function start(seq: SequenceDef) {
    const st = ensureState(seq)
    if (st.status === "running") return

    st.status = "running"
    st.cancelToken.cancelled = false
    const ws = useWebSocketStore()

    try {
      for (let i = st.index; i < seq.steps.length; i++) {
        if (st.cancelToken.cancelled) throw new Error("Sequence cancelled")
        const step = seq.steps[i]
        await execStep(step, ws)
        st.completed[i] = true
        st.index = i + 1
      }
      st.status = "completed"
      logger.info(`✅ Sequence completed: ${seq.name}`)
    } catch (err: any) {
      st.lastError = err?.message ?? String(err)
      st.status = st.cancelToken.cancelled ? "stopped" : "idle"
      logger.error(`💥 Sequence failed: ${seq.name}`, err)
    }
  }

  function stop(seq: SequenceDef) {
    const st = ensureState(seq)
    st.cancelToken.cancelled = true
    st.status = "stopped"
    logger.warn(`⏹️ Sequence stopped: ${seq.name}`)
  }

  function resetState(seq: SequenceDef) {
    states.value[seq.id] = {
      status: "idle",
      index: 0,
      completed: new Array(seq.steps.length).fill(false),
      lastError: null,
      cancelToken: { cancelled: false },
    }
    logger.debug(`♻️ Sequence state reset: ${seq.name}`)
  }

  // -------- Helpers for UI --------
  function getProgress(seq: SequenceDef): number {
    const st = ensureState(seq)
    const total = seq.steps.length
    const done = st.completed.filter(Boolean).length
    return total === 0 ? 0 : Math.round((done / total) * 100)
  }

  function isRunning(seq: SequenceDef): boolean {
    return ensureState(seq).status === "running"
  }

  function isCompleted(seq: SequenceDef): boolean {
    return ensureState(seq).status === "completed"
  }

  function hasError(seq: SequenceDef): boolean {
    return ensureState(seq).lastError !== null
  }

  function hasBlockingErrors(seq: SequenceDef): boolean {
    const validation = useValidationStore()
    return validation.errors.some((err) => {
      if (err.level === VALIDATION_LEVELS.ERROR) return false
      if (err.schemaName === "sequence" && err.itemId === seq.id) return true
      if (err.schemaName === "sequence_step") {
        return seq.steps.some((step) => step.id === err.itemId)
      }
      return false
    })
  }

  async function execStep(step: SequenceStep, ws: ReturnType<typeof useWebSocketStore>) {
    if (step.kind === StepKind.WAIT) {
      const ms = step.payload?.ms ?? 0
      logger.debug(`⏳ Wait ${ms} ms`)
      await sleep(ms)
      return
    }

    const msg = toWSMessage(step)
    if (msg) {
      logger.debug(`➡️ Exec step: ${describeStep(step)}`, msg)
      ws.send(msg)
    } else {
      logger.warn(`❓ Unknown or non-executable step: ${step.kind}`, step)
    }
  }

  function getStepDescription(seq: SequenceDef, stepIndex: number): string {
    const step = seq.steps[stepIndex]
    return step ? describeStep(step) : ""
  }

  return {
    sequences,
    states,
    // sequences API
    fetchSequences,
    createSequence,
    updateSequence,
    updateSequenceField,
    deleteSequence,
    // steps API
    addStep,
    updateStep,
    deleteStep,
    reorderSteps,
    replaceSteps,
    // execution
    start,
    stop,
    resetState,
    execStep,
    // helpers
    ensureState,
    getProgress,
    isRunning,
    isCompleted,
    hasError,
    hasBlockingErrors,
    getStepDescription,
  }
})
