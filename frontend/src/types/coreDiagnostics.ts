export type CoreDiagnosticsMode = "ok" | "degraded" | "error" | "unknown"

export interface CoreDiagHostServiceStatus {
  name: string
  active: boolean | null
}

export interface CoreDiagCpu {
  temperature_c: number | null
  load_1m: number | null
  load_5m: number | null
  load_15m: number | null
}

export interface CoreDiagMemory {
  total_bytes: number | null
  available_bytes: number | null
  used_bytes: number | null
  used_percent: number | null
}

export interface CoreDiagDisk {
  mountpoint: string
  total_bytes: number | null
  free_bytes: number | null
  used_bytes: number | null
  used_percent: number | null
}

export interface CoreDiagRequestInFlight {
  request_id: string
  entry_id?: string
  action: string
}

export interface CoreDiagnosticsSnapshot {
  mode: CoreDiagnosticsMode
  hostname: string | null
  model: string | null
  os_pretty_name: string | null
  kernel: string | null
  time_utc: string | null
  uptime_seconds: number | null
  cpu: CoreDiagCpu
  memory: CoreDiagMemory
  disk_root: CoreDiagDisk
  services: CoreDiagHostServiceStatus[]
  request_in_flight?: CoreDiagRequestInFlight | null
  last_event?: string | null
  last_error?: string | null
  updated_at: string
  request_id?: string
}

export interface CoreDiagnosticsStateResponse {
  state: CoreDiagnosticsSnapshot
}

export interface CoreDiagnosticsCommandAccepted {
  request_id: string
  action: string
  queued_at: string
}

