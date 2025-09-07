import { defineStore } from "pinia"
import { ref } from "vue"
import axios from "axios"
import { ApiBuilder } from "@/utils/api"
import type { EventLogEntry } from "@/types/eventLog"
import { getLogger } from "@/utils/logger"

const logger = getLogger("EVT")

type EventLogUIEntry = EventLogEntry & { highlight?: boolean }

export const useEventLogStore = defineStore("eventLogStore", () => {
  const items = ref<EventLogUIEntry[]>([])
  const isLoading = ref(false)

  async function fetchEvents(limit = 100) {
    isLoading.value = true
    try {
      logger.debug("⏳ Fetching /api/events ...")
      const { data } = await axios.get<EventLogEntry[]>(ApiBuilder.events(), {
        params: { limit }
      })
      items.value = data
      logger.debug("✅ Fetched:", items.value.length)

    } catch (err) {
      logger.error("💥 Failed to fetch events:", err)
    } finally {
      isLoading.value = false
    }
  }

  function add(entry: EventLogEntry) {
    const wrapped: EventLogUIEntry = { ...entry, highlight: true }
    items.value.unshift(wrapped)

    // remove highlight after 1s
    setTimeout(() => {
      const idx = items.value.indexOf(wrapped)
      if (idx !== -1) {
        items.value[idx] = { ...wrapped, highlight: false }
      }
    }, 1000)

    if (items.value.length > 500) items.value.pop()
  }

  function lastN(n: number): EventLogEntry[] {
    return items.value.slice(-n)
  }

  function clear() {
    items.value = []
    logger.info("🧹 Event log cleared (local only)")
  }

  return {
    items,
    isLoading,
    fetchEvents,
    add,
    lastN,
    clear,
  }
})
