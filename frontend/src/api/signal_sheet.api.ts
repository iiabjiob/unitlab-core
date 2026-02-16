import { http } from "./http"
import { API_V1 } from "./utils"
import type {
  SignalAllocationRow,
  SignalAllocationEnsurePayload,
  SignalAllocationEnsureResponse,
  SignalAllocationMarkTestedPayload,
  SignalAllocationUpdateItem,
  SignalAutoAllocatePayload,
  SignalAutoAllocateResponse,
  SignalImportMeta,
  SignalSheet,
  SignalSheetImportResponse,
  SignalSheetPreset,
} from "@/types/signal"

export const SignalSheetAPI = {
  get(workspaceId: number) {
    return http.get<SignalSheet>(`${API_V1}/workspaces/${workspaceId}/signal-sheet`)
  },

  import(workspaceId: number, file: File, options?: {
    metadata?: SignalImportMeta | null
    presetId?: number | null
    savePresetName?: string | null
  }) {
    const formData = new FormData()
    formData.append("file", file)
    if (options?.metadata) {
      formData.append("metadata", JSON.stringify(options.metadata))
    }
    if (Number.isFinite(options?.presetId)) {
      formData.append("preset_id", String(options?.presetId))
    }
    if (options?.savePresetName?.trim()) {
      formData.append("save_preset_name", options.savePresetName.trim())
    }
    return http.post<SignalSheetImportResponse>(`${API_V1}/workspaces/${workspaceId}/signal-sheet/import`, formData, {
      headers: { "Content-Type": "multipart/form-data" },
    })
  },

  listPresets(workspaceId: number) {
    return http.get<SignalSheetPreset[]>(`${API_V1}/workspaces/${workspaceId}/signal-sheet/presets`)
  },

  savePreset(workspaceId: number, payload: { name: string; import_meta: SignalImportMeta }) {
    return http.post<SignalSheetPreset>(`${API_V1}/workspaces/${workspaceId}/signal-sheet/presets`, payload)
  },

  deletePreset(presetId: number) {
    return http.delete<void>(`${API_V1}/signal-sheet/presets/${presetId}`)
  },

  listAllocations(workspaceId: number) {
    return http.get<SignalAllocationRow[]>(`${API_V1}/workspaces/${workspaceId}/signal-allocations`)
  },

  updateAllocations(workspaceId: number, entries: SignalAllocationUpdateItem[]) {
    return http.put<SignalAllocationRow[]>(`${API_V1}/workspaces/${workspaceId}/signal-allocations`, {
      entries,
    })
  },

  autoAllocate(workspaceId: number, payload: SignalAutoAllocatePayload) {
    return http.post<SignalAutoAllocateResponse>(`${API_V1}/workspaces/${workspaceId}/signal-allocations/auto`, payload)
  },

  ensureAllocated(workspaceId: number, payload: SignalAllocationEnsurePayload) {
    return http.post<SignalAllocationEnsureResponse>(`${API_V1}/workspaces/${workspaceId}/signal-allocations/ensure`, payload)
  },

  markTested(workspaceId: number, payload: SignalAllocationMarkTestedPayload) {
    return http.post<SignalAllocationRow[]>(`${API_V1}/workspaces/${workspaceId}/signal-allocations/tested`, payload)
  },
}
