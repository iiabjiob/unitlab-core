export type EventDirection = "IN" | "OUT";

export type EventSource = "WS_DEVICE" | "WS_COMMAND";

export interface EventLogEntry {
  id: string;               // unique id (timestamp+counter)
  ts: number;               // unix ms
  dir: EventDirection;      // IN / OUT
  source: EventSource;      // WS_DEVICE | WS_COMMAND
  channelOrAction: string;  // e.g. "devices/state" or "set_do_command"
  unit_id?: string;
  type?: string;
  summary: string;          // short human-readable line
  payload?: unknown;        // raw event/command (for future modal)
}
