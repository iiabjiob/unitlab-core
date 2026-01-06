import { http } from "./http"
import { API_V1 } from "./utils"
import type { Project, ProjectCreateInput, ProjectUpdateInput } from "@/types/project"

export const ProjectsAPI = {
  list() {
    return http.get<Project[]>(`${API_V1}/projects`)
  },

  get(id: number | string) {
    return http.get<Project>(`${API_V1}/projects/${id}`)
  },

  create(payload: ProjectCreateInput) {
    return http.post<Project>(`${API_V1}/projects`, payload)
  },

  update(id: number | string, payload: ProjectUpdateInput) {
    return http.patch<Project>(`${API_V1}/projects/${id}`, payload)
  },

  remove(id: number | string) {
    return http.delete<void>(`${API_V1}/projects/${id}`)
  },
}
