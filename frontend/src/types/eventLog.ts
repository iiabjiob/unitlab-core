export type EventType = "cmd" | "state" | "system" | "sequence" | "status"
export type EventDirection = "in" | "out" | null
export type EventResult = "ok" | "error" | "timeout" | "pending" | null

export interface EventLogEntry {
  id: number
  ts: string
  project_id?: number | null
  event_type: EventType
  source: string
  direction?: EventDirection
  result?: EventResult
  payload?: Record<string, unknown> | null
  message?: string | null
  packet_id?: string | null
  datapoint_id?: number | null
  device_id?: number | null
  channel_id?: number | null
}
