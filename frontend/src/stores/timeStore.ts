import { defineStore } from "pinia"
import { ref, computed } from "vue"
import axios from "axios"
import { ApiBuilder } from "@/utils/api"
import type { TimeStatus } from "@/types/time"
import type { TimeStatusEvent } from "@/types/ws/events"
import { getLogger } from "@/utils/logger"

const logger = getLogger("TS")

export const useTimeStore = defineStore("timeStore", () => {
  const serverTime = ref<Date | null>(null)
  const status = ref<"synced" | "unsynced">("unsynced")
  const source = ref<string | null>(null)
  const offsetUs = ref<number | null>(null)

  let timer: number | null = null

  // --- Increment local time every second ---
  function startTicker() {
    if (timer) clearInterval(timer)
    timer = window.setInterval(() => {
      if (serverTime.value) {
        serverTime.value = new Date(serverTime.value.getTime() + 1000)
      }
    }, 1000)
  }

  // --- Apply WS update ---
  function updateFromSync(event: TimeStatusEvent) {
    applyUpdate(event)
  }

  // --- Internal helper ---
  function applyUpdate(data: TimeStatus) {
    if (data.timestamp) {
      serverTime.value = new Date(data.timestamp)
    }
    status.value = data.status
    source.value = data.source ?? null
    offsetUs.value = data.offset_us ?? null
  }

  const formatted = computed(() =>
    serverTime.value
      ? serverTime.value.toLocaleTimeString("en-GB", { hour12: false })
      : "—"
  )

  const sourceLabel = computed(() => {
    if (status.value === "unsynced") return "local"
    if (status.value === "synced") return "ntp"
    return source.value ?? "unknown"
  })

  return {
    serverTime,
    status,
    source,
    offsetUs,
    formatted,
    sourceLabel,
    updateFromSync,
  }
})
