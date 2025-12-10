import { defineStore } from "pinia"
import { ref } from "vue"

export type SwitchgearLogEntry = {
  ts: string
  t: number
  type: "info" | "command" | "error"
  message: string
}

export const useSwitchgearLogStore = defineStore("switchgearLogStore", () => {
  const logs = ref<Record<number, SwitchgearLogEntry[]>>({})

  function push(swId: number, entry: Omit<SwitchgearLogEntry, "ts" | "t">) {
    const now = Date.now()
    if (!logs.value[swId]) logs.value[swId] = []
    logs.value[swId].push({
      ts: new Date(now).toLocaleTimeString(),
      t: now,
      ...entry,
    })
  }

  function clear(swId: number) {
    logs.value[swId] = []
  }

  return { logs, push, clear }
})
