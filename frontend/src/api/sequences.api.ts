import { http } from "./http"
import { API_V1, buildQuery } from "./utils"
import type { SequenceStep, SequenceStepCreate } from "@/types/sequences"

const basePath = (projectId: number | string) => `${API_V1}/projects/${projectId}/sequences`

export const SequencesAPI = {
  list(projectId: number | string, params?: Record<string, any>) {
    return http.get(buildQuery(basePath(projectId), params))
  },

  get(projectId: number | string, id: number | string) {
    return http.get(`${basePath(projectId)}/${id}`)
  },

  create(projectId: number | string, payload: any) {
    return http.post(basePath(projectId), payload)
  },

  update(projectId: number | string, id: number | string, payload: any) {
    return http.patch(`${basePath(projectId)}/${id}`, payload)
  },

  delete(projectId: number | string, id: number | string) {
    return http.delete(`${basePath(projectId)}/${id}`)
  },

  // Steps
  getSteps(projectId: number | string, seqId: number | string) {
    return http.get<SequenceStep[]>(`${basePath(projectId)}/${seqId}/steps`)
  },

  addStep(projectId: number | string, seqId: number | string, payload: SequenceStepCreate) {
    return http.post<SequenceStep>(`${basePath(projectId)}/${seqId}/steps`, payload)
  },

  updateStep(
    projectId: number | string,
    seqId: number | string,
    stepId: number | string,
    payload: Partial<SequenceStep>,
  ) {
    return http.patch<SequenceStep>(`${basePath(projectId)}/${seqId}/steps/${stepId}`, payload)
  },

  deleteStep(projectId: number | string, seqId: number | string, stepId: number | string) {
    return http.delete(`${basePath(projectId)}/${seqId}/steps/${stepId}`)
  },

  reorderSteps(projectId: number | string, seqId: number | string, payload: { new_order: number[] }) {
    return http.post<SequenceStep[]>(`${basePath(projectId)}/${seqId}/steps/reorder`, payload)
  },

  replaceSteps(projectId: number | string, seqId: number | string, payload: SequenceStepCreate[]) {
    return http.put<SequenceStep[]>(`${basePath(projectId)}/${seqId}/steps`, payload)
  },

  // Execution
  start(projectId: number | string, seqId: number | string) {
    return http.post(`${basePath(projectId)}/${seqId}/start`)
  },

  stop(projectId: number | string, seqId: number | string) {
    return http.post(`${basePath(projectId)}/${seqId}/stop`)
  },

  getState(projectId: number | string, seqId: number | string) {
    return http.get(`${basePath(projectId)}/${seqId}/state`)
  },

  export(projectId: number | string, seqId: number | string) {
    return http.get(`${basePath(projectId)}/${seqId}/export-file`)
  },

  import(projectId: number | string, formData: FormData) {
    return http.post(`${basePath(projectId)}/import-file`, formData)
  },
}
