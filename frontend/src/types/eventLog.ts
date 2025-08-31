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

export function formatTs(ms: number): string {
  // Compact time, e.g. 12:31:08.123
  const d = new Date(ms);
  const hh = String(d.getHours()).padStart(2, "0");
  const mm = String(d.getMinutes()).padStart(2, "0");
  const ss = String(d.getSeconds()).padStart(2, "0");
  const ms3 = String(d.getMilliseconds()).padStart(3, "0");
  return `${hh}:${mm}:${ss}.${ms3}`;
}
