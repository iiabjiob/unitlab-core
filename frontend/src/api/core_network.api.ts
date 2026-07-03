import { httpData } from "./http"
import { API_V1 } from "./utils"
import type { CoreNetworkCommandAccepted, CoreNetworkStateResponse, CoreNetIpv4Mode } from "@/types/coreNetwork"

export async function fetchCoreNetworkState() {
  return httpData.get<CoreNetworkStateResponse>(`${API_V1}/core-network/state`)
}

export async function enqueueCoreNetworkStatus() {
  return httpData.post<CoreNetworkCommandAccepted>(`${API_V1}/core-network/status`)
}

export async function enqueueCoreNetworkScan(timeoutSec?: number | null) {
  const payload = timeoutSec ? { timeout_sec: timeoutSec } : {}
  return httpData.post<CoreNetworkCommandAccepted>(`${API_V1}/core-network/scan`, payload)
}

export async function enqueueCoreNetworkConnect(payload: {
  ssid: string
  password?: string | null
  hidden?: boolean
  timeout_sec?: number | null
}) {
  return httpData.post<CoreNetworkCommandAccepted>(`${API_V1}/core-network/connect`, payload)
}

export async function enqueueCoreNetworkDisconnect() {
  return httpData.post<CoreNetworkCommandAccepted>(`${API_V1}/core-network/disconnect`)
}

export async function enqueueCoreNetworkRestartAp() {
  return httpData.post<CoreNetworkCommandAccepted>(`${API_V1}/core-network/restart-ap`)
}

export async function applyCoreNetworkSettings(payload: {
  interface?: string | null
  profile?: string | null
  ipv4_mode: CoreNetIpv4Mode
  address_cidr?: string | null
  gateway?: string | null
  dns_servers?: string[]
  proxy_url?: string | null
  proxy_no_proxy?: string[]
}) {
  return httpData.put<CoreNetworkCommandAccepted>(`${API_V1}/core-network/settings`, payload)
}

export async function enqueueCoreNetworkAddressProbe(payload: {
  interface: string
  addresses: string[]
  timeout_sec?: number | null
}) {
  return httpData.post<CoreNetworkCommandAccepted>(`${API_V1}/core-network/probe-addresses`, payload)
}

export async function enqueueCoreNetworkRestoreSettings() {
  return httpData.post<CoreNetworkCommandAccepted>(`${API_V1}/core-network/restore-settings`)
}
