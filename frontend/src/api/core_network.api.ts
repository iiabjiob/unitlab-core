import { http } from "./http"
import { API_V1 } from "./utils"
import type { CoreNetworkCommandAccepted, CoreNetworkStateResponse } from "@/types/coreNetwork"

export async function fetchCoreNetworkState() {
  const { data } = await http.get<CoreNetworkStateResponse>(`${API_V1}/core-network/state`)
  return data
}

export async function enqueueCoreNetworkStatus() {
  const { data } = await http.post<CoreNetworkCommandAccepted>(`${API_V1}/core-network/status`)
  return data
}

export async function enqueueCoreNetworkScan(timeoutSec?: number | null) {
  const payload = timeoutSec ? { timeout_sec: timeoutSec } : {}
  const { data } = await http.post<CoreNetworkCommandAccepted>(`${API_V1}/core-network/scan`, payload)
  return data
}

export async function enqueueCoreNetworkConnect(payload: {
  ssid: string
  password?: string | null
  hidden?: boolean
  timeout_sec?: number | null
}) {
  const { data } = await http.post<CoreNetworkCommandAccepted>(`${API_V1}/core-network/connect`, payload)
  return data
}

export async function enqueueCoreNetworkDisconnect() {
  const { data } = await http.post<CoreNetworkCommandAccepted>(`${API_V1}/core-network/disconnect`)
  return data
}

export async function enqueueCoreNetworkRestartAp() {
  const { data } = await http.post<CoreNetworkCommandAccepted>(`${API_V1}/core-network/restart-ap`)
  return data
}

