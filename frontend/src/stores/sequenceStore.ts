import { defineStore } from "pinia"
import { ref } from "vue"
import { useWebSocketStore } from "./websocketStore"
import { StepKind, type SequenceDef, type SequenceStatus, type SequenceStep } from "@/types/sequences"
import { getLogger } from "@/utils/logger"
import { describeStep, toWSMessage } from "@/utils/sequenceUtils"

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

  // -------- API integration --------
  async function fetchSequences() {
    const res = await fetch("/api/sequences")
    if (!res.ok) throw new Error("Failed to fetch sequences")
    sequences.value = await res.json()
    // re-init state for new/changed sequences
    sequences.value.forEach((seq) => ensureState(seq))
  }

  async function createSequence(payload: { name: string; description?: string; steps: any[] }) {
    const res = await fetch("/api/sequences", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    })
    if (!res.ok) throw new Error("Failed to create sequence")
    const seq = await res.json()
    sequences.value.push(seq)
    ensureState(seq)
    return seq
  }

  async function updateSequence(id: number, payload: { name?: string; description?: string }) {
    const res = await fetch(`/api/sequences/${id}`, {
      method: "PATCH",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    })
    if (!res.ok) throw new Error("Failed to update sequence")
    const seq = await res.json()
    const idx = sequences.value.findIndex((s) => s.id === id)
    if (idx !== -1) sequences.value[idx] = seq
    return seq
  }

  async function deleteSequence(id: number) {
    const res = await fetch(`/api/sequences/${id}`, { method: "DELETE" })
    if (!res.ok) throw new Error("Failed to delete sequence")
    sequences.value = sequences.value.filter((s) => s.id !== id)
    delete states.value[id]
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

  async function execStep(
    step: SequenceStep,
    ws: ReturnType<typeof useWebSocketStore>
  ) {
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

  // -------- Step API integration --------
  async function addStep(seqId: number, step: SequenceStep) {
    const res = await fetch(`/api/sequences/${seqId}/steps`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(step),
    })
    if (!res.ok) throw new Error("Failed to add step")
    const newStep = await res.json()

    // обновляем в локальном сторе
    const seq = sequences.value.find((s) => s.id === seqId)
    if (seq) {
      seq.steps.push(newStep)
      ensureState(seq)
    }
    return newStep
  }

  async function updateStep(seqId: number, stepId: number, changes: Partial<SequenceStep>) {
    const res = await fetch(`/api/sequences/${seqId}/steps/${stepId}`, {
      method: "PATCH",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(changes),
    })
    if (!res.ok) throw new Error("Failed to update step")
    const updated = await res.json()

    const seq = sequences.value.find((s) => s.id === seqId)
    if (seq) {
      const idx = seq.steps.findIndex((s) => s.id === stepId)
      if (idx !== -1) seq.steps[idx] = updated
    }
    return updated
  }

  async function deleteStep(seqId: number, stepId: number) {
    const res = await fetch(`/api/sequences/${seqId}/steps/${stepId}`, {
      method: "DELETE",
    })
    if (!res.ok) throw new Error("Failed to delete step")

    const seq = sequences.value.find((s) => s.id === seqId)
    if (seq) {
      seq.steps = seq.steps.filter((s) => s.id !== stepId)
      ensureState(seq) // пересчёт completed[]
    }
    return true
  }

  async function reorderSteps(seqId: number, newOrder: number[]) {
    const res = await fetch(`/api/sequences/${seqId}/steps/reorder`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(newOrder),
    })
    if (!res.ok) throw new Error("Failed to reorder steps")

    // после reorder перезапрашиваем шаги с бэка
    const reload = await fetch(`/api/sequences/${seqId}/steps`)
    if (reload.ok) {
      const steps = await reload.json()
      const seq = sequences.value.find((s) => s.id === seqId)
      if (seq) seq.steps = steps
    }
  }

  async function replaceSteps(seqId: number, steps: SequenceStep[]) {
    const res = await fetch(`/api/sequences/${seqId}/steps`, {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(steps),
    })
    if (!res.ok) throw new Error("Failed to replace steps")
    const updated = await res.json()

    const seq = sequences.value.find((s) => s.id === seqId)
    if (seq) {
      seq.steps = updated
      ensureState(seq)
    }
    return updated
  }

  function getStepDescription(seq: SequenceDef, stepIndex: number): string {
    const step = seq.steps[stepIndex]
    return step ? describeStep(step) : ""
  }

  return {
    sequences,
    states,
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
    addStep,
    updateStep,
    deleteStep,
    reorderSteps,
    replaceSteps,
    execStep,
    ensureState,
    getStepDescription,
  }
})
