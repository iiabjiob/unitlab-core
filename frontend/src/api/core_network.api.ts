import { httpData } from "./http"
import { API_V1 } from "./utils"
import type { CoreNetworkCommandAccepted, CoreNetworkStateResponse } from "@/types/coreNetwork"

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
