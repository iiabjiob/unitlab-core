import { httpData } from "./http"
import { API_V1 } from "./utils"
import type { CoreNtpCommandAccepted, CoreNtpStateResponse } from "@/types/coreNtp"

export async function fetchCoreNtpState() {
  return httpData.get<CoreNtpStateResponse>(`${API_V1}/core-ntp/state`)
}

export async function enqueueCoreNtpStatus() {
  return httpData.post<CoreNtpCommandAccepted>(`${API_V1}/core-ntp/status`)
}

export async function enqueueCoreNtpApplyServers(payload: { servers: string[] }) {
  return httpData.put<CoreNtpCommandAccepted>(`${API_V1}/core-ntp/servers`, payload)
}

export async function enqueueCoreNtpRestoreDefaults() {
  return httpData.post<CoreNtpCommandAccepted>(`${API_V1}/core-ntp/restore-defaults`)
}

export async function enqueueCoreNtpReload() {
  return httpData.post<CoreNtpCommandAccepted>(`${API_V1}/core-ntp/reload`)
}

export async function enqueueCoreNtpSetTime(payload: { timestamp: string }) {
  return httpData.post<CoreNtpCommandAccepted>(`${API_V1}/core-ntp/set-time`, payload)
}
