import { http } from "./http"
import { API_V1 } from "./utils"
import type {
  TestRunRecord,
  TestRunCreatePayloadV2,
  TestRunSignalSnapshot,
  TestRunPreflight,
  TestRunReallocatePayload,
} from "@/types/signal"
import type { SequenceState } from "@/types/sequences"

export const TestRunsAPI = {
  list(workspaceId: number) {
    return http.get<TestRunRecord[]>(`${API_V1}/workspaces/${workspaceId}/test-runs`)
  },

  create(workspaceId: number, payload: Omit<TestRunCreatePayloadV2, "workspace_id">) {
    const request: TestRunCreatePayloadV2 = {
      workspace_id: workspaceId,
      sequence_ids: payload.sequence_ids,
      allocation: payload.allocation,
      allow_empty_allocation: payload.allow_empty_allocation,
    }
    return http.post<TestRunRecord>(`${API_V1}/workspaces/${workspaceId}/test-runs`, request)
  },

  get(runId: number) {
    return http.get<TestRunRecord>(`${API_V1}/test-runs/${runId}`)
  },

  repeat(runId: number) {
    return http.post<TestRunRecord>(`${API_V1}/test-runs/${runId}/repeat`)
  },

  getSignalsSnapshot(runId: number) {
    return http.get<TestRunSignalSnapshot>(`${API_V1}/test-runs/${runId}/signals`)
  },

  preflight(runId: number) {
    return http.get<TestRunPreflight>(`${API_V1}/test-runs/${runId}/preflight`)
  },

  reallocate(runId: number, payload: TestRunReallocatePayload) {
    return http.post<TestRunRecord>(`${API_V1}/test-runs/${runId}/reallocate`, payload)
  },

  exportCableJournal(runId: number) {
    return http.get<string>(`${API_V1}/test-runs/${runId}/cable-journal/export`, {
      responseType: "text",
      transformResponse: [(value: string) => value],
    })
  },

  start(runId: number) {
    return http.post<SequenceState[]>(`${API_V1}/test-runs/${runId}/start`)
  },

  stop(runId: number) {
    return http.post<SequenceState[]>(`${API_V1}/test-runs/${runId}/stop`)
  },
}
