export type WorkerStatus = "online" | "degraded" | "offline" | "unknown"

export interface WorkerHealth {
  name: string
  display_name: string
  status: WorkerStatus
  last_seen_at: string | null
  impact: string
  detail?: string | null
}

export type SystemStatus = "online" | "degraded" | "offline"

export interface SystemHealthResponse {
  status: SystemStatus
  checked_at: string
  issues: string[]
  workers: WorkerHealth[]
}
