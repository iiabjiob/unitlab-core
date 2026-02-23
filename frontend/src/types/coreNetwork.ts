export type CoreNetMode = "unknown" | "ap" | "sta" | "switching" | "error"
export type CoreNetStaState = "disconnected" | "connecting" | "connected" | "failed"

export interface CoreNetWifiNetwork {
  ssid: string
  signal: number | null
  security: string | null
  in_use: boolean
}

export interface CoreNetApInfo {
  ssid: string
  password: string
  profile: string
  iface: string
  ip: string | null
  active: boolean
}

export interface CoreNetStaInfo {
  state: CoreNetStaState
  ssid: string | null
  profile: string | null
  ip: string | null
  last_error?: string | null
}

export interface CoreNetRequestInFlight {
  request_id: string
  entry_id?: string
  action: string
}

export interface CoreNetworkSnapshot {
  mode: CoreNetMode
  ap: CoreNetApInfo
  sta: CoreNetStaInfo
  wifi_iface: string
  mac: string | null
  suffix: string | null
  request_in_flight?: CoreNetRequestInFlight | null
  last_event?: string | null
  last_error?: string | null
  available_networks?: CoreNetWifiNetwork[]
  updated_at: string
  request_id?: string
}

export interface CoreNetworkStateResponse {
  state: CoreNetworkSnapshot
}

export interface CoreNetworkCommandAccepted {
  request_id: string
  action: string
  queued_at: string
}

