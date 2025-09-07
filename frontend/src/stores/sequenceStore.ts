import { defineStore } from "pinia"
import { ref } from "vue"
import { useWebSocketStore } from "./websocketStore"
import type { SequenceDef, SequenceStep } from "@/types/sequences"
import { describeStep, toResetAllCmd } from "@/types/sequences"
import { WSAction } from "@/types/ws/messages"
import { getLogger } from "@/utils/logger"

const logger = getLogger("SEQ")

export type SequenceStatus = "idle" | "running" | "stopped" | "completed"

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
  const states = ref<Record<string, SeqState>>({})

  function ensureState(seq: SequenceDef): SeqState {
    if (!states.value[seq.id]) {
      states.value[seq.id] = {
        status: "idle",
        index: 0,
        completed: new Array(seq.steps.length).fill(false),
        lastError: null,
        cancelToken: { cancelled: false }
      }
    }
    return states.value[seq.id]
  }

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

  async function resetAllDos(seq: SequenceDef) {
    const step = seq.steps.find(s => s.kind === "DO_SET" || s.kind === "DO_RESET_ALL")

    let uid: string | undefined
    if (step?.kind === "DO_SET") uid = step.cmd.unit_id
    if (step?.kind === "DO_RESET_ALL") uid = step.unit_id

    if (!uid) {
      logger.warn("⚠️ resetAllDos: no DO step with unit_id found in sequence")
      return
    }

    const ws = useWebSocketStore()
    ws.send(toResetAllCmd(uid))
    logger.info(`🔄 Reset all DOs for ${uid}`)
  }

  function resetState(seq: SequenceDef) {
    states.value[seq.id] = {
      status: "idle",
      index: 0,
      completed: new Array(seq.steps.length).fill(false),
      lastError: null,
      cancelToken: { cancelled: false }
    }
    logger.debug(`♻️ Sequence state reset: ${seq.name}`)
  }

  // ---------- Helpers for UI ----------
  function getProgress(seq: SequenceDef): number {
    const st = ensureState(seq)
    const total = seq.steps.length
    const done = st.completed.filter(Boolean).length
    return total === 0 ? 0 : Math.round((done / total) * 100)
  }

  function getStepDescription(seq: SequenceDef, stepIndex: number): string {
    return describeStep(seq.steps[stepIndex])
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

  async function execStep(step: SequenceStep, ws: ReturnType<typeof useWebSocketStore>) {
    switch (step.kind) {
      case "WAIT":
        logger.debug(`⏳ Wait ${step.ms} ms`)
        await sleep(step.ms)
        return
      case "DO_RESET_ALL":
        logger.debug(`🔄 Reset all DOs for ${step.unit_id}`)
        ws.send(toResetAllCmd(step.unit_id))
        return
      case "DO_SET":
        logger.debug(`➡️ Send DO_SET for ${step.cmd.unit_id}: ${describeStep(step)}`)
        ws.send({ action: WSAction.SET_DO_COMMAND, ...step.cmd })
        return
    }
  }

  return {
    states,
    ensureState,
    start,
    stop,
    resetAllDos,
    resetState,
    getProgress,
    getStepDescription,
    isRunning,
    isCompleted,
    hasError
  }
})
