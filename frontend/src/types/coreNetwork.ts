export type CoreNetMode = "unknown" | "ap" | "sta" | "switching" | "error"
export type CoreNetStaState = "disconnected" | "connecting" | "connected" | "failed"
export type CoreNetIpv4Mode = "auto" | "manual"

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

export interface CoreNetNetworkInterfaceInfo {
  interface_name: string
  local_ip: string | null
  netmask: string | null
  network: string | null
  connection?: string | null
  state?: string | null
}

export interface CoreNetHostNetworkSettings {
  interface: string
  profile: string
  ipv4_mode: CoreNetIpv4Mode
  address_cidr: string | null
  gateway: string | null
  dns_servers: string[]
  proxy_url: string | null
  proxy_no_proxy: string[]
  last_applied_at?: string | null
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
  host_network: CoreNetHostNetworkSettings
  wifi_iface: string
  mac: string | null
  suffix: string | null
  interfaces?: CoreNetNetworkInterfaceInfo[]
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
