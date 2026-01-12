import { http } from "./http"
import { API_V1 } from "./utils"
import type {
  Allocation,
  AllocationMappingItem,
  SignalImportMeta,
  SignalSnapshot,
  SignalSnapshotSummary,
  TestRun,
  TestRunCreatePayload,
} from "@/types/signal"
import type { SequenceState } from "@/types/sequences"

export const SignalSnapshotsAPI = {
  list(workspaceId: number) {
    return http.get<SignalSnapshotSummary[]>(`${API_V1}/workspaces/${workspaceId}/signal-snapshots`)
  },

  import(workspaceId: number, file: File, metadata?: SignalImportMeta) {
    const formData = new FormData()
    formData.append("file", file)
    if (metadata) {
      formData.append("metadata", JSON.stringify(metadata))
    }
    return http.post<SignalSnapshot>(`${API_V1}/workspaces/${workspaceId}/signal-snapshots/import`, formData, {
      headers: { "Content-Type": "multipart/form-data" },
    })
  },

  get(snapshotId: number) {
    return http.get<SignalSnapshot>(`${API_V1}/signal-snapshots/${snapshotId}`)
  },

  delete(snapshotId: number) {
    return http.delete<void>(`${API_V1}/signal-snapshots/${snapshotId}`)
  },

  lock(snapshotId: number) {
    return http.post<SignalSnapshot>(`${API_V1}/signal-snapshots/${snapshotId}/lock`)
  },

  getAllocation(snapshotId: number) {
    return http.get<Allocation>(`${API_V1}/signal-snapshots/${snapshotId}/allocation`)
  },

  updateAllocation(snapshotId: number, mapping: AllocationMappingItem[]) {
    return http.put<Allocation>(`${API_V1}/signal-snapshots/${snapshotId}/allocation`, {
      mapping,
    })
  },
}

export const TestRunsAPI = {
  list(workspaceId: number) {
    return http.get<TestRun[]>(`${API_V1}/workspaces/${workspaceId}/test-runs`)
  },

  create(workspaceId: number, payload: TestRunCreatePayload) {
    return http.post<TestRun>(`${API_V1}/workspaces/${workspaceId}/test-runs`, payload)
  },

  get(runId: number) {
    return http.get<TestRun>(`${API_V1}/test-runs/${runId}`)
  },

  repeat(runId: number) {
    return http.post<TestRun>(`${API_V1}/test-runs/${runId}/repeat`)
  },

  start(runId: number) {
    return http.post<SequenceState[]>(`${API_V1}/test-runs/${runId}/start`)
  },

  stop(runId: number) {
    return http.post<SequenceState[]>(`${API_V1}/test-runs/${runId}/stop`)
  },
}
