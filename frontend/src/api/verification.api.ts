import { http } from "./http"
import { API_V1 } from "./utils"
import type {
  VerificationAutoRunStartPayload,
  VerificationNetworkPreflightResponse,
  VerificationRunDetailResponse,
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
}
