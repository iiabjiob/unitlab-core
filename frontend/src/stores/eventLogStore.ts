import axios from "axios"
import { defineStore } from "pinia"
import { ref } from "vue"

import type { EventLogEntry } from "@/types/eventLog"
import type { WSEventLogEvent } from "@/types/ws/events"
import { ApiBuilder } from "@/utils/api"
import { getLogger } from "@/utils/logger"

const logger = getLogger("EVT")
type EventLogUIEntry = EventLogEntry & { highlight?: boolean }

function mapApiToEvent(entry: any): EventLogEntry {
  return {
    id: entry.id,
    ts: entry.ts,
    project_id: entry.project_id ?? null,
    event_type: entry.event_type,
    source: entry.source,
    direction: entry.direction ?? null,
    result: entry.result ?? null,
    payload: entry.payload ?? null,
    message: entry.message ?? null,
    packet_id: entry.packet_id ?? null,
    datapoint_id: entry.datapoint_id ?? null,
    device_id: entry.device_id ?? null,
    channel_id: entry.channel_id ?? null,
  }
}

export const useEventLogStore = defineStore("eventLogStore", () => {
  const events = ref<EventLogUIEntry[]>([])
  const loading = ref(false)
  const error = ref<string | null>(null)
  const loadedSignature = ref<string | null>(null)
  const loadingOlder = ref(false)
  const hasOlder = ref(true)
  let currentRequest: Promise<void> | null = null

  async function fetchAll(limit = 200, force = false) {
    const signature = `${limit}`

    if (!force && loadedSignature.value === signature && events.value.length) {
      return
    }

    if (!force && loading.value && currentRequest) {
      await currentRequest
      return
    }

    const request = (async () => {
      loading.value = true
      error.value = null
      try {
        logger.debug("⏳ Fetching events", { limit })
        const { data } = await axios.get(ApiBuilder.events(), {
          params: { limit },
        })
        const normalized = (data as any[]).map(mapApiToEvent).reverse()
        events.value = normalized.map(event => ({ ...event, highlight: false }))
        hasOlder.value = normalized.length === limit
        loadedSignature.value = signature
        logger.debug(`✅ Loaded ${normalized.length} events`)
      } catch (err: any) {
        error.value = err?.message ?? "Failed to load events"
        logger.error("💥 Failed to fetch events", err)
        throw err
      } finally {
        loading.value = false
        currentRequest = null
      }
    })()

    currentRequest = request
    await request
  }

  async function ensureLoaded(options?: { limit?: number; force?: boolean }) {
    const limit = options?.limit ?? 200
    const force = options?.force ?? false
    if (force) {
      hasOlder.value = true
    }
    await fetchAll(limit, force)
  }

  function push(entry: EventLogEntry) {
    if (events.value.some(e => e.id === entry.id)) {
      return
    }
    const wrapped: EventLogUIEntry = { ...entry, highlight: true }
    events.value.push(wrapped)
    if (events.value.length > 500) {
      events.value.splice(0, events.value.length - 500)
    }
    setTimeout(() => {
      const idx = events.value.indexOf(wrapped)
      if (idx !== -1) {
        events.value[idx] = { ...wrapped, highlight: false }
      }
    }, 1000)
  }

  function clear() {
    events.value = []
    loadedSignature.value = null
    currentRequest = null
    hasOlder.value = true
  }

  function resolveCursor(): number | null {
    for (const entry of events.value) {
      if (typeof entry.id === "number" && Number.isFinite(entry.id) && entry.id > 0) {
        return entry.id
      }
    }
    return null
  }

  async function loadOlder(limit = 200) {
    if (loadingOlder.value || !hasOlder.value) {
      return
    }

    const cursor = resolveCursor()
    if (cursor == null) {
      hasOlder.value = false
      return
    }

    loadingOlder.value = true
    try {
      const { data } = await axios.get(ApiBuilder.events(), {
        params: { limit, cursor },
      })
      const batch = (data as any[]).map(mapApiToEvent)
      if (!batch.length) {
        hasOlder.value = false
        return
      }
      const normalized = batch.reverse()
      const existingIds = new Set(events.value.map(e => e.id))
      const filtered = normalized
        .filter(e => !existingIds.has(e.id))
        .map(event => ({ ...event, highlight: false }))
      if (!filtered.length) {
        hasOlder.value = batch.length >= limit
        return
      }
      events.value = filtered.concat(events.value)
      if (batch.length < limit) {
        hasOlder.value = false
      }
    } catch (err) {
      logger.error("💥 Failed to load older events", err)
      throw err
    } finally {
      loadingOlder.value = false
    }
  }

  function fromWsEvent(event: WSEventLogEvent): EventLogEntry {
    return mapApiToEvent(event.event)
  }

  function handleWs(event: WSEventLogEvent) {
    push(fromWsEvent(event))
  }

  return {
    events,
    loading,
    error,
    hasOlder,
    loadingOlder,
    fetchAll,
    ensureLoaded,
    loadOlder,
    push,
    clear,
    fromWsEvent,
    handleWs,
  }
})
