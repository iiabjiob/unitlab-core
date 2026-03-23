import { formatTs } from "@/utils/datetime"
import { defineStore } from "pinia"
import { ref } from "vue"

const MAX_LOGS_PER_DEVICE = 500

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

    const deviceLogs = logs.value[deviceId]
    deviceLogs.push({
      ts: formatTs(now),
      t: now,
      ...entry,
    })

    if (deviceLogs.length > MAX_LOGS_PER_DEVICE) {
      deviceLogs.splice(0, deviceLogs.length - MAX_LOGS_PER_DEVICE)
    }
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
