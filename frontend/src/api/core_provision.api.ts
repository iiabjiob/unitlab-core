import { http } from "./http"
import { API_V1 } from "./utils"
import type { CoreProvisionCommandAccepted, CoreProvisionStateResponse } from "@/types/coreProvision"

export async function fetchCoreProvisionState() {
  const { data } = await http.get<CoreProvisionStateResponse>(`${API_V1}/core-provision/state`)
  return data
}

export async function enqueueCoreProvisionStatus() {
  const { data } = await http.post<CoreProvisionCommandAccepted>(`${API_V1}/core-provision/status`)
  return data
}

export async function enqueueCoreProvisionSmokeCheck() {
  const { data } = await http.post<CoreProvisionCommandAccepted>(`${API_V1}/core-provision/smoke-check`)
  return data
}

export async function enqueueCoreProvisionInstallNetAgent() {
  const { data } = await http.post<CoreProvisionCommandAccepted>(`${API_V1}/core-provision/install/net-agent`)
  return data
}

export async function enqueueCoreProvisionInstallNtpAgent() {
  const { data } = await http.post<CoreProvisionCommandAccepted>(`${API_V1}/core-provision/install/ntp-agent`)
  return data
}

export async function enqueueCoreProvisionInstallDiagAgent() {
  const { data } = await http.post<CoreProvisionCommandAccepted>(`${API_V1}/core-provision/install/diag-agent`)
  return data
}

