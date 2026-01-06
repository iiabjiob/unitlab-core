// src/stores/sequenceLogStore.ts
import { formatTs } from "@/utils/datetime"
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
      ts: formatTs(now),
      t: now,
      ...entry
    })
  }

  function clear(seqId: number) {
    logs.value[seqId] = []
  }

  function resetAll() {
    logs.value = {}
  }

  return { logs, push, clear, resetAll }
})
