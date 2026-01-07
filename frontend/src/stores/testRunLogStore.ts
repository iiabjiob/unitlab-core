import { formatTs } from "@/utils/datetime"
import { defineStore } from "pinia"
import { ref } from "vue"

type TestRunLogType = "info" | "step" | "error"

export interface TestRunLogEntry {
  ts: string
  t: number
  type: TestRunLogType
  message: string
}

export const useTestRunLogStore = defineStore("testRunLogStore", () => {
  const logs = ref<Record<number, TestRunLogEntry[]>>({})

  function push(runId: number, entry: Omit<TestRunLogEntry, "ts" | "t">) {
    const now = Date.now()
    if (!logs.value[runId]) logs.value[runId] = []
    logs.value[runId].push({
      ts: formatTs(now),
      t: now,
      ...entry,
    })
  }

  function clear(runId: number) {
    logs.value[runId] = []
  }

  function resetAll() {
    logs.value = {}
  }

  return { logs, push, clear, resetAll }
})
