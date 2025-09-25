export type EventDirection = "IN" | "OUT"

export type EventSource = "WS_DEVICE" | "WS_COMMAND"

export interface EventLogEntry {
  id: string                 // unique id (timestamp+counter)
  ts: number                 // unix ms
  dir: EventDirection        // IN / OUT
  source: EventSource        // WS_DEVICE | WS_COMMAND
  channelOrAction: string    // e.g. "devices/state" or "set_do_command"
  unitId?: string            // optional, derived via channel.device.unit_id
  type?: string              // optional, event type
  summary: string            // short human-readable line
  payload?: unknown          // raw event/command (for future modal)
  createdAt: string          // ISO datetime from backend
}
