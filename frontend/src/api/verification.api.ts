import { http } from "./http"
import { API_V1 } from "./utils"
import type {
  VerificationAutoRunStartPayload,
  VerificationNetworkPreflightResponse,
  VerificationRunDetailResponse,
  VerificationRuntimeOrchestrationResponse,
} from "@/types/verification"

export const VerificationAPI = {
  startSingleSignalRun(workspaceId: number, payload: VerificationAutoRunStartPayload) {
    return http.post<VerificationRunDetailResponse>(`${API_V1}/workspaces/${workspaceId}/verification/runs`, payload)
  },

  preflightSingleSignalRun(workspaceId: number, payload: VerificationAutoRunStartPayload) {
    return http.post<VerificationNetworkPreflightResponse>(`${API_V1}/workspaces/${workspaceId}/verification/preflight`, payload)
  },

  getRunDetail(workspaceId: number, testRunId: string) {
    return http.get<VerificationRunDetailResponse>(`${API_V1}/workspaces/${workspaceId}/verification/runs/${testRunId}`)
  },

  startOrchestrationFromSignals(workspaceId: number, payload: VerificationAutoRunStartPayload) {
    return http.post<VerificationRuntimeOrchestrationResponse>(`${API_V1}/workspaces/${workspaceId}/verification/orchestrations/from-signals`, payload)
  },

  stopOrchestration(workspaceId: number, orchestrationId: string) {
    return http.post<VerificationRuntimeOrchestrationResponse>(`${API_V1}/workspaces/${workspaceId}/verification/orchestrations/${encodeURIComponent(orchestrationId)}/stop`)
  },
}
