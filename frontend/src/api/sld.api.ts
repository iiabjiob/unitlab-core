import { http } from "./http"
import { API_V1 } from "./utils"
import type { StoredDiagramState } from "@/pages/switchgears/utils/switchgearSldDiagramTypes"

export const SLD_DOCUMENT_SCHEMA = "unitlab.sld.v1" as const

export type SldDocumentResponse = {
  workspace_id: number
  revision: number
  document_schema: string
  document: StoredDiagramState
  updated_at: string | null
}

export type SldDocumentUpdate = {
  base_revision: number
  document_schema: typeof SLD_DOCUMENT_SCHEMA
  document: StoredDiagramState
  change_kind?: "edit" | "import" | "migration"
}

const basePath = (workspaceId: number | string) => `${API_V1}/workspaces/${workspaceId}/sld`

export const SldAPI = {
  get(workspaceId: number | string) {
    return http.get<SldDocumentResponse>(basePath(workspaceId), { timeout: 5000 })
  },

  save(workspaceId: number | string, payload: SldDocumentUpdate) {
    return http.put<SldDocumentResponse>(basePath(workspaceId), payload)
  },
}
