import { defineStore } from "pinia"
import { ref, computed } from "vue"
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

export const useSequenceStore = defineStore("sequenceStore", () => {
  const active = ref<SequenceDef | null>(null)
  const status = ref<SequenceStatus>("idle")
  const index = ref(0)
  const completed = ref<boolean[]>([])
  const lastError = ref<string | null>(null)
  const cancelToken = ref({ cancelled: false })

  const progress = computed(() => {
    if (!active.value) return 0
    const total = active.value.steps.length
    const done = completed.value.filter(Boolean).length
    return total === 0 ? 0 : Math.round((done / total) * 100)
  })

  function setSequence(seq: SequenceDef) {
    active.value = seq
    status.value = "idle"
    index.value = 0
    completed.value = new Array(seq.steps.length).fill(false)
    lastError.value = null
    cancelToken.value = { cancelled: false }
    logger.info(`📋 Sequence set: ${seq.name}`)
  }

  async function start() {
    if (!active.value || status.value === "running") return
    status.value = "running"
    cancelToken.value.cancelled = false
    const ws = useWebSocketStore()

    try {
      for (let i = index.value; i < active.value.steps.length; i++) {
        if (cancelToken.value.cancelled) throw new Error("Sequence cancelled")
        const step = active.value.steps[i]
        await execStep(step, ws)
        completed.value[i] = true
        index.value = i + 1
      }
      status.value = "completed"
      logger.info("✅ Sequence completed")
    } catch (err: any) {
      lastError.value = err?.message ?? String(err)
      status.value = cancelToken.value.cancelled ? "stopped" : "idle"
      logger.error("💥 Sequence failed:", err)
    }
  }

  function stop() {
    cancelToken.value.cancelled = true
    status.value = "stopped"
    logger.warn("⏹️ Sequence stopped")
  }

  async function resetAllDos(unit_id?: string) {
    let uid = unit_id
    if (!uid && active.value) {
      const step = active.value.steps.find(s => s.kind === "DO_SET" || s.kind === "DO_RESET_ALL")
      if (step?.kind === "DO_SET") uid = step.cmd.unit_id
      if (step?.kind === "DO_RESET_ALL") uid = step.unit_id
    }
    if (!uid) return
    const ws = useWebSocketStore()
    ws.send(toResetAllCmd(uid))
    logger.info(`🔄 Reset all DOs for ${uid}`)
  }

  function resetState() {
    if (!active.value) return
    status.value = "idle"
    index.value = 0
    completed.value = new Array(active.value.steps.length).fill(false)
    lastError.value = null
    cancelToken.value = { cancelled: false }
    logger.debug("♻️ Sequence state reset")
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
        logger.debug(`➡️ Send DO_SET: ${describeStep(step)}`)
        ws.send({ action: WSAction.SET_DO_COMMAND, ...step.cmd })
        return
    }
  }

  function debugDescribe(stepIndex: number): string {
    if (!active.value) return "—"
    const step = active.value.steps[stepIndex]
    return describeStep(step)
  }

  return {
    active,
    status,
    index,
    completed,
    lastError,
    progress,
    setSequence,
    start,
    stop,
    resetAllDos,
    resetState,
    debugDescribe,
  }
})
