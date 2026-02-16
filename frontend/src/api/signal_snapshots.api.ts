import { http } from "./http"
import { API_V1 } from "./utils"
import type {
  Allocation,
  AllocationMappingItem,
  SignalImportMeta,
  SignalSnapshot,
  SignalSnapshotSummary,
} from "@/types/signal"

export const SignalSnapshotsAPI = {
  list(workspaceId: number, options?: { limit?: number; offset?: number }) {
    return http.get<SignalSnapshotSummary[]>(`${API_V1}/workspaces/${workspaceId}/signal-snapshots`, {
      params: {
        limit: options?.limit,
        offset: options?.offset,
      },
    })
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
