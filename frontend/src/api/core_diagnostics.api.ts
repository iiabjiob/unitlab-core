import { httpData } from "./http"
import { API_V1 } from "./utils"
import type { CoreDiagnosticsCommandAccepted, CoreDiagnosticsStateResponse } from "@/types/coreDiagnostics"

export async function fetchCoreDiagnosticsState() {
  return httpData.get<CoreDiagnosticsStateResponse>(`${API_V1}/core-diagnostics/state`)
}

export async function enqueueCoreDiagnosticsStatus() {
  return httpData.post<CoreDiagnosticsCommandAccepted>(`${API_V1}/core-diagnostics/status`)
}
