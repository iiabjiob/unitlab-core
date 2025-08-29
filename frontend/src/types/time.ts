export interface TimeStatus {
  timestamp: string       // ISO8601
  status: "synced" | "unsynced"
  source?: string
  offset_us?: number | null
}
