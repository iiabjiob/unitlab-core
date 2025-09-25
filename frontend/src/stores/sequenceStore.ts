// src/stores/sequenceStore.ts
import { defineStore } from "pinia"
import { ref } from "vue"
import { SequenceStatusEnum, type SequenceDef, type SequenceStatus } from "@/types/sequences"
import axios from "axios"
import { ApiBuilder } from "@/utils/api"
import { getLogger } from "@/utils/logger"
import { validateOne, clearOne } from "@/validators/syncValidation"
import { validateSequence } from "@/validators/sequence"
import { SCHEMA_NAMES } from "@/property-schemas/types"
import { useWebSocketStore } from "./websocketStore"
import { useSequenceStepStore } from "./sequenceStepStore"
import { useValidationStore } from "./validationStore"
import { VALIDATION_LEVELS } from "@/validators/types"
import { execStep } from "@/utils/sequenceUtils"

const logger = getLogger("SEQ")

type SeqState = {
  status: SequenceStatus
  index: number
  completed: boolean[]
  lastError: string | null
  cancelToken: { cancelled: boolean }
}

export const useSequenceStore = defineStore("sequenceStore", () => {
  const sequences = ref<SequenceDef[]>([])
  const states = ref<Record<number, SeqState>>({})

  function ensureState(seq: SequenceDef): SeqState {
    if (!states.value[seq.id]) {
      states.value[seq.id] = {
        status: SequenceStatusEnum.IDLE,
        index: 0,
        completed: [],
        lastError: null,
        cancelToken: { cancelled: false },
      }
    }
    return states.value[seq.id]
  }

  async function fetchSequences() {
    const { data } = await axios.get(ApiBuilder.sequences())
    sequences.value = data
    sequences.value.forEach((seq) => ensureState(seq))
    const stepStore = useSequenceStepStore()
    await Promise.all(data.map((seq: SequenceDef) => stepStore.fetchSteps(seq.id)))

  }

  async function createSequence(payload: Omit<SequenceDef, "id" | "steps">) {
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
    validateOne(SCHEMA_NAMES.SEQUENCE, data, validateSequence)
    return data
  }

  async function deleteSequence(id: number) {
    await axios.delete(ApiBuilder.sequence(id))
    sequences.value = sequences.value.filter((s) => s.id !== id)
    delete states.value[id]
    clearOne(SCHEMA_NAMES.SEQUENCE, id)
    logger.info(`🗑️ Sequence ${id} deleted`)
  }

  async function start(seq: SequenceDef) {
    const st = ensureState(seq)
    if (st.status === SequenceStatusEnum.RUNNING) return

    st.status = SequenceStatusEnum.RUNNING
    st.cancelToken.cancelled = false
    const ws = useWebSocketStore()
    const stepStore = useSequenceStepStore()

    try {
      const seqSteps = stepStore.stepsBySequence(seq.id).value
      for (let i = st.index; i < seqSteps.length; i++) {
        if (st.cancelToken.cancelled) throw new Error("Sequence cancelled")

        const step = seqSteps[i] // ✅ берём один шаг
        await execStep(
          step,
          ws,
          stepStore.toWSMessage,        // функция маппинга шага → WSMessage
          stepStore.getStepDescription  // функция описания шага
        )

        st.completed[i] = true
        st.index = i + 1
      }
      st.status = SequenceStatusEnum.COMPLETED
      logger.info(`✅ Sequence completed: ${seq.name}`)
    } catch (err: any) {
      st.lastError = err?.message ?? String(err)
      st.status = st.cancelToken.cancelled ? SequenceStatusEnum.STOPPED : SequenceStatusEnum.IDLE
      logger.error(`💥 Sequence failed: ${seq.name}`, err)
    }
  }

  function stop(seq: SequenceDef) {
    const st = ensureState(seq)
    st.cancelToken.cancelled = true
    st.status = SequenceStatusEnum.STOPPED
    logger.warn(`⏹️ Sequence stopped: ${seq.name}`)
  }

  function resetState(seq: SequenceDef) {
    const stepStore = useSequenceStepStore()
    const seqSteps = stepStore.stepsBySequence(seq.id).value

    states.value[seq.id] = {
      status: SequenceStatusEnum.IDLE,
      index: 0,
      completed: new Array(seqSteps.length).fill(false),
      lastError: null,
      cancelToken: { cancelled: false },
    }

    logger.debug(`♻️ Sequence state reset: ${seq.name}`)
  }

  // -------- Helpers for UI --------
  function getProgress(seq: SequenceDef): number {
    const st = ensureState(seq)
    const stepStore = useSequenceStepStore()
    const steps = stepStore.stepsBySequence(seq.id).value

    const total = steps.length
    const done = st.completed.filter(Boolean).length

    return total === 0 ? 0 : Math.round((done / total) * 100)
  }

  function isRunning(seq: SequenceDef): boolean {
    return ensureState(seq).status === SequenceStatusEnum.RUNNING
  }

  function isCompleted(seq: SequenceDef): boolean {
    return ensureState(seq).status === SequenceStatusEnum.COMPLETED
  }

  function hasError(seq: SequenceDef): boolean {
    return ensureState(seq).lastError !== null
  }

  function hasBlockingErrors(seq: SequenceDef): boolean {
    const validation = useValidationStore()
    const stepStore = useSequenceStepStore()

    return validation.errors.some((err) => {
      // блокируем только для ошибок уровня ERROR
      if (err.level !== VALIDATION_LEVELS.ERROR) return false

      // ошибка на саму последовательность
      if (err.schemaName === "sequence" && err.itemId === seq.id) {
        return true
      }

      // ошибка на шаги внутри этой последовательности
      if (err.schemaName === "sequence_step") {
        const steps = stepStore.stepsBySequence(seq.id).value
        return steps.some((step) => step.id === err.itemId)
      }

      return false
    })
  }

  return {
    sequences,
    states,
    ensureState,

    fetchSequences,
    createSequence,
    updateSequence,
    deleteSequence,

    start,
    stop,
    resetState,

    getProgress,
    isRunning,
    isCompleted,
    hasError,
    hasBlockingErrors,

  }
})
