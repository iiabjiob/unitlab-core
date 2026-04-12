export type CoreNtpMode = "ok" | "degraded" | "error" | "unknown"

export interface CoreNtpTracking {
  synced: boolean
  source?: string | null
  stratum?: number | null
  ref_time_utc?: string | null
  system_time_offset_seconds?: number | null
  last_offset_seconds?: number | null
  rms_offset_seconds?: number | null
  frequency_ppm?: number | null
  residual_freq_ppm?: number | null
  skew_ppm?: number | null
  root_delay_seconds?: number | null
  root_dispersion_seconds?: number | null
  update_interval_seconds?: number | null
  leap_status?: string | null
  raw?: Record<string, string>
}

export interface CoreNtpSource {
  mode_mark?: string | null
  state_mark?: string | null
  name: string
  stratum?: number | null
  poll?: number | null
  reach?: number | null
  last_rx?: string | null
  last_sample?: string | null
  raw_line?: string | null
}

export interface CoreNtpRequestInFlight {
  request_id: string
  entry_id?: string
  action: string
}

export interface CoreNtpSnapshot {
  mode: CoreNtpMode
  chrony_service_active: boolean | null
  chrony_service_name: string | null
  system_time_utc?: string | null
  system_time_local?: string | null
  configured_servers: string[]
  effective_servers: string[]
  tracking: CoreNtpTracking | null
  sources: CoreNtpSource[]
  request_in_flight?: CoreNtpRequestInFlight | null
  last_event?: string | null
  last_error?: string | null
  updated_at: string
  request_id?: string
}

export interface CoreNtpStateResponse {
  state: CoreNtpSnapshot
}

export interface CoreNtpCommandAccepted {
  request_id: string
  action: string
  queued_at: string
}

