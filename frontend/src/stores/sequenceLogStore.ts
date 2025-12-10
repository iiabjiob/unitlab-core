// src/stores/sequenceLogStore.ts
import { defineStore } from "pinia"
import { ref } from "vue"

export type SequenceLogEntry = {
  ts: string
  t: number
  type: "info" | "step" | "error"
  message: string
}

export const useSequenceLogStore = defineStore("sequenceLogStore", () => {
  const logs = ref<Record<number, SequenceLogEntry[]>>({})

  function push(seqId: number, entry: Omit<SequenceLogEntry, "ts" | "t">) {
    const now = Date.now()
    if (!logs.value[seqId]) logs.value[seqId] = []
    logs.value[seqId].push({
      ts: new Date(now).toLocaleTimeString(),
      t: now,
      ...entry
    })
  }

  function clear(seqId: number) {
    logs.value[seqId] = []
  }

  return { logs, push, clear }
})
