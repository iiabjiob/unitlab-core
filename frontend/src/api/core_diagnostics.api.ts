import { http } from "./http"
import { API_V1 } from "./utils"
import type { CoreDiagnosticsCommandAccepted, CoreDiagnosticsStateResponse } from "@/types/coreDiagnostics"

export async function fetchCoreDiagnosticsState() {
  const { data } = await http.get<CoreDiagnosticsStateResponse>(`${API_V1}/core-diagnostics/state`)
  return data
}

export async function enqueueCoreDiagnosticsStatus() {
  const { data } = await http.post<CoreDiagnosticsCommandAccepted>(`${API_V1}/core-diagnostics/status`)
  return data
}

