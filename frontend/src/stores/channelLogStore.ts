import { formatTs } from "@/utils/datetime"
import { defineStore } from "pinia"
import { ref } from "vue"

export type ChannelLogEntry = {
  ts: string
  t: number
  type: "info" | "cmd" | "state" | "resp" | "error"
  message: string
  actionId?: string
  reason?: string
}

export const useChannelLogStore = defineStore("channelLogStore", () => {

  // device_id → list of logs
  const logs = ref<Record<number, ChannelLogEntry[]>>({})

  function push(deviceId: number, entry: Omit<ChannelLogEntry, "ts" | "t">) {
    const now = Date.now()

    if (!logs.value[deviceId]) logs.value[deviceId] = []

    logs.value[deviceId].push({
      ts: formatTs(now),
      t: now,
      ...entry,
    })
  }

  function clear(deviceId: number) {
    logs.value[deviceId] = []
  }

  return {
    logs,
    push,
    clear
  }
})
