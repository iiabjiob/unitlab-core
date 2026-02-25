import { http } from "./http"
import { API_V1 } from "./utils"
import type {
  SignalAllocationRow,
  SignalAllocationEnsurePayload,
  SignalAllocationEnsureResponse,
  SignalAllocationJob,
  SignalAllocationMarkTestedPayload,
  SignalAllocationUpdateItem,
  SignalAutoAllocatePayload,
  SignalAutoAllocateResponse,
  SignalImportMeta,
  SignalSheet,
  SignalSheetImportResponse,
  SignalSheetImportPreviewResponse,
  SignalSheetPreset,
} from "@/types/signal"

const SIGNAL_IMPORT_REQUEST_TIMEOUT_MS = 120_000

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
      timeout: SIGNAL_IMPORT_REQUEST_TIMEOUT_MS,
    })
  },

  previewImport(workspaceId: number, file: File, options?: {
    metadata?: SignalImportMeta | null
    presetId?: number | null
  }) {
    const formData = new FormData()
    formData.append("file", file)
    if (options?.metadata) {
      formData.append("metadata", JSON.stringify(options.metadata))
    }
    if (Number.isFinite(options?.presetId)) {
      formData.append("preset_id", String(options?.presetId))
    }
    return http.post<SignalSheetImportPreviewResponse>(
      `${API_V1}/workspaces/${workspaceId}/signal-sheet/import/preview`,
      formData,
      {
        headers: { "Content-Type": "multipart/form-data" },
        timeout: SIGNAL_IMPORT_REQUEST_TIMEOUT_MS,
      },
    )
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

  listAllocations(workspaceId: number, options?: { offset?: number; limit?: number }) {
    const params: Record<string, number> = {}
    if (Number.isFinite(options?.offset)) {
      params.offset = Math.max(0, Number(options?.offset))
    }
    if (Number.isFinite(options?.limit)) {
      params.limit = Math.max(0, Number(options?.limit))
    }
    return http.get<SignalAllocationRow[]>(`${API_V1}/workspaces/${workspaceId}/signal-allocations`, {
      params,
    })
  },

  async streamAllocations(workspaceId: number): Promise<SignalAllocationRow[]> {
    const response = await fetch(`${API_V1}/workspaces/${workspaceId}/signal-allocations.ndjson`, {
      method: "GET",
      headers: {
        Accept: "application/x-ndjson",
      },
      cache: "no-store",
    })

    if (!response.ok) {
      throw new Error(`Failed to stream signal allocations (${response.status})`)
    }

    if (!response.body) {
      return []
    }

    const reader = response.body.getReader()
    const decoder = new TextDecoder()
    const rows: SignalAllocationRow[] = []
    let buffer = ""

    while (true) {
      const { value, done } = await reader.read()
      if (done) break
      buffer += decoder.decode(value, { stream: true })

      let lineBreakIndex = buffer.indexOf("\n")
      while (lineBreakIndex !== -1) {
        const line = buffer.slice(0, lineBreakIndex).trim()
        buffer = buffer.slice(lineBreakIndex + 1)
        if (line.length > 0) {
          rows.push(JSON.parse(line) as SignalAllocationRow)
        }
        lineBreakIndex = buffer.indexOf("\n")
      }
    }

    const tail = buffer.trim()
    if (tail.length > 0) {
      rows.push(JSON.parse(tail) as SignalAllocationRow)
    }

    return rows
  },

  updateAllocations(workspaceId: number, entries: SignalAllocationUpdateItem[]) {
    return http.put<SignalAllocationRow[]>(`${API_V1}/workspaces/${workspaceId}/signal-allocations`, {
      entries,
    })
  },

  autoAllocate(workspaceId: number, payload: SignalAutoAllocatePayload) {
    return http.post<SignalAutoAllocateResponse>(`${API_V1}/workspaces/${workspaceId}/signal-allocations/auto`, payload)
  },

  enqueueAutoAllocateJob(workspaceId: number, payload: SignalAutoAllocatePayload) {
    return http.post<SignalAllocationJob>(`${API_V1}/workspaces/${workspaceId}/signal-allocations/auto/jobs`, payload)
  },

  enqueueBulkAllocationJob(workspaceId: number, entries: SignalAllocationUpdateItem[]) {
    return http.post<SignalAllocationJob>(`${API_V1}/workspaces/${workspaceId}/signal-allocations/jobs`, {
      entries,
    })
  },

  enqueueTestRunJob(workspaceId: number, payload: {
    signal_ids: number[]
    signal_interval_ms?: number
    toggle_mode?: "single" | "double"
    resume_from_cursor?: boolean
    resume_job_id?: string
  }) {
    return http.post<SignalAllocationJob>(`${API_V1}/workspaces/${workspaceId}/signal-allocations/test-run/jobs`, payload)
  },

  getAllocationJob(workspaceId: number, jobId: string) {
    return http.get<SignalAllocationJob>(`${API_V1}/workspaces/${workspaceId}/signal-allocation-jobs/${jobId}`)
  },

  controlAllocationJob(workspaceId: number, jobId: string, action: "pause" | "resume" | "stop") {
    return http.post<SignalAllocationJob>(
      `${API_V1}/workspaces/${workspaceId}/signal-allocation-jobs/${jobId}/control`,
      { action },
    )
  },

  ensureAllocated(workspaceId: number, payload: SignalAllocationEnsurePayload) {
    return http.post<SignalAllocationEnsureResponse>(`${API_V1}/workspaces/${workspaceId}/signal-allocations/ensure`, payload)
  },

  markTested(workspaceId: number, payload: SignalAllocationMarkTestedPayload) {
    return http.post<SignalAllocationRow[]>(`${API_V1}/workspaces/${workspaceId}/signal-allocations/tested`, payload)
  },
}
