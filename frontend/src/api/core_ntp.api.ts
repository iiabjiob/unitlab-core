import { http } from "./http"
import { API_V1 } from "./utils"
import type { CoreNtpCommandAccepted, CoreNtpStateResponse } from "@/types/coreNtp"

export async function fetchCoreNtpState() {
  const { data } = await http.get<CoreNtpStateResponse>(`${API_V1}/core-ntp/state`)
  return data
}

export async function enqueueCoreNtpStatus() {
  const { data } = await http.post<CoreNtpCommandAccepted>(`${API_V1}/core-ntp/status`)
  return data
}

export async function enqueueCoreNtpApplyServers(payload: { servers: string[] }) {
  const { data } = await http.put<CoreNtpCommandAccepted>(`${API_V1}/core-ntp/servers`, payload)
  return data
}

export async function enqueueCoreNtpRestoreDefaults() {
  const { data } = await http.post<CoreNtpCommandAccepted>(`${API_V1}/core-ntp/restore-defaults`)
  return data
}

export async function enqueueCoreNtpReload() {
  const { data } = await http.post<CoreNtpCommandAccepted>(`${API_V1}/core-ntp/reload`)
  return data
}

export async function enqueueCoreNtpSetTime(payload: { timestamp: string }) {
  const { data } = await http.post<CoreNtpCommandAccepted>(`${API_V1}/core-ntp/set-time`, payload)
  return data
}

