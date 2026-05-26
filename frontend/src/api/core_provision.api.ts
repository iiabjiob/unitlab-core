import { httpData } from "./http"
import { API_V1 } from "./utils"
import type { CoreProvisionCommandAccepted, CoreProvisionStateResponse } from "@/types/coreProvision"

export async function fetchCoreProvisionState() {
  return httpData.get<CoreProvisionStateResponse>(`${API_V1}/core-provision/state`)
}

export async function enqueueCoreProvisionStatus() {
  return httpData.post<CoreProvisionCommandAccepted>(`${API_V1}/core-provision/status`)
}

export async function enqueueCoreProvisionSmokeCheck() {
  return httpData.post<CoreProvisionCommandAccepted>(`${API_V1}/core-provision/smoke-check`)
}

export async function enqueueCoreProvisionInstallNetAgent() {
  return httpData.post<CoreProvisionCommandAccepted>(`${API_V1}/core-provision/install/net-agent`)
}

export async function enqueueCoreProvisionInstallNtpAgent() {
  return httpData.post<CoreProvisionCommandAccepted>(`${API_V1}/core-provision/install/ntp-agent`)
}

export async function enqueueCoreProvisionInstallDiagAgent() {
  return httpData.post<CoreProvisionCommandAccepted>(`${API_V1}/core-provision/install/diag-agent`)
}
