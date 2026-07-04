import { http, type HttpRequestOptions } from "./http"
import { API_V1 } from "./utils"
import type {
  VerificationAutoRunStartPayload,
  VerificationExternalIedTargetsRequest,
  VerificationExternalIedDiscoveryTreeResponse,
  VerificationExternalIedManualReportRequest,
  VerificationExternalIedManualReportResponse,
  VerificationMmsReachabilityRequest,
  VerificationMmsReachabilityResponse,
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

  checkMmsReachability(workspaceId: number, payload: VerificationMmsReachabilityRequest, options?: HttpRequestOptions) {
    return http.post<VerificationMmsReachabilityResponse>(
      `${API_V1}/workspaces/${workspaceId}/verification/mms-reachability`,
      payload,
      options,
    )
  },

  configureExternalIedTargets(workspaceId: number, payload: VerificationExternalIedTargetsRequest, options?: HttpRequestOptions) {
    return http.put(
      `${API_V1}/workspaces/${workspaceId}/verification/external-ieds/targets`,
      payload,
      options,
    )
  },

  refreshExternalIedDiscovery(workspaceId: number, ip: string, port = 102, options?: HttpRequestOptions) {
    return http.post(
      `${API_V1}/workspaces/${workspaceId}/verification/external-ieds/${encodeURIComponent(`${ip}:${port}`)}/discovery/refresh`,
      {},
      options,
    )
  },

  getExternalIedDiscoveryTree(workspaceId: number, ip: string, port = 102, options?: HttpRequestOptions) {
    return http.get<VerificationExternalIedDiscoveryTreeResponse>(
      `${API_V1}/workspaces/${workspaceId}/verification/external-ieds/${encodeURIComponent(`${ip}:${port}`)}/discovery/tree`,
      options,
    )
  },

  setExternalIedReportEnabled(workspaceId: number, ip: string, port: number, payload: VerificationExternalIedManualReportRequest, enabled: boolean, options?: HttpRequestOptions) {
    return http.post<VerificationExternalIedManualReportResponse>(
      `${API_V1}/workspaces/${workspaceId}/verification/external-ieds/${encodeURIComponent(`${ip}:${port}`)}/reports/${enabled ? "enable" : "disable"}`,
      payload,
      options,
    )
  },

  getRunDetail(workspaceId: number, testRunId: string) {
    return http.get<VerificationRunDetailResponse>(`${API_V1}/workspaces/${workspaceId}/verification/runs/${testRunId}`)
  },

}
