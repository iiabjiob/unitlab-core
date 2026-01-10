import { http } from "./http"
import { API_V1 } from "./utils"
import type { Workspace, WorkspaceCreateInput, WorkspaceUpdateInput } from "@/types/workspace"

export const WorkspacesAPI = {
  list() {
    return http.get<Workspace[]>(`${API_V1}/workspaces`)
  },

  get(id: number | string) {
    return http.get<Workspace>(`${API_V1}/workspaces/${id}`)
  },

  create(payload: WorkspaceCreateInput) {
    return http.post<Workspace>(`${API_V1}/workspaces`, payload)
  },

  update(id: number | string, payload: WorkspaceUpdateInput) {
    return http.patch<Workspace>(`${API_V1}/workspaces/${id}`, payload)
  },

  remove(id: number | string) {
    return http.delete<void>(`${API_V1}/workspaces/${id}`)
  },
}
