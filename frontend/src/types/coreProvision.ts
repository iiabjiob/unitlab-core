export type CoreProvisionMode = "ok" | "degraded" | "error" | "unknown"

export interface CoreProvisionCheck {
  key: string
  label: string
  ok: boolean | null
  detail?: string | null
}

export interface CoreProvisionActionResult {
  action: string
  success: boolean
  message: string
  exit_code?: number | null
  duration_ms?: number | null
}

export interface CoreProvisionRequestInFlight {
  request_id: string
  entry_id?: string
  action: string
}

export interface CoreProvisionSnapshot {
  mode: CoreProvisionMode
  project_root: string
  checks: CoreProvisionCheck[]
  smoke_checks: CoreProvisionCheck[]
  last_action_result?: CoreProvisionActionResult | null
  request_in_flight?: CoreProvisionRequestInFlight | null
  last_event?: string | null
  last_error?: string | null
  updated_at: string
  request_id?: string
}

export interface CoreProvisionStateResponse {
  state: CoreProvisionSnapshot
}

export interface CoreProvisionCommandAccepted {
  request_id: string
  action: string
  queued_at: string
}

